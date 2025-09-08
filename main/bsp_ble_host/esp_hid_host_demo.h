#ifndef __ESP_HID_HOST_DEMO_H__
#define __ESP_HID_HOST_DEMO_H__

// 从nRF Connect中确认的最终UUID
#define OBD_SERVICE_UUID        0xFFF0
#define OBD_CHAR_NOTIFY_UUID    0xFFF1 // 用于订阅通知（接收数据）
#define OBD_CHAR_WRITE_UUID     0xFFF1 // 用于写入命令（发送数据）
#define CCC_DESCRIPTOR_UUID     0x2902 // 用于开启通知的描述符

// 你的设备MAC地址（可选，可用于过滤扫描）
#define TARGET_OBD_MAC_ADDR     {0x00, 0x10, 0xCC, 0x4F, 0x36, 0x03}


void app_ble_host(void);
 
#endif