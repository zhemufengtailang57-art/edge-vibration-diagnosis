/*
 * ESP32 MQTT Bridge
 * 功能: UART读取F407发来的JSON, 发布到MQTT Broker
 * 接线: ESP32 GPIO16(RX2) ← F407 PA9(TX)
 *       ESP32 GPIO17(TX2) → F407 PA10(RX)
 *       ESP32 GND           ↔ F407 GND
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>

// ============ WiFi 配置 ============
const char *WIFI_SSID     = "vivo X100s";
const char *WIFI_PASSWORD = "123456789";

// ============ MQTT 配置 ============
const char *MQTT_BROKER   = "msltese.local";   // 主机名, 永远不用改IP
const int   MQTT_PORT     = 1884;
const char *MQTT_TOPIC    = "vibration/data";

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

// ============ UART 缓冲 ============
#define RX_BUF_SIZE 512
char  rx_buf[RX_BUF_SIZE];
int   rx_pos = 0;

// ============ MQTT 重连 ============
void mqttReconnect()
{
    while (!mqtt.connected())
    {
        Serial.print("MQTT connecting...");
        if (mqtt.connect("ESP32_Vibration"))
        {
            Serial.println(" OK");
        }
        else
        {
            Serial.print(" fail, rc=");
            Serial.println(mqtt.state());
            delay(2000);
        }
    }
}

// ============ 初始化 ============
void setup()
{
    Serial.begin(115200);   // USB串口 (调试)
    Serial2.begin(115200, SERIAL_8N1, 16, 17);  // UART2 → F407 (RX=16, TX=17)

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("\nWiFi connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\nWiFi OK, IP=");
    Serial.println(WiFi.localIP());

    // mDNS: 让ESP32能解析 msltese.local
    if (MDNS.begin("esp32-vibration")) {
        Serial.println("mDNS started");
    }

    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqttReconnect();

    Serial.println("ESP32 MQTT Bridge Ready");
}

// ============ 主循环 ============
void loop()
{
    // MQTT保活
    if (!mqtt.connected())
        mqttReconnect();
    mqtt.loop();

    // UART2 → 读一行JSON (以\n结尾)
    while (Serial2.available())
    {
        char c = Serial2.read();
        if (c == '\n' || rx_pos >= RX_BUF_SIZE - 1)
        {
            rx_buf[rx_pos] = '\0';

            if (rx_pos > 0)
            {
                Serial.print("PUB: ");
                Serial.println(rx_buf);
                mqtt.publish(MQTT_TOPIC, rx_buf);
            }

            rx_pos = 0;
        }
        else
        {
            rx_buf[rx_pos++] = c;
        }
    }

    delay(10);
}
