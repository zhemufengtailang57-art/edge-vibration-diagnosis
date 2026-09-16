"""
生成16x16中文字库C数组
从Windows字体(SimHei黑体)提取点阵, 输出到 cn_font.h
含UTF-8字节→字库索引查找表
"""
from PIL import Image, ImageDraw, ImageFont

# 需要的汉字
CHARS = "正常松动不平衡振动监测系统运行状态时间传感器温度预警故障初始化自检通过版本"

font = ImageFont.truetype("C:/Windows/Fonts/simhei.ttf", 16)

lines = []
lines.append('/*')
lines.append(' * cn_font.h - 16x16中文字库 (自动生成, SimHei黑体)')
lines.append(' * 每个字32字节, 含UTF-8字节查找表')
lines.append(' */')
lines.append('#ifndef __CN_FONT_H')
lines.append('#define __CN_FONT_H')
lines.append('')
lines.append('#include <stdint.h>')
lines.append('')
lines.append('#define CN_FONT_COUNT %d' % len(CHARS))
lines.append('')

# UTF-8字节 → 索引查找表
lines.append('/* UTF-8字节 → 字库索引 */')
lines.append('typedef struct {')
lines.append('    uint8_t b0, b1, b2;   /* UTF-8三字节 */')
lines.append('    uint8_t idx;          /* 字库索引 */')
lines.append('} CN_FontMap;')
lines.append('')
lines.append('static const CN_FontMap CN_FONT_MAP[%d] = {' % len(CHARS))
for i, ch in enumerate(CHARS):
    u8 = ch.encode('utf-8')
    lines.append(f'    {{0x{u8[0]:02X}, 0x{u8[1]:02X}, 0x{u8[2]:02X}, {i}}},  /* {ch} */')
lines.append('};')
lines.append('')

# 点阵数据
lines.append('/* 16x16点阵, 每字32字节 (行优先, 2字节/行) */')
lines.append('static const uint8_t CN_FONT[%d][32] = {' % len(CHARS))
for ch in CHARS:
    img = Image.new('1', (16, 16), 0)
    draw = ImageDraw.Draw(img)
    draw.text((0, 0), ch, font=font, fill=1)
    data = []
    for y in range(16):
        byte = 0
        for x in range(16):
            if img.getpixel((x, y)):
                byte |= 0x80 >> (x & 7)
            if x & 7 == 7:
                data.append(byte)
                byte = 0
    lines.append('    {' + ', '.join(f'0x{b:02X}' for b in data) + '},')
lines.append('};')
lines.append('')
lines.append('#endif /* __CN_FONT_H */')

with open(r'../firmware/Core/Inc/cn_font.h', 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))

print(f'生成完成: {len(CHARS)}个汉字')
