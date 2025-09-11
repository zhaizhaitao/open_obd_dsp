#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/*------------------ 用户配置（仅修改时写入） ------------------*/
typedef struct {
    uint8_t protocol;      // 0: 自动, 1~9: 固定协议
    uint8_t rsv[7];        // 预留
} nvs_user_cfg_t;

/*------------------ 运行统计（定期落盘） ------------------*/
typedef struct {
    uint64_t odometer_m;   // 累计里程 (m)
    uint64_t run_time_s;   // 累计发动机运行时间 (s)
    uint8_t  rsv[4];
} nvs_stat_t;

esp_err_t nvs_storage_init(void);

/* 用户配置接口 */
const nvs_user_cfg_t * nvs_cfg_get(void);
esp_err_t nvs_cfg_set(const nvs_user_cfg_t *cfg);

/* 运行统计接口 */
const nvs_stat_t * nvs_stat_get(void);
void nvs_stat_add_odometer(uint32_t delta_m);
void nvs_stat_add_runtime(uint32_t delta_s);
