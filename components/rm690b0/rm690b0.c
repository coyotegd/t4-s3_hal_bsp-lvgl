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
    // Pixel Data uses Opcode 0x32
    // Use SPI_TRANS_MODE_QIO: Command (1-bit), Address (4-bit), Data (4-bit)
    // But we want 1-bit address.
    // ESP-IDF SPI Master driver defines:
    // SPI_TRANS_MODE_DIO/QIO: Address and Data are sent in 2/4-bit mode.
    // SPI_TRANS_MODE_DIOQIO_ADDR: Address is sent in 2/4-bit mode (if DIO/QIO flag is set).
    // If we want 1-bit address and 4-bit data, we should NOT use SPI_TRANS_MODE_QIO.
    // Instead, we use SPI_DEVICE_HALFDUPLEX (set in devcfg) and set flags=0 for transaction (default 1-bit cmd/addr).
    // BUT, we need 4-bit DATA.
    // There is no explicit SPI_TRANS_MODE_QOUT flag in older IDF versions or it might be named differently.
    // However, we can use the 'flags' field in spi_transaction_ext_t.
    // Actually, to send data in Quad mode but address in 1-bit, we just need to NOT set SPI_TRANS_MODE_QIO,
    // but we DO need to tell the driver to use Quad for data.
    // Wait, standard SPI driver doesn't support mixed modes easily in one transaction unless using specific flags.
    // Let's check if SPI_TRANS_MODE_QOUT exists. It seems it does NOT.
    // The correct way is to use SPI_TRANS_MODE_QIO but set address_bits to 0 and send address as part of command or manually? No.
    
    // Let's revert to QIO but try to fix the address phase.
    // If the display expects 1-bit address, but we send 4-bit address (QIO), it receives 6 clocks instead of 24.
    // We can simulate 1-bit address by sending it as data or command?
    // Or we can use the fact that 0x32 command usually implies Quad Data.
    
    // Alternative: Use SPI_TRANS_MODE_QIO but pad the address?
    // No, let's look at how others do it.
    // If SPI_TRANS_MODE_QOUT is missing, we might need to use the 'flags' in devcfg or just use QIO and accept it might be wrong if display expects 1-bit.
    // BUT, the error says 'SPI_TRANS_MODE_QOUT' undeclared.
    
    // Let's try to use SPI_TRANS_MODE_QIO again but maybe the address needs to be adjusted?
    // Actually, if we look at the datasheet for RM690B0 (or similar), 0x32 is "Write Memory Continue" in Quad.
    // It usually expects: Cmd(1) -> Addr(1) -> Data(4).
    // ESP32 SPI with SPI_TRANS_MODE_QIO sends: Cmd(1) -> Addr(4) -> Data(4).
    // This is the mismatch.
    
    // To achieve Cmd(1) -> Addr(1) -> Data(4), we can:
    // 1. Send Cmd + Addr as a single "Command" phase (if supported) or just standard SPI transaction for Cmd+Addr.
    // 2. Then send Data in a separate transaction using QIO (but with 0 address bits).
    // However, CS must stay active.
    // We can use SPI_TRANS_CS_KEEP_ACTIVE.
    
    // Let's try splitting the transaction.
    
    // Transaction 1: Command 0x32 + Address 0x002C00 (Standard SPI, 1-bit)
    spi_transaction_ext_t t1_ext = {
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_CS_KEEP_ACTIVE,
            .cmd = 0x32,
            .addr = 0x002C00,
            .length = 0,
        },
        .command_bits = 8,
        .address_bits = 24,
    };
    
    // Transaction 2: Data (Quad Mode, No Cmd, No Addr)
    spi_transaction_ext_t t2_ext = {
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_MODE_QIO,
            .cmd = 0, // No cmd
            .addr = 0, // No addr
            .length = len * 8,
            .tx_buffer = data,
        },
        .command_bits = 0,
        .address_bits = 0,
    };
    
    spi_device_acquire_bus(spi_handle, portMAX_DELAY);
    spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t1_ext);
    spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t2_ext);
    spi_device_release_bus(spi_handle);
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
