#include "sy6970.h"
#include <stdbool.h>
#include <esp_err.h>
#include "driver/i2c_master.h"
#include "esp_log.h"

#define I2C_MASTER_FREQ_HZ          400000 // 400kHz

static const char *TAG = "sy6970";

// Forward declaration for static function used before definition
// static esp_err_t sy6970_update_reg(uint8_t reg, uint8_t mask, uint8_t val);
static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;

// Control STAT LED (true = ON, false = OFF)
esp_err_t sy6970_set_stat_led(bool on) {
    // STAT_DIS bit (bit 6) in REG_07: 0 = enable STAT, 1 = disable STAT
    // So to turn ON, clear bit; to turn OFF, set bit
    return sy6970_update_reg(SY6970_REG_07, SY6970_REG07_STAT_DIS, on ? 0 : SY6970_REG07_STAT_DIS);
}

esp_err_t sy6970_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), -1);
}

esp_err_t sy6970_read_reg(uint8_t reg, uint8_t *data) {
    return i2c_master_transmit_receive(dev_handle, &reg, 1, data, 1, -1);
}

esp_err_t sy6970_update_reg(uint8_t reg, uint8_t mask, uint8_t val) {
    uint8_t data;
    esp_err_t ret = sy6970_read_reg(reg, &data);
    if (ret != ESP_OK) return ret;
    data &= ~mask;
    data |= (val & mask);
    return sy6970_write_reg(reg, data);
}

esp_err_t sy6970_init(void) {
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
        return ret;
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
        return ret;
    }

    uint8_t chip_id = 0;
    if (sy6970_read_reg(SY6970_REG_14, &chip_id) == ESP_OK) {
        ESP_LOGI(TAG, "SY6970 Chip ID/Rev: 0x%02X", chip_id);
    } else {
        ESP_LOGE(TAG, "Failed to read SY6970 Chip ID");
        return ESP_FAIL;
    }

    // Factory default settings (see datasheet)
    // REG_00: Input Current Limit = 2000mA (0x17), EN_HIZ=0, EN_ILIM=0
    // 0x17 = 0001 0111 -> 2.0A (Assuming 100mA steps or table lookup, typical for this class)
    // Let's try a higher limit to prevent input sagging with the screen on.
    sy6970_update_reg(SY6970_REG_00, 0xFF, 0x17); 

    // REG_01: VINDPM default, temp default (0x1B)
    sy6970_update_reg(SY6970_REG_01, 0xFF, 0x1B);

    // REG_02: ADC/Boost Freq default (0x3C)
    // Enable ADC (Bit 7) and Continuous Mode (Bit 6) -> 0xFC
    sy6970_update_reg(SY6970_REG_02, 0xFF, 0x3C | SY6970_REG02_EN_ADC | SY6970_REG02_ADC_CONT);

    // REG_03: SYS_MIN=3.5V, OTG=0, CHG=1 (0x18)
    sy6970_update_reg(SY6970_REG_03, 0xFF, 0x18);

    // REG_04: Fast Charge Current default (0x20)
    sy6970_update_reg(SY6970_REG_04, 0xFF, 0x20);

    // REG_05: Precharge/Term Current default (0x04)
    sy6970_update_reg(SY6970_REG_05, 0xFF, 0x04);

    // REG_07: EN_TERM=1, STAT_DIS=1 (Disable LED), WD=00 (Disable), EN_TIMER=1
    // 0xC9 = 1100 1001
    // We disable the STAT LED by default so we can control it manually in HAL.
    sy6970_update_reg(SY6970_REG_07, 0xFF, 0xC9);

    // REG_06: Charge Voltage
    // Default was 0xB2 (4.544V) which is too high for standard LiPo (4.2V).
    // Set to 4.208V (0x5C) to prevent BMS over-voltage protection tripping.
    // 4208mV: (4208 - 3840) / 16 = 23 (0x17). 0x17 << 2 = 0x5C.
    sy6970_update_reg(SY6970_REG_06, 0xFF, 0x5C);

    // REG_08: IR Comp/Thermal Reg default (0x7B)
    sy6970_update_reg(SY6970_REG_08, 0xFF, 0x7B);

    // REG_09: Safety Timer/BATFET default (0x8C)
    sy6970_update_reg(SY6970_REG_09, 0xFF, 0x8C);

    // REG_0A: Boost Voltage/Current default (0x20)
    sy6970_update_reg(SY6970_REG_0A, 0xFF, 0x20);

    // REG_0B: Status 0 (read only)
    // REG_0C: Status 1 (read only)
    // REG_0D: VINDPM Threshold default (0x1B)
    sy6970_update_reg(SY6970_REG_0D, 0xFF, 0x1B);

    // REG_0E: Battery Voltage (ADC, read only)
    // REG_0F: System Voltage (ADC, read only)
    // REG_10: NTC % (ADC, read only)
    // REG_11: VBUS Voltage (ADC, read only)
    // REG_12: Charge Current (ADC, read only)
    // REG_13: DPM Status (read only)
    // REG_14: Reset/Rev (read only)

    ESP_LOGI(TAG, "SY6970 set to factory defaults");
    return ESP_OK;
}

i2c_master_bus_handle_t sy6970_get_bus_handle(void)
{
    return bus_handle;
}

esp_err_t sy6970_set_input_current_limit(uint16_t current_ma) {
    if (current_ma < 100) current_ma = 100;
    if (current_ma > 3250) current_ma = 3250;
    
    uint8_t val = (current_ma - 100) / 50;
    // Also clear EN_ILIM (Bit 6) to use register setting? Datasheet says "Actual input current
    // limit is the lower of I2C or ILIM pin"
    // If we want to force I2C control, we might need to ensure ILIM pin isn't limiting it lower.
    // But usually we just set the register.
    
    return sy6970_update_reg(SY6970_REG_00, SY6970_REG00_IINLIM_MASK, val);
}

esp_err_t sy6970_set_input_voltage_limit(uint16_t voltage_mv) {
    if (voltage_mv < 3900) voltage_mv = 3900;
    if (voltage_mv > 15300) voltage_mv = 15300;
    uint8_t val = (voltage_mv - 3900) / 100;
    return sy6970_update_reg(SY6970_REG_0D, 0x7F, val);
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

esp_err_t sy6970_set_precharge_current(uint16_t current_ma) {
    if (current_ma < 64) current_ma = 64;
    if (current_ma > 1024) current_ma = 1024;
    uint8_t val = (current_ma - 64) / 64;
    return sy6970_update_reg(SY6970_REG_05, 0xF0, val << 4); // Bits 7:4
}

esp_err_t sy6970_set_termination_current(uint16_t current_ma) {
    if (current_ma < 64) current_ma = 64;
    if (current_ma > 1024) current_ma = 1024;
    uint8_t val = (current_ma - 64) / 64;
    return sy6970_update_reg(SY6970_REG_05, 0x0F, val); // Bits 3:0
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

esp_err_t sy6970_set_boost_voltage(uint16_t voltage_mv) {
    if (voltage_mv < 4550) voltage_mv = 4550;
    if (voltage_mv > 5510) voltage_mv = 5510;
    uint8_t val = (voltage_mv - 4550) / 64;
    return sy6970_update_reg(SY6970_REG_0A, 0xF0, val << 4); // Bits 7:4
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

esp_err_t sy6970_set_watchdog_timer(sy6970_wdt_t timeout) {
    return sy6970_update_reg(SY6970_REG_07, SY6970_REG07_WD_MASK, timeout << 4);
}

esp_err_t sy6970_enable_adc(bool enable, bool continuous) {
    uint8_t val = 0;
    if (enable) val |= SY6970_REG02_EN_ADC;
    if (continuous) val |= SY6970_REG02_ADC_CONT;
    return sy6970_update_reg(SY6970_REG_02, SY6970_REG02_EN_ADC | SY6970_REG02_ADC_CONT, val);
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
    // Check charge status first
    uint8_t status_reg;
    sy6970_read_reg(SY6970_REG_0B, &status_reg);
    uint8_t chg_stat = (status_reg >> 3) & 0x03;

    // If Not Charging or Done, return 0 mA to mask ADC offset/noise
    if (chg_stat == SY6970_CHG_NOT_CHARGING || chg_stat == SY6970_CHG_TERM_DONE) {
        return 0;
    }

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

uint8_t sy6970_get_faults(void) {
    uint8_t val;
    sy6970_read_reg(SY6970_REG_0C, &val);
    return val;
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
    uint8_t val = 0;
    // Use REG_0B (Status 0) PG_STAT (Bit 2)
    // This indicates input power is valid (above UVLO and above Battery)
    if (sy6970_read_reg(SY6970_REG_0B, &val) != ESP_OK) return false;
    return (val & 0x04) != 0;
}
