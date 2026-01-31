#include "wifi_mgr.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_sntp.h"
#include <time.h>
#include <sys/time.h>
#include "esp_http_client.h"
#include "cJSON.h"

static const char *TAG = "wifi_mgr";

// Callbacks
static wifi_scan_cb_t s_scan_cb = NULL;
static wifi_connect_cb_t s_connect_cb = NULL;

static bool s_is_connected = false;
static char s_ip_addr[16] = "0.0.0.0";
static char s_ssid[33] = "";
static int s_retry_num = 0;
#define MAX_RETRY 5

// --- Timezone Auto-Detection ---

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    static int output_len;
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (evt->user_data) {
                    memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                    output_len += evt->data_len;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            output_len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void wifi_mgr_fetch_timezone_task(void *pvParameters) {
    // 1. Wait briefly for stable connection (reduced from 3000ms)
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "Fetching timezone from ip-api.com...");
    
    char *response_buffer = malloc(2048);
    if (!response_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for HTTP response");
        vTaskDelete(NULL);
        return;
    }

    esp_http_client_config_t config = {
        .url = "http://ip-api.com/json/?fields=status,offset,timezone",
        .event_handler = _http_event_handler,
        .user_data = response_buffer,
        .timeout_ms = 4000, // Reduced timeout for faster failure/retry
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = ESP_FAIL;
    int retry = 0;
    
    // 2. Retry loop (3 attempts)
    while (retry < 3) {
        memset(response_buffer, 0, 2048);
        err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            break;
        } else {
            ESP_LOGW(TAG, "HTTP GET failed: %s, retrying (%d/3)...", esp_err_to_name(err), retry + 1);
            vTaskDelay(pdMS_TO_TICKS(1000)); // Reduced retry delay
            retry++;
        }
    }

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Timezone API status = %d", esp_http_client_get_status_code(client));
        
        cJSON *json = cJSON_Parse(response_buffer);
        if (json) {
            // ip-api.com returns "offset" in seconds (e.g. -18000 for EST)
            cJSON *offset_item = cJSON_GetObjectItem(json, "offset");
            
            if (cJSON_IsNumber(offset_item)) {
                int offset_sec = offset_item->valueint;
                ESP_LOGI(TAG, "Detected Offset (seconds): %d", offset_sec);
                
                // Convert seconds to Hours:Minutes
                // POSIX logic: 'GMT+5' means 5 hours WEST of Greenwich (which is offset -18000)
                // So POSIX sign is REVERSE of standard offset sign.
                
                char posix_sign = (offset_sec < 0) ? '+' : '-';
                int abs_offset = (offset_sec < 0) ? -offset_sec : offset_sec;
                int h = abs_offset / 3600;
                int m = (abs_offset % 3600) / 60;
                
                char posix_tz[64];
                if (m == 0) {
                    snprintf(posix_tz, sizeof(posix_tz), "GMT%c%d", posix_sign, h);
                } else {
                    snprintf(posix_tz, sizeof(posix_tz), "GMT%c%d:%d", posix_sign, h, m);
                }
                
                ESP_LOGI(TAG, "Setting TZ environment to: %s", posix_tz);
                setenv("TZ", posix_tz, 1);
                tzset();
            }
            cJSON_Delete(json);
        } else {
            ESP_LOGE(TAG, "Failed to parse JSON response");
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(response_buffer);
    vTaskDelete(NULL);
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
        uint16_t number = 0;
        esp_wifi_scan_get_ap_num(&number);
        
        wifi_ap_record_t *ap_info = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * number);
        uint16_t ap_count = number;
        
        if (ap_info && esp_wifi_scan_get_ap_records(&ap_count, ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "Found %d APs", ap_count);
            
            // Limit to a reasonable number for the UI or just pass all
            // We should convert to our simplifed struct to avoid exposing esp_wifi types to UI
            wifi_scan_item_t *items = (wifi_scan_item_t *)malloc(sizeof(wifi_scan_item_t) * ap_count);
            if (items) {
                for (int i = 0; i < ap_count; i++) {
                    strncpy(items[i].ssid, (char *)ap_info[i].ssid, 32);
                    items[i].ssid[32] = '\0';
                    items[i].rssi = ap_info[i].rssi;
                    items[i].auth_mode = ap_info[i].authmode;
                }
                
                if (s_scan_cb) {
                    s_scan_cb(items, ap_count);
                }
                free(items);
            }
        } else {
             if (s_scan_cb) s_scan_cb(NULL, 0);
        }
        
        if (ap_info) free(ap_info);

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi Started");
        // Try to connect to stored network if one exists
        wifi_config_t conf;
        if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
             if (strlen((const char*)conf.sta.ssid) > 0) {
                 ESP_LOGI(TAG, "Auto-connecting to %s...", conf.sta.ssid);
                 esp_wifi_connect();
             }
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_is_connected = false;
        strcpy(s_ip_addr, "0.0.0.0");
        ESP_LOGI(TAG, "Disconnected from AP");
        
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying connection... %d", s_retry_num);
        } else {
            if (s_connect_cb) {
                s_connect_cb(false);
                // Clear callback so we don't call it again on future random disconnects
                s_connect_cb = NULL; 
            }
            s_retry_num = 0;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        sprintf(s_ip_addr, IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", s_ip_addr);
        s_retry_num = 0;
        s_is_connected = true;
        if (s_connect_cb) {
            s_connect_cb(true);
            s_connect_cb = NULL;
        }
        
        // Launch Auto-Timezone Task
        xTaskCreate(wifi_mgr_fetch_timezone_task, "tz_task", 4096, NULL, 5, NULL);
    }
}

esp_err_t wifi_mgr_init(void) {
    if (s_is_connected) return ESP_OK; // Already inited?

    // Initialize these only once, handle errors gracefully if already inited
    // esp_netif_init() and esp_event_loop_create_default() might be called by main or other components
    // so we use standard checks or ignore errors if ESP_ERR_INVALID_STATE
    
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    (void)sta_netif;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
     if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH)); // Ensure storage is set to FLASH

    // Check if configuration exists in NVS (loaded by esp_wifi_init)
    wifi_config_t current_conf;
    if (esp_wifi_get_config(WIFI_IF_STA, &current_conf) == ESP_OK && strlen((char*)current_conf.sta.ssid) > 0) {
        ESP_LOGI(TAG, "Found stored WiFi config: %s", current_conf.sta.ssid);
    } else {
        ESP_LOGI(TAG, "No stored WiFi config, setting defaults");
        wifi_config_t wifi_config = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg = {
                    .capable = true,
                    .required = false
                },
            },
        };
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    }

    ESP_ERROR_CHECK(esp_wifi_start());

    // Initialize SNTP
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    
    // Set default timezone to EST/EDT (New York)
    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    tzset();

    ESP_LOGI(TAG, "wifi_mgr init finished.");
    return ESP_OK;
}

esp_err_t wifi_mgr_start_scan(wifi_scan_cb_t cb) {
    s_scan_cb = cb;
    
    // Disconnect if needed to scan? Not strictly necessary in STA mode but helps reliability
    // esp_wifi_disconnect(); 
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };
    return esp_wifi_scan_start(&scan_config, false); // false = non-blocking
}

esp_err_t wifi_mgr_connect(const char *ssid, const char *password, wifi_connect_cb_t cb) {
    s_connect_cb = cb;
    s_retry_num = 0;

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    }
    
    // Loosen security threshold for scan/connect
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN; 
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_LOGI(TAG, "Connecting to %s...", ssid);
    // Explicitly set config which saves to NVS if storage is FLASH
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    return ESP_OK;
}

bool wifi_mgr_is_connected(void) {
    return s_is_connected;
}

const char* wifi_mgr_get_ip(void) {
    return s_ip_addr;
}

const char* wifi_mgr_get_ssid(void) {
    wifi_config_t conf;
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
        strncpy(s_ssid, (char*)conf.sta.ssid, 32);
        s_ssid[32] = '\0';
    }
    return s_ssid;
}
