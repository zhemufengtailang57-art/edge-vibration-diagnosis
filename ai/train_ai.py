"""
振动故障分类模型训练
三种状态: 正常(0) / 螺丝松动(1) / 不平衡(2)
特征: RMS(mG), 频率(Hz), 幅值(mG)
模型对比: 逻辑回归 / MLP神经网络 / LightGBM
输出: 最优模型权重(供F407 C语言部署)
"""
import csv
import numpy as np
from collections import Counter

# ==================== 1. 加载数据 ====================

def load_csv(path, label):
    rows = []
    with open(path, encoding='utf-8-sig', errors='ignore') as f:
        reader = csv.reader(f)
        next(reader)  # 表头
        for r in reader:
            if len(r) >= 7:
                try:
                    rms = float(r[1])
                    freq = float(r[2])
                    amp = float(r[3])
                    if rms > 0 or freq > 0:  # 跳过静止帧
                        rows.append([rms, freq, amp, label])
                except ValueError:
                    continue
    return rows

print("加载数据...")
normal = load_csv(r'新正常.csv', 0)
loose  = load_csv(r'新螺丝松动.csv', 1)
unbal  = load_csv(r'新不平衡.csv', 2)

all_data = normal + loose + unbal
print(f'正常: {len(normal)} 条 | 松动: {len(loose)} 条 | 不平衡: {len(unbal)} 条')

X = np.array([[d[0], d[1], d[2]] for d in all_data], dtype=np.float32)
y = np.array([d[3] for d in all_data], dtype=np.int32)

# ==================== 2. 数据划分 ====================

from sklearn.model_selection import train_test_split
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42, stratify=y)
print(f'\n训练集: {len(X_train)} | 测试集: {len(X_test)}')

# 归一化
mean = X_train.mean(axis=0)
std = X_train.std(axis=0)
X_train_n = (X_train - mean) / std
X_test_n = (X_test - mean) / std

# ==================== 3. 模型对比 ====================

from sklearn.metrics import accuracy_score, confusion_matrix, classification_report

results = {}

# --- 模型A: 逻辑回归(基线) ---
from sklearn.linear_model import LogisticRegression
lr = LogisticRegression(max_iter=500)
lr.fit(X_train_n, y_train)
results['逻辑回归'] = accuracy_score(y_test, lr.predict(X_test_n))

# --- 模型B: MLP神经网络 ---
from sklearn.neural_network import MLPClassifier
mlp = MLPClassifier(hidden_layer_sizes=(8,), max_iter=5000,
                    learning_rate_init=0.01, alpha=0.0001,
                    random_state=42)
mlp.fit(X_train_n, y_train)
results['MLP(8)'] = accuracy_score(y_test, mlp.predict(X_test_n))

# --- 模型C: LightGBM ---
try:
    import lightgbm as lgb
    lgbm = lgb.LGBMClassifier(n_estimators=100, random_state=42, verbosity=-1)
    lgbm.fit(X_train_n, y_train)
    results['LightGBM'] = accuracy_score(y_test, lgbm.predict(X_test_n))
except ImportError:
    print('LightGBM 未安装, 跳过')

# --- 模型D: 随机森林 ---
from sklearn.ensemble import RandomForestClassifier
rf = RandomForestClassifier(n_estimators=100, random_state=42)
rf.fit(X_train_n, y_train)
results['随机森林'] = accuracy_score(y_test, rf.predict(X_test_n))

# ==================== 4. 结果 ====================

print('\n========== 准确率对比 ==========')
for name, acc in sorted(results.items(), key=lambda x: -x[1]):
    print(f'{name:12s}: {acc*100:.1f}%')

best_name = max(results, key=results.get)
print(f'\n最优模型: {best_name}')

# 用最优模型打混淆矩阵
if best_name == '逻辑回归': best_model = lr
elif best_name == 'MLP(8)': best_model = mlp
elif best_name == 'LightGBM': best_model = lgbm
else: best_model = rf

y_pred = best_model.predict(X_test_n)
print('\n混淆矩阵 (行=真实, 列=预测):')
print('        正常  松动  不平衡')
cm = confusion_matrix(y_test, y_pred)
for i, row in enumerate(cm):
    name = ['正常  ', '松动  ', '不平衡'][i]
    print(f'{name}  {row[0]:4d}  {row[1]:4d}  {row[2]:4d}')

# ==================== 5. 导出F407部署权重 ====================

if best_name == 'MLP(8)':
    print('\n========== F407 C语言部署参数 ==========')
    print(f'// 输入归一化: mean={mean.round(2).tolist()}, std={std.round(2).tolist()}')
    print(f'// 输入层: 3 → 隐藏层: 8 → 输出层: 3')
    print(f'// W1 (8x3):')
    for i, row in enumerate(mlp.coefs_[0].T):  # 转置成 C 友好格式
        print(f'//   {{' + ', '.join(f'{v:.4f}f' for v in row) + '},')
    print(f'// b1 (8):')
    print(f'//   {{' + ', '.join(f'{v:.4f}f' for v in mlp.intercepts_[0]) + '}')
    print(f'// W2 (3x8): 权重矩阵 coefs_[1] 形状 {mlp.coefs_[1].shape}')
    print(f'// b2 (3): {mlp.intercepts_[1].round(4).tolist()}')

    # 保存完整权重到文件
    np.savez('mlp_weights.npz',
             W1=mlp.coefs_[0], b1=mlp.intercepts_[0],
             W2=mlp.coefs_[1], b2=mlp.intercepts_[1],
             mean=mean, std=std)
    print('\n权重已保存到 mlp_weights.npz (训练数据同目录)')
    print(f'隐藏层1激活: ReLU (sklearn默认), 输出: softmax')

# ==================== 6. 特征重要性 ====================

print('\n========== 特征重要性(随机森林) ==========')
for feat, imp in zip(['RMS', '频率', '幅值'], rf.feature_importances_):
    print(f'{feat}: {imp:.3f}')
