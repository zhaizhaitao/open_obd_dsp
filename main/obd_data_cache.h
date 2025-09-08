#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void obd_data_set_rpm(uint16_t rpm);
void obd_data_set_speed(uint8_t kmh);
uint16_t obd_data_get_rpm(void);
uint8_t  obd_data_get_speed(void);

#ifdef __cplusplus
}
#endif
