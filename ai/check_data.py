import csv
import sys

path = sys.argv[1]
with open(path, encoding='utf-8-sig') as f:
    rows = list(csv.reader(f))

print(f'rows: {len(rows)-1}')
r = rows[len(rows)//2]  # 中间一行
print('RMS:', r[1], 'Freq:', r[2], 'Amp:', r[3])
bins = [int(x) for x in r[7:23]]
print('16 bins:', bins)
print('max bin:', max(bins), 'at index', bins.index(max(bins)))
