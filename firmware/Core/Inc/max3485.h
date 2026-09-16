/**
  ******************************************************************************
  * @file           : max3485.h
  * @brief          : MAX3485 RS485收发器驱动
  *                   总线: USART3 (PD8=TX, PB11=RX), PC8=DE/RE
  *                   协议: Modbus-RTU预备
  *                   DE/RE=高 → 发送模式, DE/RE=低 → 接收模式
  ******************************************************************************
  */

#ifndef __MAX3485_H
#define __MAX3485_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <string.h>

extern UART_HandleTypeDef huart3;

/* DE/RE控制脚 */
#define RS485_DE_PORT   GPIOC
#define RS485_DE_PIN    GPIO_PIN_8

/* API */

/**
  * @brief  初始化RS485 (默认接收模式)
  *         发一条测试消息验证UART正常
  * @retval 0=成功
  */
uint8_t RS485_Init(void);

/**
  * @brief  切换到发送模式 (DE=HIGH)
  */
void    RS485_SetTx(void);

/**
  * @brief  切换到接收模式 (DE=LOW)
  */
void    RS485_SetRx(void);

/**
  * @brief  发送数据 (自动切发送模式)
  * @param  buf: 数据
  * @param  len: 字节数
  */
void    RS485_Send(uint8_t *buf, uint16_t len);

/**
  * @brief  接收数据 (需在接收模式下)
  * @param  buf: 接收缓冲区
  * @param  timeout_ms: 超时
  * @retval 实际收到字节数
  */
uint16_t RS485_Recv(uint8_t *buf, uint16_t max_len, uint32_t timeout_ms);

#endif /* __MAX3485_H */
