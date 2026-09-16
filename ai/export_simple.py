"""
导出简化模型权重为C头文件
5输入(RMS+bin1/2/6/7) -> 16 -> 8 -> 3
"""

# --- 数据文件与脚本同目录，保证从任意路径运行都能找到数据 ---
import os as _os
_os.chdir(_os.path.dirname(_os.path.abspath(__file__)))

import numpy as np

data = np.load(r'mlp_simple.npz')
W1, b1 = data['W1'], data['b1']
W2, b2 = data['W2'], data['b2']
W3, b3 = data['W3'], data['b3']
mean, std = data['mean'], data['std']

lines = []
lines.append('/*')
lines.append(' * ai_model.h - 振动故障分类MLP权重 (自动生成, 勿手动编辑)')
lines.append(' * 简化结构: 5输入(RMS+bin1/2/6/7) -> 16隐藏(ReLU) -> 8隐藏(ReLU) -> 3输出')
lines.append(' * 生成自: train_simple.py, 5折CV=97.7%')
lines.append(' */')
lines.append('')
lines.append('#ifndef __AI_MODEL_H')
lines.append('#define __AI_MODEL_H')
lines.append('')
lines.append('#define AI_INPUT_SIZE   5')
lines.append('#define AI_HIDDEN1_SIZE 16')
lines.append('#define AI_HIDDEN2_SIZE 8')
lines.append('#define AI_OUTPUT_SIZE  3')
lines.append('')

lines.append('/* 输入归一化: x_norm = (x - mean) / std */')
lines.append(f'static const float AI_MEAN[{len(mean)}] = {{')
vals = ', '.join(f'{v:.6f}f' for v in mean)
lines.append(f'    {vals}')
lines.append('};')
lines.append('')
lines.append(f'static const float AI_STD[{len(std)}] = {{')
vals = ', '.join(f'{v:.6f}f' for v in std)
lines.append(f'    {vals}')
lines.append('};')
lines.append('')

lines.append(f'static const float AI_W1[{W1.shape[1]}][{W1.shape[0]}] = {{')
W1_T = W1.T
for row in W1_T:
    vals = ', '.join(f'{v:.6f}f' for v in row)
    lines.append(f'    {{{vals}}},')
lines.append('};')
lines.append('')
lines.append(f'static const float AI_B1[{len(b1)}] = {{')
vals = ', '.join(f'{v:.6f}f' for v in b1)
lines.append(f'    {vals}')
lines.append('};')
lines.append('')

lines.append(f'static const float AI_W2[{W2.shape[1]}][{W2.shape[0]}] = {{')
W2_T = W2.T
for row in W2_T:
    vals = ', '.join(f'{v:.6f}f' for v in row)
    lines.append(f'    {{{vals}}},')
lines.append('};')
lines.append('')
lines.append(f'static const float AI_B2[{len(b2)}] = {{')
vals = ', '.join(f'{v:.6f}f' for v in b2)
lines.append(f'    {vals}')
lines.append('};')
lines.append('')

lines.append(f'static const float AI_W3[{W3.shape[1]}][{W3.shape[0]}] = {{')
W3_T = W3.T
for row in W3_T:
    vals = ', '.join(f'{v:.6f}f' for v in row)
    lines.append(f'    {{{vals}}},')
lines.append('};')
lines.append('')
lines.append(f'static const float AI_B3[{len(b3)}] = {{')
vals = ', '.join(f'{v:.6f}f' for v in b3)
lines.append(f'    {vals}')
lines.append('};')
lines.append('')
lines.append('#endif /* __AI_MODEL_H */')

with open(r'../firmware/Core/Inc/ai_model.h', 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines) + '\n')

print(f'ai_model.h regenerated: {W1.shape} {W2.shape} {W3.shape}')
