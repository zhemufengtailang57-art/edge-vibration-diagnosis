/**
  ******************************************************************************
  * @file           : esp8266.h
  * @brief          : ESP8266-01S WiFi模块 AT指令驱动
  *                   总线: USART1 (PA9=TX, PA10=RX), 115200
  *                   控制: PB0=CH_PD/EN (高电平使能)
  ******************************************************************************
  */

#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <string.h>

/* ==================== 外部引用 ==================== */
extern UART_HandleTypeDef huart1;

/* EN引脚 */
#define ESP8266_EN_PORT     GPIOB
#define ESP8266_EN_PIN      GPIO_PIN_0

/* 接收缓冲区 */
#define ESP_RX_BUF_SIZE     256

/* ==================== API ==================== */

/**
  * @brief  初始化ESP8266
  *         1. 拉高EN脚使能模块
  *         2. 等待模块启动
  *         3. 发送AT测试命令
  * @retval 0=AT通信成功, 1=无响应
  */
uint8_t ESP8266_Init(void);

/**
  * @brief  发送AT命令并等待响应
  * @param  cmd:    要发送的命令 (不含\r\n, 如 "AT")
  * @param  expect: 期望的响应关键字 (如 "OK")
  * @param  timeout_ms: 超时时间(ms)
  * @retval 0=收到期望响应, 1=超时
  */
uint8_t ESP8266_SendCmd(const char *cmd, const char *expect, uint32_t timeout_ms);

/**
  * @brief  获取接收到的响应数据
  */
void ESP8266_GetRxBuf(char *buf, uint16_t *len);

/**
  * @brief  清空接收缓冲区
  */
void ESP8266_ClearRxBuf(void);

#endif /* __ESP8266_H */
