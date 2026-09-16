/**
  ******************************************************************************
  * @file           : filter.h
  * @brief          : 数字滤波管线
  *                   级联: 滑动平均(N=8) → 中值滤波(N=5)
  *                   用途: ADXL345振动信号预处理
  ******************************************************************************
  */

#ifndef __FILTER_H
#define __FILTER_H

#include <stdint.h>

/* ==================== 配置 ==================== */
#define FILTER_MA_WINDOW    8     /* 滑动平均窗口 */
#define FILTER_MED_WINDOW   5     /* 中值滤波窗口 */

/* ==================== 滤波器实例 ==================== */
typedef struct {
    int16_t buf[FILTER_MA_WINDOW];  /* 环形缓冲 */
    uint8_t idx;                     /* 当前写入位置 */
    uint8_t full;                    /* 缓冲区填满标志 */
    int32_t sum;                     /* 累加和 (快速滑动平均) */
} MovingAvgFilter;

typedef struct {
    int16_t buf[FILTER_MED_WINDOW];  /* 数据缓冲 */
    uint8_t count;                   /* 已收集数 */
} MedianFilter;

/* ==================== API ==================== */

/**
  * @brief  初始化滑动平均滤波器
  */
void Filter_MA_Init(MovingAvgFilter *f);

/**
  * @brief  喂入一个值, 返回滑动平均结果
  *         首N个值直接返回输入 (窗口未满)
  */
int16_t Filter_MA_Update(MovingAvgFilter *f, int16_t raw);

/**
  * @brief  初始化中值滤波器
  */
void Filter_Med_Init(MedianFilter *f);

/**
  * @brief  喂入一个值, 返回中值结果
  *         首N个值直接返回输入 (窗口未满)
  */
int16_t Filter_Med_Update(MedianFilter *f, int16_t raw);

/**
  * @brief  全管线: 输入原始值 → 滑动平均 → 中值 → 输出
  *         每个轴需要独立的滤波器实例
  */
typedef struct {
    MovingAvgFilter ma;
    MedianFilter    med;
} FilterPipeline;

void    FilterPipeline_Init(FilterPipeline *p);
void    FilterPipeline_PreFill(FilterPipeline *p, int16_t init_val);
int16_t FilterPipeline_Update(FilterPipeline *p, int16_t raw);

#endif /* __FILTER_H */
