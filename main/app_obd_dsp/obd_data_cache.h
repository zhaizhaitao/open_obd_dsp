#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 档位枚举
typedef enum {
    GEAR_NEUTRAL, // 空档或无法识别
    GEAR_1,
    GEAR_2,
    GEAR_3, 
    GEAR_4,
    GEAR_5,
} Gear;

void obd_data_set_rpm(uint16_t rpm);
void obd_data_set_speed(uint8_t kmh);
uint16_t obd_data_get_rpm(void);
uint8_t  obd_data_get_speed(void);
Gear calculate_gear(float rpm, float speed);
#ifdef __cplusplus
}
#endif
