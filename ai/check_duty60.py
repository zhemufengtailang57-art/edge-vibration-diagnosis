"""
60%转速下三类状态的主频分布分析
"""

# --- 数据文件与脚本同目录，保证从任意路径运行都能找到数据 ---
import os as _os
_os.chdir(_os.path.dirname(_os.path.abspath(__file__)))

import csv, glob
import numpy as np

for name, label in [('normal', 0), ('loose', 1), ('unbalance', 2)]:
    files = glob.glob(rf'{name}_duty600*.csv')
    if not files:
        print(f'{name}: MISSING')
        continue
    freqs = []
    rmss = []
    with open(files[0], encoding='utf-8-sig') as f:
        reader = csv.reader(f)
        next(reader)
        for r in reader:
            if len(r) >= 23:
                freqs.append(int(r[2]))   # 主频Hz
                rmss.append(int(r[1]))    # RMS
    freqs = np.array(freqs)
    rmss = np.array(rmss)
    print(f'{name}: 主频 mean={freqs.mean():.0f}Hz 分布={np.bincount(freqs)[:100].tolist() if freqs.max()<100 else "见下"}')
    from collections import Counter
    print(f'    主频计数: {Counter(freqs.tolist()).most_common(8)}')
    print(f'    RMS: mean={rmss.mean():.0f} min={rmss.min()} max={rmss.max()}')
    print()
