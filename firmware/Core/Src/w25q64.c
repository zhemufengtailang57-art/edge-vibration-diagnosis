/**
  ******************************************************************************
  * @file           : w25q64.c
  * @brief          : W25Q64 8MB SPI NOR Flash驱动
  *                   SPI2 Mode0, CS=PB12
  *                   命令集: 标准SPI NOR Flash指令
  ******************************************************************************
  */

#include "w25q64.h"
#include "cmsis_os.h"
#include "log.h"
#include <string.h>

/* SPI2全局互斥量 (main.c里创建, 黑匣子vs健康快照防冲突) */
osMutexId_t spi2MutexHandle = NULL;

/* ==================== 命令码 ==================== */
#define CMD_WRITE_ENABLE    0x06  /* 写使能                    */
#define CMD_WRITE_DISABLE   0x04  /* 写禁止                    */
#define CMD_READ_STATUS     0x05  /* 读状态寄存器              */
#define CMD_READ_DATA       0x03  /* 读数据                    */
#define CMD_PAGE_PROGRAM    0x02  /* 页编程 (256B)             */
#define CMD_SECTOR_ERASE    0x20  /* 扇区擦除 (4KB)            */
#define CMD_CHIP_ERASE      0xC7  /* 全片擦除                  */
#define CMD_JEDEC_ID        0x9F  /* 读JEDEC ID                */
#define CMD_POWER_DOWN      0xB9  /* 掉电模式                  */
#define CMD_RELEASE_PD      0xAB  /* 唤醒                      */

/* 状态寄存器位 */
#define STATUS_BUSY         0x01
#define STATUS_WEL          0x02

/* ==================== 前向声明 ==================== */
static void Blackbox_LoadMeta(void);

/* ==================== 底层SPI ==================== */

#define W25_CS_LOW()   HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_RESET)
#define W25_CS_HIGH()  HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_SET)

static uint8_t W25_SPI_RW(uint8_t tx)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi2, &tx, &rx, 1, 100);
    return rx;
}

static void W25_SPI_Write(uint8_t *buf, uint16_t len)
{
    HAL_SPI_Transmit(&hspi2, buf, len, 1000);
}

static void W25_SPI_Read(uint8_t *buf, uint16_t len)
{
    HAL_SPI_Receive(&hspi2, buf, len, 1000);
}

/* ==================== 命令函数 ==================== */

void W25Q64_WaitBusy(void)
{
    uint32_t deadline = HAL_GetTick() + 5000;  /* 5秒超时保护 */
    uint8_t status;
    do {
        W25_CS_LOW();
        W25_SPI_RW(CMD_READ_STATUS);
        status = W25_SPI_RW(0xFF);
        W25_CS_HIGH();
        if ((int32_t)(HAL_GetTick() - deadline) > 0) {
            LOG_ERROR("W25Q64", "Busy timeout! status=0x%02X (SPI线接触不良或芯片异常)", status);
            break;
        }
    } while (status & STATUS_BUSY);
}

static void W25_WriteEnable(void)
{
    W25_CS_LOW();
    W25_SPI_RW(CMD_WRITE_ENABLE);
    W25_CS_HIGH();
}

uint8_t W25Q64_ReadStatus(void)
{
    uint8_t status;
    W25_CS_LOW();
    W25_SPI_RW(CMD_READ_STATUS);
    status = W25_SPI_RW(0xFF);
    W25_CS_HIGH();
    return status;
}

/* ==================== 公开API ==================== */

uint8_t W25Q64_Init(void)
{
    uint8_t id[3];

    W25_CS_HIGH();
    HAL_Delay(10);

    W25Q64_ReadJEDEC(id);

    /* W25Q64 JEDEC: 厂商0xEF, 类型0x40, 容量0x17 */
    if (id[0] != 0xEF || id[1] != 0x40 || id[2] != 0x17)
        return 2;  /* ID不匹配 */

    Blackbox_LoadMeta();  /* 读取历史故障计数 */

    return 0;
}

void W25Q64_ReadJEDEC(uint8_t id[3])
{
    W25_CS_LOW();
    W25_SPI_RW(CMD_JEDEC_ID);
    id[0] = W25_SPI_RW(0xFF);
    id[1] = W25_SPI_RW(0xFF);
    id[2] = W25_SPI_RW(0xFF);
    W25_CS_HIGH();
}

void W25Q64_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    W25_CS_LOW();
    W25_SPI_RW(CMD_READ_DATA);
    W25_SPI_RW((addr >> 16) & 0xFF);  /* A23-A16 */
    W25_SPI_RW((addr >> 8)  & 0xFF);  /* A15-A8  */
    W25_SPI_RW( addr        & 0xFF);  /* A7-A0   */
    W25_SPI_Read(buf, len);
    W25_CS_HIGH();
}

void W25Q64_PageWrite(uint32_t addr, uint8_t *buf, uint16_t len)
{
    W25_WriteEnable();
    W25_CS_LOW();
    W25_SPI_RW(CMD_PAGE_PROGRAM);
    W25_SPI_RW((addr >> 16) & 0xFF);
    W25_SPI_RW((addr >> 8)  & 0xFF);
    W25_SPI_RW( addr        & 0xFF);
    W25_SPI_Write(buf, len);
    W25_CS_HIGH();
    W25Q64_WaitBusy();
}

void W25Q64_SectorErase(uint32_t addr)
{
    W25_WriteEnable();
    W25_CS_LOW();
    W25_SPI_RW(CMD_SECTOR_ERASE);
    W25_SPI_RW((addr >> 16) & 0xFF);
    W25_SPI_RW((addr >> 8)  & 0xFF);
    W25_SPI_RW( addr        & 0xFF);
    W25_CS_HIGH();
    W25Q64_WaitBusy();
}

void W25Q64_ChipErase(void)
{
    W25_WriteEnable();
    W25_CS_LOW();
    W25_SPI_RW(CMD_CHIP_ERASE);
    W25_CS_HIGH();
    W25Q64_WaitBusy();  /* 全片擦除需要约40秒 */
}

/* ==================== 黑匣子 ==================== */

static uint16_t blackbox_count = 0;
static uint16_t blackbox_idx = 0;      /* 当前写入位置(持久化) */

/* 擦除黑匣子整个数据区 (4096~12096, 跨3个扇区) */
static void Blackbox_EraseDataArea(void)
{
    uint32_t a = BLACKBOX_ADDR;
    uint32_t end = a + BLACKBOX_MAX_REC * BLACKBOX_REC_SIZE;
    while (a < end)
    {
        W25Q64_SectorErase(a);
        W25Q64_WaitBusy();
        a += W25Q64_SECTOR_SIZE;
    }
}

static void Blackbox_SaveMeta(void)
{
    uint8_t buf[4];
    buf[0] = blackbox_count >> 8;
    buf[1] = blackbox_count & 0xFF;
    buf[2] = blackbox_idx >> 8;
    buf[3] = blackbox_idx & 0xFF;
    W25Q64_SectorErase(BLACKBOX_META_ADDR);
    W25Q64_WaitBusy();
    W25Q64_PageWrite(BLACKBOX_META_ADDR, buf, 4);
    W25Q64_WaitBusy();
}

static void Blackbox_LoadMeta(void)
{
    uint8_t buf[4];
    W25Q64_Read(BLACKBOX_META_ADDR, buf, 4);
    blackbox_count = ((uint16_t)buf[0] << 8) | buf[1];
    blackbox_idx   = ((uint16_t)buf[2] << 8) | buf[3];
    if (blackbox_count > BLACKBOX_MAX_REC) blackbox_count = 0;
    if (blackbox_idx   > BLACKBOX_MAX_REC) blackbox_idx   = 0;
}

uint16_t Blackbox_Write(const BlackboxRecord *rec)
{
    if (spi2MutexHandle) osMutexAcquire(spi2MutexHandle, osWaitForever);

    /* 如果idx=0(从头写或循环回), 擦除即将写入的扇区 */
    if (blackbox_idx == 0)
    {
        Blackbox_EraseDataArea();
        blackbox_count = 0;  /* 全擦后计数清零 */
    }

    if (blackbox_idx >= BLACKBOX_MAX_REC) blackbox_idx = 0;

    uint32_t addr = BLACKBOX_ADDR + blackbox_idx * BLACKBOX_REC_SIZE;
    W25Q64_PageWrite(addr, (uint8_t *)rec, BLACKBOX_REC_SIZE);
    W25Q64_WaitBusy();

    /* 诊断: 写后立即读回对比 */
    {
        BlackboxRecord verify;
        W25Q64_Read(addr, (uint8_t *)&verify, BLACKBOX_REC_SIZE);
        if (memcmp(&verify, rec, BLACKBOX_REC_SIZE) != 0)
            LOG_ERROR("BBOX", "Verify FAIL addr=%d: wrote T=%ds RMS=%d St=%d | back T=%ds RMS=%d St=%d",
                      addr, rec->timestamp, rec->rms_mg, rec->status,
                      verify.timestamp, verify.rms_mg, verify.status);
    }

    blackbox_idx++;
    if (blackbox_count < BLACKBOX_MAX_REC) blackbox_count++;
    Blackbox_SaveMeta();

    if (spi2MutexHandle) osMutexRelease(spi2MutexHandle);
    return blackbox_count;
}

uint16_t Blackbox_Read(uint16_t index, BlackboxRecord *rec)
{
    if (spi2MutexHandle) osMutexAcquire(spi2MutexHandle, osWaitForever);
    if (index >= blackbox_count) {
        if (spi2MutexHandle) osMutexRelease(spi2MutexHandle);
        return 0;
    }
    W25Q64_Read(BLACKBOX_ADDR + index * BLACKBOX_REC_SIZE,
                (uint8_t *)rec, BLACKBOX_REC_SIZE);
    if (spi2MutexHandle) osMutexRelease(spi2MutexHandle);
    return 1;
}

void Blackbox_ClearAll(void)
{
    if (spi2MutexHandle) osMutexAcquire(spi2MutexHandle, osWaitForever);
    Blackbox_EraseDataArea();   /* 擦掉所有旧记录 */
    blackbox_count = 0;
    blackbox_idx   = 0;         /* 写指针回到开头 */
    Blackbox_SaveMeta();
    if (spi2MutexHandle) osMutexRelease(spi2MutexHandle);
}

uint16_t Blackbox_GetCount(void)
{
    return blackbox_count;
}
