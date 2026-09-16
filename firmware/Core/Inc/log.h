/**
  ******************************************************************************
  * @file           : log.h
  * @brief          : 分级日志系统 (工业级)
  *                   等级: ERROR > WARN > INFO > DEBUG
  *                   编译时可通过 LOG_LEVEL 宏裁剪输出
  *                   USART2 printf输出, 自带运行时间戳
  ******************************************************************************
  */

#ifndef __LOG_H
#define __LOG_H

#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdint.h>

/* ==================== 编译时等级裁剪 ==================== */
/* 设置 LOG_LEVEL 可屏蔽低优先级日志以节省性能：
 *   LOG_LEVEL = 0 → 全部输出 (开发阶段)
 *   LOG_LEVEL = 1 → 屏蔽 DEBUG
 *   LOG_LEVEL = 2 → 屏蔽 DEBUG+INFO
 *   LOG_LEVEL = 3 → 仅 ERROR */
#define LOG_LEVEL_DEFAULT   0   /* 开发阶段全开 */

/* ==================== 等级枚举 ==================== */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
    LOG_NONE  = 4   /* 全部关闭 */
} LogLevel;

/* ==================== API ==================== */

/**
  * @brief  初始化日志系统 (记录启动时间戳)
  */
void Log_Init(void);

/**
  * @brief  核心日志函数
  * @param  level: 等级
  * @param  tag:   模块标签 (如 "MPU6050", "MQTT")
  * @param  fmt:   printf格式化字符串
  * @param  ...:   可变参数
  */
void Log_Write(LogLevel level, const char *tag, const char *fmt, ...);

/**
  * @brief  便捷宏 ———— 自动展开为 Log_Write()
  *         用法: LOG_ERROR("I2C", "Read fail, reg=0x%02X", reg);
  */
#define LOG_DEBUG(tag, fmt, ...)  Log_Write(LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag, fmt, ...)   Log_Write(LOG_INFO,  tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...)   Log_Write(LOG_WARN,  tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...)  Log_Write(LOG_ERROR, tag, fmt, ##__VA_ARGS__)

/**
  * @brief  条件日志 ———— 条件为真时写ERROR
  *         用法: LOG_ASSERT(ret == 0, "Init fail, ret=%d", ret);
  */
#define LOG_ASSERT(cond, tag, fmt, ...) \
    do { if (!(cond)) LOG_ERROR(tag, fmt, ##__VA_ARGS__); } while(0)

/**
  * @brief  运行时间获取（秒+毫秒）
  */
uint32_t Log_GetUptimeSec(void);

#endif /* __LOG_H */
