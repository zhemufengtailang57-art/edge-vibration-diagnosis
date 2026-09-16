"""
导出MLP权重为F407可用的C头文件
读取 mlp_weights_v2.npz → 生成 ai_model.h
"""

# --- 数据文件与脚本同目录，保证从任意路径运行都能找到数据 ---
import os as _os
_os.chdir(_os.path.dirname(_os.path.abspath(__file__)))

import numpy as np

data = np.load(r'mlp_weights_final.npz')
W1, b1 = data['W1'], data['b1']  # 17->32
W2, b2 = data['W2'], data['b2']  # 32->16
W3, b3 = data['W3'], data['b3']  # 16->3
mean, std = data['mean'], data['std']

lines = []
lines.append('/*')
lines.append(' * ai_model.h - 振动故障分类MLP权重 (自动生成, 勿手动编辑)')
lines.append(' * 结构: 17输入(1 RMS + 16频谱) -> 32隐藏(ReLU) -> 16隐藏(ReLU) -> 3输出')
lines.append(' * 生成自: train_final.py (90%转速单工况), 5折CV=99.1%, 测试=100%')
lines.append(' */')
lines.append('')
lines.append('#ifndef __AI_MODEL_H')
lines.append('#define __AI_MODEL_H')
lines.append('')
lines.append('#define AI_INPUT_SIZE   17')
lines.append('#define AI_HIDDEN1_SIZE 32')
lines.append('#define AI_HIDDEN2_SIZE 16')
lines.append('#define AI_OUTPUT_SIZE  3')
lines.append('')

# 归一化参数
lines.append('/* 输入归一化: x_norm = (x - mean) / std */')
lines.append(f'static const float AI_MEAN[{len(mean)}] = {{')
for i in range(0, len(mean), 8):
    vals = ', '.join(f'{v:.6f}f' for v in mean[i:i+8])
    lines.append(f'    {vals},')
lines.append('};')
lines.append('')
lines.append(f'static const float AI_STD[{len(std)}] = {{')
for i in range(0, len(std), 8):
    vals = ', '.join(f'{v:.6f}f' for v in std[i:i+8])
    lines.append(f'    {vals},')
lines.append('};')
lines.append('')

# W1: 17x32, 转置为 32x17 存储 (行=隐藏神经元)
lines.append(f'/* W1: {W1.shape[0]}x{W1.shape[1]} 输入->隐藏层1 */')
lines.append(f'static const float AI_W1[{W1.shape[1]}][{W1.shape[0]}] = {{')
W1_T = W1.T
for i in range(W1_T.shape[0]):
    vals = ', '.join(f'{v:.6f}f' for v in W1_T[i])
    lines.append(f'    {{{vals}}},')
lines.append('};')
lines.append('')

# b1: 32
lines.append(f'static const float AI_B1[{len(b1)}] = {{')
for i in range(0, len(b1), 8):
    vals = ', '.join(f'{v:.6f}f' for v in b1[i:i+8])
    lines.append(f'    {vals},')
lines.append('};')
lines.append('')

# W2: 32x16 -> 16x32
lines.append(f'/* W2: {W2.shape[0]}x{W2.shape[1]} 隐藏层1->隐藏层2 */')
lines.append(f'static const float AI_W2[{W2.shape[1]}][{W2.shape[0]}] = {{')
W2_T = W2.T
for i in range(W2_T.shape[0]):
    vals = ', '.join(f'{v:.6f}f' for v in W2_T[i])
    lines.append(f'    {{{vals}}},')
lines.append('};')
lines.append('')

# b2: 16
lines.append(f'static const float AI_B2[{len(b2)}] = {{')
for i in range(0, len(b2), 8):
    vals = ', '.join(f'{v:.6f}f' for v in b2[i:i+8])
    lines.append(f'    {vals},')
lines.append('};')
lines.append('')

# W3: 16x3 -> 3x16
lines.append(f'/* W3: {W3.shape[0]}x{W3.shape[1]} 隐藏层2->输出 */')
lines.append(f'static const float AI_W3[{W3.shape[1]}][{W3.shape[0]}] = {{')
W3_T = W3.T
for i in range(W3_T.shape[0]):
    vals = ', '.join(f'{v:.6f}f' for v in W3_T[i])
    lines.append(f'    {{{vals}}},')
lines.append('};')
lines.append('')

# b3: 3
lines.append(f'static const float AI_B3[{len(b3)}] = {{')
vals = ', '.join(f'{v:.6f}f' for v in b3)
lines.append(f'    {vals}')
lines.append('};')
lines.append('')
lines.append('#endif /* __AI_MODEL_H */')

with open(r'../firmware/Core/Inc/ai_model.h', 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines) + '\n')

print('ai_model.h 已生成')
print(f'W1: {W1.shape} W2: {W2.shape} W3: {W3.shape}')
print(f'总参数: {W1.size + b1.size + W2.size + b2.size + W3.size + b3.size}')
