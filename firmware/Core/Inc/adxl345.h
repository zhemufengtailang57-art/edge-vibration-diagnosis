/**
  ******************************************************************************
  * @file           : adxl345.h
  * @brief          : ADXL345 三轴加速度传感器 SPI驱动头文件
  *                   SPI1 Mode3, PA4=CS, 4线SPI
  ******************************************************************************
  */

#ifndef __ADXL345_H
#define __ADXL345_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* SPI句柄 ———— CubeMX生成的SPI1 */
extern SPI_HandleTypeDef hspi1;

/* CS引脚 ———— PA4, 软件控制 */
#define ADXL345_CS_PORT      GPIOA
#define ADXL345_CS_PIN       GPIO_PIN_4

/* ==================== API ==================== */

/**
  * @brief  初始化ADXL345
  *         1. 初始化CS引脚
  *         2. 检查DEVID (应为0xE5)
  *         3. 配置±16g, 13位分辨率, 200Hz采样率
  *         4. 启动测量
  * @retval 0=成功, 1=SPI通信失败, 2=DEVID校验失败
  */
uint8_t ADXL345_Init(void);

/**
  * @brief  读取三轴加速度原始值
  * @param  ax/ay/az: 输出指针，单位LSB
  *         ±16g全分辨率: 3.9mg/LSB ≈ 256 LSB/g
  *         g值换算: g = raw * 3.9 / 1000.0f
  */
void    ADXL345_ReadAccel(int16_t *ax, int16_t *ay, int16_t *az);

/**
  * @brief  读取DEVID寄存器（调试用，应为0xE5）
  */
uint8_t ADXL345_ReadDevid(void);

#endif /* __ADXL345_H */
