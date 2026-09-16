import csv, sys
from collections import Counter

path = sys.argv[1] if len(sys.argv) > 1 else r'新螺丝松动.csv'

with open(path, encoding='utf-8-sig', errors='ignore') as f:
    reader = csv.reader(f)
    header = next(reader)
    rows = [r for r in reader if len(r) >= 7]

print(f'rows: {len(rows)}')

rms_list = [int(r[1]) for r in rows if r[1].strip()]
freq_list = [int(r[2]) for r in rows if r[2].strip()]
amp_list = [int(r[3]) for r in rows if r[3].strip()]

print(f'RMS  : min={min(rms_list)} max={max(rms_list)} mean={sum(rms_list)//len(rms_list)}')
print(f'Freq : min={min(freq_list)} max={max(freq_list)} mode={Counter(freq_list).most_common(5)}')
print(f'Amp  : min={min(amp_list)} max={max(amp_list)} mean={sum(amp_list)//len(amp_list)}')

for r in rows[:8]:
    print(r[0], r[1], r[2], r[3])
