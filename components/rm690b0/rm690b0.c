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

// Current display dimensions and offsets (updated by rotation)
static uint16_t current_width = RM690B0_PHYSICAL_H;   // Default: 600 (Portrait)
static uint16_t current_height = RM690B0_PHYSICAL_W;  // Default: 450 (Portrait)
static uint16_t offset_x = 0;
static uint16_t offset_y = 16;
// (no logical shift)

void rm690b0_send_cmd(uint8_t cmd, const uint8_t *data, size_t len) {
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
    // Split transaction to achieve Cmd(1-bit) -> Addr(1-bit) -> Data(4-bit QSPI)
    // Transaction 1: Command 0x32 + Address 0x002C00 (Standard SPI)
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
    // Apply rotation-specific offsets
    x1 += offset_x;
    x2 += offset_x;
    y1 += offset_y;
    y2 += offset_y;

    uint8_t caset[] = { (x1 >> 8), (x1 & 0xFF), (x2 >> 8), (x2 & 0xFF) };
    uint8_t raset[] = { (y1 >> 8), (y1 & 0xFF), (y2 >> 8), (y2 & 0xFF) };
    rm690b0_send_cmd(RM690B0_CASET, caset, 4);
    rm690b0_send_cmd(RM690B0_RASET, raset, 4);
}

void rm690b0_set_rotation(rm690b0_rotation_t rot) {
    uint8_t madctl = RM690B0_MADCTL_RGB;
    
    // Match LilyGo's rotation implementation for BOARD_AMOLED_241 (RM690B0)
    switch (rot) {
        case RM690B0_ROTATION_0: // Portrait (Default - USB Left)
            madctl = RM690B0_MADCTL_MX | RM690B0_MADCTL_MV | RM690B0_MADCTL_RGB;
            current_width = RM690B0_PHYSICAL_H;  // 600
            current_height = RM690B0_PHYSICAL_W; // 450
            offset_x = 0;
            offset_y = 16;
            
            break;
        case RM690B0_ROTATION_90: // Landscape
            madctl = RM690B0_MADCTL_RGB;
            current_width = RM690B0_PHYSICAL_W;  // 450
            current_height = RM690B0_PHYSICAL_H; // 600
            offset_x = 16;
            offset_y = 0;
            break;
        case RM690B0_ROTATION_180: // Portrait Inverted
            madctl = RM690B0_MADCTL_MV | RM690B0_MADCTL_MY | RM690B0_MADCTL_RGB;
            current_width = RM690B0_PHYSICAL_H;  // 600
            current_height = RM690B0_PHYSICAL_W; // 450
            offset_x = 0;
            offset_y = 16;
            
            break;
        case RM690B0_ROTATION_270: // Landscape Inverted
            madctl = RM690B0_MADCTL_MX | RM690B0_MADCTL_MY | RM690B0_MADCTL_RGB;
            current_width = RM690B0_PHYSICAL_W;  // 450
            current_height = RM690B0_PHYSICAL_H; // 600
            offset_x = 16;
            offset_y = 0;
            
            break;
        default:
            madctl = RM690B0_MADCTL_MX | RM690B0_MADCTL_MV | RM690B0_MADCTL_RGB;
            current_width = RM690B0_PHYSICAL_H;
            current_height = RM690B0_PHYSICAL_W;
            offset_x = 0;
            offset_y = 16;
            
            break;
    }
    rm690b0_send_cmd(RM690B0_MADCTR, &madctl, 1);
}

// Get current display width (changes with rotation)
uint16_t rm690b0_get_width(void) {
    return current_width;
}

// Get current display height (changes with rotation)
uint16_t rm690b0_get_height(void) {
    return current_height;
}

// Set brightness level (0-255)
void rm690b0_set_brightness(uint8_t level) {
    rm690b0_send_cmd(RM690B0_WRDISBV, &level, 1);
}

// Enter or exit sleep mode
void rm690b0_sleep_mode(bool sleep) {
    if (sleep) {
        rm690b0_send_cmd(RM690B0_SLPIN, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else {
        rm690b0_send_cmd(RM690B0_SLPOUT, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(120));
    }
}

// Turn display output on or off
void rm690b0_display_power(bool on) {
    if (on) {
        rm690b0_send_cmd(RM690B0_DISPON, NULL, 0);
    } else {
        rm690b0_send_cmd(RM690B0_DISPOFF, NULL, 0);
    }
}

// Invert display colors
void rm690b0_invert_colors(bool invert) {
    if (invert) {
        rm690b0_send_cmd(RM690B0_INVON, NULL, 0);
    } else {
        rm690b0_send_cmd(RM690B0_INVOFF, NULL, 0);
    }
}

// Enable Tearing Effect signal
void rm690b0_enable_te(bool enable) {
    if (enable) {
        uint8_t param = 0x00;
        rm690b0_send_cmd(RM690B0_TEON, &param, 1);
    } else {
        rm690b0_send_cmd(RM690B0_TEOFF, NULL, 0);
    }
}

// Clear the full physical display including offset regions to prevent artifacts
void rm690b0_clear_full_display(uint16_t color) {
    // Temporarily set window to full physical area without offsets
    uint8_t caset[] = { 0, 0, ((RM690B0_PHYSICAL_W - 1) >> 8), ((RM690B0_PHYSICAL_W - 1) & 0xFF) };
    uint8_t raset[] = { 0, 0, ((RM690B0_PHYSICAL_H - 1) >> 8), ((RM690B0_PHYSICAL_H - 1) & 0xFF) };
    rm690b0_send_cmd(RM690B0_CASET, caset, 4);
    rm690b0_send_cmd(RM690B0_RASET, raset, 4);
    
    // Calculate total pixels and prepare buffer
    // Use a smaller chunk to be safe with DMA limits
    const size_t CHUNK_PIXELS = 1024;  // 1024 pixels = 2KB per chunk
    uint16_t *buf = heap_caps_malloc(CHUNK_PIXELS * 2, MALLOC_CAP_SPIRAM);
    if (!buf) {
        buf = malloc(CHUNK_PIXELS * 2);
    }
    if (!buf) return;
    
    // Swap bytes for SPI (Big Endian)
    uint16_t color_be = (color << 8) | (color >> 8);
    for (size_t i = 0; i < CHUNK_PIXELS; i++) {
        buf[i] = color_be;
    }
    
    // Clear full physical display (450 x 600 = 270,000 pixels)
    // Send RAMWR command once, then stream all data
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
    
    size_t total_pixels = RM690B0_PHYSICAL_W * RM690B0_PHYSICAL_H;
    size_t sent_pixels = 0;
    
    spi_device_acquire_bus(spi_handle, portMAX_DELAY);
    
    // Send RAMWR command once
    spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t1_ext);
    
    // Stream pixel data in chunks without sending RAMWR again
    while (sent_pixels < total_pixels) {
        size_t chunk_pixels = (total_pixels - sent_pixels > CHUNK_PIXELS) ? CHUNK_PIXELS : (total_pixels - sent_pixels);
        
        spi_transaction_ext_t t2_ext = {
            .base = {
                .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_MODE_QIO | 
                         ((sent_pixels + chunk_pixels < total_pixels) ? SPI_TRANS_CS_KEEP_ACTIVE : 0),
                .cmd = 0,
                .addr = 0,
                .length = chunk_pixels * 2 * 8,
                .tx_buffer = buf,
            },
            .command_bits = 0,
            .address_bits = 0,
        };
        
        spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t2_ext);
        sent_pixels += chunk_pixels;
    }
    
    spi_device_release_bus(spi_handle);
    free(buf);
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
        .max_transfer_sz = RM690B0_PHYSICAL_H * RM690B0_PHYSICAL_W * 2 + 1024,
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
    uint8_t param_fe_20[] = {0x20};
    rm690b0_send_cmd(0xFE, param_fe_20, 1);

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
