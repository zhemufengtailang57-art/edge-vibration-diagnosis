/**
  ******************************************************************************
  * @file           : max3485.c
  * @brief          : MAX3485 RS485驱动
  *                   引脚: PD8=TX, PB11=RX, PC8=DE(发送使能=RE反相)
  *                   模式: DE=1 → 发送, DE=0 → 接收 (RO使能)
  *                   半双工: 默认接收, 发送时切发送模式, 发完立即切回接收
  ******************************************************************************
  */

#include "max3485.h"

/* ==================== 初始化 ==================== */

uint8_t RS485_Init(void)
{
    /* 默认进入接收模式 */
    RS485_SetRx();

    /* 发一条测试数据 (需要RS485另一端设备才能看到) */
    uint8_t test[] = "RS485 Init OK\r\n";
    RS485_Send(test, sizeof(test) - 1);

    return 0;
}

/* ==================== 收发控制 ==================== */

void RS485_SetTx(void)
{
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_SET);   /* DE=HIGH → 发送 */
}

void RS485_SetRx(void)
{
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET); /* DE=LOW  → 接收 */
}

/* ==================== 数据收发 ==================== */

void RS485_Send(uint8_t *buf, uint16_t len)
{
    RS485_SetTx();
    HAL_Delay(1);  /* 等待DE稳定 (MAX3485切换时间~100ns, 1ms保守) */

    HAL_UART_Transmit(&huart3, buf, len, 1000);

    /* 必须等TX完成寄存器(TXC)清零, 否则最后几个字节发不出去就切回接收了 */
    while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) == RESET);

    RS485_SetRx();  /* 发完立刻切回接收, 否则收不到应答 */
}

uint16_t RS485_Recv(uint8_t *buf, uint16_t max_len, uint32_t timeout_ms)
{
    uint16_t rx_len = 0;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        uint8_t ch;
        if (HAL_UART_Receive(&huart3, &ch, 1, 1) == HAL_OK)
        {
            if (rx_len < max_len)
            {
                buf[rx_len++] = ch;
            }
        }
    }

    return rx_len;
}
