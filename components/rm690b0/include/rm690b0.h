#ifndef RM690B0_H
#define RM690B0_H

#include <stdint.h>
#include <stddef.h>

// Physical Hardware Limits
#define RM690B0_HW_WIDTH     480
#define RM690B0_HW_HEIGHT    600

// T4-S3 Specific Physical Display (Active Area)
#define T4S3_PHYSICAL_W      450
#define T4S3_PHYSICAL_H      600

// Logical Landscape Limits (90 deg CCW)
#define LOGICAL_WIDTH        600
#define LOGICAL_HEIGHT       450

// Register Definitions (8-bit)
#define RM690B0_SWRESET      0x01
#define RM690B0_SLPOUT       0x11
#define RM690B0_DISPON       0x29
#define RM690B0_CASET        0x2A
#define RM690B0_RASET        0x2B
#define RM690B0_RAMWR        0x2C
#define RM690B0_MADCTR       0x36
#define RM690B0_COLMOD       0x3A
#define RM690B0_WRDISBV      0x51

typedef enum {
    RM690B0_ROT_PORTRAIT = 0,
    RM690B0_ROT_90_CCW   = 1
} rm690b0_rotation_t;

void rm690b0_init(void);
void rm690b0_send_cmd(uint8_t cmd, const uint8_t *data, size_t len);
void rm690b0_send_pixels(const uint8_t *data, size_t len);
void rm690b0_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void rm690b0_set_rotation(rm690b0_rotation_t rot);

#endif