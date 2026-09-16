/**
  ******************************************************************************
  * @file           : fft_analyzer.h
  * @brief          : 振动信号FFT频谱分析
  *                   算法: CMSIS-DSP arm_rfft_fast_f32 (实数FFT)
  *                   输入: 128组ADXL345采样 @1kHz
  *                   输出: 频谱幅值 + 主频峰值
  *                   频域分辨率: 1000Hz/128 = 7.81Hz/bin
  *                   可测频率范围: 0~500Hz (Nyquist)
  ******************************************************************************
  */

#ifndef __FFT_ANALYZER_H
#define __FFT_ANALYZER_H

#include "stm32f4xx_hal.h"
#include "arm_math.h"               /* CMSIS-DSP */
#include <stdint.h>

/* ==================== 配置 ==================== */
#define FFT_SIZE        128         /* FFT点数 (必须2的幂) */
#define FFT_FS          1000.0f     /* 采样频率 Hz */
#define FFT_RESOLUTION  (FFT_FS / FFT_SIZE)  /* 7.81Hz/bin */

/* ==================== 数据结构 ==================== */
typedef struct {
    float    freq;              /* 主频 (Hz)           */
    float    amplitude;         /* 主频幅值 (g)        */
    float    rms;               /* RMS振动总值 (g)     */
    float    spectrum[FFT_SIZE/2 + 1]; /* 幅值谱 (0~500Hz, 含Nyquist) */
    uint16_t peak_bin;          /* 峰值所在bin         */
    uint8_t  valid;             /* 计算完成标志        */
} FFT_Result;

/* ==================== API ==================== */

/**
  * @brief  初始化FFT (初始化CMSIS-DSP RFFT实例)
  */
void FFT_Analyzer_Init(void);

/**
  * @brief  对128组ADXL345采样做FFT分析
  * @param  data: 128组int16_t原始采样 (单轴)
  * @param  result: 输出FFT结果
  *         处理流程: int16→float→去直流→Hanning窗→RFFT→幅值→找峰值
  */
void FFT_Analyze(const int16_t *data, uint32_t count, FFT_Result *result);

/**
  * @brief  获取指定频率的幅值 (调试用)
  * @param  result: FFT结果
  * @param  freq: 目标频率(Hz)
  * @retval 该频率附近的幅值(g)
  */
float FFT_GetAmplitudeAt(const FFT_Result *result, float freq);

#endif /* __FFT_ANALYZER_H */
