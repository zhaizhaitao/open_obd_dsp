#include "obd_data_cache.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include <math.h>

// 车辆常量定义 (根据您的东南菱悦V3 11款手动挡 195/55R15轮胎)
#define FINAL_DRIVE_RATIO      4.052f
#define TIRE_ROLLING_RADIUS_M  0.298f
#define CONSTANT_C             0.377f
#define CALCULATION_CONSTANT   5.128f // 1 / (FINAL_DRIVE_RATIO * CONSTANT_C * TIRE_ROLLING_RADIUS_M)
 
// 档位传动比范围结构体
typedef struct {
    float min_ratio;
    float max_ratio;
    Gear gear;
} GearRatioRange;

// 各档位理论总传动比范围（根据您的车辆参数预设）
const GearRatioRange gear_ranges[] = {
    {22.0f,  26.0f,  GEAR_1},     // 1档范围
    {12.0f,  14.0f,  GEAR_2},     // 2档范围
    {8.0f,   9.5f,   GEAR_3},     // 3档范围
    {5.8f,   6.8f,   GEAR_4},     // 4档范围
    {4.8f,   5.5f,   GEAR_5},     // 5档范围
    //{21.0f,  25.0f,  GEAR_REVERSE}, // 倒挡范围
};

#define GEAR_RANGE_COUNT (sizeof(gear_ranges) / sizeof(gear_ranges[0]))


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





/**
 * @brief 根据转速和车速计算并判断档位
 * @param rpm 发动机转速 (RPM)
 * @param speed 车速 (km/h)
 * @return 计算出的档位
 */
Gear calculate_gear(float rpm, float speed) {
    // 1. 检查输入数据有效性
    if (rpm <= 0 || speed <= 0) {
        return GEAR_NEUTRAL;
    }
    
    // 2. 计算总传动比
    float total_ratio = rpm / (speed * CALCULATION_CONSTANT);
    
    printf("calculate_gear: RPM=%.0f, Speed=%.1f km/h, total_ratio=%.2f\n", 
           rpm, speed, total_ratio);
    
    // 3. 与各档位范围进行比较
    for (int i = 0; i < GEAR_RANGE_COUNT; i++) {
        if (total_ratio >= gear_ranges[i].min_ratio && 
            total_ratio <= gear_ranges[i].max_ratio) {
            return gear_ranges[i].gear;
        }
    }
    
    // 4. 如果在所有范围外，检查是否可能为空档（转速高车速为零）
    if (rpm > 700 && speed < 5) { // 怠速以上且几乎静止
        return GEAR_NEUTRAL;
    }
    
    // 5. 无法识别的传动比
    return GEAR_NEUTRAL;
}

  