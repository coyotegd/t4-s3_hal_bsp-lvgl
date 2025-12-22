
#include "include/button.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUTTON_GPIO 0

#define DOUBLE_PRESS_WINDOW_MS 750
#define LONG_PRESS_THRESHOLD_MS 2000

typedef enum {
	BUTTON_STATE_IDLE = 0,
	BUTTON_STATE_PRESSED,
	BUTTON_STATE_WAIT_DOUBLE,
	BUTTON_STATE_LONG_PRESSED
} button_state_t;

static button_state_t btn_state = BUTTON_STATE_IDLE;
static TickType_t press_tick = 0;
static TickType_t release_tick = 0;
static bool long_press_sent = false;

void button_init(void) {
	// Configure GPIO0 as input with pull-up
	gpio_config_t io_conf = {
		.pin_bit_mask = (1ULL << BUTTON_GPIO),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	gpio_config(&io_conf);
}



static button_event_t last_event = BUTTON_EVENT_NONE;

void button_poll(void) {
	bool pressed = gpio_get_level(BUTTON_GPIO) == 0;
	TickType_t now = xTaskGetTickCount();

	switch (btn_state) {
		case BUTTON_STATE_IDLE:
			if (pressed) {
				btn_state = BUTTON_STATE_PRESSED;
				press_tick = now;
				long_press_sent = false;
			}
			break;
		case BUTTON_STATE_PRESSED:
			if (!pressed) {
				release_tick = now;
				btn_state = BUTTON_STATE_WAIT_DOUBLE;
			} else if (!long_press_sent && (now - press_tick) * portTICK_PERIOD_MS > LONG_PRESS_THRESHOLD_MS) {
				last_event = BUTTON_EVENT_LONG_PRESS;
				long_press_sent = true;
				btn_state = BUTTON_STATE_LONG_PRESSED;
			}
			break;
		case BUTTON_STATE_WAIT_DOUBLE:
			if (pressed) {
				// Second press detected within window
				btn_state = BUTTON_STATE_IDLE;
				last_event = BUTTON_EVENT_DOUBLE_PRESS;
			} else if ((now - release_tick) * portTICK_PERIOD_MS > DOUBLE_PRESS_WINDOW_MS) {
				// No second press, treat as single press
				btn_state = BUTTON_STATE_IDLE;
				last_event = BUTTON_EVENT_PRESS;
			}
			break;
		case BUTTON_STATE_LONG_PRESSED:
			if (!pressed) {
				btn_state = BUTTON_STATE_IDLE;
			}
			break;
		default:
			btn_state = BUTTON_STATE_IDLE;
			break;
	}
}

button_event_t button_get_event(void) {
	button_event_t event = last_event;
	last_event = BUTTON_EVENT_NONE;
	return event;
}

// Deprecated: Use hal event callbacks instead. These avoid warnings.
bool button_is_pressed(void) { return false; }
bool button_is_double_pressed(void) { return false; }
bool button_is_long_pressed(void) { return false; }
