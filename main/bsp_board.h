// 公共板级配置头文件，集中声明背光 IO 宏，供全局引用
#ifndef BSP_BOARD_H_
#define BSP_BOARD_H_

#include "driver/gpio.h"

// LCD 背光 GPIO 及电平定义

#define EXAMPLE_PIN_NUM_BK_LIGHT   GPIO_NUM_40

#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL   1

#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL  (!EXAMPLE_LCD_BK_LIGHT_ON_LEVEL)

#endif /* BSP_BOARD_H_ */
