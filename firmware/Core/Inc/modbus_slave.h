/**
  ******************************************************************************
  * @file           : modbus_slave.h
  * @brief          : Modbus-RTU从站协议栈
  *                   物理层: RS485 (MAX3485, USART3, 半双工)
  *                   功能码: 0x03(读保持寄存器), 0x04(读输入寄存器)
  *                   帧格式: [地址][功能码][数据][CRC16]
  *                   地址: 0x01 (可配置)
  ******************************************************************************
  */

#ifndef __MODBUS_SLAVE_H
#define __MODBUS_SLAVE_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ==================== 配置 ==================== */
#define MODBUS_ADDR         0x01    /* 从站地址 */
#define MODBUS_FRAME_MAX    64      /* 最大帧长 */
#define MODBUS_T15_TIMEOUT  3500    /* 1.5字符超时(µs), 115200≈130µs */

/* ==================== 寄存器映射 ==================== */
/* 保持寄存器 (0x03 读, 0x06 写) */
#define REG_VIB_RMS_H       0x0000  /* 振动RMS (mG) */
#define REG_VIB_FREQ        0x0001  /* 主频 (Hz) */
#define REG_VIB_AMP_H       0x0002  /* 峰值幅值 (mG) */
#define REG_TEMP            0x0003  /* 温度 ×10 (°C) */
#define REG_UPTIME_H        0x0004  /* 运行时间秒 (高16位) */
#define REG_UPTIME_L        0x0005  /* 运行时间秒 (低16位) */
#define REG_STATUS          0x0006  /* 系统状态 (0=正常, 1=预警, 2=故障) */

/* 频谱特征寄存器 (AI输入, 16个频点能量)
 * 频点i对应频率 = i * 7.81Hz (FFT分辨率)
 * bin0=0Hz, bin1=7.8Hz, ..., bin15=117Hz
 * 值 = 该频点幅值 * 1000 (mG), 0-65535
 */
#define REG_SPEC_BASE       0x0007  /* 频谱起始地址 (7~22) */
#define REG_SPEC_COUNT      16      /* 16个频点 */
#define REG_MOTOR           0x0017  /* 电机控制: 0=停, 1-999=转(占空比) */

/* 黑匣子读取接口 (24~33) */
#define REG_BB_COUNT        0x0018  /* 黑匣子记录总数 (只读) */
#define REG_BB_READ_IDX     0x0019  /* 写N→把第N条装入26~33 */
#define REG_BB_REC_BASE     0x001A  /* 记录内容起始 (26~33, 共8个寄存器) */
#define REG_HOLDING_COUNT   34      /* 保持寄存器总数=34 */

/* 输入寄存器 (0x04 只读) */
#define IREG_RMS_RAW        0x0000  /* RMS原始值(LSB) */
#define IREG_FREQ_BIN       0x0001  /* FFT峰值bin */
#define IREG_INPUT_COUNT    2       /* 输入寄存器总数 */

/* ==================== API ==================== */

/**
  * @brief  初始化Modbus (启动USART3接收轮询)
  */
void Modbus_Init(void);

/**
  * @brief  Modbus处理任务 (放入FreeRTOS任务循环)
  *         轮询接收帧 → 解析 → 构建响应 → 发送
  *         建议20-50ms调用一次
  */
void Modbus_Poll(void);

/**
  * @brief  更新寄存器值 (由Sensor任务定期调用)
  */
void Modbus_UpdateRegs(uint16_t rms_mg, uint16_t freq_hz, uint16_t amp_mg,
                       int16_t temp_x10, uint32_t uptime_s, uint8_t status);

/**
  * @brief  更新频谱特征寄存器 (AI输入, 16个频点)
  * @param  spec_mg: 频点幅值数组(单位mG)
  * @param  count: 频点数量(最多16)
  */
void Modbus_UpdateSpectrum(const uint16_t *spec_mg, uint16_t count);

/**
  * @brief  获取最近一次错误码 (调试用)
  */
uint8_t Modbus_GetLastError(void);

/**
  * @brief  USART3中断回调: 将接收字节写入环形缓冲
  *         由USART3_IRQHandler调用
  */
void Modbus_ISR_RxByte(uint8_t ch);

#endif /* __MODBUS_SLAVE_H */
