/**
  ******************************************************************************
  * @file           : filter.c
  * @brief          : 数字滤波管线实现
  *                   滑动平均: O(1)优化 (维护累加和, 非O(N)遍历)
  *                   中值滤波: 插入排序 (N=5, 排序开销可忽略)
  ******************************************************************************
  */

#include "filter.h"
#include <string.h>

/* ==================== 滑动平均 (Moving Average) ==================== */

void Filter_MA_Init(MovingAvgFilter *f)
{
    memset(f->buf, 0, sizeof(f->buf));
    f->idx  = 0;
    f->full = 0;
    f->sum  = 0;
}

/**
  * @brief  滑动平均更新 — 累加和优化, O(1)复杂度
  *         窗口满(N=8)后: 新均值 = (旧和 - 旧值 + 新值) / N
  */
int16_t Filter_MA_Update(MovingAvgFilter *f, int16_t raw)
{
    /* 移除即将被覆盖的旧值 */
    f->sum -= f->buf[f->idx];

    /* 写入新值 */
    f->buf[f->idx] = raw;
    f->sum += raw;

    /* 移动指针 */
    f->idx++;
    if (f->idx >= FILTER_MA_WINDOW)
    {
        f->idx = 0;
        f->full = 1;
    }

    /* 返回均值 */
    if (f->full)
        return (int16_t)(f->sum / FILTER_MA_WINDOW);
    else
        return raw;  /* 窗口未满, 返回原值 */
}

/* ==================== 中值滤波 (Median Filter) ==================== */

void Filter_Med_Init(MedianFilter *f)
{
    memset(f->buf, 0, sizeof(f->buf));
    f->count = 0;
}

/**
  * @brief  中值滤波 — 冒泡排序取中间值
  *         N=5, 最坏排序25次比较, ~0.5µs @168MHz
  */
int16_t Filter_Med_Update(MedianFilter *f, int16_t raw)
{
    uint8_t i, j;
    int16_t tmp[FILTER_MED_WINDOW];

    /* 移入新数据 */
    for (i = 0; i < FILTER_MED_WINDOW - 1; i++)
        f->buf[i] = f->buf[i + 1];
    f->buf[FILTER_MED_WINDOW - 1] = raw;

    if (f->count < FILTER_MED_WINDOW)
        f->count++;

    /* 窗口未满, 返回原值 */
    if (f->count < FILTER_MED_WINDOW)
        return raw;

    /* 复制 + 冒泡排序 */
    memcpy(tmp, f->buf, sizeof(tmp));
    for (i = 0; i < FILTER_MED_WINDOW - 1; i++)
    {
        for (j = 0; j < FILTER_MED_WINDOW - 1 - i; j++)
        {
            if (tmp[j] > tmp[j + 1])
            {
                int16_t t = tmp[j];
                tmp[j] = tmp[j + 1];
                tmp[j + 1] = t;
            }
        }
    }

    /* 返回中值 (索引2 = 5个元素的中间) */
    return tmp[FILTER_MED_WINDOW / 2];
}

/* ==================== 全管线 ==================== */

void FilterPipeline_Init(FilterPipeline *p)
{
    Filter_MA_Init(&p->ma);
    Filter_Med_Init(&p->med);
}

/**
  * @brief  预热滤波器 — 喂入N个相同值填满窗口
  *         消除冷启动延迟（否则需要等MA+MED窗口填满才有效输出）
  */
void FilterPipeline_PreFill(FilterPipeline *p, int16_t init_val)
{
    uint8_t i;
    uint8_t total = FILTER_MA_WINDOW + FILTER_MED_WINDOW;  /* 8+5=13 */

    for (i = 0; i < total; i++)
    {
        FilterPipeline_Update(p, init_val);
    }
}

/**
  * @brief  管线处理: raw → 滑动平均 → 中值 → clean
  */
int16_t FilterPipeline_Update(FilterPipeline *p, int16_t raw)
{
    int16_t ma_val = Filter_MA_Update(&p->ma, raw);
    return Filter_Med_Update(&p->med, ma_val);
}
