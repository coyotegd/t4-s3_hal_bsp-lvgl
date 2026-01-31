#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ssid[33];
    int8_t rssi;
    int auth_mode; // Corresponds to wifi_auth_mode_t
} wifi_scan_item_t;

typedef void (*wifi_scan_cb_t)(wifi_scan_item_t *networks, int count);
typedef void (*wifi_connect_cb_t)(bool connected);

esp_err_t wifi_mgr_init(void);
esp_err_t wifi_mgr_start_scan(wifi_scan_cb_t cb);
esp_err_t wifi_mgr_connect(const char *ssid, const char *password, wifi_connect_cb_t cb);
bool wifi_mgr_is_connected(void);
const char* wifi_mgr_get_ip(void);
const char* wifi_mgr_get_ssid(void);

#ifdef __cplusplus
}
#endif
