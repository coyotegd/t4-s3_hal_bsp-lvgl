#include "ui_private.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "sy6970.h"
#include "sd_card.h"

void update_stats_timer_cb(lv_timer_t * timer) {
    // Update System Info
    if (lbl_sys_info) {
        uint32_t free_heap = esp_get_free_heap_size();
        uint32_t min_free_heap = esp_get_minimum_free_heap_size();
        int64_t uptime = esp_timer_get_time() / 1000000;

        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);

        const char * model_str = "Unknown";
        const char * bt_ver = "";
        
        switch(chip_info.model) {
            case CHIP_ESP32:   model_str = "ESP32";    bt_ver = "BT 4.2 "; break;
            case CHIP_ESP32S2: model_str = "ESP32-S2"; bt_ver = "";        break;
            case CHIP_ESP32S3: model_str = "ESP32-S3"; bt_ver = "BLE 5.0 "; break;
            case CHIP_ESP32C3: model_str = "ESP32-C3"; bt_ver = "BLE 5.0 "; break;
            case CHIP_ESP32C2: model_str = "ESP32-C2"; bt_ver = "BLE 5.0 "; break;
            case CHIP_ESP32C6: model_str = "ESP32-C6"; bt_ver = "BLE 5.3 "; break;
            case CHIP_ESP32H2: model_str = "ESP32-H2"; bt_ver = "BLE 5.3 "; break;
            default:           model_str = "Generic";  bt_ver = "BLE ";     break;
        }

        bool sd_mounted = sd_card_is_mounted();
        
        lv_label_set_text_fmt(lbl_sys_info, 
            "System Info:\n"
            "Free Heap: %" PRIu32 " bytes\n"
            "Min Free Heap: %" PRIu32 " bytes\n"
            "Uptime: %" PRId64 " s\n\n"
            "Chip: %s\n"
            "Cores: %d\n"
            "Revision: %d\n"
            "Features: %s%s%s\n\n"
            "Components:\n"
            "- PMIC: SY6970\n"
            "- Display: RM690B0 (AMOLED)\n"
            "- Touch: CST226SE\n"
            "- SD Card: %s",
            free_heap, min_free_heap, uptime,
            model_str,
            chip_info.cores, chip_info.revision,
            (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi - " : "No WiFi - ",
            (chip_info.features & CHIP_FEATURE_BLE) ? bt_ver : "No BLE",
            (chip_info.features & CHIP_FEATURE_IEEE802154) ? "- Zigbee/Thread" : "- No Zigbee/Thread ",
            sd_mounted ? "Mounted" : "Unmounted");
    }

    // Update PMIC Info
    if (lbl_sys_volts) { // Check one label to assume others are ready (or check all if safer)
        uint16_t sys_volts = sy6970_get_system_voltage();
        // Use get_battery_voltage_accurate() to get true battery voltage even when charging.
        // This function briefly pauses charging to take the reading, ensuring we don't just see the system/charging voltage.
        uint16_t batt_volts = sy6970_get_battery_voltage_accurate();
        uint16_t chg_curr = sy6970_get_charge_current();
        uint16_t usb_volts = sy6970_get_vbus_voltage();
        uint8_t ntc_pct = sy6970_get_ntc_percentage();
        bool vbus_conn = sy6970_is_vbus_connected();
        sy6970_charge_status_t chg_status = sy6970_get_charge_status();
        uint8_t faults = sy6970_get_faults();

        // Heuristic to detect No Battery via voltage fluctuation or NTC fault
        // When no battery is present, the charging circuit often "hiccups" causing rapid voltage swings
        static uint16_t last_batt_volts = 0;
        static int volatility_score = 0;
        
        // Calculate absolute difference (simple math to avoid stdlib dep if needed)
        int diff = (int)batt_volts - (int)last_batt_volts;
        if (diff < 0) diff = -diff;
        
        if (vbus_conn) {
             // If voltage moves >50mV between samples, it's likely unstable/no battery
             // A real battery is very stable.
             if (diff > 50) {
                 // Fast attack: jump up quickly on detection
                 // Cap at 20 (max duration ~10s at 2 ticks/sec decay)
                 if (volatility_score < 20) volatility_score += 5;
             } else {
                 // Slow decay: require many stable samples to clear
                 if (volatility_score > 0) volatility_score--;
             }
        } else {
             // No USB, so if we are running, we must have a battery (or large cap, but treated as batt)
             volatility_score = 0;
        }
        last_batt_volts = batt_volts;

        // Detect battery disconnected: NTC fault (bits 2:0) OR High Volatility (hysteresis at 5)
        bool battery_disconnected = ((faults & SY6970_FAULT_NTC_MASK) != 0) || (volatility_score >= 5);

        const char * chg_str = "Not Charging";
        if (chg_status == SY6970_CHG_PRE_CHARGE) chg_str = "Pre-Charge";
        else if (chg_status == SY6970_CHG_FAST_CHARGE) chg_str = "Fast Charge";
        else if (chg_status == SY6970_CHG_TERM_DONE) chg_str = "Done";

        if (lbl_sys_volts) lv_label_set_text_fmt(lbl_sys_volts, "System Volts:\n%d mV", sys_volts);
        if (lbl_batt) {
            if (battery_disconnected) {
                lv_label_set_text(lbl_batt, "Battery Volts:\nNo Battery");
            } else {
                lv_label_set_text_fmt(lbl_batt, "Battery Volts:\n%d mV", batt_volts);
            }
        }
        if (lbl_chg_stat) {
            if (battery_disconnected) {
                lv_label_set_text(lbl_chg_stat, "Charge Status:\nN/A - No Battery");
            } else {
                lv_label_set_text_fmt(lbl_chg_stat, "Charge Status:\n%s", chg_str);
            }
        }
        if (lbl_chg_curr) lv_label_set_text_fmt(lbl_chg_curr, "Charging Current:\n%d mA", chg_curr);
        if (lbl_usb) lv_label_set_text_fmt(lbl_usb, "USB:\n%s", vbus_conn ? "Connected" : "Disconnected");
        if (lbl_usb_volts) lv_label_set_text_fmt(lbl_usb_volts, "USB Volts:\n%d mV", usb_volts);
        if (lbl_ntc) {
            const char* temp_status = sy6970_get_ntc_temperature_status(ntc_pct);
            lv_label_set_text_fmt(lbl_ntc, "Temperature: %d%%\n%s", ntc_pct, temp_status);
            
            // Set color based on temperature status
            if (ntc_pct >= 73) {
                // COLD (<0°C) - Blue
                lv_obj_set_style_text_color(lbl_ntc, lv_color_hex(0x4DA6FF), 0);
            } else if (ntc_pct >= 68) {
                // COOL (0-10°C) - Light Blue
                lv_obj_set_style_text_color(lbl_ntc, lv_color_hex(0x80D4FF), 0);
            } else if (ntc_pct >= 45) {
                // NORMAL (10-45°C) - Green
                lv_obj_set_style_text_color(lbl_ntc, lv_color_hex(0x00FF00), 0);
            } else if (ntc_pct >= 38) {
                // WARM (45-60°C) - Orange
                lv_obj_set_style_text_color(lbl_ntc, lv_color_hex(0xFFA500), 0);
            } else {
                // HOT (>60°C) - Red
                lv_obj_set_style_text_color(lbl_ntc, lv_color_hex(0xFF0000), 0);
            }
        }
        
        // USB Wattage (Approx)
        if (lbl_usb_pg) {
             // Simple P = V * I calculation if we had input current, but we only have charge current.
             // For now, just show N/A or maybe calculate if charging.
             // Let's just show VBUS status for now or remove it if not useful.
             // Or maybe "Power Good" status
             bool pg = sy6970_is_power_good();
             lv_label_set_text_fmt(lbl_usb_pg, "USB Power:\n%s", pg ? "Yes" : "No");
        }
        
        // Fault Status
        if (lbl_fault) {
            bool has_fault = false;
            
            if (battery_disconnected) {
                // Battery disconnected - show specific message
                lv_label_set_text(lbl_fault, "Fault:\nBattery Disconnected\n(LED blinking 1Hz)");
                lv_obj_set_style_text_color(lbl_fault, lv_color_hex(0xFFA500), 0); // Orange warning
                has_fault = true;
            } else if (faults == 0) {
                sy6970_charge_status_t chg_status = sy6970_get_charge_status();
                if (chg_status == SY6970_CHG_PRE_CHARGE || chg_status == SY6970_CHG_FAST_CHARGE) {
                    lv_label_set_text(lbl_fault, "Fault:\nNone (LED on) charging");
                } else if (chg_status == SY6970_CHG_TERM_DONE) {
                    lv_label_set_text(lbl_fault, "Fault:\nNone (LED off) charge done");
                } else {
                    lv_label_set_text(lbl_fault, "Fault:\nNone (LED off) no USB");
                }
                lv_obj_set_style_text_color(lbl_fault, lv_color_hex(0x00FF00), 0); // Green
            } else {
                const char* fault_desc = sy6970_decode_faults(faults);
                lv_label_set_text_fmt(lbl_fault, "Fault:\n%s (LED blinking 1Hz)", fault_desc);
                lv_obj_set_style_text_color(lbl_fault, lv_color_hex(0xFF0000), 0); // Red
                has_fault = true;
            }
            
            // Enable/Disable LED Switch based on fault status
            if (sw_disable_led) {
                if (has_fault) {
                    lv_obj_remove_state(sw_disable_led, LV_STATE_DISABLED);
                } else {
                    lv_obj_add_state(sw_disable_led, LV_STATE_DISABLED);
                    if (lv_obj_has_state(sw_disable_led, LV_STATE_CHECKED)) {
                        lv_obj_remove_state(sw_disable_led, LV_STATE_CHECKED);
                        // Make sure LED is re-enabled if we disabled the switch
                         sy6970_enable_stat_led(true);
                    }
                }
            }
        }
    }
}
