/**
  ******************************************************************************
  * @file           : modbus_slave.c
  * @brief          : Modbus-RTU从站实现
  *                   CRC16: 多项式0xA001 (标准Modbus)
  *                   帧检测: 接收间隔>3ms视为帧结束
  ******************************************************************************
  */

#include "modbus_slave.h"
#include "w25q64.h"
#include "log.h"
#include "motor.h"
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

/* ==================== 寄存器表 ==================== */
static uint16_t holding_regs[REG_HOLDING_COUNT];  /* 保持寄存器 */
static uint16_t input_regs[IREG_INPUT_COUNT];      /* 输入寄存器 */
static uint8_t  last_error = 0;

/* 接收/发送缓冲 */
static uint8_t  rx_buf[MODBUS_FRAME_MAX];
static uint16_t rx_len = 0;
static uint8_t  tx_buf[MODBUS_FRAME_MAX];

/* 中断环形缓冲 (USART3 RXNE ISR写入, Modbus_Poll读取) */
#define RX_RING_SIZE 256
static volatile uint8_t  rx_ring[RX_RING_SIZE];
static volatile uint16_t rx_ring_wr = 0;  /* ISR写指针 */
static volatile uint16_t rx_ring_rd = 0;  /* Poll读指针 */
static volatile uint32_t last_isr_tick = 0; /* ISR最后收字节时刻 */

/* 中断接收: 由USART3_IRQHandler调用 */
void Modbus_ISR_RxByte(uint8_t ch)
{
    uint16_t next = (rx_ring_wr + 1) % RX_RING_SIZE;
    if (next != rx_ring_rd)  /* 未满 */
    {
        rx_ring[rx_ring_wr] = ch;
        rx_ring_wr = next;
        last_isr_tick = HAL_GetTick();  /* 记录ISR收包时间 */
    }
    /* 满了就丢弃(不应该发生) */
}

/* 非阻塞读环形缓冲: 返回0=无数据 */
static uint8_t Modbus_RingRead(uint8_t *ch)
{
    if (rx_ring_rd == rx_ring_wr) return 0;  /* 空 */
    *ch = rx_ring[rx_ring_rd];
    rx_ring_rd = (rx_ring_rd + 1) % RX_RING_SIZE;
    return 1;
}

/* ==================== CRC16 ==================== */

/* Modbus CRC16 查找表 (多项式 0xA001) */
static const uint16_t crc16_table[256] = {
    0x0000,0xC0C1,0xC181,0x0140,0xC301,0x03C0,0x0280,0xC241,
    0xC601,0x06C0,0x0780,0xC741,0x0500,0xC5C1,0xC481,0x0440,
    0xCC01,0x0CC0,0x0D80,0xCD41,0x0F00,0xCFC1,0xCE81,0x0E40,
    0x0A00,0xCAC1,0xCB81,0x0B40,0xC901,0x09C0,0x0880,0xC841,
    0xD801,0x18C0,0x1980,0xD941,0x1B00,0xDBC1,0xDA81,0x1A40,
    0x1E00,0xDEC1,0xDF81,0x1F40,0xDD01,0x1DC0,0x1C80,0xDC41,
    0x1400,0xD4C1,0xD581,0x1540,0xD701,0x17C0,0x1680,0xD641,
    0xD201,0x12C0,0x1380,0xD341,0x1100,0xD1C1,0xD081,0x1040,
    0xF001,0x30C0,0x3180,0xF141,0x3300,0xF3C1,0xF281,0x3240,
    0x3600,0xF6C1,0xF781,0x3740,0xF501,0x35C0,0x3480,0xF441,
    0x3C00,0xFCC1,0xFD81,0x3D40,0xFF01,0x3FC0,0x3E80,0xFE41,
    0xFA01,0x3AC0,0x3B80,0xFB41,0x3900,0xF9C1,0xF881,0x3840,
    0x2800,0xE8C1,0xE981,0x2940,0xEB01,0x2BC0,0x2A80,0xEA41,
    0xEE01,0x2EC0,0x2F80,0xEF41,0x2D00,0xEDC1,0xEC81,0x2C40,
    0xE401,0x24C0,0x2580,0xE541,0x2700,0xE7C1,0xE681,0x2640,
    0x2200,0xE2C1,0xE381,0x2340,0xE101,0x21C0,0x2080,0xE041,
    0xA001,0x60C0,0x6180,0xA141,0x6300,0xA3C1,0xA281,0x6240,
    0x6600,0xA6C1,0xA781,0x6740,0xA501,0x65C0,0x6480,0xA441,
    0x6C00,0xACC1,0xAD81,0x6D40,0xAF01,0x6FC0,0x6E80,0xAE41,
    0xAA01,0x6AC0,0x6B80,0xAB41,0x6900,0xA9C1,0xA881,0x6840,
    0x7800,0xB8C1,0xB981,0x7940,0xBB01,0x7BC0,0x7A80,0xBA41,
    0xBE01,0x7EC0,0x7F80,0xBF41,0x7D00,0xBDC1,0xBC81,0x7C40,
    0xB401,0x74C0,0x7580,0xB541,0x7700,0xB7C1,0xB681,0x7640,
    0x7200,0xB2C1,0xB381,0x7340,0xB101,0x71C0,0x7080,0xB041,
    0x5000,0x90C1,0x9181,0x5140,0x9301,0x53C0,0x5280,0x9241,
    0x9601,0x56C0,0x5780,0x9741,0x5500,0x95C1,0x9481,0x5440,
    0x9C01,0x5CC0,0x5D80,0x9D41,0x5F00,0x9FC1,0x9E81,0x5E40,
    0x5A00,0x9AC1,0x9B81,0x5B40,0x9901,0x59C0,0x5880,0x9841,
    0x8801,0x48C0,0x4980,0x8941,0x4B00,0x8BC1,0x8A81,0x4A40,
    0x4E00,0x8EC1,0x8F81,0x4F40,0x8D01,0x4DC0,0x4C80,0x8C41,
    0x4400,0x84C1,0x8581,0x4540,0x8701,0x47C0,0x4680,0x8641,
    0x8201,0x42C0,0x4380,0x8341,0x4100,0x81C1,0x8081,0x4040
};

static uint16_t Modbus_CRC16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++)
        crc = (crc >> 8) ^ crc16_table[(crc ^ data[i]) & 0xFF];
    return crc;
}

/* ==================== 初始化 ==================== */

void Modbus_Init(void)
{
    memset(holding_regs, 0, sizeof(holding_regs));
    memset(input_regs,  0, sizeof(input_regs));
    memset(rx_buf, 0, sizeof(rx_buf));
    rx_len = 0;
    rx_ring_wr = 0;
    rx_ring_rd = 0;
    holding_regs[REG_BB_COUNT] = Blackbox_GetCount();

    /* 使能USART3 RXNE中断 (寄存器直设) */
    USART3->CR1 |= USART_CR1_RXNEIE;
    NVIC_SetPriority(USART3_IRQn, 6);
    NVIC_EnableIRQ(USART3_IRQn);

    /* 波特率保持115200 (CubeMX配置):
     * 曾试降到57600, 但面包板MAX3485模块的自动方向电路按115200字符时间调教,
     * 57600下最后一字节停止位被DE提前切断→帧尾CRC必坏。PCB版MAX13487无此问题 */
    LOG_INFO("MODBUS", "Init OK (IRQ mode), addr=0x%02X, 115200 8N1", MODBUS_ADDR);
}

/* ==================== 寄存器更新 (由Sensor任务调用) ==================== */

void Modbus_UpdateRegs(uint16_t rms_mg, uint16_t freq_hz, uint16_t amp_mg,
                       int16_t temp_x10, uint32_t uptime_s, uint8_t status)
{
    holding_regs[REG_VIB_RMS_H]   = rms_mg;
    holding_regs[REG_VIB_FREQ]    = freq_hz;
    holding_regs[REG_VIB_AMP_H]   = amp_mg;
    holding_regs[REG_TEMP]        = (uint16_t)(int16_t)temp_x10;
    holding_regs[REG_UPTIME_H]    = (uint16_t)(uptime_s >> 16);
    holding_regs[REG_UPTIME_L]    = (uint16_t)(uptime_s & 0xFFFF);
    holding_regs[REG_STATUS]      = status;
}

/* 更新频谱特征寄存器 (AI输入16个频点)
 * spec_mg: 16个频点幅值(mG), 来自FFT spectrum[0..15]*1000
 */
void Modbus_UpdateSpectrum(const uint16_t *spec_mg, uint16_t count)
{
    if (count > REG_SPEC_COUNT) count = REG_SPEC_COUNT;
    for (uint16_t i = 0; i < count; i++)
        holding_regs[REG_SPEC_BASE + i] = spec_mg[i];
}

/* ==================== 帧处理 ==================== */

/**
  * @brief  处理功能码0x03: 读保持寄存器
  */
static uint8_t Modbus_Handle03(uint8_t *req, uint16_t *resp_len)
{
    uint16_t start_addr = ((uint16_t)req[2] << 8) | req[3];
    uint16_t quantity   = ((uint16_t)req[4] << 8) | req[5];

    if (quantity < 1 || quantity > REG_HOLDING_COUNT ||
        start_addr + quantity > REG_HOLDING_COUNT)
    {
        /* 异常响应: 0x83 + 异常码0x02(非法地址) */
        tx_buf[0] = MODBUS_ADDR;
        tx_buf[1] = 0x83;
        tx_buf[2] = 0x02;
        *resp_len = 3;
        return 1;
    }

    /* 正常响应 */
    tx_buf[0] = MODBUS_ADDR;
    tx_buf[1] = 0x03;
    tx_buf[2] = quantity * 2;  /* 字节数 */

    for (uint16_t i = 0; i < quantity; i++)
    {
        uint16_t val = holding_regs[start_addr + i];
        tx_buf[3 + i * 2]     = (val >> 8) & 0xFF;
        tx_buf[3 + i * 2 + 1] =  val       & 0xFF;
    }

    *resp_len = 3 + quantity * 2;
    return 0;
}

/**
  * @brief  处理功能码0x04: 读输入寄存器
  */
static uint8_t Modbus_Handle04(uint8_t *req, uint16_t *resp_len)
{
    uint16_t start_addr = ((uint16_t)req[2] << 8) | req[3];
    uint16_t quantity   = ((uint16_t)req[4] << 8) | req[5];

    if (quantity < 1 || quantity > IREG_INPUT_COUNT ||
        start_addr + quantity > IREG_INPUT_COUNT)
    {
        tx_buf[0] = MODBUS_ADDR;
        tx_buf[1] = 0x84;
        tx_buf[2] = 0x02;
        *resp_len = 3;
        return 1;
    }

    tx_buf[0] = MODBUS_ADDR;
    tx_buf[1] = 0x04;
    tx_buf[2] = quantity * 2;

    for (uint16_t i = 0; i < quantity; i++)
    {
        uint16_t val = input_regs[start_addr + i];
        tx_buf[3 + i * 2]     = (val >> 8) & 0xFF;
        tx_buf[3 + i * 2 + 1] =  val       & 0xFF;
    }

    *resp_len = 3 + quantity * 2;
    return 0;
}

/**
  * @brief  处理功能码0x06: 写单个寄存器
  */
static uint8_t Modbus_Handle06(uint8_t *req, uint16_t *resp_len)
{
    uint16_t addr  = ((uint16_t)req[2] << 8) | req[3];
    uint16_t value = ((uint16_t)req[4] << 8) | req[5];

    if (addr >= REG_HOLDING_COUNT)
    {
        tx_buf[0] = MODBUS_ADDR;
        tx_buf[1] = 0x86;
        tx_buf[2] = 0x02;
        *resp_len = 3;
        return 1;
    }

    /* 特殊命令: 写寄存器23 → 电机控制 (0=停, 1-999=转+占空比) */
    if (addr == REG_MOTOR)
    {
        if (value == 0)
            Motor_Stop();
        else
            Motor_Forward(value);
    }

    /* 特殊命令: 写寄存器25 → 读第N条黑匣子记录, 装入26~33 */
    if (addr == REG_BB_READ_IDX)
    {
        BlackboxRecord rec;
        if (Blackbox_Read(value, &rec))
        {
            holding_regs[REG_BB_REC_BASE + 0] = (uint16_t)(rec.timestamp >> 16);
            holding_regs[REG_BB_REC_BASE + 1] = (uint16_t)(rec.timestamp & 0xFFFF);
            holding_regs[REG_BB_REC_BASE + 2] = rec.rms_mg;
            holding_regs[REG_BB_REC_BASE + 3] = rec.freq_hz;
            holding_regs[REG_BB_REC_BASE + 4] = rec.amp_mg;
            holding_regs[REG_BB_REC_BASE + 5] = (uint16_t)(int16_t)rec.temp_x10;
            holding_regs[REG_BB_REC_BASE + 6] = rec.peak_bin;
            holding_regs[REG_BB_REC_BASE + 7] = rec.status;
        }
        else
        {
            /* 索引无效, 清零 */
            for (int i = 0; i < 8; i++)
                holding_regs[REG_BB_REC_BASE + i] = 0;
        }
    }

    /* 特殊命令: 写STATUS=0xCC00 → 清空黑匣子 */
    if (addr == REG_STATUS && value == 0xCC00)
    {
        Blackbox_ClearAll();
        LOG_INFO("MODBUS", "Blackbox clear command received");
    }

    holding_regs[addr] = value;

    /* 响应 = 回显请求 */
    memcpy(tx_buf, req, 6);
    *resp_len = 6;
    return 0;
}

/* ==================== 帧解析 ==================== */

void Modbus_Poll(void)
{
    uint8_t ch;

    /* 中断环形缓冲读数, 非阻塞 */
    while (Modbus_RingRead(&ch))
    {
        if (rx_len < MODBUS_FRAME_MAX - 1)
            rx_buf[rx_len++] = ch;
    }

    /* 无数据 → 返回 (顺手刷新黑匣子计数) */
    if (rx_len == 0)
    {
        static uint32_t bb_last = 0;
        if ((HAL_GetTick() - bb_last) > 500)
        {
            holding_regs[REG_BB_COUNT] = Blackbox_GetCount();
            bb_last = HAL_GetTick();
        }
        return;
    }

    /* ISR最后收字节后静默3ms → 帧结束 */
    if ((int32_t)(HAL_GetTick() - last_isr_tick) < 3) return;

    if (rx_len < 4) { rx_len = 0; return; }  /* 帧太短 */

    /* CRC校验 */
    uint16_t crc_rcvd = (rx_buf[rx_len - 1] << 8) | rx_buf[rx_len - 2];
    uint16_t crc_calc = Modbus_CRC16(rx_buf, rx_len - 2);
    if (crc_rcvd != crc_calc)
    {
        rx_len = 0;
        return;
    }

    /* 检查地址 */
    if (rx_buf[0] != MODBUS_ADDR && rx_buf[0] != 0x00)
    {
        rx_len = 0;
        return;  /* 不是发给我们的 */
    }

    /* 解析功能码 */
    uint8_t  func     = rx_buf[1];
    uint16_t resp_len = 0;

    switch (func)
    {
        case 0x03: Modbus_Handle03(rx_buf, &resp_len); break;
        case 0x04: Modbus_Handle04(rx_buf, &resp_len); break;
        case 0x06: Modbus_Handle06(rx_buf, &resp_len); break;
        default:
            /* 非法功能码 */
            tx_buf[0] = MODBUS_ADDR;
            tx_buf[1] = func | 0x80;
            tx_buf[2] = 0x01;
            resp_len = 3;
            last_error = func;
            break;
    }

    /* 追加CRC */
    uint16_t crc = Modbus_CRC16(tx_buf, resp_len);
    tx_buf[resp_len]     = crc & 0xFF;
    tx_buf[resp_len + 1] = (crc >> 8) & 0xFF;
    resp_len += 2;

    /* 寄存器直写USART3发送
     * vTaskSuspendAll: 发送期间禁任务调度, 防止高优先级任务抢占导致
     * 字节流出现间隙 (Modbus-RTU要求字节间隔<1.5字符时间, 否则主站判帧截断)
     * 不影响TIM4中断喂狗 */
    vTaskSuspendAll();
    for (uint16_t i = 0; i < resp_len; i++)
    {
        while (!(USART3->SR & USART_SR_TXE));
        USART3->DR = tx_buf[i];
    }
    while (!(USART3->SR & USART_SR_TC));
    xTaskResumeAll();

    /* 清空接收缓冲 + 环形缓冲 */
    rx_len = 0;
    {
        uint8_t dummy;
        while (Modbus_RingRead(&dummy)) {}  /* 丢弃残余字节 */
    }
}

uint8_t Modbus_GetLastError(void)
{
    uint8_t e = last_error;
    last_error = 0;
    return e;
}
