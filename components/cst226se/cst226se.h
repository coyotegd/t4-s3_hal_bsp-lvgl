
#ifndef CST226SE_H
#define CST226SE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CST226SE_ROTATION_0 = 0,
    CST226SE_ROTATION_90 = 1,
    CST226SE_ROTATION_180 = 2,
    CST226SE_ROTATION_270 = 3,
} cst226se_rotation_t;

typedef struct {
    bool pressed;
    uint16_t x;
    uint16_t y;
    uint8_t id;
} cst226se_data_t;

typedef void (*cst226se_event_callback_t)(const cst226se_data_t *data, void *user_ctx);
void cst226se_register_callback(cst226se_event_callback_t cb, void *user_ctx);
void cst226se_set_rotation(cst226se_rotation_t rot);
cst226se_rotation_t cst226se_get_rotation(void);
void cst226se_set_max_coordinates(uint16_t x, uint16_t y);
bool cst226se_get_resolution(int16_t *x, int16_t *y);
bool cst226se_read(cst226se_data_t *data);
bool cst226se_wait_event(uint32_t timeout_ms);

// Power management
esp_err_t cst226se_init(void);
void cst226se_reset(void);
void cst226se_sleep(void);
void cst226se_wake(void);


#ifdef __cplusplus
}
#endif

#endif // CST226SE_H
