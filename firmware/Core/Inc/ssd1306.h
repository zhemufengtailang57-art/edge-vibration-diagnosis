/**
  ******************************************************************************
  * @file           : ssd1306.h
  * @brief          : SSD1306 OLED 0.96寸 128x64 I2C驱动头文件
  ******************************************************************************
  */

#ifndef __SSD1306_H
#define __SSD1306_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <string.h>

/* 引用CubeMX生成的I2C句柄 */
extern I2C_HandleTypeDef hi2c1;

/* ==================== 用户配置 ==================== */
#define SSD1306_I2C_PORT      hi2c1             /* I2C句柄 */
#define SSD1306_ADDR          0x78              /* I2C地址: 0x3C左移1位=0x78 */
#define SSD1306_WIDTH         128
#define SSD1306_HEIGHT        64
#define SSD1306_PAGES         (SSD1306_HEIGHT / 8)  /* 8页 */

/* ==================== API ==================== */
void    SSD1306_Init(void);
void    SSD1306_Clear(void);
void    SSD1306_Refresh(void);
void    SSD1306_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t size);
void    SSD1306_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size);
void    SSD1306_ShowNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size);
void    SSD1306_SetPixel(uint8_t x, uint8_t y, uint8_t color);
void    SSD1306_Fill(uint8_t color);

/**
  * @brief  显示16x16中文字符串
  * @param  x: 列坐标(0-127)
  * @param  y: 行坐标(0-7, 每行8像素高)
  * @param  str: 中文UTF-8字符串, 每个字占16像素宽
  */
void    SSD1306_ShowCN(uint8_t x, uint8_t y, const char *str);

#endif /* __SSD1306_H */
