"""
简化模型: RMS + 4个关键频点 (bin1/2/6/7 = 7.8/15.6/46.9/54.7Hz)
松动状态频谱不稳, 用少量鲁棒特征
"""

# --- 数据文件与脚本同目录，保证从任意路径运行都能找到数据 ---
import os as _os
_os.chdir(_os.path.dirname(_os.path.abspath(__file__)))

import csv
import glob
import numpy as np

def load_csv(path, label):
    rows = []
    with open(path, encoding='utf-8-sig') as f:
        reader = csv.reader(f)
        next(reader)
        for r in reader:
            if len(r) >= 23:
                try:
                    rms = float(r[1])
                    bins = [float(x) for x in r[7:23]]
                    # 只取4个关键频点
                    feat = [rms, bins[1], bins[2], bins[6], bins[7]]
                    rows.append(feat + [label])
                except ValueError:
                    continue
    return rows

normal = load_csv(glob.glob(r'normal_duty600_20260817*.csv')[0], 0)
loose  = load_csv(glob.glob(r'loose_duty600_20260817*.csv')[0], 1)
unbal  = load_csv(glob.glob(r'unbalance_duty600_20260817*.csv')[0], 2)
print(f'normal={len(normal)} loose={len(loose)} unbalance={len(unbal)}')

all_data = normal + loose + unbal
X = np.array([[d[i] for i in range(5)] for d in all_data], dtype=np.float32)
y = np.array([d[5] for d in all_data], dtype=np.int32)

from sklearn.model_selection import train_test_split, cross_val_score
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import accuracy_score, confusion_matrix
from sklearn.neural_network import MLPClassifier

sc = StandardScaler()
X_scaled = sc.fit_transform(X)
mlp = MLPClassifier(hidden_layer_sizes=(16, 8), max_iter=3000, random_state=42)
cv_scores = cross_val_score(mlp, X_scaled, y, cv=5)
print(f'5折交叉验证: {cv_scores.mean()*100:.1f}% ± {cv_scores.std()*100:.1f}%')

X_tr, X_te, y_tr, y_te = train_test_split(X_scaled, y, test_size=0.2, random_state=42, stratify=y)
mlp.fit(X_tr, y_tr)
print(f'独立测试: {accuracy_score(y_te, mlp.predict(X_te))*100:.1f}%')
cm = confusion_matrix(y_te, mlp.predict(X_te))
print('\n混淆矩阵:')
print('        正常  松动  不平衡')
for i, name in enumerate(['正常  ', '松动  ', '不平衡']):
    print(f'{name}  {cm[i,0]:4d}  {cm[i,1]:4d}  {cm[i,2]:4d}')

# 保存
np.savez(r'mlp_simple.npz',
         W1=mlp.coefs_[0], b1=mlp.intercepts_[0],
         W2=mlp.coefs_[1], b2=mlp.intercepts_[1],
         W3=mlp.coefs_[2], b3=mlp.intercepts_[2],
         mean=sc.mean_.astype(np.float32), std=sc.scale_.astype(np.float32))
n = sum(c.size for c in mlp.coefs_) + sum(i.size for i in mlp.intercepts_)
print(f'\n模型: 5->16->8->3, {n}参数')
print('已保存: mlp_simple.npz')

# 用当前实时数据验证
print('\n=== 当前实时数据验证 ===')
import serial, time

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
for attempt in range(10):
    base = read_regs(s, 0, 7)
    s1 = read_regs(s, 7, 8)
    s2 = read_regs(s, 15, 8)
    if base and s1 and s2:
        cur = base + s1 + s2
        rms = cur[0]
        bins = cur[7:23]
        feat = np.array([rms, bins[1], bins[2], bins[6], bins[7]], dtype=np.float32)
        x = (feat - sc.mean_) / sc.scale_
        out = mlp.predict_proba(x.reshape(1,-1))[0]
        pred = mlp.predict(x.reshape(1,-1))[0]
        names = ['normal','loose','unbalance']
        print(f'#{attempt+1}: RMS={rms} → {names[pred]} (conf={out.max():.0%})')
        break
    time.sleep(0.3)
s.close()
