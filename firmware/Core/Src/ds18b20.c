/**
  ******************************************************************************
  * @file           : ds18b20.c
  * @brief          : DS18B20 OneWire驱动（软件模拟时序）
  *                   引脚: PB5, 上拉: 外接4.7kΩ VCC→DATA
  *                   延时: DWT周期计数器 (168MHz → 168cyc/µs)
  *                   精度: 12位 = 0.0625℃, 转换时间750ms
  ******************************************************************************
  */

#include "ds18b20.h"

/* ==================== DWT微秒延时 ==================== */

static void DWT_Init(void)
{
    /* 使能DWT跟踪单元 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
  * @brief  微秒级延时 (168MHz → 168cyc/µs)
  *         精确度: ±0.5µs @168MHz
  */
static void DWT_Delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * 168;  /* 168MHz: 1µs = 168周期 */
    while ((DWT->CYCCNT - start) < ticks);
}

/* ==================== OneWire底层 (寄存器直操作, 快) ==================== */

/* 设置PB5为输出 */
#define OW_OUTPUT()  (GPIOB->MODER |=  (1 << (5 * 2)))

/* 设置PB5为输入 */
#define OW_INPUT()   (GPIOB->MODER &= ~(3 << (5 * 2)))

/* 拉低PB5 */
#define OW_LOW()     (GPIOB->BSRR = (GPIO_PIN_5 << 16))

/* 释放PB5 (拉高) */
#define OW_HIGH()    (GPIOB->BSRR = GPIO_PIN_5)

/* 读取PB5电平 */
#define OW_READ()    ((GPIOB->IDR & GPIO_PIN_5) ? 1 : 0)

/**
  * @brief  发送复位脉冲，检测设备应答
  * @retval 0=有设备应答, 1=无设备
  */
static uint8_t OW_Reset(void)
{
    uint8_t presence;

    /* 主机拉低 480µs+ */
    OW_OUTPUT();
    OW_LOW();
    DWT_Delay_us(500);

    /* 释放总线 (上拉电阻拉高) */
    OW_INPUT();
    DWT_Delay_us(60);

    /* 采样应答: DS18B20会在15-60µs内拉低总线 */
    presence = OW_READ();

    /* 等待应答脉冲结束 */
    DWT_Delay_us(420);

    return presence;  /* 0=有设备, 1=无设备 */
}

/**
  * @brief  写一个bit到OneWire
  *         bit=0: 拉低60µs
  *         bit=1: 拉低1µs, 释放, 等60µs
  */
static void OW_WriteBit(uint8_t bit)
{
    if (bit)
    {
        /* 写1: 短拉低1-15µs */
        OW_OUTPUT();
        OW_LOW();
        DWT_Delay_us(2);       /* 2µs */
        OW_INPUT();            /* 释放 */
        DWT_Delay_us(60);      /* 等待整个时隙结束 */
    }
    else
    {
        /* 写0: 持续拉低60-120µs */
        OW_OUTPUT();
        OW_LOW();
        DWT_Delay_us(65);
        OW_INPUT();
        DWT_Delay_us(5);
    }
}

/**
  * @brief  从OneWire读一个bit
  *         主机先拉低1-5µs, 释放后等~10µs采样
  */
static uint8_t OW_ReadBit(void)
{
    uint8_t bit;

    OW_OUTPUT();
    OW_LOW();
    DWT_Delay_us(2);           /* 拉低2µs */

    OW_INPUT();                /* 释放总线 */
    DWT_Delay_us(8);           /* 等待数据稳定 */

    bit = OW_READ();           /* 采样！ */

    DWT_Delay_us(50);          /* 等待时隙结束 */

    return bit;
}

/**
  * @brief  写一个字节到OneWire (LSB First!)
  */
static void OW_WriteByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        OW_WriteBit(data & 0x01);
        data >>= 1;
    }
}

/**
  * @brief  从OneWire读一个字节 (LSB First!)
  */
static uint8_t OW_ReadByte(void)
{
    uint8_t data = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        if (OW_ReadBit())
            data |= (1 << i);
    }

    return data;
}

/* ==================== DS18B20公开函数 ==================== */

/**
  * @brief  初始化
  */
uint8_t DS18B20_Init(void)
{
    DWT_Init();
    return OW_Reset();  /* 0=检测到设备, 1=没检测到 */
}

/**
  * @brief  检测OneWire总线上是否有设备
  */
uint8_t DS18B20_CheckPresence(void)
{
    return OW_Reset();
}

/**
  * @brief  启动一次温度转换（不等待）
  *         发送 SKIP_ROM + CONVERT_T
  */
void DS18B20_StartConversion(void)
{
    OW_Reset();
    OW_WriteByte(0xCC);   /* SKIP ROM — 只有一个设备时跳过地址 */
    OW_WriteByte(0x44);   /* CONVERT T — 开始转换 */
}

/**
  * @brief  读取温度
  *         前提: 调用前已 DS18B20_StartConversion() + HAL_Delay(750)
  * @retval 温度值 × 10 (例: 256 → 25.6℃)
  */
int16_t DS18B20_ReadTemp(void)
{
    uint8_t  buf[9];
    int16_t  temp;
    uint8_t  sign;        /* 0=正温度, 1=负温度 */

    OW_Reset();
    OW_WriteByte(0xCC);   /* SKIP ROM */
    OW_WriteByte(0xBE);   /* READ SCRATCHPAD */

    /* 读9字节暂存器 */
    for (uint8_t i = 0; i < 9; i++)
    {
        buf[i] = OW_ReadByte();
    }

    /* 温度计算: 12位分辨率 */
    temp = ((int16_t)buf[1] << 8) | buf[0];

    /* 负数处理: 高5位为1表示负温度, 取补码 */
    if (temp & 0xF800)
    {
        sign = 1;
        temp = (~temp) + 1;   /* 补码转原码 */
    }
    else
    {
        sign = 0;
    }

    /* 换算为℃×10 */
    temp = (int16_t)((float)temp * 0.625f);   /* 0.0625 * 10 = 0.625 */

    if (sign)
        temp = -temp;

    return temp;
}
