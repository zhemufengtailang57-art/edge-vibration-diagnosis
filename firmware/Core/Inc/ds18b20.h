/**
  ******************************************************************************
  * @file           : ds18b20.h
  * @brief          : DS18B20 防水温度探头 OneWire驱动头文件
  *                   PB5, 需外接4.7kΩ上拉电阻 (VCC → DATA)
  *                   使用DWT周期计数器做微秒延时
  ******************************************************************************
  */

#ifndef __DS18B20_H
#define __DS18B20_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* OneWire引脚定义 */
#define DS18B20_PORT    GPIOB
#define DS18B20_PIN     GPIO_PIN_5

/* ==================== API ==================== */

/**
  * @brief  初始化DS18B20 (DWT + GPIO)
  *         检查总线上是否有设备
  * @retval 0=检测到设备, 1=未检测到(上拉电阻接了没?)
  */
uint8_t DS18B20_Init(void);

/**
  * @brief  启动一次温度转换（异步，不等待）
  *         12位精度需要750ms转换时间
  */
void    DS18B20_StartConversion(void);

/**
  * @brief  读取温度
  *         调用前需先 DS18B20_StartConversion() + 等750ms
  * @retval 温度值（℃），带一位小数 = 实际值×10
  *         例: 返回 256 → 25.6℃
  */
int16_t DS18B20_ReadTemp(void);

/**
  * @brief  检测OneWire总线上是否有设备
  * @retval 0=有设备, 1=无设备
  */
uint8_t DS18B20_CheckPresence(void);

#endif /* __DS18B20_H */
