"""
复现F407端推理: 读实时Modbus数据, 用npz权重算出分类
对比F407实际显示, 找不一致原因
"""

# --- 数据文件与脚本同目录，保证从任意路径运行都能找到数据 ---
import os as _os
_os.chdir(_os.path.dirname(_os.path.abspath(__file__)))

import serial
import time
import numpy as np

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1: crc = (crc >> 1) ^ 0xA001
            else: crc >>= 1
    return crc

def read_regs(s, addr, cnt):
    req = bytes([1, 3, (addr>>8)&0xFF, addr&0xFF, (cnt>>8)&0xFF, cnt&0xFF])
    c = crc16(req)
    req += bytes([c&0xFF, (c>>8)&0xFF])
    s.reset_input_buffer()
    s.write(req)
    time.sleep(0.4)
    r = s.read(128)
    if r and len(r) >= 5 and crc16(r[:-2]) == (r[-1]<<8|r[-2]):
        bc = r[2]
        return [(r[3+i*2]<<8)|r[3+i*2+1] for i in range(bc//2)]
    return None

# 读当前数据
s = serial.Serial('COM6', 115200, timeout=0.5)
cur = None
for _ in range(5):
    base = read_regs(s, 0, 7)
    s1 = read_regs(s, 7, 8)
    s2 = read_regs(s, 15, 8)
    if base and s1 and s2:
        cur = base + s1 + s2
        break
    time.sleep(0.5)
s.close()

if not cur:
    print('FAIL: cannot read')
    exit()

# 当前寄存器值
rms_reg = cur[0]
bins_reg = cur[7:23]
print(f'寄存器RMS={rms_reg} 状态寄存器={cur[6]}')
print(f'bins={bins_reg}')

# F407端输入构建
ai_input = np.array([rms_reg] + bins_reg, dtype=np.float32)

# 加载权重
d = np.load(r'mlp_weights_final.npz')
W1, b1 = d['W1'], d['b1']
W2, b2 = d['W2'], d['b2']
W3, b3 = d['W3'], d['b3']
mean, std = d['mean'], d['std']

# 推理
x = (ai_input - mean) / std
h1 = np.maximum(0, x @ W1 + b1)
h2 = np.maximum(0, h1 @ W2 + b2)
out = h2 @ W3 + b3
pred = np.argmax(out)
print(f'\nPython推理结果: class={pred} ({["normal","loose","unbalance"][pred]})')
print(f'输出logits: {out}')

# 置信度
mx = out.max()
exp = np.exp(out - mx)
conf = exp / exp.sum()
print(f'置信度: {conf}')

# RMS门控检查
rms_g = cur[0] / 1000.0  # mG→g
print(f'\nRMS(g)={rms_g:.4f} 门控阈值=0.05 → {"停转判normal" if rms_g < 0.05 else "正常推理"}')
