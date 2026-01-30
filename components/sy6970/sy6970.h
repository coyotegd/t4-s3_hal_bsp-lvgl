#ifndef SY6970_H
#define SY6970_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SY6970_I2C_ADDR 0x6A
#define SY6970_SDA_PIN  6
#define SY6970_SCL_PIN  7

// Register Map
#define SY6970_REG_00 0x00 // Input Current Limit
#define SY6970_REG_01 0x01 // VINDPM/Temp
#define SY6970_REG_02 0x02 // ADC/Boost Freq
#define SY6970_REG_03 0x03 // Sys Min/OTG/Charge Config
#define SY6970_REG_04 0x04 // Fast Charge Current
#define SY6970_REG_05 0x05 // Precharge/Term Current
#define SY6970_REG_06 0x06 // Charge Voltage
#define SY6970_REG_07 0x07 // Term/Stat/Watchdog/Timer
#define SY6970_REG_08 0x08 // IR Comp/Thermal Reg
#define SY6970_REG_09 0x09 // Safety Timer/BATFET
#define SY6970_REG_0A 0x0A // Boost Voltage/Current
#define SY6970_REG_0B 0x0B // Status 0 (BUS/Charge)
#define SY6970_REG_0C 0x0C // Status 1 (Faults)
#define SY6970_REG_0D 0x0D // VINDPM Threshold
#define SY6970_REG_0E 0x0E // Battery Voltage (ADC)
#define SY6970_REG_0F 0x0F // System Voltage (ADC)
#define SY6970_REG_10 0x10 // NTC % (ADC)
#define SY6970_REG_11 0x11 // USB VBUS Voltage (ADC)
#define SY6970_REG_12 0x12 // Charge Current (ADC)
#define SY6970_REG_13 0x13 // DPM Status
#define SY6970_REG_14 0x14 // Reset/Rev

// Bit Masks & Definitions
#define SY6970_REG00_EN_HIZ         (1 << 7)
#define SY6970_REG00_EN_ILIM        (1 << 6)
#define SY6970_REG00_IINLIM_MASK    0x3F
#define SY6970_REG02_EN_ADC         (1 << 7)
#define SY6970_REG02_ADC_CONT       (1 << 6)
#define SY6970_REG03_WD_RST         (1 << 6)
#define SY6970_REG03_OTG_CONFIG     (1 << 5)
#define SY6970_REG03_CHG_CONFIG     (1 << 4)
#define SY6970_REG03_SYS_MIN_MASK   0x0E
#define SY6970_REG07_EN_TERM        (1 << 7)
#define SY6970_REG07_STAT_DIS       (1 << 6)
#define SY6970_REG07_WD_MASK        0x30
#define SY6970_REG07_EN_TIMER       (1 << 3)
#define SY6970_REG09_BATFET_DIS     (1 << 5)
#define SY6970_REG09_BATFET_RST_EN  (1 << 2)

// Status Flags (REG_0B)
#define SY6970_REG0B_VBUS_STAT_MASK 0xC0
#define SY6970_REG0B_CHG_STAT_MASK  0x18
#define SY6970_REG0B_PG_STAT        (1 << 2)

// Fault Flags (REG_0C)
#define SY6970_FAULT_WDT        (1 << 7)
#define SY6970_FAULT_BOOST      (1 << 6)
#define SY6970_FAULT_CHG_MASK   0x30
#define SY6970_FAULT_BAT_OVP    (1 << 3)
#define SY6970_FAULT_NTC_MASK   0x07

// Status Enums
typedef enum {
    SY6970_CHG_NOT_CHARGING = 0,
    SY6970_CHG_PRE_CHARGE   = 1,
    SY6970_CHG_FAST_CHARGE  = 2,
    SY6970_CHG_TERM_DONE    = 3
} sy6970_charge_status_t;

typedef enum {
    SY6970_WDT_DISABLE = 0,
    SY6970_WDT_40S     = 1,
    SY6970_WDT_80S     = 2,
    SY6970_WDT_160S    = 3
} sy6970_wdt_t;

// API Functions
esp_err_t sy6970_init(void);
esp_err_t sy6970_deinit(void);

// Return the I2C bus handle created by `sy6970_init()` so other components
// can share the same I2C bus. Returns NULL if the bus hasn't been created.
i2c_master_bus_handle_t sy6970_get_bus_handle(void);

// Register Access (Exposed for debugging)
esp_err_t sy6970_read_reg(uint8_t reg_addr, uint8_t *data);
esp_err_t sy6970_write_reg(uint8_t reg_addr, uint8_t data);
esp_err_t sy6970_update_reg(uint8_t reg_addr, uint8_t mask, uint8_t val);

esp_err_t sy6970_enable_adc(bool enable, bool continuous);  // Enable/disable ADC monitoring
esp_err_t sy6970_enable_charging(bool enable);  
esp_err_t sy6970_enable_otg(bool enable);  // Enable/disable Boost OTG (On The Go) mode
// HIZ - enabled the input Field-Effect Transistor turns OFF as if the USB cable is unplugged. Data + & - 
// still work. SoC runs purely off the battery. It stops charging and stops drawing any power from VBUS.
esp_err_t sy6970_enable_hiz_mode(bool enable); 
bool sy6970_get_hiz_status(void); // Getter for HIZ status
esp_err_t sy6970_disable_batfet(bool disable); // Ship, storage, complete shutdown. Plugin USB to enable

// Power Control
// -- Charging / Input Control --
esp_err_t sy6970_set_input_current_limit(uint16_t current_ma);
esp_err_t sy6970_set_input_voltage_limit(uint16_t voltage_mv); // VINDPM
esp_err_t sy6970_set_charge_current(uint16_t current_ma);
esp_err_t sy6970_set_precharge_current(uint16_t current_ma);
esp_err_t sy6970_set_termination_current(uint16_t current_ma);
esp_err_t sy6970_set_charge_voltage(uint16_t voltage_mv);
esp_err_t sy6970_set_min_system_voltage(uint16_t voltage_mv);  // when USB & low bat min voltage
// Configuration getters
uint16_t sy6970_get_input_current_limit(void);
uint16_t sy6970_get_input_voltage_limit(void);
uint16_t sy6970_get_charge_current_limit(void);
uint16_t sy6970_get_precharge_current_limit(void);
uint16_t sy6970_get_termination_current_limit(void);
uint16_t sy6970_get_charge_voltage_limit(void);
uint16_t sy6970_get_min_system_voltage_limit(void);

// -- OTG / Boost Control --
esp_err_t sy6970_set_boost_voltage(uint16_t voltage_mv);
uint16_t sy6970_get_boost_voltage(void); // Getter for Boost Voltage
bool sy6970_get_otg_status(void);        // Getter for OTG Status

esp_err_t sy6970_set_watchdog_timer(sy6970_wdt_t timeout);  // SY6970_WDT_DISABLE = 0
esp_err_t sy6970_reset_watchdog(void);                      // or "kick", "feed" the watchdog timer

// ADC Monitoring
uint16_t sy6970_get_vbus_voltage(void);  // USB VBUS voltage in mV
uint16_t sy6970_get_battery_voltage(void);  // Battery voltage (may show elevated during charging)
uint16_t sy6970_get_system_voltage(void);
uint16_t sy6970_get_charge_current(void);
uint8_t sy6970_get_ntc_percentage(void);
const char* sy6970_get_ntc_temperature_status(uint8_t ntc_percent);  // Temperature status from NTC%
uint8_t sy6970_get_faults(void);
const char* sy6970_decode_faults(uint8_t fault_reg);  // Human-readable fault description
sy6970_charge_status_t sy6970_get_charge_status(void);
bool sy6970_is_power_good(void);
bool sy6970_is_vbus_connected(void);  // USB VBUS present plugged or unplugged

// STAT LED Control
// The STAT pin (pin 4) is hardware-controlled by the SY6970 charger IC:
//   - STAT LOW (LED ON/solid) = Charging in progress
//   - STAT HIGH (LED OFF) = Charge done or disabled
//   - STAT blinking at 1Hz = Fault condition (hardware automatic)
// 
// Software can only enable/disable the STAT pin output via STAT_DIS bit (REG_07[6]):
//   - enable=true: STAT pin enabled (hardware controls LED based on charge/fault state)
//   - enable=false: STAT pin disabled (LED always off)
esp_err_t sy6970_enable_stat_led(bool enable);

#ifdef __cplusplus
}
#endif

#endif
