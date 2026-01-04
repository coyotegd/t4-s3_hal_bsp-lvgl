// Full implementation ported from LilyGo's TouchClassCST226.cpp
#include "cst226se.h"
#include "esp_log.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "sy6970.h"
#include <string.h>
#include "freertos/semphr.h"
// Callback support
#include <stddef.h>

static const char *TAG = "cst226se";

#define CST226SE_I2C_ADDR      0x5A
#define CST226SE_I2C_FREQ_HZ   400000
#define CST226SE_BUFFER_NUM    28
#define CST226SE_CHIPTYPE      0xA8


// Use PMIC's I2C pin definitions for SDA/SCL
#define CST226SE_SDA_PIN SY6970_SDA_PIN
#define CST226SE_SCL_PIN SY6970_SCL_PIN
#ifndef CST226SE_RST_PIN
#define CST226SE_RST_PIN 17
#endif
#ifndef CST226SE_IRQ_PIN
#define CST226SE_IRQ_PIN 8
#endif

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;
static SemaphoreHandle_t s_touch_sem = NULL;
// Track touch IC power state
static bool s_touch_awake = true;

// Internal state mirroring
static int16_t s_resX = 600;
static int16_t s_resY = 450;
static int16_t s_xMax = 600;
static int16_t s_yMax = 450;
static bool s_swapXY = false;
static bool s_mirrorX = true;
static bool s_mirrorY = true;
static uint32_t s_chipID = 0;
static cst226se_rotation_t s_current_rotation = CST226SE_ROTATION_270;

// --- Callback logic ---
typedef void (*cst226se_event_callback_t)(const cst226se_data_t *data, void *user_ctx);
static cst226se_event_callback_t s_touch_callback = NULL;
static void *s_touch_callback_ctx = NULL;

void cst226se_register_callback(cst226se_event_callback_t cb, void *user_ctx) {
    s_touch_callback = cb;
    s_touch_callback_ctx = user_ctx;
}

// --- Static helper function definitions ---

static void log_pin_states(const char *where) {
    int rst = gpio_get_level(CST226SE_RST_PIN);
    int irq = gpio_get_level(CST226SE_IRQ_PIN);
    ESP_LOGI(TAG, "%s: RST=%d IRQ=%d", where, rst, irq);
}
    
void cst226se_set_swap_xy(bool swap)
{
    s_swapXY = swap;
}

void cst226se_set_mirror_xy(bool mirror_x, bool mirror_y)
{
// Helper: log raw I2C data
    s_mirrorX = mirror_x;
    s_mirrorY = mirror_y;
}

void cst226se_set_max_coordinates(uint16_t x, uint16_t y)
{
    s_xMax = x;
    s_yMax = y;
}

// Helper: log chip ID
bool cst226se_get_resolution(int16_t *x, int16_t *y)
{
    if (!x || !y) return false;
    *x = s_resX;
    *y = s_resY;
    return true;
}

static esp_err_t write_single(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), -1);
}

static esp_err_t read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, -1);
}

static esp_err_t write_then_read(const uint8_t *write_buf, size_t write_len, uint8_t *read_buf, size_t read_len)
{
    return i2c_master_transmit_receive(s_dev, (uint8_t *)write_buf, write_len, read_buf, read_len, -1);
}

void cst226se_set_rotation(cst226se_rotation_t rotation)
{
    s_current_rotation = rotation;
    // Native sensor is 450x600 (Portrait)
    // We map this to the display's 600x450 or 450x600 workspace
    switch (rotation) {
    case CST226SE_ROTATION_0: // Landscape (Default)
        s_swapXY = true; s_mirrorX = false; s_mirrorY = true;
        s_xMax = 599; s_yMax = 449;
        break;
    case CST226SE_ROTATION_90: // Portrait
        s_swapXY = false; s_mirrorX = true; s_mirrorY = false;
        s_xMax = 449; s_yMax = 599;
        break;
    case CST226SE_ROTATION_180: // Landscape Inverted
        s_swapXY = true; s_mirrorX = true; s_mirrorY = true;
        s_xMax = 599; s_yMax = 449;
        break;
    case CST226SE_ROTATION_270: // Portrait Inverted
        s_swapXY = false; s_mirrorX = false; s_mirrorY = true;
        s_xMax = 449; s_yMax = 599;
        break;
    default:
        s_swapXY = false; s_mirrorX = false; s_mirrorY = false;
        s_xMax = 599; s_yMax = 449;
        break;
    }
    ESP_LOGI(TAG, "Rotation set %d: swap:%d mirrorX:%d mirrorY:%d (xMax=%d yMax=%d)", 
             rotation, s_swapXY, s_mirrorX, s_mirrorY, s_xMax, s_yMax);
}

cst226se_rotation_t cst226se_get_rotation(void)
{
    return s_current_rotation;
}

static void IRAM_ATTR cst226se_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_touch_sem) {
        xSemaphoreGiveFromISR(s_touch_sem, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

bool cst226se_wait_event(uint32_t timeout_ms) {
    if (!s_touch_sem) return false;
    return xSemaphoreTake(s_touch_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void cst226se_reset(void)
{
    // Try to toggle RST pin if available
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << CST226SE_RST_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(CST226SE_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(CST226SE_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
}

esp_err_t cst226se_init(void)
{

    ESP_LOGI(TAG, "Initializing CST226SE touch driver...");
    // Always use the PMIC's I2C bus handle
    s_bus = sy6970_get_bus_handle();
    if (!s_bus) {
        ESP_LOGE(TAG, "No I2C bus handle from PMIC. Ensure sy6970_init() is called first.");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_dev) {
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = CST226SE_I2C_ADDR,
            .scl_speed_hz = CST226SE_I2C_FREQ_HZ,
        };
        if (i2c_master_bus_add_device(s_bus, &dev_config, &s_dev) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add CST226SE device");
            return ESP_FAIL;
        }
    }

    // Configure default rotation
    cst226se_set_rotation(CST226SE_ROTATION_0);

    // Configure IRQ pin
    gpio_config_t irq_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << CST226SE_IRQ_PIN),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&irq_conf);

    if (s_touch_sem == NULL) {
        s_touch_sem = xSemaphoreCreateBinary();
    }

    gpio_isr_handler_add(CST226SE_IRQ_PIN, cst226se_isr_handler, NULL);

    // Try to init chip: enter command mode and read info
    uint8_t buffer[8];

    // Reset first
    cst226se_reset();

    // Always wake the chip in case it was left in sleep mode
    cst226se_wake();
    vTaskDelay(pdMS_TO_TICKS(10));

    // Enter Command Mode
    write_single(0xD1, 0x01);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t write_buf[2] = {0xD1, 0xFC};
    if (write_then_read(write_buf, 2, buffer, 4) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read chip checkcode");
        return ESP_FAIL;
    }

    uint32_t checkcode = 0;
    checkcode = buffer[3]; checkcode <<= 8; checkcode |= buffer[2]; checkcode <<= 8; checkcode |= buffer[1]; checkcode <<= 8; checkcode |= buffer[0];
    ESP_LOGI(TAG, "Chip checkcode:0x%08lx.", checkcode);

    // Read resolution
    write_buf[0] = 0xD1; write_buf[1] = 0xF8;
    if (write_then_read(write_buf, 2, buffer, 4) == ESP_OK) {
        s_resX = (buffer[1] << 8) | buffer[0];
        s_resY = (buffer[3] << 8) | buffer[2];
        ESP_LOGI(TAG, "Chip native resolution X:%u Y:%u", s_resX, s_resY);
    }

    // Read chip type & project id
    write_buf[0] = 0xD2; write_buf[1] = 0x04;
    if (write_then_read(write_buf, 2, buffer, 4) == ESP_OK) {
        uint32_t chipType = buffer[3]; chipType <<= 8; chipType |= buffer[2];
        uint32_t ProjectID = buffer[1]; ProjectID <<= 8; ProjectID |= buffer[0];
        ESP_LOGI(TAG, "Chip type :0x%lx, ProjectID:0X%lx", chipType, ProjectID);
        if (chipType != CST226SE_CHIPTYPE) {
            ESP_LOGE(TAG, "Chip ID does not match, should be 0x%02X", CST226SE_CHIPTYPE);
        } else {
            s_chipID = chipType;
        }
    }

    // Exit Command Mode and ensure touch is awake
    write_single(0xD1, 0x09);
    s_touch_awake = true;
    return ESP_OK;
}

void cst226se_set_i2c_bus(i2c_master_bus_handle_t bus)
{
    // Deprecated: bus sharing is now always via PMIC. Do nothing.
}

bool cst226se_read(cst226se_data_t *data)
{
    // Log IRQ pin state for hardware diagnosis, but only periodically
    static int irq_log_counter = 0;
    int irq_level = gpio_get_level(CST226SE_IRQ_PIN);
    if (++irq_log_counter >= 100) { // About every 2 seconds at 50Hz
        ESP_LOGI(TAG, "Touch IRQ pin state: %d", irq_level);
        irq_log_counter = 0;
    }

    if (!data) return false;

    static bool last_pressed = false;

    uint8_t buffer[CST226SE_BUFFER_NUM];
    esp_err_t reg_result = read_reg(0x00, buffer, CST226SE_BUFFER_NUM);
    if (reg_result != ESP_OK) {
        ESP_LOGE(TAG, "Touch read_reg failed: %d", reg_result);
        if (last_pressed != false) {
            data->pressed = false;
            last_pressed = false;
            ESP_LOGI(TAG, "Touch released (read error)");
            if (s_touch_callback) s_touch_callback(data, s_touch_callback_ctx);
            return true;
        }
        return false;
    }

    // Home button detection

    if (buffer[0] == 0x83 && buffer[1] == 0x17 && buffer[5] == 0x80) {
        ESP_LOGI(TAG, "Touch: Home button pattern detected, no touch");
        data->pressed = false;
        if (last_pressed != false) {
            last_pressed = false;
            ESP_LOGI(TAG, "Touch released (home pattern)");
            if (s_touch_callback) s_touch_callback(data, s_touch_callback_ctx);
            return true;
        }
        return false;
    }


    if (buffer[6] != 0xAB) {
        ESP_LOGI(TAG, "Touch: buffer[6] != 0xAB, no valid touch");
        if (last_pressed != false) {
            data->pressed = false;
            last_pressed = false;
            ESP_LOGI(TAG, "Touch released (buffer[6] check)");
            if (s_touch_callback) s_touch_callback(data, s_touch_callback_ctx);
            return true;
        }
        return false;
    }
    if (buffer[0] == 0xAB) {
        ESP_LOGI(TAG, "Touch: buffer[0] == 0xAB, no valid touch");
        if (last_pressed != false) {
            data->pressed = false;
            last_pressed = false;
            ESP_LOGI(TAG, "Touch released (buffer[0] check)");
            if (s_touch_callback) s_touch_callback(data, s_touch_callback_ctx);
            return true;
        }
        return false;
    }
    if (buffer[5] == 0x80) {
        ESP_LOGI(TAG, "Touch: buffer[5] == 0x80, no valid touch");
        if (last_pressed != false) {
            data->pressed = false;
            last_pressed = false;
            ESP_LOGI(TAG, "Touch released (buffer[5] == 0x80)");
            if (s_touch_callback) s_touch_callback(data, s_touch_callback_ctx);
            return true;
        }
        return false;
    }


    uint8_t point = buffer[5] & 0x7F;
    ESP_LOGI(TAG, "Touch: point count = %u", point);
    if (point > 5 || point == 0) {
        ESP_LOGI(TAG, "Touch: point out of range or zero, writing 0xAB");
        write_single(0x00, 0xAB);
        if (last_pressed != false) {
            data->pressed = false;
            last_pressed = false;
            ESP_LOGI(TAG, "Touch released (point out of range)");
            if (s_touch_callback) s_touch_callback(data, s_touch_callback_ctx);
            return true;
        }
        return false;
    }


    // Parse first point
    uint8_t index = 0;
    uint16_t x = 0, y = 0;
    for (int i = 0; i < point; i++) {
        uint8_t id = buffer[index] >> 4;
        uint8_t status = buffer[index] & 0x0F;
        uint16_t px = (uint16_t)((buffer[index + 1] << 4) | ((buffer[index + 3] >> 4) & 0x0F));
        uint16_t py = (uint16_t)((buffer[index + 2] << 4) | (buffer[index + 3] & 0x0F));
        ESP_LOGI(TAG, "Touch: i=%d id=%u status=%u px=%u py=%u", i, id, status, px, py);
        if (i == 0) { x = px; y = py; }
        index = (i == 0) ? (index + 7) : (index + 5);
    }

    // Filter out obviously bogus values (e.g., y=65504U) before transforms

    if (y == 65504U) {
        ESP_LOGI(TAG, "Touch: y == 65504U, bogus value");
        if (last_pressed != false) {
            data->pressed = false;
            last_pressed = false;
            ESP_LOGI(TAG, "Touch released (bogus y)");
            if (s_touch_callback) s_touch_callback(data, s_touch_callback_ctx);
            return true;
        }
        return false;
    }


    // Apply rotation transforms (chip reports native landscape 450x600)
    int16_t tx = x;
    int16_t ty = y;
    if (s_swapXY) {
        int16_t tmp = tx; tx = ty; ty = tmp;
    }

    // Clamp to limits to avoid overflow/underflow during mirror or out-of-bounds errors
    if (tx < 0) tx = 0;
    if (s_xMax && tx > s_xMax) tx = s_xMax;
    if (ty < 0) ty = 0;
    if (s_yMax && ty > s_yMax) ty = s_yMax;

    if (s_mirrorX && s_xMax) tx = s_xMax - tx;
    if (s_mirrorY && s_yMax) ty = s_yMax - ty;

    ESP_LOGI(TAG, "Touch: transformed tx=%d ty=%d", tx, ty);

    // Filter out out-of-bounds values (Should be handled by clamp above, but keeping for safety)
    if (tx < 0 || (s_xMax && tx > s_xMax) || ty < 0 || (s_yMax && ty > s_yMax)) {
        ESP_LOGI(TAG, "Touch: out of bounds (tx,ty)=%d,%d", tx, ty);
        // Instead of releasing, we just clamp (already done) or ignore.
        // But if we are here, something is very wrong (e.g. negative after mirror?)
        // Let's just clamp again to be safe and NOT release.
        if (tx < 0) tx = 0;
        if (s_xMax && tx > s_xMax) tx = s_xMax;
        if (ty < 0) ty = 0;
        if (s_yMax && ty > s_yMax) ty = s_yMax;
    }


    // Only report if state or coordinates changed
    // if (last_pressed == true && last_x == (uint16_t)tx && last_y == (uint16_t)ty) {
    //     ESP_LOGI(TAG, "Touch: no change in position, skipping event");
    //     return false;
    // }

    data->pressed = true;
    data->x = (uint16_t)tx;
    data->y = (uint16_t)ty;
    data->id = 0;
    last_pressed = true;
    ESP_LOGI(TAG, "Touch: pressed at (%u, %u)", data->x, data->y);
    if (s_touch_callback) s_touch_callback(data, s_touch_callback_ctx);
    return true;
}

// Put CST226SE into deep sleep mode
void cst226se_sleep(void) {
    if (!s_touch_awake) {
        ESP_LOGI(TAG, "CST226SE: Already asleep, not sending sleep command");
        return;
    }
    uint8_t buf[2] = {0xD1, 0x05};
    if (s_dev) {
        if (i2c_master_transmit(s_dev, buf, 2, 100 / portTICK_PERIOD_MS) == ESP_OK) {
            ESP_LOGI(TAG, "CST226SE: Sent sleep command (0x05 to 0xD1)");
            s_touch_awake = false;
        } else {
            ESP_LOGW(TAG, "CST226SE: Failed to send sleep command");
        }
    }
}

// Wake CST226SE to normal mode
void cst226se_wake(void) {
    // Always toggle reset before wake for robustness
    cst226se_reset();
    vTaskDelay(pdMS_TO_TICKS(50));
    log_pin_states("Before wake cmd");
    uint8_t buf[2] = {0xD1, 0x09};
    if (s_dev) {
        if (i2c_master_transmit(s_dev, buf, 2, 100 / portTICK_PERIOD_MS) == ESP_OK) {
            ESP_LOGI(TAG, "CST226SE: Sent wake command (0x09 to 0xD1)");
            s_touch_awake = true;
            vTaskDelay(pdMS_TO_TICKS(200));
            log_pin_states("After wake cmd");
        } else {
            ESP_LOGW(TAG, "CST226SE: Failed to send wake command");
            log_pin_states("Wake cmd failed");
        }
    }
    s_touch_awake = true;
}
