/**
  ******************************************************************************
  * @file           : mqtt_simple.c
  * @brief          : 轻量MQTT实现 — 手动组MQTT包, 通过ESP8266 TCP发送
  *                   依赖: esp8266.h (USART1 AT指令)
  ******************************************************************************
  */

#include "mqtt_simple.h"
#include "esp8266.h"
#include "log.h"
#include <string.h>
#include <stdio.h>

/* MQTT包类型 */
#define MQTT_CONNECT    0x10
#define MQTT_PUBLISH    0x30
#define MQTT_DISCONNECT 0xE0

/* ==================== 工具函数 ==================== */

/* 写16位大端 */
static void mqtt_write16(uint8_t *buf, uint16_t val)
{
    buf[0] = (val >> 8) & 0xFF;
    buf[1] =  val       & 0xFF;
}

/* 写MQTT字符串 (2字节长度 + 数据) */
static uint8_t mqtt_write_str(uint8_t *buf, const char *str)
{
    uint16_t len = strlen(str);
    mqtt_write16(buf, len);
    memcpy(buf + 2, str, len);
    return len + 2;
}

/* 编码剩余长度 (MQTT可变长度, 最多4字节) */
static uint8_t mqtt_encode_rem_len(uint8_t *buf, uint32_t len)
{
    uint8_t i = 0;
    do {
        uint8_t byte = len % 128;
        len /= 128;
        if (len > 0) byte |= 0x80;
        buf[i++] = byte;
    } while (len > 0 && i < 4);
    return i;
}

/* ==================== MQTT CONNECT ==================== */

/**
  * @brief  构造MQTT CONNECT包
  *         [固定头 0x10] [剩余长度] [协议名] [协议级] [连接标志] [KeepAlive] [ClientID]
  */
static uint8_t mqtt_build_connect(uint8_t *packet)
{
    uint8_t *p = packet;
    uint8_t  rem_len_bytes;
    uint16_t var_len = 0;

    /* 跳过固定头, 先算可变部分长度 */
    uint8_t *var_start = p + 2;  /* 预留: 1字节类型 + 最多1字节剩余长度 */

    /* 协议名 "MQTT" */
    var_len += mqtt_write_str(var_start + var_len, "MQTT");

    /* 协议级别 = 4 (MQTT v3.1.1) */
    var_start[var_len++] = 4;

    /* 连接标志: Clean Session + no Will/User/Pass */
    var_start[var_len++] = 0x02;

    /* Keep Alive = 60秒 */
    mqtt_write16(var_start + var_len, 60);
    var_len += 2;

    /* Client ID */
    var_len += mqtt_write_str(var_start + var_len, MQTT_CLIENT_ID);

    /* 回到固定头 */
    p[0] = MQTT_CONNECT;
    rem_len_bytes = mqtt_encode_rem_len(p + 1, var_len);

    /* 总长度 = 固定头(1+rem_len_bytes) + 可变部分 */
    return 1 + rem_len_bytes + var_len;
}

/* ==================== MQTT PUBLISH ==================== */

/**
  * @brief  构造MQTT PUBLISH包 (QoS 0)
  */
static uint8_t mqtt_build_publish(uint8_t *packet, const char *topic, const char *payload)
{
    uint8_t *p = packet;
    uint8_t  rem_len_bytes;
    uint16_t topic_len = strlen(topic);
    uint16_t pay_len  = strlen(payload);
    uint16_t var_len  = 2 + topic_len + pay_len;  /* Topic长度(2B) + Topic + Payload */

    p[0] = MQTT_PUBLISH;
    rem_len_bytes = mqtt_encode_rem_len(p + 1, var_len);

    uint8_t *var = p + 1 + rem_len_bytes;
    mqtt_write16(var, topic_len);
    memcpy(var + 2, topic, topic_len);
    memcpy(var + 2 + topic_len, payload, pay_len);

    return 1 + rem_len_bytes + var_len;
}

/* ==================== 通过ESP8266 TCP发送 ==================== */

/**
  * @brief  通过 AT+CIPSEND 发送原始字节
  */
static uint8_t mqtt_send_raw(const uint8_t *data, uint16_t len)
{
    char cmd[32];
    char rx_buf[128];
    uint16_t rx_len = 0;
    uint32_t start;

    sprintf(cmd, "AT+CIPSEND=%d", len);
    if (ESP8266_SendCmd(cmd, ">", 2000) != 0)
        return 1;

    HAL_Delay(50);

    /* 发送原始数据 */
    ESP8266_ClearRxBuf();
    HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 3000);

    /* 主动轮询读取响应 (1秒超时, 不堵太久) */
    start = HAL_GetTick();
    while ((HAL_GetTick() - start) < 1000)
    {
        uint8_t ch;
        if (HAL_UART_Receive(&huart1, &ch, 1, 1) == HAL_OK)
        {
            if (rx_len < sizeof(rx_buf) - 1)
                rx_buf[rx_len++] = ch;
            if (ch == '\n')
            {
                /* 收到换行, 继续多读一会 */
                continue;
            }
        }
    }
    rx_buf[rx_len] = '\0';

    LOG_DEBUG("MQTT", "send_raw rx(%d): %.64s", rx_len, rx_buf);
    if (rx_len > 0 && strstr(rx_buf, "SEND OK") != NULL)
        return 0;

    return 2;
}

/* 连接状态 */
static uint8_t mqtt_connected = 0;

/* ==================== 公开函数 ==================== */

uint8_t MQTT_Connected(void)
{
    return mqtt_connected;
}

uint8_t MQTT_Connect(void)
{
    uint8_t packet[128];
    uint8_t pkt_len;

    /* 1. 先查WiFi模式 */
    ESP8266_SendCmd("AT+CWMODE=1", "OK", 2000);   /* Station模式 */

    /* 2. 连WiFi */
    LOG_INFO("MQTT", "Connecting WiFi...");
    {
        char rx[128]; uint16_t len;
        ESP8266_SendCmd("AT+CWJAP=\"vivo X100s\",\"123456789\"", "OK", 15000);
        HAL_Delay(3000);
        ESP8266_GetRxBuf(rx, &len);
        if (len > 0) LOG_DEBUG("MQTT", "WiFi(%d): %.64s", len, rx);
    }

    /* 3. 查IP */
    {
        char rx[128]; uint16_t len;
        ESP8266_SendCmd("AT+CIFSR", "OK", 2000);
        ESP8266_GetRxBuf(rx, &len);
        if (len > 0) LOG_INFO("MQTT", "IP: %.48s", rx);
        if (strstr(rx, "ERROR") || strstr(rx, "0.0.0.0"))
        {
            LOG_ERROR("MQTT", "WiFi FAIL (no IP)");
            return 1;
        }
    }
    LOG_INFO("MQTT", "WiFi OK");

    /* 2. TCP连Broker */
    LOG_INFO("MQTT", "Connecting TCP...");
    {
        char tcp_cmd[64];
        sprintf(tcp_cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", MQTT_BROKER_IP, MQTT_BROKER_PORT);
        if (ESP8266_SendCmd(tcp_cmd, "CONNECT", 10000) != 0)
        {
            LOG_ERROR("MQTT", "TCP connect FAIL");
            return 2;
        }
    }
    HAL_Delay(500);
    LOG_INFO("MQTT", "TCP OK");

    /* 3. MQTT CONNECT */
    pkt_len = mqtt_build_connect(packet);
    LOG_INFO("MQTT", "Sending CONNECT (%d bytes)...", pkt_len);
    if (mqtt_send_raw(packet, pkt_len) != 0)
    {
        LOG_ERROR("MQTT", "CONNECT send FAIL");
        return 3;
    }
    HAL_Delay(500);
    LOG_INFO("MQTT", "MQTT Broker connected!");
    mqtt_connected = 1;

    return 0;
}

uint8_t MQTT_Publish(const char *json)
{
    uint8_t packet[256];
    uint8_t pkt_len;

    if (!mqtt_connected) return 1;  /* 没连上, 不发 */

    pkt_len = mqtt_build_publish(packet, MQTT_TOPIC, json);

    if (mqtt_send_raw(packet, pkt_len) != 0)
    {
        LOG_WARN("MQTT", "Publish send FAIL");
        mqtt_connected = 0;  /* 标记断开 */
        return 1;
    }

    return 0;
}

void MQTT_Disconnect(void)
{
    uint8_t dis[2] = {MQTT_DISCONNECT, 0x00};
    mqtt_send_raw(dis, 2);
    ESP8266_SendCmd("AT+CIPCLOSE", "OK", 2000);
}
