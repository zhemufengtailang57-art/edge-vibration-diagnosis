/**
  ******************************************************************************
  * @file           : mpu6050.h
  * @brief          : MPU6050 六轴传感器 I2C驱动头文件
  *                   三轴加速度 + 三轴角速度 + 温度
  ******************************************************************************
  */

#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* MPU6050 I2C地址（7位 = 0x68, 左移1位 = 0xD0） */
#define MPU6050_ADDR        0xD0

/* 量程枚举 */
typedef enum {
    MPU6050_ACCEL_2G  = 0x00,  /* ±2g  (默认) */
    MPU6050_ACCEL_4G  = 0x08,  /* ±4g       */
    MPU6050_ACCEL_8G  = 0x10,  /* ±8g       */
    MPU6050_ACCEL_16G = 0x18   /* ±16g      */
} MPU6050_AccelRange;

typedef enum {
    MPU6050_GYRO_250  = 0x00,  /* ±250°/s (默认) */
    MPU6050_GYRO_500  = 0x08,  /* ±500°/s       */
    MPU6050_GYRO_1000 = 0x10,  /* ±1000°/s      */
    MPU6050_GYRO_2000 = 0x18   /* ±2000°/s      */
} MPU6050_GyroRange;

/* ==================== API ==================== */

/**
  * @brief  初始化MPU6050（唤醒 + 检查WHO_AM_I）
  * @retval 0=成功, 1=I2C通信失败, 2=WHO_AM_I校验失败
  */
uint8_t  MPU6050_Init(void);

/**
  * @brief  读取三轴加速度原始值
  * @param  ax/ay/az: 输出指针，单位LSB（取决于量程换算）
  */
void     MPU6050_ReadAccel(int16_t *ax, int16_t *ay, int16_t *az);

/**
  * @brief  读取三轴角速度原始值
  * @param  gx/gy/gz: 输出指针，单位LSB（取决于量程换算）
  */
void     MPU6050_ReadGyro(int16_t *gx, int16_t *gy, int16_t *gz);

/**
  * @brief  读取温度
  * @retval 温度值（℃）
  */
float    MPU6050_ReadTemp(void);

/**
  * @brief  读取WHO_AM_I寄存器
  * @retval 应返回 0x68
  */
uint8_t  MPU6050_ReadWhoAmI(void);

/**
  * @brief  设置加速度/角速度量程
  */
void     MPU6050_SetAccelRange(MPU6050_AccelRange range);
void     MPU6050_SetGyroRange(MPU6050_GyroRange range);

#endif /* __MPU6050_H */
