#include <stddef.h>
#include "rm690b0.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_attr.h"

static const char *TAG = "rm690b0";

typedef struct {
    rm690b0_done_cb_t cb;
    void *user_ctx;
} rm690b0_trans_ctx_t;

static rm690b0_trans_ctx_t s_trans_ctx;
static int s_pending_trans_count = 0;
static spi_transaction_ext_t s_trans_caset, s_trans_raset, s_trans_pixels;
static uint8_t s_caset_data[4];
static uint8_t s_raset_data[4];

static void spi_trans_post_cb(spi_transaction_t *trans) {
    rm690b0_trans_ctx_t *ctx = (rm690b0_trans_ctx_t *)trans->user;
    if (ctx && ctx->cb) {
        ctx->cb(ctx->user_ctx);
    }
}

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

// CS is handled by SPI driver

// Current display dimensions and offsets (updated by rotation)
static uint16_t current_width = 600;
static uint16_t current_height = 450;
static uint16_t offset_x = 0;
static uint16_t offset_y = 0;
static rm690b0_rotation_t s_current_rotation = RM690B0_ROTATION_270;

// --- Callback storage ---
static rm690b0_vsync_cb_t s_vsync_cb = NULL;
static void *s_vsync_ctx = NULL;
static rm690b0_error_cb_t s_error_cb = NULL;
static void *s_error_ctx = NULL;
static rm690b0_power_cb_t s_power_cb = NULL;
static void *s_power_ctx = NULL;

void rm690b0_register_vsync_callback(rm690b0_vsync_cb_t cb, void *user_ctx) {
    s_vsync_cb = cb;
    s_vsync_ctx = user_ctx;
}
void rm690b0_register_error_callback(rm690b0_error_cb_t cb, void *user_ctx) {
    s_error_cb = cb;
    s_error_ctx = user_ctx;
}
void rm690b0_register_power_callback(rm690b0_power_cb_t cb, void *user_ctx) {
    s_power_cb = cb;
    s_power_ctx = user_ctx;
}

void rm690b0_send_cmd(uint8_t cmd, const uint8_t *data, size_t len) {
    spi_transaction_ext_t t = {0};
    t.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
    t.base.cmd = 0x02;
    t.base.addr = ((uint32_t)cmd) << 8;
    t.base.length = len * 8;
    t.base.tx_buffer = data;
    t.command_bits = 8;
    t.address_bits = 24;

    esp_err_t ret = spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t);
    if (ret != ESP_OK && s_error_cb) s_error_cb(ret, s_error_ctx);
}

void rm690b0_send_pixels(const uint8_t *data, size_t len) {
    if (len == 0) return;

    // Use 64KB chunks to match the max_transfer_sz set in spi_bus_initialize.
    const size_t CHUNK_SIZE = 65536; 
    size_t sent = 0;
    
    spi_device_acquire_bus(spi_handle, portMAX_DELAY);
    
    while (sent < len) {
        size_t chunk = (len - sent > CHUNK_SIZE) ? CHUNK_SIZE : (len - sent);
        
        spi_transaction_ext_t t = {0};
        t.base.flags = SPI_TRANS_MODE_QIO;

        if (sent == 0) {
            t.base.flags |= SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
            t.base.cmd = 0x32;
            t.base.addr = 0x002C00;
            t.command_bits = 8;
            t.address_bits = 24;
        } else {
            t.command_bits = 0;
            t.address_bits = 0;
        }

        if (sent + chunk < len) {
            t.base.flags |= SPI_TRANS_CS_KEEP_ACTIVE;
        }

        t.base.length = chunk * 8;
        t.base.tx_buffer = data + sent;
        
        spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t);
        sent += chunk;
    }
    
    spi_device_release_bus(spi_handle);
}

void rm690b0_read_id(uint8_t *id) {
    spi_transaction_ext_t t = {0};
    t.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_USE_RXDATA;
    t.base.cmd = 0x03; // Read Opcode
    t.base.addr = 0x000400; // RDDID (0x04) shifted
    t.base.length = 0;
    t.base.rxlength = 24; // Read 3 bytes
    t.command_bits = 8;
    t.address_bits = 24;

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

void rm690b0_flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const uint8_t *data) {
    // Acquire bus for the entire flush sequence (window + pixels) to ensure atomicity.
    // This prevents the "skewing" that happens if a window command is interrupted.
    spi_device_acquire_bus(spi_handle, portMAX_DELAY);
    
    rm690b0_set_window(x1, y1, x2, y2);
    esp_rom_delay_us(50); // Increased breather for the IC to stabilize the window
    
    size_t len = (size_t)(x2 - x1 + 1) * (y2 - y1 + 1) * 2;
    
    spi_transaction_ext_t t = {0};
    t.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_MODE_QIO;
    t.base.cmd = 0x32;
    t.base.addr = 0x002C00;
    t.base.length = len * 8;
    t.base.tx_buffer = data;
    t.command_bits = 8;
    t.address_bits = 24;
    
    spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t);
    
    esp_rom_delay_us(20); 
    spi_device_release_bus(spi_handle);
}

void rm690b0_flush_async(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const uint8_t *data, rm690b0_done_cb_t cb, void *user_ctx) {
    // If transactions are already pending, wait for them to finish
    while (s_pending_trans_count > 0) {
        spi_transaction_t *rtrans;
        esp_err_t ret = spi_device_get_trans_result(spi_handle, &rtrans, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get trans result: %s", esp_err_to_name(ret));
        }
        s_pending_trans_count--;
    }

    // Apply rotation-specific offsets
    x1 += offset_x;
    x2 += offset_x;
    y1 += offset_y;
    y2 += offset_y;

    // Prepare CASET
    s_caset_data[0] = (x1 >> 8); s_caset_data[1] = (x1 & 0xFF);
    s_caset_data[2] = (x2 >> 8); s_caset_data[3] = (x2 & 0xFF);
    
    memset(&s_trans_caset, 0, sizeof(s_trans_caset));
    s_trans_caset.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
    s_trans_caset.base.cmd = 0x02;
    s_trans_caset.base.addr = ((uint32_t)RM690B0_CASET) << 8;
    s_trans_caset.base.length = 32;
    s_trans_caset.base.tx_buffer = s_caset_data;
    s_trans_caset.command_bits = 8;
    s_trans_caset.address_bits = 24;

    // Prepare RASET
    s_raset_data[0] = (y1 >> 8); s_raset_data[1] = (y1 & 0xFF);
    s_raset_data[2] = (y2 >> 8); s_raset_data[3] = (y2 & 0xFF);

    memset(&s_trans_raset, 0, sizeof(s_trans_raset));
    s_trans_raset.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
    s_trans_raset.base.cmd = 0x02;
    s_trans_raset.base.addr = ((uint32_t)RM690B0_RASET) << 8;
    s_trans_raset.base.length = 32;
    s_trans_raset.base.tx_buffer = s_raset_data;
    s_trans_raset.command_bits = 8;
    s_trans_raset.address_bits = 24;
    
    // Prepare Pixels
    size_t len = (size_t)(x2 - x1 + 1) * (y2 - y1 + 1) * 2;
    
    memset(&s_trans_pixels, 0, sizeof(s_trans_pixels));
    s_trans_pixels.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_MODE_QIO;
    s_trans_pixels.base.cmd = 0x32;
    s_trans_pixels.base.addr = 0x002C00;
    s_trans_pixels.base.length = len * 8;
    s_trans_pixels.base.tx_buffer = data;
    s_trans_pixels.command_bits = 8;
    s_trans_pixels.address_bits = 24;
    
    // Set callback only on the last transaction (pixels)
    s_trans_ctx.cb = cb;
    s_trans_ctx.user_ctx = user_ctx;
    s_trans_pixels.base.user = &s_trans_ctx;
    
    // Queue all transactions
    // Note: We don't need to acquire bus for queued transactions
    esp_err_t ret;
    ret = spi_device_queue_trans(spi_handle, (spi_transaction_t *)&s_trans_caset, portMAX_DELAY);
    if (ret != ESP_OK) ESP_LOGE(TAG, "Failed to queue CASET: %s", esp_err_to_name(ret));
    
    ret = spi_device_queue_trans(spi_handle, (spi_transaction_t *)&s_trans_raset, portMAX_DELAY);
    if (ret != ESP_OK) ESP_LOGE(TAG, "Failed to queue RASET: %s", esp_err_to_name(ret));
    
    ret = spi_device_queue_trans(spi_handle, (spi_transaction_t *)&s_trans_pixels, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue Pixels: %s", esp_err_to_name(ret));
        // Prevent LVGL hang by calling callback even on failure
        if (cb) cb(user_ctx);
    }
    
    s_pending_trans_count = 3;
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
    
    // Refactored Mapping based on user feedback:
    // Rot 0: USB Bottom (Landscape 600x450) - Desired Default
    // Rot 1: USB Left (Portrait 450x600) - Hardware Default
    // Rot 2: USB Top (Landscape 600x450)
    // Rot 3: USB Right (Portrait 450x600)
    
    switch (rot) {
        case RM690B0_ROTATION_0: // USB Bottom (Landscape)
            madctl = RM690B0_MADCTL_MV | RM690B0_MADCTL_MX; 
            current_width = 600;
            current_height = 446; // Reduced by 4px to bring bottom edge up
            offset_x = 0; 
            offset_y = 18; // Adjusted to 18 based on feedback
            break;
            
        case RM690B0_ROTATION_90: // USB Left (Portrait)
            madctl = 0x00; 
            current_width = 446; 
            current_height = 600;
            offset_x = 18; // Adjusted to 18 based on feedback (34 was 16px too far right)
            offset_y = 0;
            break;
            
        case RM690B0_ROTATION_180: // USB Top (Landscape Inverted)
            madctl = RM690B0_MADCTL_MV | RM690B0_MADCTL_MY;
            current_width = 600;
            current_height = 446; // Match Rot 0
            offset_x = 0;
            offset_y = 18; // Match Rot 0
            break;
            
        case RM690B0_ROTATION_270: // USB Right (Portrait Inverted)
            madctl = RM690B0_MADCTL_MX | RM690B0_MADCTL_MY;
            current_width = 446; // Match Rot 1
            current_height = 600;
            offset_x = 18; // Match Rot 1
            offset_y = 0;
            break;
            
        default:
            madctl = RM690B0_MADCTL_MV | RM690B0_MADCTL_MX;
            current_width = 600;
            current_height = 450;
            offset_x = 0;
            offset_y = 0;
            break;
    }
    s_current_rotation = rot;
    rm690b0_send_cmd(RM690B0_MADCTR, &madctl, 1);
}

rm690b0_rotation_t rm690b0_get_rotation(void) {
    return s_current_rotation;
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
    if (s_power_cb) s_power_cb(on, s_power_ctx);
}

// Invert display colors
void rm690b0_invert_colors(bool invert) {
    if (invert) {
        rm690b0_send_cmd(RM690B0_INVON, NULL, 0);
    } else {
        rm690b0_send_cmd(RM690B0_INVOFF, NULL, 0);
    }
}

// --- TE (VSYNC) ISR handler ---
static void IRAM_ATTR rm690b0_te_isr_handler(void *arg) {
    if (s_vsync_cb) s_vsync_cb(s_vsync_ctx);
}

// Enable Tearing Effect signal
void rm690b0_enable_te(bool enable) {
    if (enable) {
        uint8_t param = 0x00;
        rm690b0_send_cmd(RM690B0_TEON, &param, 1);
        // Configure TE pin interrupt for VSYNC
        gpio_set_intr_type(PIN_NUM_TE, GPIO_INTR_POSEDGE);
        gpio_isr_handler_add(PIN_NUM_TE, rm690b0_te_isr_handler, NULL);
    } else {
        rm690b0_send_cmd(RM690B0_TEOFF, NULL, 0);
        gpio_isr_handler_remove(PIN_NUM_TE);
    }
}

// Clear the full physical display including offset regions to prevent artifacts
void rm690b0_clear_full_display(uint16_t color) {
    // Determine physical limits based on current rotation
    uint16_t hw_w = RM690B0_HW_WIDTH;
    uint16_t hw_h = RM690B0_HW_HEIGHT;

    // If in Landscape mode (Rot 0 or 2), swap width/height for the clear window
    if (s_current_rotation == RM690B0_ROTATION_0 || s_current_rotation == RM690B0_ROTATION_180) {
        hw_w = RM690B0_HW_HEIGHT; // 640
        hw_h = RM690B0_HW_WIDTH;  // 520
    }

    // Temporarily set window to full hardware area without offsets
    uint8_t caset[] = { 0, 0, ((hw_w - 1) >> 8), ((hw_w - 1) & 0xFF) };
    uint8_t raset[] = { 0, 0, ((hw_h - 1) >> 8), ((hw_h - 1) & 0xFF) };
    rm690b0_send_cmd(RM690B0_CASET, caset, 4);
    rm690b0_send_cmd(RM690B0_RASET, raset, 4);
    
    // Use a 16KB buffer for clearing (8192 pixels)
    const size_t CLEAR_BUF_PIXELS = 8192;
    uint16_t *buf = heap_caps_malloc(CLEAR_BUF_PIXELS * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf) return;
    
    uint16_t color_be = (color << 8) | (color >> 8);
    for (size_t i = 0; i < CLEAR_BUF_PIXELS; i++) buf[i] = color_be;
    
    size_t total_pixels = (size_t)hw_w * hw_h;
    size_t sent_pixels = 0;
    
    spi_device_acquire_bus(spi_handle, portMAX_DELAY);

    while (sent_pixels < total_pixels) {
        size_t chunk = (total_pixels - sent_pixels > CLEAR_BUF_PIXELS) ? CLEAR_BUF_PIXELS : (total_pixels - sent_pixels);
        
        if (sent_pixels == 0) {
            spi_transaction_ext_t t = {0};
            t.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_MODE_QIO;
            t.base.cmd = 0x32;
            t.base.addr = 0x002C00;
            t.base.length = chunk * 16;
            t.base.tx_buffer = buf;
            t.command_bits = 8;
            t.address_bits = 24;
            if (sent_pixels + chunk < total_pixels) t.base.flags |= SPI_TRANS_CS_KEEP_ACTIVE;
            spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t);
        } else {
            spi_transaction_t t = {0};
            t.flags = SPI_TRANS_MODE_QIO;
            t.length = chunk * 16;
            t.tx_buffer = buf;
            if (sent_pixels + chunk < total_pixels) t.flags |= SPI_TRANS_CS_KEEP_ACTIVE;
            spi_device_polling_transmit(spi_handle, &t);
        }
        sent_pixels += chunk;
    }

    esp_rom_delay_us(20);
    spi_device_release_bus(spi_handle);
    free(buf);
    
    // Restore window to current rotation settings
    rm690b0_set_window(0, 0, current_width - 1, current_height - 1);
}

// Draw 8 vertical color bars for testing edge artifacts and color reproduction
void rm690b0_draw_test_pattern(void) {
    ESP_LOGI(TAG, "Drawing 8-color rainbow test pattern...");
    
    // Clear full physical panel first to remove garbage in offset regions
    rm690b0_clear_full_display(0x0000);

    uint16_t w = current_width;
    uint16_t h = current_height;
    uint16_t bar_w = w / 8; 
    uint16_t colors[] = {
        0xFFFF, // White
        0xFFE0, // Yellow
        0x0000, // Black
        0x07FF, // Cyan
        0x07E0, // Green
        0xF81F, // Magenta
        0xF800, // Red
        0x001F  // Blue
    };

    const size_t BAR_BUF_PIXELS = 4096;
    uint16_t *buf = heap_caps_malloc(BAR_BUF_PIXELS * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf) return;

    for (int i = 0; i < 8; i++) {
        uint16_t x1 = i * bar_w;
        uint16_t x2 = (i == 7) ? (w - 1) : (x1 + bar_w - 1);
        // Ensure we don't exceed physical width
        if (x2 >= w) x2 = w - 1;
        
        rm690b0_set_window(x1, 0, x2, h - 1);
        
        uint16_t color_be = (colors[i] << 8) | (colors[i] >> 8);
        for (size_t j = 0; j < BAR_BUF_PIXELS; j++) buf[j] = color_be;

        size_t total_pixels = (size_t)(x2 - x1 + 1) * h;
        size_t sent_pixels = 0;

        spi_device_acquire_bus(spi_handle, portMAX_DELAY);

        while (sent_pixels < total_pixels) {
            size_t chunk = (total_pixels - sent_pixels > BAR_BUF_PIXELS) ? BAR_BUF_PIXELS : (total_pixels - sent_pixels);
            
            if (sent_pixels == 0) {
                spi_transaction_ext_t t = {0};
                t.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_MODE_QIO;
                t.base.cmd = 0x32;
                t.base.addr = 0x002C00;
                t.base.length = chunk * 16;
                t.base.tx_buffer = buf;
                t.command_bits = 8;
                t.address_bits = 24;
                if (sent_pixels + chunk < total_pixels) t.base.flags |= SPI_TRANS_CS_KEEP_ACTIVE;
                spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t);
            } else {
                spi_transaction_t t = {0};
                t.flags = SPI_TRANS_MODE_QIO;
                t.length = chunk * 16;
                t.tx_buffer = buf;
                if (sent_pixels + chunk < total_pixels) t.flags |= SPI_TRANS_CS_KEEP_ACTIVE;
                spi_device_polling_transmit(spi_handle, &t);
            }
            sent_pixels += chunk;
        }

        esp_rom_delay_us(20);
        spi_device_release_bus(spi_handle);
        
        // Small delay between bars to let the controller settle
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    free(buf);
    
    // Restore window to full active area
    rm690b0_set_window(0, 0, current_width - 1, current_height - 1);
}

esp_err_t rm690b0_init(void) {
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
        .max_transfer_sz = 65536, // 64KB limit
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_QUAD,
    };
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPI bus initialized with max_transfer_sz: %d", buscfg.max_transfer_sz);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000, // Reduced to 20MHz to fix "hair thin lines" artifacts
        .mode = 0,
        .spics_io_num = PIN_NUM_QSPI_CS, // Hardware CS
        .queue_size = 10,
        .flags = SPI_DEVICE_HALFDUPLEX, 
        .post_cb = spi_trans_post_cb,
    };
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Read ID to verify communication
    uint8_t id[3] = {0};
    rm690b0_read_id(id);
    ESP_LOGI(TAG, "Display ID: %02X %02X %02X", id[0], id[1], id[2]);

    // Initialization Sequence from LilyGo-AMOLED-Series (BOARD_AMOLED_241)
    uint8_t param_fe_20[] = {0x20};
    rm690b0_send_cmd(0xFE, param_fe_20, 1); // SET PAGE

    uint8_t param_26_0a[] = {0x0A};
    rm690b0_send_cmd(0x26, param_26_0a, 1); // MIPI OFF

    uint8_t param_24_80[] = {0x80};
    rm690b0_send_cmd(0x24, param_24_80, 1); // SPI write RAM

    uint8_t param_5a_51[] = {0x51};
    rm690b0_send_cmd(0x5A, param_5a_51, 1); // SWIRE FOR BV6804

    uint8_t param_5b_2e[] = {0x2E};
    rm690b0_send_cmd(0x5B, param_5b_2e, 1); // SWIRE FOR BV6804

    uint8_t param_fe_00[] = {0x00};
    rm690b0_send_cmd(0xFE, param_fe_00, 1); // SET PAGE

    uint8_t param_3a_55[] = {0x55};
    rm690b0_send_cmd(RM690B0_COLMOD, param_3a_55, 1); // 16-bit pixel format

    rm690b0_send_cmd(0xC2, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t param_35_00[] = {0x00};
    rm690b0_send_cmd(0x35, param_35_00, 1); // TE ON

    uint8_t param_51_00[] = {0x00};
    rm690b0_send_cmd(RM690B0_WRDISBV, param_51_00, 1); // Brightness 0

    rm690b0_send_cmd(RM690B0_SLPOUT, NULL, 0); // Sleep Out
    vTaskDelay(pdMS_TO_TICKS(120));

    rm690b0_send_cmd(RM690B0_DISPON, NULL, 0); // Display On
    vTaskDelay(pdMS_TO_TICKS(120));

    uint8_t param_51_d0[] = {0xD0};
    rm690b0_send_cmd(RM690B0_WRDISBV, param_51_d0, 1); // Brightness D0 (LilyGo default)

    rm690b0_set_rotation(RM690B0_ROTATION_0);

    ESP_LOGI(TAG, "Initialization Done");
    return ESP_OK;
}
