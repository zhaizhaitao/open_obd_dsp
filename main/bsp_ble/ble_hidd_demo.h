#ifndef __BLE_HIDD_DEMO_H__
#define __BLE_HIDD_DEMO_H__

#include "esp_hidd_prf_api.h"
#include "hid_dev.h"

#define HID_DEMO_TAG "HID_DEMO"

void app_hid_ctrl(void);
extern bool sec_conn ;
extern uint16_t hid_conn_id;

#endif