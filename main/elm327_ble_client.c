#include "elm327_ble_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_defs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "obd_data_cache.h"
#include <string.h>

// UUID 常量
#define UUID16_OBD_SERVICE 0xFFF0
#define UUID16_OBD_WRITE_CHAR 0xFFF1
#define UUID16_CCCD 0x2902

static const char *TAG = "elm327_ble";

static esp_gatt_if_t s_gattc_if = 0;
static uint16_t s_conn_id = 0xFFFF;
static esp_bd_addr_t s_peer_bda = {0};
static bool s_connected = false;
static bool s_have_service = false;
static uint16_t s_service_start = 0, s_service_end = 0;
static uint16_t s_char_write_handle = 0; // FFF1
static uint16_t s_char_notify_handle = 0; // 优先 FFF2，没有则回落 FFF1
static uint16_t s_cccd_handle = 0;
static elm327_ble_callbacks_t s_cbs = {0};
static char s_target_name[32] = "OBDII";

// 默认回调与轮询任务（可选）
static void default_on_connected(void) { ESP_LOGI(TAG, "OBD BLE connected"); }
static void default_on_disconnected(void) { ESP_LOGI(TAG, "OBD BLE disconnected"); }
static void default_on_raw_notify(const uint8_t *data, size_t len) {
    ESP_LOGI(TAG, "RAW (%d):", (int)len);
    for (size_t i = 0; i < len; ++i) printf("%02X ", data[i]);
    printf("str: %s \n", data);
}
static void default_on_parsed_rpm(uint16_t rpm) { ESP_LOGI(TAG, "RPM: %u", rpm); obd_data_set_rpm(rpm); }
static void default_on_parsed_speed(uint8_t kmh) { ESP_LOGI(TAG, "SPEED: %u km/h", kmh); obd_data_set_speed(kmh); }
static void default_on_parsed_coolant_temp(uint32_t coolant_temp) { ESP_LOGI(TAG, "COOLANT TEMP: %u °C", coolant_temp); }
static void default_on_parsed_intake_temp(uint32_t intake_temp) { ESP_LOGI(TAG, "INTAKE TEMP: %u °C", intake_temp); }
static void default_on_parsed_manifold_pressure(uint32_t manifold_pressure) { ESP_LOGI(TAG, "MANIFOLD PRESSURE: %u kPa", manifold_pressure); }
static void default_on_parsed_control_module_voltage(uint32_t control_module_voltage) { ESP_LOGI(TAG, "CONTROL MODULE VOLTAGE: %u V", control_module_voltage); }
static void default_on_parsed_fuel_level(uint32_t fuel_level) { ESP_LOGI(TAG, "FUEL LEVEL: %u %", fuel_level); }
static void default_on_parsed_throttle_position(uint32_t throttle_position) { ESP_LOGI(TAG, "THROTTLE POSITION: %u %", throttle_position); }
 
static void obd_poll_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(3000)); // 等待连接建立
    uint8_t buf[16];
    uint32_t tick_count = 0;
    // 初始化阶段：发送 ELM327 AT 指令
    const char *init_cmds[] = {
        "ATZ\r",      // 复位
        "ATE0\r",     // Echo off
        "ATL0\r",     // 行宽 off
        "ATS1\r",     // 空格 on/off
        "ATH0\r",     // 关闭头部数据（可选）ATH1是打開
        "ATAT1\r",    // 适应时序
        "ATST 32\r",  // 设置超时（4*50=200ms，可按车况调 默認200ms） 这个后面改小/大试试；
        "ATSP0\r",  //ATSP = Set Protocol（设置 OBD 协议） 0是自动 后面可以换一下试试6
    };

    for (size_t i = 0; i < (sizeof(init_cmds) / sizeof(init_cmds[0])); ++i) {
        size_t n = elm327_ble_ascii_cmd_to_bytes(init_cmds[i], buf, sizeof(buf));
        if (n) { elm327_ble_send_command(buf, n); }
        ESP_LOGI(TAG, " AT init Cmd send %s",init_cmds[i]);
        vTaskDelay(pdMS_TO_TICKS(i == 0 ? 2000 : 100)); // ATZ 后多等一会
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    // 协议选择后做一次能力探测，加速稳定
    size_t n2 = elm327_ble_ascii_cmd_to_bytes("01 00\r", buf, sizeof(buf));
    if (n2) { elm327_ble_send_command(buf, n2); }
    ESP_LOGI(TAG, " CMD 01 00 send \n");
    vTaskDelay(pdMS_TO_TICKS(100));

    while (1) {
        tick_count++;
        
        // 转速/车速 - 1000ms 查询一次
         if (tick_count % (1000 / 200) == 0)/*1s*/
         { // 每200ms执行
            size_t n = elm327_ble_ascii_cmd_to_bytes("01 0C\r", buf, sizeof(buf));
            if (n) { elm327_ble_send_command(buf, n); }
            vTaskDelay(pdMS_TO_TICKS(100));
            // n = elm327_ble_ascii_cmd_to_bytes("01 0D\r", buf, sizeof(buf));
            // if (n) { elm327_ble_send_command(buf, n); }
            // vTaskDelay(pdMS_TO_TICKS(1000));
        }

        if (tick_count % (5000 / 200) == 0)/*5s*/
         { // 每200ms执行
            size_t n = elm327_ble_ascii_cmd_to_bytes("01 00\r", buf, sizeof(buf));
            if (n) { elm327_ble_send_command(buf, n); }
         }


    #if 0    
        // 绝对压力和节气门位置 - 1s 查询一次
        if (tick_count % 5 == 0) { // 每1s执行
            size_t n = elm327_ble_ascii_cmd_to_bytes("01 10\r", buf, sizeof(buf));
            if (n) { elm327_ble_send_command(buf, n); }
            vTaskDelay(pdMS_TO_TICKS(50));
            n = elm327_ble_ascii_cmd_to_bytes("01 11\r", buf, sizeof(buf));
            if (n) { elm327_ble_send_command(buf, n); }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        // 冷却液温度/进气温度/控制模块电压 - 5s 查询一次
        if (tick_count % 25 == 0) { // 每5s执行
            size_t n = elm327_ble_ascii_cmd_to_bytes("01 05\r", buf, sizeof(buf));
            if (n) { elm327_ble_send_command(buf, n); }
            vTaskDelay(pdMS_TO_TICKS(50));
            n = elm327_ble_ascii_cmd_to_bytes("01 0F\r", buf, sizeof(buf));
            if (n) { elm327_ble_send_command(buf, n); }
            vTaskDelay(pdMS_TO_TICKS(50));
            n = elm327_ble_ascii_cmd_to_bytes("01 42\r", buf, sizeof(buf));
            if (n) { elm327_ble_send_command(buf, n); }
            vTaskDelay(pdMS_TO_TICKS(50));
        }  
        // 燃油液位 - 10s 查询一次
        if (tick_count % 50 == 0) { // 每10s执行
            size_t n = elm327_ble_ascii_cmd_to_bytes("01 2F\r", buf, sizeof(buf));
            if (n) { elm327_ble_send_command(buf, n); }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    #endif
        vTaskDelay(pdMS_TO_TICKS(200)); // 基础周期200ms
    }
}

static void start_scan(void) {
    esp_ble_gap_start_scanning(10); // 10s
}

static bool match_device_name(const uint8_t *adv_data, uint8_t adv_data_len, const char *name) {
    if (name == NULL || name[0] == '\0') return true;
    uint8_t len = 0;
    uint8_t *p = (uint8_t *)esp_ble_resolve_adv_data((uint8_t *)adv_data, ESP_BLE_AD_TYPE_NAME_CMPL, &len);
    if (p && len) {
        return (len == strlen(name) && memcmp(p, name, len) == 0);
    }
    p = (uint8_t *)esp_ble_resolve_adv_data((uint8_t *)adv_data, ESP_BLE_AD_TYPE_NAME_SHORT, &len);
    if (p && len) {
        return (len == strlen(name) && memcmp(p, name, len) == 0);
    }
    return false;
}

static void request_discovery(void) {
    esp_bt_uuid_t svc_uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid = {.uuid16 = UUID16_OBD_SERVICE}
    };
    esp_ble_gattc_search_service(s_gattc_if, s_conn_id, &svc_uuid);
}

static void enable_notify_if_ready(void) {
    if (s_cccd_handle) {
        uint8_t notify_en[2] = {0x01, 0x00};
        esp_ble_gattc_write_char_descr(s_gattc_if, s_conn_id, s_cccd_handle,
                                       sizeof(notify_en), notify_en,
                                       ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    }
}

static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

void elm327_ble_init_and_start(const char *target_name, const elm327_ble_callbacks_t *cbs) {
    if (cbs) s_cbs = *cbs;
    if (target_name && target_name[0]) {
        strncpy(s_target_name, target_name, sizeof(s_target_name)-1);
        s_target_name[sizeof(s_target_name)-1] = '\0';
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
        ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    }
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    }
    if (!esp_bluedroid_get_status()) {
        ESP_ERROR_CHECK(esp_bluedroid_init());
        ESP_ERROR_CHECK(esp_bluedroid_enable());
    } else if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        ESP_ERROR_CHECK(esp_bluedroid_enable());
    }

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(gattc_event_handler));
    ESP_ERROR_CHECK(esp_ble_gattc_app_register(0));
}

bool elm327_ble_send_command(const uint8_t *data, size_t len) {
    if (!s_connected || s_char_write_handle == 0) return false;
    if (len == 0 || data == NULL) return false;
    esp_err_t err = esp_ble_gattc_write_char(s_gattc_if, s_conn_id, s_char_write_handle,
                                             len, (uint8_t *)data,
                                             ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    return err == ESP_OK;
}

// 将 ASCII 指令(如 "01 0C\r")复制到输出缓冲区，同时去除空白字符，保持 ELM327 所需的 ASCII 格式
size_t elm327_ble_ascii_cmd_to_bytes(const char *ascii, uint8_t *out_buf, size_t out_buf_len) {
    size_t out = 0;
    const char *p = ascii;
    while (*p && out < out_buf_len) {
        if (*p == ' ' || *p == '\t') {
            p++;                // 跳过空白符
            continue;
        }
        out_buf[out++] = (uint8_t)(*p++); // 直接复制 ASCII 字节
    }
    return out;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        start_scan();
        break;
    }
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *pr = param;
        if (pr->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            if (match_device_name(pr->scan_rst.ble_adv, pr->scan_rst.adv_data_len, s_target_name)) {
                ESP_LOGI(TAG, "Found target %s, connecting...", s_target_name);
                esp_ble_gap_stop_scanning();
                esp_ble_gattc_open(s_gattc_if, pr->scan_rst.bda, pr->scan_rst.ble_addr_type, true);
            }
        }
        break;
    }
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
    default:
        break;
    }
}

static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    switch (event) {
    case ESP_GATTC_REG_EVT: {
        s_gattc_if = gattc_if;
        esp_ble_scan_params_t scan_params = {
            .scan_type              = BLE_SCAN_TYPE_ACTIVE,
            .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
            .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
            .scan_interval          = 0x60,
            .scan_window            = 0x30,
            .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
        };
        esp_ble_gap_set_scan_params(&scan_params);
        break;
    }
    case ESP_GATTC_CONNECT_EVT: {
        s_connected = true;
        s_conn_id = param->connect.conn_id;
        memcpy(s_peer_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        if (s_cbs.on_connected) s_cbs.on_connected();
        request_discovery();
        break;
    }
    case ESP_GATTC_OPEN_EVT: {
        if (param->open.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "Open failed status=%d", param->open.status);
            start_scan();
        }
        break;
    }
    case ESP_GATTC_SEARCH_RES_EVT: {
        const esp_gatt_id_t *srvc_id = &param->search_res.srvc_id;
        if (srvc_id->uuid.len == ESP_UUID_LEN_16 && srvc_id->uuid.uuid.uuid16 == UUID16_OBD_SERVICE) {
            s_have_service = true;
            ESP_LOGI(TAG, "Service FFF0 found");
            s_service_start = param->search_res.start_handle;
            s_service_end = param->search_res.end_handle;
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
        if (!s_have_service) {
            ESP_LOGW(TAG, "Service 0xFFF0 not found");
            break;
        }
        ESP_LOGI(TAG, "Service discovery complete. Start char discovery.");

        // 通过 UUID 查询特征，兼容 IDF v5 API
        esp_bt_uuid_t uuid_write = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = UUID16_OBD_WRITE_CHAR } };
        esp_gattc_char_elem_t char_elems[2];
        uint16_t count = 2; // 【必须初始化】! 告诉函数数组的最大容量

        // 查写特征 (0xFFF1)
        esp_err_t ret = esp_ble_gattc_get_char_by_uuid(gattc_if, s_conn_id, s_service_start, s_service_end, uuid_write, char_elems, &count);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Get char by UUID failed: %s (0x%x)", esp_err_to_name(ret), ret);
            break;
        }
        if (count == 0) {
            ESP_LOGE(TAG, "Characteristic 0xFFF1 not found!");
            break;
        }
        s_char_write_handle = char_elems[0].char_handle;
        ESP_LOGI(TAG, "Found char FFF1, handle: 0x%04X", s_char_write_handle);

        s_char_notify_handle = s_char_write_handle; // 直接使用 FFF1 进行通知

        // 必须向协议栈注册通知回调，否则 ESP_GATTC_NOTIFY_EVT 不会上报
        int  sret = esp_ble_gattc_register_for_notify(gattc_if, s_peer_bda, s_char_notify_handle);
        ESP_LOGI(TAG, "register_for_notify ret=%d", sret);

        // 查找 CCCD 描述符
        if (s_char_notify_handle) {
            esp_gattc_descr_elem_t descr_elems[2];
            count = 2; // 【必须初始化】!
            esp_bt_uuid_t cccd_uuid = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = UUID16_CCCD } };
            ret = esp_ble_gattc_get_descr_by_char_handle(gattc_if, s_conn_id, s_char_notify_handle, cccd_uuid, descr_elems, &count);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Get descr by char handle failed: %s (0x%x)", esp_err_to_name(ret), ret);
                break;
            }
            if (count == 0) {
                ESP_LOGW(TAG, "CCCD descriptor not found on handle 0x%04X. Notifications may not work.", s_char_notify_handle);
            } else {
                s_cccd_handle = descr_elems[0].handle;
                ESP_LOGI(TAG, "Found CCCD descr, handle: 0x%04X", s_cccd_handle);
            }
        }
        enable_notify_if_ready();
        break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT: {
        if (param->write.status == ESP_GATT_OK) {
            ESP_LOGI(TAG, "Notifications enabled");
        } else {
            ESP_LOGW(TAG, "Enable notify failed status=%d", param->write.status);
        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
        if (s_cbs.on_raw_notify) s_cbs.on_raw_notify(param->notify.value, param->notify.value_len);
        const uint8_t *v = param->notify.value;
        int n = param->notify.value_len;
        
        if (n >= 3 && v[0] == 0x41) 
        {
            ESP_LOGW(TAG, "OBD responded in binary format!!!");
        }
        else if (n >= 6 && v[0] == '4' && v[1] == '1')
        { // ASCII格式: "41 0C 1B F8"   新增ASCII格式解析
            // 定义足够大的数据数组
            #define MAX_DATA_BYTES 6  // 大多数OBD响应不超过6个字节
            uint8_t data[MAX_DATA_BYTES] = {0};
            char response[128] = {0};
            memcpy(response, v, n < 127 ? n : 127);// 解析响应  
            // 解析ASCII格式的OBD响应
            uint8_t mode, pid;
            int values = sscanf(response, "%x %x %x %x %x %x %x %x", 
                &mode, &pid, 
                &data[0], &data[1], &data[2], &data[3], 
                &data[4], &data[5]);
                ESP_LOGI(TAG, "response: %s,mode: %x,pid: %x,data: %x %x %x %x %x %x", response, mode, pid , data[0], data[1], data[2], data[3], data[4], data[5] );

            if (values >= 3 && mode == 0x41) 
            {
                int data_count = values - 2; // 减去mode和pid
                ESP_LOGI(TAG, "PID 0x%02X with %d data bytes", pid, data_count);
                switch (pid) {
                    case 0x05: // 发动机冷却液温度
                        if (data_count >= 2 && s_cbs.on_parsed_coolant_temp) {
                            uint32_t temp = ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
                            s_cbs.on_parsed_coolant_temp(temp - 40); // 转换为摄氏度
                            ESP_LOGI(TAG, "ASCII COOLANT TEMP: %u °C", temp - 40);
                        }
                        break;
                    case 0x0C: // 转速
                        if (data_count >= 2 && s_cbs.on_parsed_rpm) {
                            uint16_t rpm = ((data[0] << 8) | data[1]) / 4;
                            s_cbs.on_parsed_rpm(rpm);
                            ESP_LOGI(TAG, "ASCII RPM: %u", rpm);
                        }
                        break;
                    case 0x0D: // 车速
                        if (data_count >= 1 && s_cbs.on_parsed_speed_kmh) {
                            s_cbs.on_parsed_speed_kmh(data[0]);
                            ESP_LOGI(TAG, "ASCII Speed: %u km/h", data[0]);
                        }
                        break;
                    case 0x0F: // 进气温度
                        if (data_count >= 2 && s_cbs.on_parsed_intake_temp) {
                            uint32_t temp = ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
                            s_cbs.on_parsed_intake_temp(temp - 40); // 转换为摄氏度
                            ESP_LOGI(TAG, "ASCII INTAKE TEMP: %u °C", temp - 40);
                        }
                        break;
                    case 0x0B: // 进气歧管绝对压力
                        if (data_count >= 2 && s_cbs.on_parsed_manifold_pressure) {
                            uint32_t pressure = ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
                            s_cbs.on_parsed_manifold_pressure(pressure);
                            ESP_LOGI(TAG, "ASCII MANIFOLD PRESSURE: %u kPa", pressure);
                        }
                        break;
                    case 0x11: // 节气门位置
                        if (data_count >= 2 && s_cbs.on_parsed_throttle_position) {
                            uint32_t position = ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
                            s_cbs.on_parsed_throttle_position(position);
                            ESP_LOGI(TAG, "ASCII THROTTLE POSITION: %u %", position);
                        }
                        break;
                    case 0x2F: // 燃油液位
                        if (data_count >= 2 && s_cbs.on_parsed_fuel_level) {
                            uint32_t level = ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
                            s_cbs.on_parsed_fuel_level(level);
                            ESP_LOGI(TAG, "ASCII FUEL LEVEL: %u %", level);
                        }
                        break;
                    case 0x42: // 控制模块电压
                        if (data_count >= 2 && s_cbs.on_parsed_control_module_voltage) {
                            uint32_t voltage = ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
                            s_cbs.on_parsed_control_module_voltage(voltage);
                            ESP_LOGI(TAG, "ASCII CONTROL MODULE VOLTAGE: %u V", voltage);
                        }
                        break;
                }
            }
        }
        // 3. 处理其他响应（如"NO DATA"）
        else 
        {
            // 检查是否是"NO DATA"等文本响应
            char response[64] = {0};
            memcpy(response, v, n < 63 ? n : 63);
            if (strstr(response, "NO DATA") != NULL) 
            {
                ESP_LOGW(TAG, "OBD responded: NO DATA");
            } 
            else if (strstr(response, "SEARCHING") != NULL) 
            {
                ESP_LOGI(TAG, "OBD is searching for protocol...");
            }
            else 
            {   
                ESP_LOGI(TAG," Other OBD responded: %s",response);
            }
        }
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
        if (param->write.status != ESP_GATT_OK) {
            ESP_LOGW(TAG, "Write failed status=%d", param->write.status);
        }
        break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
        s_connected = false;
        s_conn_id = 0xFFFF;
        s_have_service = false;
        s_service_start = s_service_end = 0;
        s_char_write_handle = s_char_notify_handle = s_cccd_handle = 0;
        if (s_cbs.on_disconnected) s_cbs.on_disconnected();
        start_scan();
        break;
    }
    default:
        break;
    }
}


void elm327_ble_start_default(const char *target_name) {

    // 初始化 NVS 
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    const elm327_ble_callbacks_t cbs = {
        .on_connected = default_on_connected,
        .on_disconnected = default_on_disconnected,
        .on_raw_notify = default_on_raw_notify,
        .on_parsed_rpm = default_on_parsed_rpm,
        .on_parsed_speed_kmh = default_on_parsed_speed,
        .on_parsed_coolant_temp = default_on_parsed_coolant_temp,
        .on_parsed_intake_temp = default_on_parsed_intake_temp,
        .on_parsed_manifold_pressure = default_on_parsed_manifold_pressure,
        .on_parsed_control_module_voltage = default_on_parsed_control_module_voltage,
        .on_parsed_fuel_level = default_on_parsed_fuel_level,
        .on_parsed_throttle_position = default_on_parsed_throttle_position,
    };
    elm327_ble_init_and_start(target_name, &cbs);
    xTaskCreate(obd_poll_task, "obd_poll", 3072, NULL, 4, NULL);
}

