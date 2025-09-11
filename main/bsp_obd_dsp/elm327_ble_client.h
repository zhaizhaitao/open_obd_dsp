#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 默认目标：名称 "OBDII"，服务 UUID 0xFFF0，写特征 0xFFF1，通知特征优先 0xFFF2（若不存在则回落到 0xFFF1）
     // 010C - 发动机转速：41 0C AA BB  -> RPM = ((AA*256)+BB)/4
        // 010D - 车速：41 0D AA          -> SPEED = AA km/h
        // 0105 - 发动机冷却液温度：41 05 AA -> TEMP = AA - 40 °C
        // 010F - 进气温度：41 0F AA      -> TEMP = AA - 40 °C
        // 010B - 进气歧管绝对压力：41 0B AA -> PRESSURE = AA kPa
        // 0111 - 节气门位置：41 11 AA    -> POSITION = (AA*100)/255 %
        // 012F - 燃油液位：41 2F AA      -> LEVEL = (AA*100)/255 %
        // 0142 - 控制模块电压：41 42 AA BB -> VOLTAGE = ((AA*256)+BB)/1000 V

typedef struct {
    void (*on_connected)(void);
    void (*on_disconnected)(void);
    void (*on_raw_notify)(const uint8_t *data, size_t len);
    void (*on_parsed_rpm)(uint16_t rpm);//发动机转速
    void (*on_parsed_speed_kmh)(uint8_t kmh);//车速
    void (*on_parsed_coolant_temp)(uint32_t coolant_temp);//发动机冷却液温度
    void (*on_parsed_intake_temp)(uint32_t intake_temp);//进气温度
    void (*on_parsed_manifold_pressure)(uint32_t manifold_pressure);//进气歧管绝对压力
    void (*on_parsed_throttle_position)(uint32_t throttle_position);//节气门位置
    void (*on_parsed_fuel_level)(uint32_t fuel_level);//燃油液位
    void (*on_parsed_control_module_voltage)(uint32_t control_module_voltage);//控制模块电压

} elm327_ble_callbacks_t;

// 初始化 BLE 客户端并开始扫描连接
// target_name 可为 NULL 使用默认 "OBDII"
void elm327_ble_init_and_start(const char *target_name, const elm327_ble_callbacks_t *cbs);

// 发送 OBD 命令（如 "01 0C\r" 转成字节再调用本函数）
bool elm327_ble_send_command(const uint8_t *data, size_t len);

// 小工具：将形如 "01 0C\r" 的 ASCII 命令转换为字节（空格可有可无）
// 返回写入的字节数；out_buf_len 为 out_buf 容量
size_t elm327_ble_ascii_cmd_to_bytes(const char *ascii, uint8_t *out_buf, size_t out_buf_len);

// 启动带默认日志回调与周期轮询（010C/010D）的便捷接口
void elm327_ble_start_default(const char *target_name);

#ifdef __cplusplus
}
#endif


