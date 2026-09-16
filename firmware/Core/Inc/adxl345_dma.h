/**
  ******************************************************************************
  * @file           : adxl345_dma.h
  * @brief          : ADXL345 双缓冲高速采样驱动 (TIM4 1kHz触发)
  *                   不依赖DMA外设 — 使用TIM4中断 + SPI轮询读
  *                   双缓冲: 一块填数据, 同时主循环处理另一块
  *                   采样率: 1000Hz (TIM4 1kHz)
  *                   缓冲深度: 128组/块 ≈ 128ms/块
  ******************************************************************************
  */

#ifndef __ADXL345_DMA_H
#define __ADXL345_DMA_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ==================== 配置 ==================== */
#define ADXL_BUF_SAMPLES    128     /* 每块128组采样 (可改) */
#define ADXL_AXIS           3       /* X/Y/Z */
#define ADXL_SAMPLE_BYTES   (ADXL_AXIS * 2)  /* 每采样6字节 */

/* ==================== 数据结构 ==================== */
typedef struct {
    int16_t x, y, z;        /* 单组三轴数据 */
} ADXL_Sample;

typedef struct {
    ADXL_Sample data[ADXL_BUF_SAMPLES];  /* 采样数据 */
    uint16_t    count;                    /* 当前已采样数 */
    uint8_t     ready;                    /* 1=满, 待主循环处理 */
} ADXL_Buffer;

/* ==================== API ==================== */

/**
  * @brief  初始化双缓冲 + 启动TIM4
  */
void ADXL_DMA_Init(void);

/**
  * @brief  TIM4中断处理 (由main.c的HAL_TIM_PeriodElapsedCallback调用)
  */
void ADXL_DMA_TIM4_ISR(void);

/**
  * @brief  检查缓冲区是否就绪并返回
  * @param  buf: 输出，指向就绪缓冲区的指针
  * @retval 1=有就绪数据, 0=还没有
  */
uint8_t ADXL_DMA_GetReadyBuf(ADXL_Buffer **buf);

/**
  * @brief  获取当前采样计数（调试用）
  */
uint32_t ADXL_DMA_GetSampleCount(void);

#endif /* __ADXL345_DMA_H */
