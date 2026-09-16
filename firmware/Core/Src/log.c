/**
  ******************************************************************************
  * @file           : log.c
  * @brief          : 分级日志实现
  *                   输出: USART2 printf (huart2)
  *                   格式: [HH:MM:SS.mmm] [LEVEL] TAG: message\r\n
  *                   线程安全: 关中断保护（FreeRTOS阶段改为互斥锁）
  ******************************************************************************
  */

#include "log.h"
#include <stdarg.h>
#include "FreeRTOS.h"
#include "task.h"

/* ==================== 全局变量 ==================== */

extern UART_HandleTypeDef huart2;

static LogLevel g_log_level = LOG_DEBUG;  /* 运行时等级，可动态修改 */

/* ==================== 等级字面量 ==================== */

static const char *level_str[] = {
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR"
};

/* ==================== 公开函数 ==================== */

void Log_Init(void)
{
    g_log_level = LOG_DEBUG;
    LOG_INFO("LOG", "Log system init OK, level=DEBUG");
}

uint32_t Log_GetUptimeSec(void)
{
    return HAL_GetTick() / 1000;
}

/**
  * @brief  核心日志输出
  *         格式: [hh:mm:ss.ms] [LEVEL] TAG: message\r\n
  *         例:  [00:05:12.340] [WARN ] WIFI: MQTT timeout
  */
void Log_Write(LogLevel level, const char *tag, const char *fmt, ...)
{
    /* 等级过滤：运行时低于阈值的直接丢弃 */
    if (level < g_log_level)
        return;

    uint8_t buf[256];
    uint16_t pos = 0;
    uint32_t tick;
    uint32_t h, m, s, ms;
    va_list args;

    /* 保护: 防多任务日志交错, 但不屏蔽中断
     * (曾用__disable_irq, 导致USART3收Modbus字节丢失→CRC错误→主站超时)
     * 注意: 调度器启动前不能调用vTaskSuspendAll, 此时用关中断(初始化阶段无Modbus流量) */
    BaseType_t sched_on = (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);
    if (sched_on)
        vTaskSuspendAll();
    else
        __disable_irq();

    tick = HAL_GetTick();

    /* 构造时间戳 */
    s  = tick / 1000;
    ms = tick % 1000;
    h  = s / 3600;
    m  = (s % 3600) / 60;
    s  = s % 60;

    pos += sprintf((char *)buf + pos, "[%02lu:%02lu:%02lu.%03lu] ",
                   h, m, s, ms);

    /* 等级标签 */
    pos += sprintf((char *)buf + pos, "[%s] ", level_str[level]);

    /* 模块标签 */
    pos += sprintf((char *)buf + pos, "%-8s: ", tag);

    /* 用户消息 (可变参数) */
    va_start(args, fmt);
    pos += vsnprintf((char *)buf + pos, sizeof(buf) - pos - 4, fmt, args);
    va_end(args);

    /* 换行 */
    pos += sprintf((char *)buf + pos, "\r\n");

    /* 输出到USART2 (寄存器直写, 无HAL阻塞) */
    for (uint16_t i = 0; i < pos; i++)
    {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = buf[i];
    }

    if (sched_on)
        xTaskResumeAll();
    else
        __enable_irq();
}
