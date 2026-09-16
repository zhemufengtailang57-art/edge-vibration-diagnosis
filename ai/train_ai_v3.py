"""
振动故障分类模型训练 V3 - 16频谱 + 3统计特征
特征: RMS + 16bin + 质心 + 峰度 + 频带比 = 20维
"""
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
                    rms = float(r[1])
                    bins = np.array([float(x) for x in r[7:23]])
                    rows.append((rms, bins, label))
                except ValueError:
                    continue
    return rows

def extract_features(rms, bins):
    """16bin → 3统计特征"""
    # 频谱质心 (加权平均频率)
    total = bins.sum()
    if total > 0:
        centroid = np.dot(np.arange(16), bins) / total
    else:
        centroid = 0.0

    # 频谱峰度 (归一化四阶矩)
    mean = bins.mean()
    std = bins.std()
    if std > 1e-8:
        kurtosis = np.mean(((bins - mean) / std) ** 4) - 3.0  # 超高斯>0
    else:
        kurtosis = 0.0

    # 频带能量比: 低4bin / 高4bin
    low = bins[0:4].sum()
    high = bins[12:16].sum()
    band_ratio = low / (high + 1e-8)

    return np.array([centroid, kurtosis, band_ratio])

print("加载数据...")
normal = load_csv(r'normal_20260815_215635.csv', 0)
loose  = load_csv(r'loose_20260815_220445.csv', 1)
unbal  = load_csv(r'unbalance_20260815_221120.csv', 2)
print(f'正常: {len(normal)} | 松动: {len(loose)} | 不平衡: {len(unbal)}')

all_data = normal + loose + unbal
X_raw = []
X_feat = []
y = []
for rms, bins, label in all_data:
    X_raw.append([rms] + bins.tolist())
    X_feat.append(extract_features(rms, bins))
    y.append(label)

X_raw = np.array(X_raw, dtype=np.float32)
X_feat = np.array(X_feat, dtype=np.float32)
y = np.array(y)

# 打印三类特征分布
print('\n特征分布 (均值):')
for cls, name in [(0, '正常'), (1, '松动'), (2, '不平衡')]:
    mask = y == cls
    print('{}: centroid={:.2f} kurt={:.2f} ratio={:.2f}'.format(
        name, X_feat[mask,0].mean(), X_feat[mask,1].mean(), X_feat[mask,2].mean()))

from sklearn.model_selection import train_test_split, cross_val_score
from sklearn.metrics import accuracy_score, confusion_matrix
from sklearn.linear_model import LogisticRegression
from sklearn.neural_network import MLPClassifier
from sklearn.ensemble import RandomForestClassifier

def train_eval(X, name):
    """5折交叉验证 + 独立测试集"""
    X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
    mean = X_tr.mean(axis=0)
    std = X_tr.std(axis=0) + 1e-8
    X_tr_n = (X_tr - mean) / std
    X_te_n = (X_te - mean) / std

    results = {}
    models = {
        'LR': LogisticRegression(max_iter=2000),
        'MLP': MLPClassifier(hidden_layer_sizes=(32, 16), max_iter=3000, random_state=42),
        'RF': RandomForestClassifier(n_estimators=200, random_state=42),
    }
    for mname, model in models.items():
        model.fit(X_tr_n, y_tr)
        results[mname] = accuracy_score(y_te, model.predict(X_te_n))
    return results

print('\n========== 特征方案对比 ==========')
header = '{:<20s} {:>8s} {:>8s} {:>8s}'.format('特征组合', 'LR', 'MLP', 'RF')
print(header)
print('-' * 48)

# 方案A: 只用3个统计特征
rA = train_eval(X_feat, 'A')
print('{:<20s} {:7.1f}% {:7.1f}% {:7.1f}%'.format('3统计特征', rA['LR']*100, rA['MLP']*100, rA['RF']*100))

# 方案B: 原始16bin+RMS
rB = train_eval(X_raw, 'B')
print('{:<20s} {:7.1f}% {:7.1f}% {:7.1f}%'.format('17原始特征', rB['LR']*100, rB['MLP']*100, rB['RF']*100))

# 方案C: 全特征 20维
X_all = np.hstack([X_raw, X_feat])
rC = train_eval(X_all, 'C')
print('{:<20s} {:7.1f}% {:7.1f}% {:7.1f}%'.format('20全特征', rC['LR']*100, rC['MLP']*100, rC['RF']*100))

# 方案D: RMS+3统计 (5维, 最轻量)
X_light = np.hstack([X_raw[:, :1], X_feat])
rD = train_eval(X_light, 'D')
print('{:<20s} {:7.1f}% {:7.1f}% {:7.1f}%'.format('RMS+3统计', rD['LR']*100, rD['MLP']*100, rD['RF']*100))
