"""
振动故障分类模型训练 V2 - 16维频谱特征
三种状态: 正常(0) / 螺丝松动(1) / 不平衡(2)
特征: RMS + 16个FFT频点能量 (17维)
模型对比: 逻辑回归 / MLP / 随机森林
"""

# --- 数据文件与脚本同目录，保证从任意路径运行都能找到数据 ---
import os as _os
_os.chdir(_os.path.dirname(_os.path.abspath(__file__)))

import csv
import numpy as np

def load_csv(path, label):
    rows = []
    with open(path, encoding='utf-8-sig') as f:
        reader = csv.reader(f)
        next(reader)
        for r in reader:
            if len(r) >= 23:
                try:
                    vals = [float(r[1])] + [float(x) for x in r[7:23]]  # RMS + 16 bins
                    rows.append(vals + [label])
                except ValueError:
                    continue
    return rows

print("加载数据...")
normal = load_csv(r'normal_20260815_215635.csv', 0)
loose  = load_csv(r'loose_20260815_220445.csv', 1)
unbal  = load_csv(r'unbalance_20260815_221120.csv', 2)

all_data = normal + loose + unbal
print(f'正常: {len(normal)} | 松动: {len(loose)} | 不平衡: {len(unbal)}')

X = np.array([[d[i] for i in range(17)] for d in all_data], dtype=np.float32)
y = np.array([d[17] for d in all_data], dtype=np.int32)

from sklearn.model_selection import train_test_split
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42, stratify=y)
print(f'训练: {len(X_train)} | 测试: {len(X_test)}')

mean = X_train.mean(axis=0)
std = X_train.std(axis=0) + 1e-8
X_train_n = (X_train - mean) / std
X_test_n = (X_test - mean) / std

from sklearn.metrics import accuracy_score, confusion_matrix

results = {}

from sklearn.linear_model import LogisticRegression
lr = LogisticRegression(max_iter=2000)
lr.fit(X_train_n, y_train)
results['LogisticRegression'] = accuracy_score(y_test, lr.predict(X_test_n))

from sklearn.neural_network import MLPClassifier
mlp = MLPClassifier(hidden_layer_sizes=(32, 16), max_iter=3000,
                    learning_rate_init=0.001, alpha=0.0001, random_state=42)
mlp.fit(X_train_n, y_train)
results['MLP(32-16)'] = accuracy_score(y_test, mlp.predict(X_test_n))

from sklearn.ensemble import RandomForestClassifier
rf = RandomForestClassifier(n_estimators=200, random_state=42)
rf.fit(X_train_n, y_train)
results['RandomForest'] = accuracy_score(y_test, rf.predict(X_test_n))

print('\n========== 准确率 ==========')
for name, acc in sorted(results.items(), key=lambda x: -x[1]):
    print(f'{name:20s}: {acc*100:.1f}%')

best_name = max(results, key=results.get)
best_model = {'LogisticRegression': lr, 'MLP(32-16)': mlp, 'RandomForest': rf}[best_name]
y_pred = best_model.predict(X_test_n)

print(f'\n最优: {best_name}')
print('混淆矩阵 (行=真实, 列=预测):')
print('        正常  松动  不平衡')
cm = confusion_matrix(y_test, y_pred)
for i, row in enumerate(cm):
    name = ['正常  ', '松动  ', '不平衡'][i]
    print(f'{name}  {row[0]:4d}  {row[1]:4d}  {row[2]:4d}')

# 特征重要性
print('\n特征重要性 (前5):')
feat_names = ['RMS'] + [f'bin{i}' for i in range(16)]
for feat, imp in sorted(zip(feat_names, rf.feature_importances_), key=lambda x: -x[1])[:5]:
    print(f'{feat}: {imp:.3f}')

# 保存MLP权重供F407部署 (MLP准确率100%, 部署用MLP)
np.savez(r'mlp_weights_v2.npz',
         W1=mlp.coefs_[0], b1=mlp.intercepts_[0],
         W2=mlp.coefs_[1], b2=mlp.intercepts_[1],
         W3=mlp.coefs_[2], b3=mlp.intercepts_[2],
         mean=mean, std=std)
print(f'\nMLP权重已保存: mlp_weights_v2.npz')
print(f'结构: 17输入 → 32隐藏 → 16隐藏 → 3输出')
print(f'参数量: {mlp.coefs_[0].size + mlp.coefs_[1].size + mlp.coefs_[2].size}')
print(f'MLP准确率: {results["MLP(32-16)"]*100:.1f}%')
