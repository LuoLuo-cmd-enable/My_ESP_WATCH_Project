#ifndef _ONENET_MQTT_H_
#define _ONENET_MQTT_H_
#include "esp_err.h"

//产品ID
#define  ONENET_PRODUCT_ID  "a0F2sycd0O"

//产品秘钥
#define  ONENET_ACCESS_KEY  "XXMb1xMpPR0DT8OrHQ8BNfNRK7XHvRnDDUvdN1knUfM=="

//设备名称
#define ONENET_DEVICE_NAME  "ESP32S3"

/**
 * 上报数据
 * @param data 数据
 * @return 错误
 */
esp_err_t onenet_post_property_data(const char* data);

/**
 * 启动onenet连接
 * @return 错误码
 */
esp_err_t onenet_start(void);

/**
 * 停止onenet连接
 * @param 无
 * @return 错误码
 */
esp_err_t onenet_stop(void);

#endif
