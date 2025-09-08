#include "obd_data_cache.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

// 使用简单全局变量 + 临界区保护
static volatile uint16_t s_rpm = 0;
static volatile uint8_t  s_speed = 0;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

void obd_data_set_rpm(uint16_t rpm)
{
    portENTER_CRITICAL(&s_mux);
    s_rpm = rpm;
    portEXIT_CRITICAL(&s_mux);
}

void obd_data_set_speed(uint8_t kmh)
{
    portENTER_CRITICAL(&s_mux);
    s_speed = kmh;
    portEXIT_CRITICAL(&s_mux);
}

uint16_t obd_data_get_rpm(void)
{
    uint16_t v;
    portENTER_CRITICAL(&s_mux);
    v = s_rpm;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

uint8_t obd_data_get_speed(void)
{
    uint8_t v;
    portENTER_CRITICAL(&s_mux);
    v = s_speed;
    portEXIT_CRITICAL(&s_mux);
    return v;
}
