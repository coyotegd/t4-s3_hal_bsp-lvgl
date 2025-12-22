#pragma once

#include <stdbool.h>
#include <stdint.h>
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

void cst226se_init(void);
void cst226se_set_i2c_bus(i2c_master_bus_handle_t bus);
void cst226se_set_rotation(cst226se_rotation_t rotation);
void cst226se_set_swap_xy(bool swap);
void cst226se_set_mirror_xy(bool mirror_x, bool mirror_y);
void cst226se_set_max_coordinates(uint16_t x, uint16_t y);
bool cst226se_get_resolution(int16_t *x, int16_t *y);
bool cst226se_read(cst226se_data_t *data);
void cst226se_reset(void);

#ifdef __cplusplus
}
#endif

