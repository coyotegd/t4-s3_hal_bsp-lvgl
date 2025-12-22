#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>

// Initialize the button (GPIO0)
void button_init(void);


// Poll or handle button events (to be called from main loop or task)
void button_poll(void);

// Button event type
typedef enum {
	BUTTON_EVENT_NONE = 0,
	BUTTON_EVENT_PRESS,
	BUTTON_EVENT_DOUBLE_PRESS,
	BUTTON_EVENT_LONG_PRESS
} button_event_t;

// Get and clear the last button event
button_event_t button_get_event(void);

#endif // BUTTON_H
