/**
  ******************************************************************************
  * @file           : esp8266.c
  * @brief          : ESP8266-01S AT指令驱动（纯轮询模式，简单可靠）
  *                   USART1: PA9=TX→ESP RX, PA10=RX→ESP TX
  *                   PB0=EN/CH_PD, 高电平使能
  *                   启动时间: ~2s（上电→蓝灯闪烁→就绪）
  ******************************************************************************
  */

#include "esp8266.h"

/* Rx缓冲区 */
static char    rx_buf[ESP_RX_BUF_SIZE];
static uint16_t rx_len = 0;

/* ==================== 初始化 ==================== */

uint8_t ESP8266_Init(void)
{
    /* 1. 拉高EN = 使能ESP8266 */
    HAL_GPIO_WritePin(ESP8266_EN_PORT, ESP8266_EN_PIN, GPIO_PIN_SET);

    /* 2. 等待模块启动 (ESP8266上电→就绪约需2秒) */
    HAL_Delay(2500);

    /* 3. 发AT测试（有时第一帧是上电乱码，多试一次） */
    for (uint8_t i = 0; i < 3; i++)
    {
        if (ESP8266_SendCmd("AT", "OK", 1500) == 0)
            return 0;
        HAL_Delay(500);
    }

    return 1;  /* 无响应 */
}

/* ==================== 发送 + 轮询接收 ==================== */

/**
  * @brief  通过USART1发送字符串
  */
static void ESP_SendStr(const char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), 1000);
}

/**
  * @brief  阻塞读取USART1数据（有超时）
  * @retval 读到的字节数
  */
static uint16_t ESP_ReadBytes(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    rx_len = 0;
    memset(rx_buf, 0, ESP_RX_BUF_SIZE);

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        uint8_t ch;
        /* 尝试读1字节，1ms超时 */
        if (HAL_UART_Receive(&huart1, &ch, 1, 1) == HAL_OK)
        {
            if (rx_len < ESP_RX_BUF_SIZE - 1)
            {
                rx_buf[rx_len++] = ch;
            }
            /* 读到行尾 \n 可以提前退出 */
            if (ch == '\n' && rx_len > 2)
            {
                /* 继续多读50ms确保后面数据也进来 */
                continue;
            }
        }
    }

    return rx_len;
}

/**
  * @brief  发送AT命令，等待期望关键字
  * @retval 0=成功, 1=超时未收到关键字
  */
uint8_t ESP8266_SendCmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    /* 清空缓冲区 */
    rx_len = 0;
    memset(rx_buf, 0, ESP_RX_BUF_SIZE);

    /* 发送命令 */
    ESP_SendStr(cmd);
    ESP_SendStr("\r\n");

    /* 等待响应 */
    ESP_ReadBytes(timeout_ms);

    /* 检查是否包含期望关键字 */
    if (strstr(rx_buf, expect) != NULL)
        return 0;
    else
        return 1;
}

/* ==================== 工具函数 ==================== */

void ESP8266_GetRxBuf(char *buf, uint16_t *len)
{
    *len = rx_len;
    memcpy(buf, rx_buf, rx_len);
    buf[rx_len] = '\0';
}

void ESP8266_ClearRxBuf(void)
{
    memset(rx_buf, 0, ESP_RX_BUF_SIZE);
    rx_len = 0;
}
