// Full implementation ported from LilyGo's TouchClassCST226.cpp
#include "cst226se.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "sy6970.h"

static const char *TAG = "cst226se";

#define CST226SE_I2C_ADDR      0x5A
#define CST226SE_I2C_FREQ_HZ   400000
#define CST226SE_BUFFER_NUM    28
#define CST226SE_CHIPTYPE      0xA8

// Default pins (can be overridden by defines)
#ifndef CST226SE_SDA_PIN
#define CST226SE_SDA_PIN SY6970_SDA_PIN
#endif
#ifndef CST226SE_SCL_PIN
#define CST226SE_SCL_PIN SY6970_SCL_PIN
#endif
#ifndef CST226SE_RST_PIN
#define CST226SE_RST_PIN 8
#endif
#ifndef CST226SE_IRQ_PIN
#define CST226SE_IRQ_PIN 17
#endif

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;

// Internal state mirroring SensorCommon
static int16_t s_resX = 450;
static int16_t s_resY = 600;
static int16_t s_xMax = 450;
static int16_t s_yMax = 600;
static bool s_swapXY = false;
static bool s_mirrorX = false;
static bool s_mirrorY = false;
static uint32_t s_chipID = 0;

void cst226se_set_swap_xy(bool swap)
{
    s_swapXY = swap;
}

void cst226se_set_mirror_xy(bool mirror_x, bool mirror_y)
{
    s_mirrorX = mirror_x;
    s_mirrorY = mirror_y;
}

void cst226se_set_max_coordinates(uint16_t x, uint16_t y)
{
    s_xMax = x;
    s_yMax = y;
}

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
    // Based on the mapping in README: chip native is landscape 450x600
    switch (rotation) {
    case CST226SE_ROTATION_0:
        s_swapXY = true; s_mirrorX = false; s_mirrorY = true;
        break; // display 600x450
    case CST226SE_ROTATION_90:
        s_swapXY = false; s_mirrorX = false; s_mirrorY = false;
        break; // display 450x600
    case CST226SE_ROTATION_180:
        s_swapXY = true; s_mirrorX = true; s_mirrorY = false;
        break; // display 600x450
    case CST226SE_ROTATION_270:
        s_swapXY = false; s_mirrorX = true; s_mirrorY = true;
        break; // display 450x600
    default:
        s_swapXY = false; s_mirrorX = false; s_mirrorY = false;
        break;
    }
    ESP_LOGI(TAG, "Rotation set swap:%d mirrorX:%d mirrorY:%d (xMax=%d yMax=%d)", s_swapXY, s_mirrorX, s_mirrorY, s_xMax, s_yMax);
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

void cst226se_init(void)
{
    ESP_LOGI(TAG, "Initializing CST226SE touch driver...");
    // If a bus was set externally (via cst226se_set_i2c_bus), use it.
    if (!s_bus) {
        // Try to use the PMIC's bus if available
        i2c_master_bus_handle_t pmic_bus = sy6970_get_bus_handle();
        if (pmic_bus) {
            s_bus = pmic_bus;
        }
    }

    if (!s_bus) {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = -1,
            .sda_io_num = CST226SE_SDA_PIN,
            .scl_io_num = CST226SE_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        if (i2c_new_master_bus(&bus_config, &s_bus) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create I2C bus");
            return;
        }
    }

    if (!s_dev) {
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = CST226SE_I2C_ADDR,
            .scl_speed_hz = CST226SE_I2C_FREQ_HZ,
        };
        if (i2c_master_bus_add_device(s_bus, &dev_config, &s_dev) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add CST226SE device");
            return;
        }
    }

    // Configure default rotation
    cst226se_set_rotation(CST226SE_ROTATION_0);

    // Try to init chip: enter command mode and read info
    uint8_t buffer[8];

    // Reset first
    cst226se_reset();

    // Enter Command Mode
    write_single(0xD1, 0x01);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t write_buf[2] = {0xD1, 0xFC};
    if (write_then_read(write_buf, 2, buffer, 4) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read chip checkcode");
        return;
    }

    uint32_t checkcode = 0;
    checkcode = buffer[3]; checkcode <<= 8; checkcode |= buffer[2]; checkcode <<= 8; checkcode |= buffer[1]; checkcode <<= 8; checkcode |= buffer[0];
    ESP_LOGI(TAG, "Chip checkcode:0x%08lx.", checkcode);

    // Read resolution
    write_buf[0] = 0xD1; write_buf[1] = 0xF8;
    if (write_then_read(write_buf, 2, buffer, 4) == ESP_OK) {
        s_resX = (buffer[1] << 8) | buffer[0];
        s_resY = (buffer[3] << 8) | buffer[2];
        ESP_LOGI(TAG, "Chip resolution X:%u Y:%u", s_resX, s_resY);
        s_xMax = s_resX; s_yMax = s_resY;
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

    // Exit Command Mode
    write_single(0xD1, 0x09);
}

void cst226se_set_i2c_bus(i2c_master_bus_handle_t bus)
{
    if (bus) {
        s_bus = bus;
    }
}

bool cst226se_read(cst226se_data_t *data)
{
    if (!data) return false;

    uint8_t buffer[CST226SE_BUFFER_NUM];
    if (read_reg(0x00, buffer, CST226SE_BUFFER_NUM) != ESP_OK) {
        return false;
    }

#ifdef CONFIG_LOG_DEFAULT_LEVEL
    ESP_LOGD(TAG, "RAW:");
    for (int i = 0; i < CST226SE_BUFFER_NUM; ++i) {
        ESP_LOGD(TAG, "%02X,", buffer[i]);
    }
#endif

    // Home button detection
    if (buffer[0] == 0x83 && buffer[1] == 0x17 && buffer[5] == 0x80) {
        // Home button pressed - report no touch to caller but could trigger callback
        data->pressed = false;
        return true;
    }

    if (buffer[6] != 0xAB) return false;
    if (buffer[0] == 0xAB) return false;
    if (buffer[5] == 0x80) return false;

    uint8_t point = buffer[5] & 0x7F;
    if (point > 5 || point == 0) {
        write_single(0x00, 0xAB);
        data->pressed = false;
        return true;
    }

    // Parse first point
    uint8_t index = 0;
    uint16_t x = 0, y = 0;
    for (int i = 0; i < point; i++) {
        uint8_t id = buffer[index] >> 4;
        uint8_t status = buffer[index] & 0x0F;
        uint16_t px = (uint16_t)((buffer[index + 1] << 4) | ((buffer[index + 3] >> 4) & 0x0F));
        uint16_t py = (uint16_t)((buffer[index + 2] << 4) | (buffer[index + 3] & 0x0F));
        (void)status;
        (void)id;
        if (i == 0) { x = px; y = py; }
        index = (i == 0) ? (index + 7) : (index + 5);
    }

    // Apply rotation transforms (chip reports native landscape 450x600)
    int16_t tx = x;
    int16_t ty = y;
    if (s_swapXY) {
        int16_t tmp = tx; tx = ty; ty = tmp;
    }
    if (s_mirrorX && s_xMax) tx = s_xMax - tx;
    if (s_mirrorY && s_yMax) ty = s_yMax - ty;

    data->pressed = true;
    data->x = (uint16_t)tx;
    data->y = (uint16_t)ty;
    data->id = 0;
    return true;
}
