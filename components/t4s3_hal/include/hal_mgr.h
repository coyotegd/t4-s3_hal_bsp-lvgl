// Button access through HAL
void hal_button_poll(void);

#ifndef HAL_MGR_H
#define HAL_MGR_H

#include <stdbool.h>
#include <esp_err.h>

// SY6970 STAT LED control
esp_err_t hal_set_stat_led(bool on);

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


typedef struct {
	uint16_t x;
	uint16_t y;
	bool pressed;
} hal_touch_data_t;

void hal_init(void);
void hal_set_rotation(uint8_t rot);
void hal_redraw_screen(void);
void hal_set_brightness(uint8_t brightness);
uint16_t hal_get_display_width(void);
uint16_t hal_get_display_height(void);
void hal_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

void hal_clear_full_display(uint16_t color);
void hal_cycle_rotation(void);
bool hal_handle_touch(void);

// HAL display control wrappers
void hal_display_power(bool on);
void hal_display_sleep(bool sleep);
void hal_display_set_brightness(uint8_t brightness);

// Button event stubs
void hal_button_press(void);
void hal_button_double_press(void);
void hal_button_long_press(void);

#endif // HAL_MGR_H
