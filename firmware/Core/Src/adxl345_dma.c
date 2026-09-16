/**
  ******************************************************************************
  * @file           : adxl345_dma.c
  * @brief          : ADXL345 双缓冲1kHz采样
  *                   触发: TIM4 1kHz → HAL_TIM_PeriodElapsedCallback
  *                   读取: 复用 ADXL345_ReadAccel (SPI1, ~20µs/次)
  *                   缓冲: 双缓冲乒乓切换, 128组/块 → 128ms累积
  *
  *    ┌──────────┐     ┌─────────┐     ┌──────────┐
  *    │ TIM4 1kHz│────→│ read    │────→│ buf[0]   │─→ ready → 主循环处理
  *    │ 中断     │     │ ADXL345 │  │  │ buf[1]   │─→ ready → ...
  *    └──────────┘     └─────────┘  │  └──────────┘
  *                                  └── 写满一块换另一块 ──┘
  ******************************************************************************
  */

#include "adxl345_dma.h"
#include "adxl345.h"
#include "log.h"
#include <string.h>

/* CubeMX生成的TIM4句柄 (定义在main.c) */
extern TIM_HandleTypeDef htim4;

/* ==================== 全局变量 ==================== */

static ADXL_Buffer  buf[2];              /* 双缓冲 */
static uint8_t      buf_idx = 0;        /* 0 或 1: 当前正在填哪块 */
static uint32_t     total_samples = 0;  /* 总采样计数 */

/* ==================== 初始化 ==================== */

void ADXL_DMA_Init(void)
{
    memset(&buf[0], 0, sizeof(ADXL_Buffer));
    memset(&buf[1], 0, sizeof(ADXL_Buffer));
    buf_idx = 0;
    total_samples = 0;

    LOG_INFO("ADXL_DMA", "Dual buffer ready, %d samples/buf, 1kHz",
             ADXL_BUF_SAMPLES);

    /* 启动TIM4, 开始1kHz采样 */
    HAL_TIM_Base_Start_IT(&htim4);
    LOG_INFO("ADXL_DMA", "TIM4 started, sampling @1kHz");
}

/* ==================== TIM4中断回调 ==================== */

/**
  * @brief  TIM4每1ms触发一次, 在中断上下文中执行
  *         耗时约20µs (SPI读7字节 @5.25MHz)
  *         占总CPU: 20µs/1000µs = 2%
  */
/* TIM4中断实际处理 — 由 main.c 的 HAL_TIM_PeriodElapsedCallback 调用 */
void ADXL_DMA_TIM4_ISR(void)
{

    ADXL_Buffer *cur = &buf[buf_idx];

    /* 当前块还没满 → 填数据 */
    if (cur->count < ADXL_BUF_SAMPLES)
    {
        int16_t ax, ay, az;
        ADXL345_ReadAccel(&ax, &ay, &az);

        cur->data[cur->count].x = ax;
        cur->data[cur->count].y = ay;
        cur->data[cur->count].z = az;
        cur->count++;
        total_samples++;
    }

    /* 当前块满了 → 标记就绪, 切换到另一块 */
    if (cur->count >= ADXL_BUF_SAMPLES && !cur->ready)
    {
        cur->ready = 1;
        buf_idx = (buf_idx + 1) % 2;

        /* 如果另一块还没被主循环取走 (主循环太快了不打印)
           说明处理速度跟不上采样, 发WARN */
        if (buf[buf_idx].ready)
        {
            /* 两块都满了——丢数据警告 */
            static uint32_t overflow_cnt = 0;
            overflow_cnt++;
            if (overflow_cnt % 100 == 1)
            {
                /* 不在ISR里打LOG (LOG会发UART太慢),
                   用全局标志位, 主循环轮询时可打印 */
            }
            /* 强制覆盖旧数据 */
            memset(&buf[buf_idx], 0, sizeof(ADXL_Buffer));
        }
    }
}

/* ==================== 主循环调用 ==================== */

/**
  * @brief  获取已就绪的缓冲区
  * @retval 1=有就绪数据, 0=没有
  */
uint8_t ADXL_DMA_GetReadyBuf(ADXL_Buffer **out)
{
    for (uint8_t i = 0; i < 2; i++)
    {
        if (buf[i].ready)
        {
            *out = &buf[i];
            return 1;
        }
    }
    return 0;
}

uint32_t ADXL_DMA_GetSampleCount(void)
{
    return total_samples;
}
