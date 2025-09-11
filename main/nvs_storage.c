#include "nvs_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#define TAG                   "nvs_storage"
#define NS_CFG                "cfg"
#define KEY_CFG               "settings"
#define NS_STAT               "stat"
#define KEY_STAT              "runtime"
#define STAT_FLUSH_PERIOD_MS  5000

static nvs_user_cfg_t s_cfg =   { 
                                    .protocol = 0//车辆OBD的协议类型选择 0:自动,1~9:固定协议 默认为0:自动
                                };
static nvs_stat_t     s_stat = {0};
static bool           s_stat_dirty = false;
static SemaphoreHandle_t s_mux;

/* 前向声明 */
static esp_err_t load_blob(const char *ns,const char *key,void *out,size_t len);
static esp_err_t save_blob(const char *ns,const char *key,const void *data,size_t len);
static void stat_flush_task(void *arg);

esp_err_t nvs_storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    load_blob(NS_CFG, KEY_CFG, &s_cfg, sizeof(s_cfg));
    load_blob(NS_STAT, KEY_STAT, &s_stat, sizeof(s_stat));

    s_mux = xSemaphoreCreateMutex();
    xTaskCreate(stat_flush_task, "nvs_flush", 2048, NULL, 4, NULL);
    return ESP_OK;
}

/* 用户配置 */
const nvs_user_cfg_t * nvs_cfg_get(void){ return &s_cfg; }

esp_err_t nvs_cfg_set(const nvs_user_cfg_t *cfg)
{
    if(!cfg) return ESP_ERR_INVALID_ARG;
    if(memcmp(cfg,&s_cfg,sizeof(s_cfg))==0) return ESP_OK;
    s_cfg=*cfg;
    return save_blob(NS_CFG, KEY_CFG, &s_cfg, sizeof(s_cfg));
}

/* 统计 */
const nvs_stat_t * nvs_stat_get(void){return &s_stat;}
void nvs_stat_add_odometer(uint32_t d){
    xSemaphoreTake(s_mux,portMAX_DELAY);
    s_stat.odometer_m+=d;
    s_stat_dirty=true;
    xSemaphoreGive(s_mux);
}
void nvs_stat_add_runtime(uint32_t d){
    xSemaphoreTake(s_mux,portMAX_DELAY);
    s_stat.run_time_s+=d;
    s_stat_dirty=true;
    xSemaphoreGive(s_mux);
}

/* 后台任务 */
static void stat_flush_task(void *arg){
    while(1){
        vTaskDelay(pdMS_TO_TICKS(STAT_FLUSH_PERIOD_MS));
        if(!s_stat_dirty) continue;
        xSemaphoreTake(s_mux,portMAX_DELAY);
        if(save_blob(NS_STAT,KEY_STAT,&s_stat,sizeof(s_stat))==ESP_OK) s_stat_dirty=false;
        xSemaphoreGive(s_mux);
    }
}

/* 工具函数 */
static esp_err_t load_blob(const char *ns,const char *key,void *out,size_t len)
{
    nvs_handle_t h; size_t size=len; esp_err_t err;
    if(nvs_open(ns,NVS_READONLY,&h)==ESP_OK){
        err=nvs_get_blob(h,key,out,&size);
        nvs_close(h);
        if(err==ESP_OK && size==len) return ESP_OK;
    }
    memset(out,0,len);
    return save_blob(ns,key,out,len);
}

static esp_err_t save_blob(const char *ns,const char *key,const void *data,size_t len)
{
    nvs_handle_t h; esp_err_t err=nvs_open(ns,NVS_READWRITE,&h);
    if(err!=ESP_OK) return err;
    err=nvs_set_blob(h,key,data,len);
    if(err==ESP_OK) err=nvs_commit(h);
    nvs_close(h);
    return err;
}
