"""
当前模型(60%训练, 5特征)在其他转速下的表现
用8/16采集的15组数据测试
"""

# --- 数据文件与脚本同目录，保证从任意路径运行都能找到数据 ---
import os as _os
_os.chdir(_os.path.dirname(_os.path.abspath(__file__)))

import csv
import glob
import numpy as np
from sklearn.preprocessing import StandardScaler
from sklearn.neural_network import MLPClassifier

def load(path, label):
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

# 用8/17新采的60%数据训练 (跟部署模型一致)
normal60 = load(glob.glob(r'normal_duty600_20260817*.csv')[0], 0)
loose60  = load(glob.glob(r'loose_duty600_20260817*.csv')[0], 1)
unbal60  = load(glob.glob(r'unbalance_duty600_20260817*.csv')[0], 2)
train_data = normal60 + loose60 + unbal60
X_tr = np.array([[d[i] for i in range(5)] for d in train_data], dtype=np.float32)
y_tr = np.array([d[5] for d in train_data], dtype=np.int32)

sc = StandardScaler().fit(X_tr)
mlp = MLPClassifier(hidden_layer_sizes=(16, 8), max_iter=3000, random_state=42)
mlp.fit(sc.transform(X_tr), y_tr)

# 测其他转速(8/16数据)
print('当前模型(60%训练)在其他转速的表现:')
print(f'{"":12s} {"60%":>8s} {"70%":>8s} {"80%":>8s} {"90%":>8s} {"99%":>8s}')
for state, label in [('normal',0), ('loose',1), ('unbalance',2)]:
    row = []
    for duty in [600, 700, 800, 900, 999]:
        files = glob.glob(rf'{state}_duty{duty}_2026081[67]*.csv')
        if not files:
            row.append('   -')
            continue
        data = load(files[0], label)
        X_te = np.array([[d[i] for i in range(5)] for d in data], dtype=np.float32)
        y_te = np.array([d[5] for d in data], dtype=np.int32)
        pred = mlp.predict(sc.transform(X_te))
        acc = (pred == y_te).mean() * 100
        row.append(f'{acc:6.0f}%')
    print(f'{state:12s}' + ''.join(row))
