"""
实时对比: 当前F407频谱 vs 训练时90%转速数据
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

s = serial.Serial('COM6', 115200, timeout=0.5)

# 当前数据
cur_bins = None
for _ in range(5):
    spec1 = read_regs(s, 7, 8)
    spec2 = read_regs(s, 15, 8)
    if spec1 and spec2:
        cur_bins = spec1 + spec2
        break
    time.sleep(0.5)

if not cur_bins:
    print('FAIL: cannot read spectrum')
    exit()

print('当前频谱:', cur_bins)

# 加载训练数据 (90%转速)
import csv, glob
def load(path):
    bins_list = []
    with open(path, encoding='utf-8-sig') as f:
        reader = csv.reader(f)
        next(reader)
        for r in reader:
            if len(r) >= 23:
                bins_list.append([float(x) for x in r[7:23]])
    return np.array(bins_list)

print('\n训练数据对比 (60%转速, 8月17日新数据):')
for name in ['normal', 'loose', 'unbalance']:
    files = glob.glob(rf'{name}_duty600_20260817*.csv')
    if not files: continue
    data = load(files[0])
    mean_bins = data.mean(axis=0)
    # 计算与当前频谱的欧氏距离
    dist = np.linalg.norm(np.array(cur_bins) - mean_bins)
    print(f'{name}: 距离={dist:.0f}')
    print(f'  训练均值: {[int(b) for b in mean_bins]}')
    print(f'  当前值:   {cur_bins}')

s.close()
