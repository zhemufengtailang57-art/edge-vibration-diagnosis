"""
最终模型训练 V2 - 60%转速, 两螺丝全卸数据
数据: normal/loose/unbalance duty600 (2026-08-17采集)
特征: 17维 (RMS + 16bin)
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
                    rows.append([rms] + bins + [label])
                except ValueError:
                    continue
    return rows

normal = load_csv(glob.glob(r'normal_duty600_20260817*.csv')[0], 0)
loose  = load_csv(glob.glob(r'loose_duty600_20260817*.csv')[0], 1)
unbal  = load_csv(glob.glob(r'unbalance_duty600_20260817*.csv')[0], 2)
print(f'normal={len(normal)} loose={len(loose)} unbalance={len(unbal)}')

all_data = normal + loose + unbal
X = np.array([[d[i] for i in range(17)] for d in all_data], dtype=np.float32)
y = np.array([d[17] for d in all_data], dtype=np.int32)

from sklearn.model_selection import train_test_split, cross_val_score
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import accuracy_score, confusion_matrix
from sklearn.neural_network import MLPClassifier

sc = StandardScaler()
X_scaled = sc.fit_transform(X)
mlp = MLPClassifier(hidden_layer_sizes=(32, 16), max_iter=3000, random_state=42)
cv_scores = cross_val_score(mlp, X_scaled, y, cv=5)
print(f'5折交叉验证: {cv_scores.mean()*100:.1f}% ± {cv_scores.std()*100:.1f}%')

X_tr, X_te, y_tr, y_te = train_test_split(X_scaled, y, test_size=0.2, random_state=42, stratify=y)
mlp.fit(X_tr, y_tr)
acc = accuracy_score(y_te, mlp.predict(X_te))
print(f'独立测试集: {acc*100:.1f}%')

cm = confusion_matrix(y_te, mlp.predict(X_te))
print('\n混淆矩阵:')
print('        正常  松动  不平衡')
for i, name in enumerate(['正常  ', '松动  ', '不平衡']):
    print(f'{name}  {cm[i,0]:4d}  {cm[i,1]:4d}  {cm[i,2]:4d}')

np.savez(r'mlp_weights_final.npz',
         W1=mlp.coefs_[0], b1=mlp.intercepts_[0],
         W2=mlp.coefs_[1], b2=mlp.intercepts_[1],
         W3=mlp.coefs_[2], b3=mlp.intercepts_[2],
         mean=sc.mean_.astype(np.float32), std=sc.scale_.astype(np.float32))
n = mlp.coefs_[0].size + mlp.intercepts_[0].size + mlp.coefs_[1].size + mlp.intercepts_[1].size + mlp.coefs_[2].size + mlp.intercepts_[2].size
print(f'\n模型: 17->32->16->3, {n}参数')
print('权重已保存: mlp_weights_final.npz')
