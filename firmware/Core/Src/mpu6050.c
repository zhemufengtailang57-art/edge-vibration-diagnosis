/**
  ******************************************************************************
  * @file           : mpu6050.c
  * @brief          : MPU6050 六轴传感器驱动
  *                   I2C1总线, 地址0x68(7bit)
  *                   数据就绪后INT引脚变高，本驱动使用轮询模式
  ******************************************************************************
  */

#include "mpu6050.h"

/* ==================== 寄存器地址 ==================== */
#define MPU_REG_WHO_AM_I      0x75  /* 应返回 0x68 */
#define MPU_REG_PWR_MGMT_1    0x6B  /* 电源管理: bit6=SLEEP */
#define MPU_REG_SMPLRT_DIV    0x19  /* 采样率分频 */
#define MPU_REG_CONFIG         0x1A  /* 数字低通滤波 */
#define MPU_REG_GYRO_CONFIG   0x1B  /* 陀螺仪量程 */
#define MPU_REG_ACCEL_CONFIG  0x1C  /* 加速度计量程 */
#define MPU_REG_ACCEL_XOUT_H  0x3B  /* 加速度 X高8位(共14位有效) */
#define MPU_REG_TEMP_OUT_H    0x41  /* 温度 高8位 */
#define MPU_REG_GYRO_XOUT_H   0x43  /* 角速度 X高8位(共16位有效) */

/* 外部引用I2C句柄（CubeMX生成在i2c.c中） */
extern I2C_HandleTypeDef hi2c1;

/* ==================== 底层I2C读写 ==================== */

static uint8_t MPU_I2C_ReadByte(uint8_t reg, uint8_t *data)
{
    /* HAL_I2C_Mem_Read: 先发寄存器地址，再读1字节 */
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, reg,
                         I2C_MEMADD_SIZE_8BIT, data, 1, 100) != HAL_OK)
        return 1;  /* 读失败 */
    return 0;
}

static uint8_t MPU_I2C_WriteByte(uint8_t reg, uint8_t data)
{
    if (HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, reg,
                          I2C_MEMADD_SIZE_8BIT, &data, 1, 100) != HAL_OK)
        return 1;
    return 0;
}

static uint8_t MPU_I2C_ReadMulti(uint8_t reg, uint8_t *buf, uint8_t len)
{
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, reg,
                         I2C_MEMADD_SIZE_8BIT, buf, len, 100) != HAL_OK)
        return 1;
    return 0;
}

/* ==================== 公开函数 ==================== */

/**
  * @brief  初始化MPU6050
  *         1. 唤醒（退出睡眠模式）
  *         2. 配置采样率、低通滤波
  *         3. 检查WHO_AM_I
  * @retval 0=成功
  */
uint8_t MPU6050_Init(void)
{
    uint8_t whoami;

    /* 1. 唤醒MPU6050: PWR_MGMT_1 bit6(SLEEP)写0 */
    MPU_I2C_WriteByte(MPU_REG_PWR_MGMT_1, 0x00);
    HAL_Delay(100);  /* 等待内部晶振起振 */

    /* 2. 采样率分频: 0 → 1kHz (陀螺仪采样率=8kHz/(1+0)=8kHz,
          但DLPF配了之后实际输出1kHz) */
    MPU_I2C_WriteByte(MPU_REG_SMPLRT_DIV, 0x07);  /* 1kHz / (7+1) = 125Hz */

    /* 3. 数字低通滤波: DLPF_CFG=3 (44Hz带宽, 适合振动检测) */
    MPU_I2C_WriteByte(MPU_REG_CONFIG, 0x03);

    /* 4. 默认量程不配（±2g / ±250°/s），用默认值即可 */

    /* 5. 检查WHO_AM_I */
    if (MPU_I2C_ReadByte(MPU_REG_WHO_AM_I, &whoami))
        return 1;  /* I2C通信失败 */

    if (whoami != 0x70)
        return 2;  /* WHO_AM_I不匹配，模块有问题 */

    return 0;  /* 初始化成功 */
}

/**
  * @brief  读取WHO_AM_I（调试用）
  */
uint8_t MPU6050_ReadWhoAmI(void)
{
    uint8_t val = 0;
    MPU_I2C_ReadByte(MPU_REG_WHO_AM_I, &val);
    return val;
}

/**
  * @brief  读取三轴加速度（原始值，单位LSB）
  *         默认±2g量程: 16384 LSB/g
  *         换算: g = raw / 16384.0f
  */
void MPU6050_ReadAccel(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t buf[6];

    if (MPU_I2C_ReadMulti(MPU_REG_ACCEL_XOUT_H, buf, 6))
    {
        /* 读失败，返回0 */
        *ax = *ay = *az = 0;
        return;
    }

    /* 大端→小端, 14位有效(高8位+低8位的高6位) */
    *ax = (int16_t)((buf[0] << 8) | buf[1]);
    *ay = (int16_t)((buf[2] << 8) | buf[3]);
    *az = (int16_t)((buf[4] << 8) | buf[5]);
}

/**
  * @brief  读取三轴角速度（原始值，单位LSB）
  *         默认±250°/s量程: 131 LSB/(°/s)
  *         换算: deg/s = raw / 131.0f
  */
void MPU6050_ReadGyro(int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t buf[6];

    if (MPU_I2C_ReadMulti(MPU_REG_GYRO_XOUT_H, buf, 6))
    {
        *gx = *gy = *gz = 0;
        return;
    }

    *gx = (int16_t)((buf[0] << 8) | buf[1]);
    *gy = (int16_t)((buf[2] << 8) | buf[3]);
    *gz = (int16_t)((buf[4] << 8) | buf[5]);
}

/**
  * @brief  读取温度
  *         公式: Temp(℃) = raw / 340.0f + 36.53f
  */
float MPU6050_ReadTemp(void)
{
    uint8_t buf[2];
    int16_t raw;

    if (MPU_I2C_ReadMulti(MPU_REG_TEMP_OUT_H, buf, 2))
        return 0.0f;

    raw = (int16_t)((buf[0] << 8) | buf[1]);
    return (float)raw / 340.0f + 36.53f;
}

/**
  * @brief  设置加速度量程
  */
void MPU6050_SetAccelRange(MPU6050_AccelRange range)
{
    MPU_I2C_WriteByte(MPU_REG_ACCEL_CONFIG, range);
}

/**
  * @brief  设置角速度量程
  */
void MPU6050_SetGyroRange(MPU6050_GyroRange range)
{
    MPU_I2C_WriteByte(MPU_REG_GYRO_CONFIG, range);
}
