#include "ui_private.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

void update_stats_timer_cb(lv_timer_t * timer) {
    if (!lbl_sys_info) return;

    // Update System Info
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_free_heap = esp_get_minimum_free_heap_size();
    int64_t uptime = esp_timer_get_time() / 1000000;

    lv_label_set_text_fmt(lbl_sys_info, 
        "System Info:\n"
        "Free Heap: %" PRIu32 " bytes\n"
        "Min Free Heap: %" PRIu32 " bytes\n"
        "Uptime: %" PRId64 " s",
        free_heap, min_free_heap, uptime);
}
