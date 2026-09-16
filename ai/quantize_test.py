"""
int8量化验证: 模拟量化前后的精度对比
方法: 对称量化 scale = max(abs(w)) / 127
     w_int8 = round(w / scale)
     w_dequant = w_int8 * scale
对比float32模型和int8模拟模型在测试集上的准确率
"""

# --- 数据文件与脚本同目录，保证从任意路径运行都能找到数据 ---
import os as _os
_os.chdir(_os.path.dirname(_os.path.abspath(__file__)))

import csv
import glob
import numpy as np

# ============ 加载数据 ============
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
print(f'normal={len(normal)} loose={len(loose)} unbalance={len(unbal)}')

all_data = normal + loose + unbal
X = np.array([[d[i] for i in range(5)] for d in all_data], dtype=np.float32)
y = np.array([d[5] for d in all_data], dtype=np.int32)

# ============ 训练float32模型 ============
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import accuracy_score
from sklearn.neural_network import MLPClassifier

X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
sc = StandardScaler().fit(X_tr)
X_tr_s = sc.transform(X_tr).astype(np.float32)
X_te_s = sc.transform(X_te).astype(np.float32)

mlp = MLPClassifier(hidden_layer_sizes=(16, 8), max_iter=3000, random_state=42)
mlp.fit(X_tr_s, y_tr)
acc_f32 = accuracy_score(y_te, mlp.predict(X_te_s))
print(f'\nfloat32模型测试集准确率: {acc_f32*100:.1f}%')

# ============ 提取权重 ============
weights = mlp.coefs_
biases = mlp.intercepts_

# ============ int8量化 ============
def quantize_layer(w, b):
    """对称量化一层: 权重和偏置各自独立scale"""
    w_scale = np.max(np.abs(w)) / 127.0
    w_q = np.round(w / w_scale).astype(np.int8)
    w_dq = (w_q.astype(np.float32) * w_scale)

    b_scale = np.max(np.abs(b)) / 127.0 if np.max(np.abs(b)) > 0 else 1.0/127.0
    b_q = np.round(b / b_scale).astype(np.int8)
    b_dq = (b_q.astype(np.float32) * b_scale)
    return w_dq, b_dq, w_scale, b_scale

w1_dq, b1_dq, s1, sb1 = quantize_layer(weights[0], biases[0])
w2_dq, b2_dq, s2, sb2 = quantize_layer(weights[1], biases[1])
w3_dq, b3_dq, s3, sb3 = quantize_layer(weights[2], biases[2])

# ============ 模拟int8推理 ============
def predict_int8(X, sc):
    X_n = sc.transform(X).astype(np.float32)
    h1 = np.maximum(0, X_n @ w1_dq + b1_dq)
    h2 = np.maximum(0, h1 @ w2_dq + b2_dq)
    out = h2 @ w3_dq + b3_dq
    return np.argmax(out, axis=1)

y_pred_i8 = predict_int8(X_te, sc)
acc_i8 = accuracy_score(y_te, y_pred_i8)
print(f'int8模拟模型测试集准确率: {acc_i8*100:.1f}%')

# ============ 对比 ============
print(f'\n========== 量化对比 ==========')
print(f'float32: {acc_f32*100:.1f}%')
print(f'int8模拟: {acc_i8*100:.1f}%')
print(f'精度损失: {(acc_f32-acc_i8)*100:.2f} 个百分点')

# 存储对比
f32_bytes = sum(w.nbytes + b.nbytes for w, b in zip(weights, biases))
i8_bytes = sum(w.size * 1 + b.size * 1 for w, b in zip(weights, biases))
print(f'\n权重存储: float32={f32_bytes}B → int8={i8_bytes}B (降{f32_bytes-i8_bytes}B, {(f32_bytes-i8_bytes)/f32_bytes*100:.0f}%)')

# 量化误差
w_orig = np.concatenate([w.flatten() for w in weights])
w_deq = np.concatenate([w.flatten() for w in [w1_dq, w2_dq, w3_dq]])
max_err = np.max(np.abs(w_orig - w_deq))
print(f'最大权重误差: {max_err:.6f}')

# 保存量化信息供F407使用
np.savez(r'quant_info.npz',
         w1_scale=s1, b1_scale=sb1,
         w2_scale=s2, b2_scale=sb2,
         w3_scale=s3, b3_scale=sb3)
print('\n量化scale已保存: quant_info.npz')
