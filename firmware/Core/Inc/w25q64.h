/**
  ******************************************************************************
  * @file           : w25q64.h
  * @brief          : W25Q64 8MB SPI Flash驱动
  *                   总线: SPI2 Mode0, CS=PB12(软件)
  *                   用途: MQTT断网缓存(前6MB) + 黑匣子(后2MB)
  ******************************************************************************
  */

#ifndef __W25Q64_H
#define __W25Q64_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* SPI句柄 ———— CubeMX生成的SPI2 */
extern SPI_HandleTypeDef hspi2;

/* CS引脚 */
#define W25Q64_CS_PORT      GPIOB
#define W25Q64_CS_PIN       GPIO_PIN_12

/* Flash参数 */
#define W25Q64_PAGE_SIZE        256     /* 页大小 */
#define W25Q64_SECTOR_SIZE      4096    /* 扇区大小 (4KB) */
#define W25Q64_BLOCK_SIZE       65536   /* 块大小 (64KB) */
#define W25Q64_TOTAL_SIZE       8388608 /* 8MB */

/* ==================== API ==================== */

/**
  * @brief  初始化W25Q64 (读JEDEC ID验证)
  * @retval 0=成功, 1=SPI通信失败, 2=ID校验失败
  */
uint8_t W25Q64_Init(void);

/**
  * @brief  读JEDEC ID
  * @param  id[3]: 输出 [厂商, 类型, 容量]
  */
void    W25Q64_ReadJEDEC(uint8_t id[3]);

/**
  * @brief  读Flash数据
  * @param  addr: 24位地址
  * @param  buf: 输出缓冲区
  * @param  len: 读取字节数
  */
void    W25Q64_Read(uint32_t addr, uint8_t *buf, uint32_t len);

/**
  * @brief  写一页 (最多256字节, 不跨页)
  *         调用前需先 W25Q64_WriteEnable() + 等待WEL位
  * @param  addr: 24位地址
  * @param  buf: 数据
  * @param  len: ≤256字节
  */
void    W25Q64_PageWrite(uint32_t addr, uint8_t *buf, uint16_t len);

/**
  * @brief  扇区擦除 (4KB)
  */
void    W25Q64_SectorErase(uint32_t addr);

/**
  * @brief  芯片全部擦除 (慢, ~40秒)
  */
void    W25Q64_ChipErase(void);

/**
  * @brief  读取状态寄存器
  */
uint8_t W25Q64_ReadStatus(void);

/**
  * @brief  等待Flash空闲 (轮询BUSY位)
  */
void    W25Q64_WaitBusy(void);

/* 黑匣子: 故障记录 */
#define BLACKBOX_ADDR       4096    /* 黑匣子起始地址 (扇区1) */
#define BLACKBOX_META_ADDR  8192    /* 元数据地址 (扇区2, 独立, 不被R/W测试破坏) */
#define BLACKBOX_MAX_REC     500     /* 最大记录数 */
#define BLACKBOX_REC_SIZE    16      /* 每条记录16字节 */

typedef struct {
    uint32_t timestamp;     /* 运行秒 */
    uint16_t rms_mg;        /* RMS (mG) */
    uint16_t freq_hz;       /* 主频 (Hz) */
    uint16_t amp_mg;        /* 幅值 (mG) */
    int16_t  temp_x10;      /* 温度×10 */
    uint16_t peak_bin;      /* FFT bin */
    uint8_t  status;        /* 0=正常, 1=预警, 2=故障 */
    uint8_t  reserved;
} BlackboxRecord;

uint16_t Blackbox_Write(const BlackboxRecord *rec);
uint16_t Blackbox_Read(uint16_t index, BlackboxRecord *rec);
void     Blackbox_ClearAll(void);
uint16_t Blackbox_GetCount(void);

#endif /* __W25Q64_H */
