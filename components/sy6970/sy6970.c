#include "sy6970.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "sy6970";

#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          400000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0

static esp_err_t sy6970_write_reg(uint8_t reg, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SY6970_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t sy6970_read_reg(uint8_t reg, uint8_t *data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SY6970_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SY6970_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

void sy6970_init(void) {
    ESP_LOGI(TAG, "Initializing SY6970 PMIC...");
    
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SY6970_SDA_PIN,
        .scl_io_num = SY6970_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0));

    uint8_t chip_id = 0;
    if (sy6970_read_reg(0x14, &chip_id) == ESP_OK) {
        ESP_LOGI(TAG, "SY6970 Chip ID: 0x%02X", chip_id);
    } else {
        ESP_LOGE(TAG, "Failed to read SY6970 Chip ID");
        return;
    }

    // Disable Watchdog (Reg 0x07, clear bits 5:4)
    uint8_t reg07 = 0;
    sy6970_read_reg(0x07, &reg07);
    reg07 &= 0xCF; // Clear bits 4 and 5
    sy6970_write_reg(0x07, reg07);
    ESP_LOGI(TAG, "Watchdog Disabled (Reg 0x07: 0x%02X)", reg07);

    // Disable OTG (Reg 0x03, Bit 5) - Matches LilyGo implementation
    uint8_t reg03 = 0;
    sy6970_read_reg(0x03, &reg03);
    reg03 &= ~(1 << 5); // Clear Bit 5
    sy6970_write_reg(0x03, reg03);
    ESP_LOGI(TAG, "OTG Disabled (Reg 0x03: 0x%02X)", reg03);

    // Set Input Current Limit to Max (Reg 0x00)
    uint8_t reg00 = 0;
    sy6970_read_reg(0x00, &reg00);
    reg00 |= 0x3F; // Set bits 0-5 to 1 (Max current)
    reg00 &= ~(1 << 7); // Clear Bit 7 (Enable ILIM)
    sy6970_write_reg(0x00, reg00);
    ESP_LOGI(TAG, "Input Current Limit Max (Reg 0x00: 0x%02X)", reg00);
    
    // Dump Registers for Debugging
    ESP_LOGI(TAG, "--- SY6970 Register Dump ---");
    for (uint8_t r = 0; r <= 0x14; r++) {
        uint8_t val = 0;
        sy6970_read_reg(r, &val);
        ESP_LOGI(TAG, "Reg 0x%02X: 0x%02X", r, val);
    }
    ESP_LOGI(TAG, "----------------------------");
}
