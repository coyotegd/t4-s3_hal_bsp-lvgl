#include "rm690b0.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "rm690b0";

// Pin Map from t4s3pins.txt
#define PIN_NUM_QSPI_D0   14
#define PIN_NUM_QSPI_D1   10
#define PIN_NUM_QSPI_D2   16
#define PIN_NUM_QSPI_D3   12
#define PIN_NUM_QSPI_SCK  15
#define PIN_NUM_QSPI_CS   11
#define PIN_NUM_LCD_RST   13
#define PIN_NUM_PMIC_EN   9
#define PIN_NUM_TE        18

static spi_device_handle_t spi_handle;

void rm690b0_send_cmd(uint8_t cmd, const uint8_t *data, size_t len) {
    // ESP_LOGI(TAG, "Cmd: 0x%02X, Len: %d", cmd, len);
    
    // Send the Command Byte using Opcode 0x02
    // The actual command is sent in the Address phase (24 bits: 0x00, cmd, 0x00)
    // Data follows immediately in the same transaction (Standard SPI)
    spi_transaction_ext_t t = {
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR,
            .cmd = 0x02,
            .addr = ((uint32_t)cmd) << 8,
            .length = len * 8,
            .tx_buffer = data,
        },
        .command_bits = 8,
        .address_bits = 24,
    };
    spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t);
}

void rm690b0_send_pixels(const uint8_t *data, size_t len) {
    // Pixel Data uses Opcode 0x32 and Quad Mode (QOUT: 1-bit Cmd, 1-bit Addr, 4-bit Data)
    // Address is 0x003C00 (RAMWR_CONTINUE) - Matches Arduino_ESP32QSPI.cpp
    spi_transaction_ext_t t = {
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_MODE_QIO,
            .cmd = 0x32,
            .addr = 0x003C00,
            .length = len * 8,
            .tx_buffer = data,
        },
        .command_bits = 8,
        .address_bits = 24,
    };
    spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t);
}

void rm690b0_read_id(uint8_t *id) {
    // Read ID using Opcode 0x03 (Read Data)
    // Address: 0x00, 0x04 (RDDID), 0x00
    spi_transaction_ext_t t = {
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_USE_RXDATA,
            .cmd = 0x03, // Read Opcode
            .addr = ((uint32_t)0x04) << 8, // RDDID
            .length = 0,
            .rxlength = 24, // Read 3 bytes
        },
        .command_bits = 8,
        .address_bits = 24,
    };
    esp_err_t ret = spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t);
    if (ret == ESP_OK) {
        id[0] = t.base.rx_data[0];
        id[1] = t.base.rx_data[1];
        id[2] = t.base.rx_data[2];
        ESP_LOGI(TAG, "Read ID: %02X %02X %02X", id[0], id[1], id[2]);
    } else {
        ESP_LOGE(TAG, "Failed to read ID");
    }
}

void rm690b0_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    uint8_t caset[] = { (x1 >> 8), (x1 & 0xFF), (x2 >> 8), (x2 & 0xFF) };
    uint8_t raset[] = { (y1 >> 8), (y1 & 0xFF), (y2 >> 8), (y2 & 0xFF) };
    rm690b0_send_cmd(RM690B0_CASET, caset, 4);
    rm690b0_send_cmd(RM690B0_RASET, raset, 4);
}

void rm690b0_set_rotation(rm690b0_rotation_t rot) {
    uint8_t madctr = (rot == RM690B0_ROT_90_CCW) ? 0x60 : 0x00;
    rm690b0_send_cmd(RM690B0_MADCTR, &madctr, 1);
}

void rm690b0_init(void) {
    ESP_LOGI(TAG, "Initializing RM690B0...");

    // Enable PMIC/Display Power (GPIO 9)
    gpio_set_direction(PIN_NUM_PMIC_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_PMIC_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Configure TE Pin (Input)
    gpio_set_direction(PIN_NUM_TE, GPIO_MODE_INPUT);

    // Reset Sequence (High -> Low -> High)
    gpio_set_direction(PIN_NUM_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(PIN_NUM_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(300));
    gpio_set_level(PIN_NUM_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    spi_bus_config_t buscfg = {
        .data0_io_num = PIN_NUM_QSPI_D0,
        .data1_io_num = PIN_NUM_QSPI_D1,
        .sclk_io_num = PIN_NUM_QSPI_SCK,
        .data2_io_num = PIN_NUM_QSPI_D2,
        .data3_io_num = PIN_NUM_QSPI_D3,
        .max_transfer_sz = LOGICAL_WIDTH * LOGICAL_HEIGHT * 2 + 1024,
    };
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return;
    }

    spi_device_interface_config_t devcfg = {
        .command_bits = 8,
        .clock_speed_hz = 20 * 1000 * 1000, // 20MHz
        .mode = 0,
        .spics_io_num = PIN_NUM_QSPI_CS,
        .queue_size = 10,
        .flags = SPI_DEVICE_HALFDUPLEX, // Removed 3WIRE
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);

    // Initialization Sequence from LilyGo-AMOLED-Series
    // Matches Arduino_RM690B0.cpp EXACTLY
    
    uint8_t param_fe_20[] = {0x20};
    rm690b0_send_cmd(0xFE, param_fe_20, 1);

    // Read ID removed to match LilyGo sequence
    
    uint8_t param_26_0a[] = {0x0A};
    rm690b0_send_cmd(0x26, param_26_0a, 1);

    uint8_t param_24_80[] = {0x80};
    rm690b0_send_cmd(0x24, param_24_80, 1);

    uint8_t param_5a_51[] = {0x51};
    rm690b0_send_cmd(0x5A, param_5a_51, 1);

    uint8_t param_5b_2e[] = {0x2E};
    rm690b0_send_cmd(0x5B, param_5b_2e, 1);

    uint8_t param_fe_00[] = {0x00};
    rm690b0_send_cmd(0xFE, param_fe_00, 1);

    uint8_t param_3a_55[] = {0x55};
    rm690b0_send_cmd(RM690B0_COLMOD, param_3a_55, 1);

    rm690b0_send_cmd(0xC2, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t param_35_00[] = {0x00};
    rm690b0_send_cmd(0x35, param_35_00, 1);

    uint8_t param_51_00[] = {0x00};
    rm690b0_send_cmd(RM690B0_WRDISBV, param_51_00, 1);

    rm690b0_send_cmd(RM690B0_SLPOUT, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));

    rm690b0_send_cmd(RM690B0_DISPON, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t param_51_ff[] = {0xFF};
    rm690b0_send_cmd(RM690B0_WRDISBV, param_51_ff, 1);
    ESP_LOGI(TAG, "Initialization Done");
}