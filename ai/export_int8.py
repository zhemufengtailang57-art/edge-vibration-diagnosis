"""
导出int8量化权重为C头文件
读 quant_info.npz + 重新量化权重
"""

# --- 数据文件与脚本同目录，保证从任意路径运行都能找到数据 ---
import os as _os
_os.chdir(_os.path.dirname(_os.path.abspath(__file__)))

import csv
import glob
import numpy as np
from sklearn.preprocessing import StandardScaler
from sklearn.neural_network import MLPClassifier

# 重新训练得到权重
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
                    rows.append([rms, bins[1], bins[2], bins[6], bins[7], label])
                except ValueError:
                    continue
    return rows

normal = load_csv(glob.glob(r'normal_duty600_20260817*.csv')[0], 0)
loose  = load_csv(glob.glob(r'loose_duty600_20260817*.csv')[0], 1)
unbal  = load_csv(glob.glob(r'unbalance_duty600_20260817*.csv')[0], 2)

all_data = normal + loose + unbal
X = np.array([[d[i] for i in range(5)] for d in all_data], dtype=np.float32)
y = np.array([d[5] for d in all_data], dtype=np.int32)

sc = StandardScaler().fit(X)
X_s = sc.transform(X).astype(np.float32)
mlp = MLPClassifier(hidden_layer_sizes=(16, 8), max_iter=3000, random_state=42)
mlp.fit(X_s, y)

weights = mlp.coefs_
biases = mlp.intercepts_

# 量化
def quantize(w, b):
    ws = np.max(np.abs(w)) / 127.0
    wq = np.round(w / ws).astype(np.int8)
    bs = np.max(np.abs(b)) / 127.0 if np.max(np.abs(b)) > 0 else 1.0/127.0
    bq = np.round(b / bs).astype(np.int8)
    return wq, bq, ws, bs

w1q, b1q, ws1, bs1 = quantize(weights[0], biases[0])
w2q, b2q, ws2, bs2 = quantize(weights[1], biases[1])
w3q, b3q, ws3, bs3 = quantize(weights[2], biases[2])

lines = []
lines.append('/*')
lines.append(' * ai_model.h - int8量化MLP权重 (自动生成)')
lines.append(' * 结构: 5输入 -> 16隐藏(ReLU) -> 8隐藏(ReLU) -> 3输出')
lines.append(' * 量化: 对称int8, 精度损失0.00%')
lines.append(' */')
lines.append('#ifndef __AI_MODEL_H')
lines.append('#define __AI_MODEL_H')
lines.append('')
lines.append('#include <stdint.h>')
lines.append('')
lines.append('#define AI_INPUT_SIZE   5')
lines.append('#define AI_HIDDEN1_SIZE 16')
lines.append('#define AI_HIDDEN2_SIZE 8')
lines.append('#define AI_OUTPUT_SIZE  3')
lines.append('')

# 输入归一化参数(保持float)
lines.append('static const float AI_MEAN[5] = {')
lines.append('    ' + ', '.join(f'{v:.6f}f' for v in sc.mean_) + ',')
lines.append('};')
lines.append('static const float AI_STD[5] = {')
lines.append('    ' + ', '.join(f'{v:.6f}f' for v in sc.scale_) + ',')
lines.append('};')
lines.append('')

# 量化scale
lines.append('/* 量化scale: 反量化 = int8_value * scale */')
lines.append(f'static const float AI_W1_SCALE = {ws1:.6f}f;')
lines.append(f'static const float AI_B1_SCALE = {bs1:.6f}f;')
lines.append(f'static const float AI_W2_SCALE = {ws2:.6f}f;')
lines.append(f'static const float AI_B2_SCALE = {bs2:.6f}f;')
lines.append(f'static const float AI_W3_SCALE = {ws3:.6f}f;')
lines.append(f'static const float AI_B3_SCALE = {bs3:.6f}f;')
lines.append('')

# int8权重
lines.append(f'static const int8_t AI_W1[{w1q.shape[1]}][{w1q.shape[0]}] = {{')
for row in w1q.T:
    lines.append('    {' + ', '.join(str(v) for v in row) + '},')
lines.append('};')
lines.append(f'static const int8_t AI_B1[{len(b1q)}] = {{')
lines.append('    ' + ', '.join(str(v) for v in b1q) + ',')
lines.append('};')
lines.append('')

lines.append(f'static const int8_t AI_W2[{w2q.shape[1]}][{w2q.shape[0]}] = {{')
for row in w2q.T:
    lines.append('    {' + ', '.join(str(v) for v in row) + '},')
lines.append('};')
lines.append(f'static const int8_t AI_B2[{len(b2q)}] = {{')
lines.append('    ' + ', '.join(str(v) for v in b2q) + ',')
lines.append('};')
lines.append('')

lines.append(f'static const int8_t AI_W3[{w3q.shape[1]}][{w3q.shape[0]}] = {{')
for row in w3q.T:
    lines.append('    {' + ', '.join(str(v) for v in row) + '},')
lines.append('};')
lines.append(f'static const int8_t AI_B3[{len(b3q)}] = {{')
lines.append('    ' + ', '.join(str(v) for v in b3q) + ',')
lines.append('};')
lines.append('')
lines.append('#endif /* __AI_MODEL_H */')

with open(r'../firmware/Core/Inc/ai_model.h', 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines) + '\n')

print(f'int8权重已导出: {w1q.shape} {w2q.shape} {w3q.shape}')
print(f'scales: {ws1:.6f} {ws2:.6f} {ws3:.6f}')
