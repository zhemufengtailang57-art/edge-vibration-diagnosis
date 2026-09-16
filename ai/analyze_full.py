"""
全量数据分析 - 15组多转速数据集
5档转速(60/70/80/90/100%) x 3状态(normal/loose/unbalance)
评估:
  1. 数据质量统计
  2. 特征分布可视化(文本)
  3. 标准训练评估 (分层留出)
  4. 泛化实验: 留一档转速做测试 (核心!)
  5. 特征方案对比: 原始17 vs 统计3 vs 全20
"""

# --- 数据文件与脚本同目录，保证从任意路径运行都能找到数据 ---
import os as _os
_os.chdir(_os.path.dirname(_os.path.abspath(__file__)))

import csv
import numpy as np
from collections import defaultdict

DATA_DIR = r'.'
DUTIES = [600, 700, 800, 900, 999]
STATES = {'normal': 0, 'loose': 1, 'unbalance': 2}

def load_all():
    data = defaultdict(list)
    for duty in DUTIES:
        for state, label in STATES.items():
            path = f'{DATA_DIR}\\{state}_duty{duty}_2026*.csv'
            import glob
            files = glob.glob(path)
            if not files:
                print(f'MISSING: {state} duty{duty}')
                continue
            with open(files[0], encoding='utf-8-sig') as f:
                reader = csv.reader(f)
                next(reader)
                for r in reader:
                    if len(r) >= 23:
                        try:
                            rms = float(r[1])
                            bins = np.array([float(x) for x in r[7:23]])
                            data[(state, duty)].append((rms, bins))
                        except ValueError:
                            continue
    return data

def extract_stats(rms, bins):
    total = bins.sum()
    centroid = np.dot(np.arange(16), bins) / total if total > 0 else 0.0
    mean = bins.mean(); std = bins.std()
    kurt = np.mean(((bins - mean) / std) ** 4) - 3.0 if std > 1e-8 else 0.0
    low = bins[0:4].sum(); high = bins[12:16].sum()
    ratio = low / (high + 1e-8)
    return np.array([centroid, kurt, ratio])

print('=' * 60)
print('1. 数据质量统计')
print('=' * 60)
data = load_all()
for state, label in STATES.items():
    counts = [len(data[(state, d)]) for d in DUTIES]
    print(f'{state:12s}: ' + ' '.join(f'{c:4d}' for c in counts) + f'  共{sum(counts)}')

# 构建特征矩阵
X17_list, X3_list, X20_list, y_list, duty_list = [], [], [], [], []
for duty in DUTIES:
    for state, label in STATES.items():
        for rms, bins in data[(state, duty)]:
            stats = extract_stats(rms, bins)
            x17 = np.array([rms] + bins.tolist())
            X17_list.append(x17)
            X3_list.append(stats)
            X20_list.append(np.concatenate([x17, stats]))
            y_list.append(label)
            duty_list.append(duty)

X17 = np.array(X17_list); X3 = np.array(X3_list); X20 = np.array(X20_list)
y = np.array(y_list); duty_arr = np.array(duty_list)
print(f'\n总样本: {len(y)}')

print('\n' + '=' * 60)
print('2. 各状态特征分布 (跨转速)')
print('=' * 60)
for name, label in STATES.items():
    mask = y == label
    print(f'\n{name}:')
    print(f'  RMS:   mean={X17[mask,0].mean():.0f}  std={X17[mask,0].std():.0f}  range=[{X17[mask,0].min():.0f}, {X17[mask,0].max():.0f}]')
    print(f'  centroid: mean={X3[mask,0].mean():.2f}  std={X3[mask,0].std():.2f}')
    print(f'  kurtosis: mean={X3[mask,1].mean():.2f}  std={X3[mask,1].std():.2f}')
    print(f'  ratio:    mean={X3[mask,2].mean():.2f}  std={X3[mask,2].std():.2f}')

# 各转速下RMS
print('\n' + '=' * 60)
print('3. 各转速档RMS变化 (看转速漂移影响)')
print('=' * 60)
print(f'{"":12s}' + ''.join(f'{d//10:>6d}%' for d in DUTIES))
for name, label in STATES.items():
    row = []
    for duty in DUTIES:
        mask = (y == label) & (duty_arr == duty)
        row.append(f'{X17[mask,0].mean():6.0f}')
    print(f'{name:12s}' + ''.join(row))

print('\n' + '=' * 60)
print('4. 模型评估')
print('=' * 60)

from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, confusion_matrix
from sklearn.preprocessing import StandardScaler
from sklearn.neural_network import MLPClassifier
from sklearn.ensemble import RandomForestClassifier
from sklearn.linear_model import LogisticRegression

def standard_eval(X, y, name):
    """标准分层留出评估"""
    X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
    sc = StandardScaler().fit(X_tr)
    X_tr_s, X_te_s = sc.transform(X_tr), sc.transform(X_te)
    res = {}
    for mname, model in [
        ('LR', LogisticRegression(max_iter=3000)),
        ('MLP', MLPClassifier(hidden_layer_sizes=(32,16), max_iter=3000, random_state=42)),
        ('RF', RandomForestClassifier(n_estimators=200, random_state=42)),
    ]:
        model.fit(X_tr_s, y_tr)
        res[mname] = accuracy_score(y_te, model.predict(X_te_s))
    return res

print('\n标准评估 (随机分层划分):')
print(f'{"特征":16s} {"LR":>7s} {"MLP":>7s} {"RF":>7s}')
r = standard_eval(X17, y, '17raw'); print(f'{"17原始":16s} {r["LR"]*100:6.1f}% {r["MLP"]*100:6.1f}% {r["RF"]*100:6.1f}%')
r = standard_eval(X3, y, '3stats'); print(f'{"3统计":16s} {r["LR"]*100:6.1f}% {r["MLP"]*100:6.1f}% {r["RF"]*100:6.1f}%')
r = standard_eval(X20, y, '20full'); print(f'{"20全特征":16s} {r["LR"]*100:6.1f}% {r["MLP"]*100:6.1f}% {r["RF"]*100:6.1f}%')

print('\n' + '=' * 60)
print('5. 泛化实验: 留一档转速做测试 (核心!)')
print('=' * 60)
print('训练: 4档转速 | 测试: 剩下1档 (完全没见过的转速)')

def loo_duty_eval(X, y, duty_arr):
    """留一档交叉评估, 返回每档准确率"""
    results = {}
    for holdout in DUTIES:
        tr_mask = duty_arr != holdout
        te_mask = duty_arr == holdout
        sc = StandardScaler().fit(X[tr_mask])
        X_tr_s = sc.transform(X[tr_mask]); X_te_s = sc.transform(X[te_mask])
        mlp = MLPClassifier(hidden_layer_sizes=(32,16), max_iter=3000, random_state=42)
        mlp.fit(X_tr_s, y[tr_mask])
        results[holdout] = accuracy_score(y[te_mask], mlp.predict(X_te_s))
    return results

print('\nMLP 留一档转速泛化:')
print(f'{"测试档":>10s} {"17原始":>10s} {"3统计":>10s} {"20全特征":>10s}')
loo17 = loo_duty_eval(X17, y, duty_arr)
loo3  = loo_duty_eval(X3, y, duty_arr)
loo20 = loo_duty_eval(X20, y, duty_arr)
for d in DUTIES:
    print(f'{d//10:6d}%    {loo17[d]*100:7.1f}%   {loo3[d]*100:7.1f}%   {loo20[d]*100:7.1f}%')
print(f'{"平均":>10s} {np.mean(list(loo17.values()))*100:7.1f}%   {np.mean(list(loo3.values()))*100:7.1f}%   {np.mean(list(loo20.values()))*100:7.1f}%')

print('\n' + '=' * 60)
print('5b. 转速作为特征: 占空比加入输入')
print('=' * 60)

# 占空比归一化后作为第0个特征
duty_norm = (duty_arr / 999.0).reshape(-1, 1)
X20_duty = np.hstack([duty_norm, X20])
X17_duty = np.hstack([duty_norm, X17])

print('\nMLP 留一档转速泛化 (带转速特征):')
print(f'{"测试档":>10s} {"17原始+转速":>12s} {"20全特征+转速":>14s}')
loo17d = loo_duty_eval(X17_duty, y, duty_arr)
loo20d = loo_duty_eval(X20_duty, y, duty_arr)
for d in DUTIES:
    print(f'{d//10:6d}%    {loo17d[d]*100:8.1f}%     {loo20d[d]*100:8.1f}%')
print(f'{"平均":>10s} {np.mean(list(loo17d.values()))*100:8.1f}%     {np.mean(list(loo20d.values()))*100:8.1f}%')

print('\n标准评估 (带转速特征):')
r = standard_eval(X17_duty, y, '17raw+duty'); print(f'{"17原始+转速":16s} {r["LR"]*100:6.1f}% {r["MLP"]*100:6.1f}% {r["RF"]*100:6.1f}%')
r = standard_eval(X20_duty, y, '20full+duty'); print(f'{"20全特征+转速":16s} {r["LR"]*100:6.1f}% {r["MLP"]*100:6.1f}% {r["RF"]*100:6.1f}%')

print('\n' + '=' * 60)
print('6. 混淆矩阵 (20全特征, 标准评估)')
print('=' * 60)
X_tr, X_te, y_tr, y_te = train_test_split(X20, y, test_size=0.2, random_state=42, stratify=y)
sc = StandardScaler().fit(X_tr)
mlp = MLPClassifier(hidden_layer_sizes=(32,16), max_iter=3000, random_state=42)
mlp.fit(sc.transform(X_tr), y_tr)
y_pred = mlp.predict(sc.transform(X_te))
cm = confusion_matrix(y_te, y_pred)
print('        正常  松动  不平衡')
for i, name in enumerate(['正常  ', '松动  ', '不平衡']):
    print(f'{name}  {cm[i,0]:4d}  {cm[i,1]:4d}  {cm[i,2]:4d}')
