#include "sy6970.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "sy6970";

#define I2C_MASTER_FREQ_HZ          400000

static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;

static esp_err_t sy6970_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), -1);
}

static esp_err_t sy6970_read_reg(uint8_t reg, uint8_t *data) {
    return i2c_master_transmit_receive(dev_handle, &reg, 1, data, 1, -1);
}

static esp_err_t sy6970_update_reg(uint8_t reg, uint8_t mask, uint8_t val) {
    uint8_t data;
    esp_err_t ret = sy6970_read_reg(reg, &data);
    if (ret != ESP_OK) return ret;
    
    data &= ~mask;
    data |= (val & mask);
    
    return sy6970_write_reg(reg, data);
}

void sy6970_init(void) {
    ESP_LOGI(TAG, "Initializing SY6970 PMIC...");
    
    // Initialize I2C Master Bus
    i2c_master_bus_config_t bus_config = {
        .i2c_port = -1, // Auto-select
        .sda_io_num = SY6970_SDA_PIN,
        .scl_io_num = SY6970_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return;
    }

    // Add SY6970 Device
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SY6970_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device: %s", esp_err_to_name(ret));
        return;
    }

    uint8_t chip_id = 0;
    if (sy6970_read_reg(SY6970_REG_14, &chip_id) == ESP_OK) {
        ESP_LOGI(TAG, "SY6970 Chip ID/Rev: 0x%02X", chip_id);
    } else {
        ESP_LOGE(TAG, "Failed to read SY6970 Chip ID");
        return;
    }

    // Disable Watchdog (Reg 0x07, clear bits 5:4)
    sy6970_update_reg(SY6970_REG_07, SY6970_REG07_WD_MASK, 0);
    ESP_LOGI(TAG, "Watchdog Disabled");

    // Disable OTG (Reg 0x03, Bit 5)
    sy6970_enable_otg(false);

    // Enable Charging (Reg 0x03, Bit 4)
    sy6970_enable_charging(true);

    // Set Input Current Limit to Max (3.25A)
    sy6970_set_input_current_limit(3250);
    
    // Enable ADC for monitoring (Reg 0x02, Bit 7 = 1, Bit 6 = 1 for continuous)
    sy6970_update_reg(SY6970_REG_02, 0xC0, 0xC0);
}

esp_err_t sy6970_set_input_current_limit(uint16_t current_ma) {
    if (current_ma < 100) current_ma = 100;
    if (current_ma > 3250) current_ma = 3250;
    
    uint8_t val = (current_ma - 100) / 50;
    // Also clear EN_ILIM (Bit 6) to use register setting? Datasheet says "Actual input current limit is the lower of I2C or ILIM pin"
    // If we want to force I2C control, we might need to ensure ILIM pin isn't limiting it lower.
    // But usually we just set the register.
    
    return sy6970_update_reg(SY6970_REG_00, SY6970_REG00_IINLIM_MASK, val);
}

esp_err_t sy6970_enable_otg(bool enable) {
    return sy6970_update_reg(SY6970_REG_03, SY6970_REG03_OTG_CONFIG, enable ? SY6970_REG03_OTG_CONFIG : 0);
}

esp_err_t sy6970_enable_charging(bool enable) {
    return sy6970_update_reg(SY6970_REG_03, SY6970_REG03_CHG_CONFIG, enable ? SY6970_REG03_CHG_CONFIG : 0);
}

esp_err_t sy6970_set_charge_current(uint16_t current_ma) {
    if (current_ma > 5056) current_ma = 5056;
    uint8_t val = current_ma / 64;
    return sy6970_update_reg(SY6970_REG_04, 0x7F, val); // Bits 6:0
}

esp_err_t sy6970_set_charge_voltage(uint16_t voltage_mv) {
    if (voltage_mv < 3840) voltage_mv = 3840;
    if (voltage_mv > 4608) voltage_mv = 4608;
    
    uint8_t val = (voltage_mv - 3840) / 16;
    return sy6970_update_reg(SY6970_REG_06, 0xFC, val << 2); // Bits 7:2
}

esp_err_t sy6970_set_min_system_voltage(uint16_t voltage_mv) {
    if (voltage_mv < 3000) voltage_mv = 3000;
    if (voltage_mv > 3700) voltage_mv = 3700;
    
    uint8_t val = (voltage_mv - 3000) / 100;
    return sy6970_update_reg(SY6970_REG_03, SY6970_REG03_SYS_MIN_MASK, val << 1); // Bits 3:1
}

esp_err_t sy6970_enable_hiz_mode(bool enable) {
    return sy6970_update_reg(SY6970_REG_00, SY6970_REG00_EN_HIZ, enable ? SY6970_REG00_EN_HIZ : 0);
}

esp_err_t sy6970_disable_batfet(bool disable) {
    return sy6970_update_reg(SY6970_REG_09, SY6970_REG09_BATFET_DIS, disable ? SY6970_REG09_BATFET_DIS : 0);
}

esp_err_t sy6970_reset_watchdog(void) {
    return sy6970_update_reg(SY6970_REG_03, SY6970_REG03_WD_RST, SY6970_REG03_WD_RST);
}

uint16_t sy6970_get_vbus_voltage(void) {
    uint8_t val;
    sy6970_read_reg(SY6970_REG_11, &val);
    if (!(val & 0x80)) return 0; // BUS_GD not set
    val &= 0x7F;
    return 2600 + (val * 100);
}

uint16_t sy6970_get_battery_voltage(void) {
    uint8_t val;
    sy6970_read_reg(SY6970_REG_0E, &val);
    val &= 0x7F;
    return 2304 + (val * 20);
}

uint16_t sy6970_get_system_voltage(void) {
    uint8_t val;
    sy6970_read_reg(SY6970_REG_0F, &val);
    val &= 0x7F;
    return 2304 + (val * 20);
}

uint16_t sy6970_get_charge_current(void) {
    uint8_t val;
    sy6970_read_reg(SY6970_REG_12, &val);
    val &= 0x7F;
    return val * 50;
}

uint8_t sy6970_get_ntc_percentage(void) {
    uint8_t val;
    sy6970_read_reg(SY6970_REG_10, &val);
    val &= 0x7F;
    // NTC/REGN = 21% + [val]*0.465%
    // We return integer percentage, so roughly:
    return 21 + (val * 465 / 1000);
}

sy6970_bus_status_t sy6970_get_bus_status(void) {
    uint8_t val;
    sy6970_read_reg(SY6970_REG_0B, &val);
    return (sy6970_bus_status_t)((val >> 5) & 0x07);
}

sy6970_charge_status_t sy6970_get_charge_status(void) {
    uint8_t val;
    sy6970_read_reg(SY6970_REG_0B, &val);
    return (sy6970_charge_status_t)((val >> 3) & 0x03);
}

bool sy6970_is_power_good(void) {
    uint8_t val;
    sy6970_read_reg(SY6970_REG_0B, &val);
    return (val & 0x04) ? true : false;
}

bool sy6970_is_vbus_connected(void) {
    uint8_t val;
    sy6970_read_reg(SY6970_REG_11, &val);
    return (val & 0x80) ? true : false;
}
