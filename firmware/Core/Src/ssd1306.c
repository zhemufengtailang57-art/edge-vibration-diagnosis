/**
  ******************************************************************************
  * @file           : ssd1306.c
  * @brief          : SSD1306 OLED 128x64 I2C驱动（帧缓冲区模式）
  *                   适用: 0.96寸 OLED, 控制器SSD1306, I2C地址0x3C
  *                   字库: 6x8 ASCII（仅可打印字符 0x20-0x7E）
  ******************************************************************************
  */

#include "ssd1306.h"
#include "cn_font.h"

/* ==================== 6x8 ASCII字库 ==================== */
static const uint8_t F6x8[][6] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // space
    { 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00 },   // !
    { 0x00, 0x07, 0x00, 0x07, 0x00, 0x00 },   // "
    { 0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00 },   // #
    { 0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00 },   // $
    { 0x23, 0x13, 0x08, 0x64, 0x62, 0x00 },   // %
    { 0x36, 0x49, 0x55, 0x22, 0x50, 0x00 },   // &
    { 0x00, 0x05, 0x03, 0x00, 0x00, 0x00 },   // '
    { 0x00, 0x1C, 0x22, 0x41, 0x00, 0x00 },   // (
    { 0x00, 0x41, 0x22, 0x1C, 0x00, 0x00 },   // )
    { 0x08, 0x2A, 0x1C, 0x2A, 0x08, 0x00 },   // *
    { 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00 },   // +
    { 0x00, 0x50, 0x30, 0x00, 0x00, 0x00 },   // ,
    { 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 },   // -
    { 0x00, 0x60, 0x60, 0x00, 0x00, 0x00 },   // .
    { 0x20, 0x10, 0x08, 0x04, 0x02, 0x00 },   // /
    { 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00 },   // 0
    { 0x00, 0x42, 0x7F, 0x40, 0x00, 0x00 },   // 1
    { 0x42, 0x61, 0x51, 0x49, 0x46, 0x00 },   // 2
    { 0x21, 0x41, 0x45, 0x4B, 0x31, 0x00 },   // 3
    { 0x18, 0x14, 0x12, 0x7F, 0x10, 0x00 },   // 4
    { 0x27, 0x45, 0x45, 0x45, 0x39, 0x00 },   // 5
    { 0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00 },   // 6
    { 0x01, 0x71, 0x09, 0x05, 0x03, 0x00 },   // 7
    { 0x36, 0x49, 0x49, 0x49, 0x36, 0x00 },   // 8
    { 0x06, 0x49, 0x49, 0x29, 0x1E, 0x00 },   // 9
    { 0x00, 0x36, 0x36, 0x00, 0x00, 0x00 },   // :
    { 0x00, 0x56, 0x36, 0x00, 0x00, 0x00 },   // ;
    { 0x00, 0x08, 0x14, 0x22, 0x41, 0x00 },   // <
    { 0x14, 0x14, 0x14, 0x14, 0x14, 0x00 },   // =
    { 0x41, 0x22, 0x14, 0x08, 0x00, 0x00 },   // >
    { 0x02, 0x01, 0x51, 0x09, 0x06, 0x00 },   // ?
    { 0x32, 0x49, 0x79, 0x41, 0x3E, 0x00 },   // @
    { 0x7E, 0x11, 0x11, 0x11, 0x7E, 0x00 },   // A
    { 0x7F, 0x49, 0x49, 0x49, 0x36, 0x00 },   // B
    { 0x3E, 0x41, 0x41, 0x41, 0x22, 0x00 },   // C
    { 0x7F, 0x41, 0x41, 0x22, 0x1C, 0x00 },   // D
    { 0x7F, 0x49, 0x49, 0x49, 0x41, 0x00 },   // E
    { 0x7F, 0x09, 0x09, 0x01, 0x01, 0x00 },   // F
    { 0x3E, 0x41, 0x41, 0x51, 0x32, 0x00 },   // G
    { 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00 },   // H
    { 0x00, 0x41, 0x7F, 0x41, 0x00, 0x00 },   // I
    { 0x20, 0x40, 0x41, 0x3F, 0x01, 0x00 },   // J
    { 0x7F, 0x08, 0x14, 0x22, 0x41, 0x00 },   // K
    { 0x7F, 0x40, 0x40, 0x40, 0x40, 0x00 },   // L
    { 0x7F, 0x02, 0x04, 0x02, 0x7F, 0x00 },   // M
    { 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00 },   // N
    { 0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00 },   // O
    { 0x7F, 0x09, 0x09, 0x09, 0x06, 0x00 },   // P
    { 0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00 },   // Q
    { 0x7F, 0x09, 0x19, 0x29, 0x46, 0x00 },   // R
    { 0x46, 0x49, 0x49, 0x49, 0x31, 0x00 },   // S
    { 0x01, 0x01, 0x7F, 0x01, 0x01, 0x00 },   // T
    { 0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00 },   // U
    { 0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00 },   // V
    { 0x7F, 0x20, 0x18, 0x20, 0x7F, 0x00 },   // W
    { 0x63, 0x14, 0x08, 0x14, 0x63, 0x00 },   // X
    { 0x03, 0x04, 0x78, 0x04, 0x03, 0x00 },   // Y
    { 0x61, 0x51, 0x49, 0x45, 0x43, 0x00 },   // Z
    { 0x00, 0x00, 0x7F, 0x41, 0x41, 0x00 },   // [
    { 0x02, 0x04, 0x08, 0x10, 0x20, 0x00 },   /* \ */
    { 0x41, 0x41, 0x7F, 0x00, 0x00, 0x00 },   // ]
    { 0x04, 0x02, 0x01, 0x02, 0x04, 0x00 },   // ^
    { 0x40, 0x40, 0x40, 0x40, 0x40, 0x00 },   // _
    { 0x00, 0x01, 0x02, 0x04, 0x00, 0x00 },   // `
    { 0x20, 0x54, 0x54, 0x54, 0x78, 0x00 },   // a
    { 0x7F, 0x48, 0x44, 0x44, 0x38, 0x00 },   // b
    { 0x38, 0x44, 0x44, 0x44, 0x20, 0x00 },   // c
    { 0x38, 0x44, 0x44, 0x48, 0x7F, 0x00 },   // d
    { 0x38, 0x54, 0x54, 0x54, 0x18, 0x00 },   // e
    { 0x08, 0x7E, 0x09, 0x01, 0x02, 0x00 },   // f
    { 0x08, 0x14, 0x54, 0x54, 0x3C, 0x00 },   // g
    { 0x7F, 0x08, 0x04, 0x04, 0x78, 0x00 },   // h
    { 0x00, 0x44, 0x7D, 0x40, 0x00, 0x00 },   // i
    { 0x20, 0x40, 0x44, 0x3D, 0x00, 0x00 },   // j
    { 0x00, 0x7F, 0x10, 0x28, 0x44, 0x00 },   // k
    { 0x00, 0x41, 0x7F, 0x40, 0x00, 0x00 },   // l
    { 0x7C, 0x04, 0x18, 0x04, 0x78, 0x00 },   // m
    { 0x7C, 0x08, 0x04, 0x04, 0x78, 0x00 },   // n
    { 0x38, 0x44, 0x44, 0x44, 0x38, 0x00 },   // o
    { 0x7C, 0x14, 0x14, 0x14, 0x08, 0x00 },   // p
    { 0x08, 0x14, 0x14, 0x18, 0x7C, 0x00 },   // q
    { 0x7C, 0x08, 0x04, 0x04, 0x08, 0x00 },   // r
    { 0x48, 0x54, 0x54, 0x54, 0x20, 0x00 },   // s
    { 0x04, 0x3F, 0x44, 0x40, 0x20, 0x00 },   // t
    { 0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00 },   // u
    { 0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00 },   // v
    { 0x3C, 0x40, 0x30, 0x40, 0x3C, 0x00 },   // w
    { 0x44, 0x28, 0x10, 0x28, 0x44, 0x00 },   // x
    { 0x0C, 0x50, 0x50, 0x50, 0x3C, 0x00 },   // y
    { 0x44, 0x64, 0x54, 0x4C, 0x44, 0x00 },   // z
    { 0x00, 0x08, 0x36, 0x41, 0x00, 0x00 },   // {
    { 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00 },   // |
    { 0x00, 0x41, 0x36, 0x08, 0x00, 0x00 },   // }
    { 0x08, 0x04, 0x08, 0x10, 0x08, 0x00 },   // ~
};

/* ==================== 帧缓冲区 ==================== */
static uint8_t SSD1306_Buffer[SSD1306_WIDTH * SSD1306_PAGES];  /* 128×8 = 1024 bytes */

/* ==================== 底层I2C写 ==================== */
static void SSD1306_WriteCmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};  /* Co=0, D/C#=0 → 命令 */
    HAL_I2C_Master_Transmit(&SSD1306_I2C_PORT, SSD1306_ADDR, buf, 2, 100);
}

static void SSD1306_WriteData(uint8_t data)
{
    uint8_t buf[2] = {0x40, data};  /* Co=0, D/C#=1 → 数据 */
    HAL_I2C_Master_Transmit(&SSD1306_I2C_PORT, SSD1306_ADDR, buf, 2, 100);
}

/* ==================== 初始化序列 ==================== */
void SSD1306_Init(void)
{
    HAL_Delay(100);  /* 等OLED上电稳定 */

    SSD1306_WriteCmd(0xAE);  /* Display OFF */

    SSD1306_WriteCmd(0xD5);  /* Set Oscillator Frequency */
    SSD1306_WriteCmd(0x80);

    SSD1306_WriteCmd(0xA8);  /* Set MUX Ratio */
    SSD1306_WriteCmd(0x3F);  /* 64 lines */

    SSD1306_WriteCmd(0xD3);  /* Set Display Offset */
    SSD1306_WriteCmd(0x00);

    SSD1306_WriteCmd(0x40);  /* Set Display Start Line = 0 */

    SSD1306_WriteCmd(0x8D);  /* Charge Pump */
    SSD1306_WriteCmd(0x14);  /* Enable (0x14=开, 0x10=外供) */

    SSD1306_WriteCmd(0x20);  /* Memory Addressing Mode */
    SSD1306_WriteCmd(0x00);  /* Horizontal */

    SSD1306_WriteCmd(0xA1);  /* Segment Remap (左右镜像) */
    SSD1306_WriteCmd(0xC8);  /* COM Output Scan Direction (上下镜像) */

    SSD1306_WriteCmd(0xDA);  /* COM Pins Hardware Config */
    SSD1306_WriteCmd(0x12);

    SSD1306_WriteCmd(0x81);  /* Contrast */
    SSD1306_WriteCmd(0xCF);

    SSD1306_WriteCmd(0xD9);  /* Pre-charge Period */
    SSD1306_WriteCmd(0xF1);

    SSD1306_WriteCmd(0xDB);  /* VCOMH Deselect */
    SSD1306_WriteCmd(0x40);

    SSD1306_WriteCmd(0xA4);  /* Entire Display ON → Resume to RAM */
    SSD1306_WriteCmd(0xA6);  /* Normal Display (非反白) */

    SSD1306_WriteCmd(0x2E);  /* Deactivate scroll */

    SSD1306_WriteCmd(0xAF);  /* Display ON */

    SSD1306_Clear();
    SSD1306_Refresh();
}

/* ==================== 清空帧缓冲区 ==================== */
void SSD1306_Clear(void)
{
    memset(SSD1306_Buffer, 0x00, sizeof(SSD1306_Buffer));
}

/* ==================== 全屏填充 ==================== */
void SSD1306_Fill(uint8_t color)
{
    memset(SSD1306_Buffer, color ? 0xFF : 0x00, sizeof(SSD1306_Buffer));
}

/* ==================== 画点 ==================== */
void SSD1306_SetPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;

    if (color)
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= (1 << (y % 8));
    else
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
}

/* ==================== 刷新：帧缓冲区→OLED ==================== */
void SSD1306_Refresh(void)
{
    for (uint8_t page = 0; page < SSD1306_PAGES; page++)
    {
        SSD1306_WriteCmd(0xB0 + page);   /* Set Page */
        SSD1306_WriteCmd(0x00);           /* Set Low Column = 0 */
        SSD1306_WriteCmd(0x10);           /* Set High Column = 0 */

        /* 逐字节写（I2C不支持多字节Data模式，用单字节循环） */
        for (uint16_t col = 0; col < SSD1306_WIDTH; col++)
        {
            SSD1306_WriteData(SSD1306_Buffer[page * SSD1306_WIDTH + col]);
        }
    }
}

/* ==================== 显示字符 ==================== */
void SSD1306_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t size)
{
    uint8_t i, j;

    if (ch < ' ' || ch > '~') ch = ' ';  /* 不可打印字符替换为空格 */

    for (i = 0; i < 6; i++)
    {
        uint8_t line;
        if (size == 1)
        {
            line = F6x8[ch - ' '][i];
            for (j = 0; j < 8; j++)
            {
                if (line & (1 << j))
                    SSD1306_SetPixel(x + i, y + j, 1);
                else
                    SSD1306_SetPixel(x + i, y + j, 0);
            }
        }
        else  /* 放大倍数 >1 */
        {
            line = F6x8[ch - ' '][i];
            for (j = 0; j < 8; j++)
            {
                uint8_t bit = (line & (1 << j)) ? 1 : 0;
                for (uint8_t dy = 0; dy < size; dy++)
                {
                    for (uint8_t dx = 0; dx < size; dx++)
                    {
                        SSD1306_SetPixel(x + i * size + dx, y + j * size + dy, bit);
                    }
                }
            }
        }
    }
}

/* ==================== 显示字符串 ==================== */
void SSD1306_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size)
{
    while (*str)
    {
        SSD1306_ShowChar(x, y, *str, size);
        x += 6 * size;  /* 6是字宽，size是放大倍数 */

        if (x + 6 * size > SSD1306_WIDTH)
        {
            x = 0;           /* 换行 */
            y += 8 * size;
        }
        if (y + 8 * size > SSD1306_HEIGHT)
        {
            break;           /* 超出屏幕，停止 */
        }
        str++;
    }
}

/* ==================== 显示数字 ==================== */
void SSD1306_ShowNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size)
{
    char buf[12];
    uint8_t i;

    /* 负数处理 */
    if (num < 0)
    {
        SSD1306_ShowChar(x, y, '-', size);
        num = -num;
        x += 6 * size;
        len--;
    }

    /* 数字转字符串 */
    for (i = 0; i < len; i++)
    {
        buf[len - 1 - i] = (num % 10) + '0';
        num /= 10;
    }
    buf[len] = '\0';

    SSD1306_ShowString(x, y, buf, size);
}

/* ==================== 显示中文 (16x16点阵) ==================== */
void SSD1306_ShowCN(uint8_t x, uint8_t y, const char *str)
{
    uint8_t i, j, k;
    uint8_t cx = x;

    while (*str)
    {
        /* UTF-8解码: 中文字符3字节 */
        uint8_t b0 = (uint8_t)*str;
        if (b0 < 0x80)
        {
            /* ASCII字符交给普通显示 */
            SSD1306_ShowChar(cx, y, (char)b0, 1);
            cx += 6;
            str++;
            continue;
        }

        uint8_t b1 = (uint8_t)str[1];
        uint8_t b2 = (uint8_t)str[2];

        /* 查找字库 */
        int8_t idx = -1;
        for (i = 0; i < CN_FONT_COUNT; i++)
        {
            if (CN_FONT_MAP[i].b0 == b0 && CN_FONT_MAP[i].b1 == b1 && CN_FONT_MAP[i].b2 == b2)
            {
                idx = (int8_t)CN_FONT_MAP[i].idx;
                break;
            }
        }

        if (idx >= 0 && cx + 16 <= SSD1306_WIDTH)
        {
            /* 16行 x 2字节/行 */
            for (k = 0; k < 16; k++)
            {
                uint8_t left  = CN_FONT[idx][k * 2];
                uint8_t right = CN_FONT[idx][k * 2 + 1];

                for (j = 0; j < 8; j++)
                {
                    if (left & (0x80 >> j))
                        SSD1306_SetPixel(cx + j, y + k, 1);
                    else
                        SSD1306_SetPixel(cx + j, y + k, 0);

                    if (right & (0x80 >> j))
                        SSD1306_SetPixel(cx + 8 + j, y + k, 1);
                    else
                        SSD1306_SetPixel(cx + 8 + j, y + k, 0);
                }
            }
            cx += 16;
        }
        else
        {
            /* 未找到的字显示为方块 */
            for (k = 0; k < 16; k++)
            {
                for (j = 0; j < 16; j++)
                    SSD1306_SetPixel(cx + j, y + k, 1);
            }
            cx += 16;
        }

        str += 3;  /* 跳过3字节 */
    }
}
