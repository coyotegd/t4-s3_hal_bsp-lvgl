#ifndef SY6970_H
#define SY6970_H

#include <esp_err.h>

#define SY6970_I2C_ADDR 0x6A
#define SY6970_SDA_PIN  6
#define SY6970_SCL_PIN  7

void sy6970_init(void);

#endif
