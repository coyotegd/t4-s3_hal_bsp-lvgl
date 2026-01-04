#include "ui_private.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "sy6970.h"

void update_stats_timer_cb(lv_timer_t * timer) {
    // Update System Info
    if (lbl_sys_info) {
        uint32_t free_heap = esp_get_free_heap_size();
        uint32_t min_free_heap = esp_get_minimum_free_heap_size();
        int64_t uptime = esp_timer_get_time() / 1000000;

        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);

        lv_label_set_text_fmt(lbl_sys_info, 
            "System Info:\n"
            "Free Heap: %" PRIu32 " bytes\n"
            "Min Free Heap: %" PRIu32 " bytes\n"
            "Uptime: %" PRId64 " s\n\n"
            "Chip: ESP32-S3\n"
            "Cores: %d\n"
            "Revision: %d\n"
            "Features: %s%s%s\n\n"
            "Components:\n"
            "- PMIC: SY6970\n"
            "- Display: RM690B0 (AMOLED)\n"
            "- Touch: CST226SE\n"
            "- SD Card: SPI Mode",
            free_heap, min_free_heap, uptime,
            chip_info.cores, chip_info.revision,
            (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi " : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "BLE " : "",
            (chip_info.features & CHIP_FEATURE_IEEE802154) ? "802.15.4" : "");
    }

    // Update PMIC Info
    if (lbl_sys_volts) { // Check one label to assume others are ready (or check all if safer)
        uint16_t sys_volts = sy6970_get_system_voltage();
        uint16_t batt_volts = sy6970_get_battery_voltage();
        uint16_t chg_curr = sy6970_get_charge_current();
        uint16_t usb_volts = sy6970_get_vbus_voltage();
        uint8_t ntc_pct = sy6970_get_ntc_percentage();
        bool vbus_conn = sy6970_is_vbus_connected();
        sy6970_charge_status_t chg_status = sy6970_get_charge_status();

        const char * chg_str = "Not Charging";
        if (chg_status == SY6970_CHG_PRE_CHARGE) chg_str = "Pre-Charge";
        else if (chg_status == SY6970_CHG_FAST_CHARGE) chg_str = "Fast Charge";
        else if (chg_status == SY6970_CHG_TERM_DONE) chg_str = "Done";

        if (lbl_sys_volts) lv_label_set_text_fmt(lbl_sys_volts, "System Volts:\n%d mV", sys_volts);
        if (lbl_batt) lv_label_set_text_fmt(lbl_batt, "Battery Volts:\n%d mV", batt_volts);
        if (lbl_chg_stat) lv_label_set_text_fmt(lbl_chg_stat, "Charge Status:\n%s", chg_str);
        if (lbl_chg_curr) lv_label_set_text_fmt(lbl_chg_curr, "Charging Current:\n%d mA", chg_curr);
        if (lbl_usb) lv_label_set_text_fmt(lbl_usb, "USB:\n%s", vbus_conn ? "Connected" : "Disconnected");
        if (lbl_usb_volts) lv_label_set_text_fmt(lbl_usb_volts, "USB Volts:\n%d mV", usb_volts);
        if (lbl_ntc) lv_label_set_text_fmt(lbl_ntc, "Temperature:\n%d %%", ntc_pct);
        
        // USB Wattage (Approx)
        if (lbl_usb_pg) {
             // Simple P = V * I calculation if we had input current, but we only have charge current.
             // For now, just show N/A or maybe calculate if charging.
             // Let's just show VBUS status for now or remove it if not useful.
             // Or maybe "Power Good" status
             bool pg = sy6970_is_power_good();
             lv_label_set_text_fmt(lbl_usb_pg, "Power Good:\n%s", pg ? "Yes" : "No");
        }
    }
}
