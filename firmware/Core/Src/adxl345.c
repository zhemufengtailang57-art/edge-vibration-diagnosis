/**
  ******************************************************************************
  * @file           : adxl345.c
  * @brief          : ADXL345 三轴加速度传感器 SPI驱动
  *                   总线: SPI1 Mode3 (CPOL=1, CPHA=1)
  *                   片选: PA4 (软件控制)
  *                   量程: ±16g, 13位全分辨率, 200Hz
  ******************************************************************************
  */

#include "adxl345.h"

/* ==================== 寄存器地址 ==================== */
#define ADXL_REG_DEVID          0x00  /* 设备ID, 应返回 0xE5       */
#define ADXL_REG_BW_RATE        0x2C  /* 采样率 + 低功耗模式        */
#define ADXL_REG_POWER_CTL      0x2D  /* 电源控制 bit3=测量模式    */
#define ADXL_REG_DATA_FORMAT    0x31  /* 数据格式: 量程+分辨率     */
#define ADXL_REG_DATAX0         0x32  /* X轴低8位 (起点, 共6字节)  */
#define ADXL_REG_INT_ENABLE     0x2E  /* 中断使能                  */
#define ADXL_REG_INT_MAP        0x2F  /* 中断映射                  */

/* ==================== SPI底层 ==================== */

/* CS拉低/拉高 */
#define ADXL_CS_LOW()   HAL_GPIO_WritePin(ADXL345_CS_PORT, ADXL345_CS_PIN, GPIO_PIN_RESET)
#define ADXL_CS_HIGH()  HAL_GPIO_WritePin(ADXL345_CS_PORT, ADXL345_CS_PIN, GPIO_PIN_SET)

/**
  * @brief  SPI读写: 发一个字节, 同时收一个字节 (全双工)
  */
static uint8_t ADXL_SPI_RW(uint8_t tx_data)
{
    uint8_t rx_data;
    HAL_SPI_TransmitReceive(&hspi1, &tx_data, &rx_data, 1, 100);
    return rx_data;
}

/**
  * @brief  读单字节寄存器
  */
static uint8_t ADXL_ReadReg(uint8_t reg)
{
    uint8_t val;
    ADXL_CS_LOW();
    ADXL_SPI_RW(0x80 | reg);   /* R/W=1(读), MB=0(单字节) */
    val = ADXL_SPI_RW(0x00);   /* 空写一字节, 接收返回值 */
    ADXL_CS_HIGH();
    return val;
}

/**
  * @brief  写单字节寄存器
  */
static void ADXL_WriteReg(uint8_t reg, uint8_t data)
{
    ADXL_CS_LOW();
    ADXL_SPI_RW(reg & 0x3F);   /* R/W=0(写), MB=0 */
    ADXL_SPI_RW(data);
    ADXL_CS_HIGH();
}

/**
  * @brief  连续读多字节 (地址自动递增)
  *         MB=1: 发0xC0|reg, ADXL345自动地址递增
  */
static void ADXL_ReadMulti(uint8_t reg, uint8_t *buf, uint8_t len)
{
    ADXL_CS_LOW();
    ADXL_SPI_RW(0xC0 | reg);   /* R/W=1(读), MB=1(连续读) */
    for (uint8_t i = 0; i < len; i++)
    {
        buf[i] = ADXL_SPI_RW(0x00);
    }
    ADXL_CS_HIGH();
}

/* ==================== 公开函数 ==================== */

/**
  * @brief  初始化ADXL345
  */
uint8_t ADXL345_Init(void)
{
    uint8_t devid;

    /* 1. 初始CS引脚为高 */
    ADXL_CS_HIGH();

    /* 2. 检查DEVID */
    devid = ADXL_ReadReg(ADXL_REG_DEVID);
    if (devid != 0xE5)
        return 2;  /* DEVID校验失败 */

    /* 3. 配置 DATA_FORMAT: 全分辨率(bit3=1) + ±16g(bit1:0=11) */
    ADXL_WriteReg(ADXL_REG_DATA_FORMAT, 0x0B);  /* 0000 1011 */

    /* 4. 配置 BW_RATE: 200Hz采样率 (bit3:0=1011) */
    ADXL_WriteReg(ADXL_REG_BW_RATE, 0x0B);      /* 0000 1011 = 200Hz */

    /* 5. 无需中断 (保持默认) */

    /* 6. 启动测量: POWER_CTL bit3(Measure)=1, bit2(Sleep)=0 */
    ADXL_WriteReg(ADXL_REG_POWER_CTL, 0x08);    /* 0000 1000 */

    HAL_Delay(10);  /* 启动后等待第一个采样 */

    return 0;  /* 初始化成功 */
}

/**
  * @brief  读取三轴加速度原始值
  *         数据格式: 小端, 先低8位后高8位
  *         ±16g全分辨率: ~3.9mg/LSB, 即 ~256 LSB/g
  */
void ADXL345_ReadAccel(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t buf[6];

    ADXL_ReadMulti(ADXL_REG_DATAX0, buf, 6);

    /* 小端拼接: 低8位 | (高8位 << 8) */
    *ax = (int16_t)((buf[1] << 8) | buf[0]);
    *ay = (int16_t)((buf[3] << 8) | buf[2]);
    *az = (int16_t)((buf[5] << 8) | buf[4]);
}

/**
  * @brief  读取DEVID（调试用，应为0xE5）
  */
uint8_t ADXL345_ReadDevid(void)
{
    return ADXL_ReadReg(ADXL_REG_DEVID);
}
