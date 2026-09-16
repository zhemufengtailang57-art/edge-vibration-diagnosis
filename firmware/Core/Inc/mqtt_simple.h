/**
  ******************************************************************************
  * @file           : mqtt_simple.h
  * @brief          : 轻量MQTT客户端 (基于ESP8266 TCP AT指令)
  *                   协议: MQTT v3.1.1, QoS 0
  *                   不需要固件支持MQTT AT — 手动组包通过TCP发送
  ******************************************************************************
  */

#ifndef __MQTT_SIMPLE_H
#define __MQTT_SIMPLE_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* MQTT Broker 地址和端口 */
#define MQTT_BROKER_IP      "10.31.108.115"
#define MQTT_BROKER_PORT    1883
#define MQTT_CLIENT_ID      "f407_vib"
#define MQTT_TOPIC          "vibration/data"

/* API */

/**
  * @brief  连接WiFi + 连MQTT Broker
  *         1. 连接WiFi (SSID=手机热点)
  *         2. TCP连接到Broker
  *         3. 发送MQTT CONNECT握手
  * @retval 0=成功
  */
uint8_t MQTT_Connect(void);
uint8_t MQTT_Connected(void);

/**
  * @brief  发布JSON数据到主题
  * @param  json: JSON字符串 (如 {"freq":15,"rms":0.12})
  * @retval 0=成功
  */
uint8_t MQTT_Publish(const char *json);

/**
  * @brief  断开MQTT连接
  */
void    MQTT_Disconnect(void);

#endif /* __MQTT_SIMPLE_H */
