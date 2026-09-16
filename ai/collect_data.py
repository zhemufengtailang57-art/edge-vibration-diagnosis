"""
振动数据采集脚本 V2 (16维频谱特征 + 电机遥控)
用法: python collect_data.py <标签> <时长秒> <占空比0-999>
例:   python collect_data.py normal 180 600   (60%占空比采3分钟)
      python collect_data.py normal 180 0     (不启电机, 静止采集)
流程: 设置占空比→电机转→等15秒稳定→采集→电机停→提示换状态→回车继续下一组
"""
import serial
import time
import csv
import sys
from datetime import datetime

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1: crc = (crc >> 1) ^ 0xA001
            else: crc >>= 1
    return crc

def write_register(ser, addr, value):
    """写单个寄存器 (功能码06)"""
    req = bytes([1, 0x06, (addr >> 8) & 0xFF, addr & 0xFF,
                 (value >> 8) & 0xFF, value & 0xFF])
    crc = crc16(req)
    req += bytes([crc & 0xFF, (crc >> 8) & 0xFF])
    ser.reset_input_buffer()
    ser.write(req)
    time.sleep(0.3)
    r = ser.read(16)
    # 成功响应=回显请求(6字节), 异常=5字节
    return r is not None and len(r) >= 5

def read_holding_regs(ser, addr, count):
    req = bytes([1, 0x03, (addr >> 8) & 0xFF, addr & 0xFF,
                 (count >> 8) & 0xFF, count & 0xFF])
    crc = crc16(req)
    req += bytes([crc & 0xFF, (crc >> 8) & 0xFF])
    ser.reset_input_buffer()
    ser.write(req)
    time.sleep(0.3)
    r = ser.read(5 + count * 2 + 10)
    if len(r) < 5:
        return None
    for i in range(len(r) - 2):
        if r[i] == 1 and r[i+1] == 0x03:
            bc = r[i+2]
            expected = 3 + bc + 2
            if i + expected <= len(r):
                frame = r[i:i+expected]
                if crc16(frame[:-2]) == (frame[-1] << 8 | frame[-2]):
                    regs = []
                    for j in range(bc // 2):
                        regs.append((frame[3+j*2] << 8) | frame[3+j*2+1])
                    return regs
    return None

def motor_control(ser, duty):
    """duty=0停, 1-999转"""
    ok = write_register(ser, 23, duty)
    if not ok:
        print('WARN: 电机命令未确认')
    return ok

def main():
    label = sys.argv[1] if len(sys.argv) > 1 else 'test'
    duration = int(sys.argv[2]) if len(sys.argv) > 2 else 180
    duty = int(sys.argv[3]) if len(sys.argv) > 3 else 600

    ser = serial.Serial('COM6', 115200, timeout=0.3)
    print(f'采集: {label} | {duration}秒 | 占空比{duty/10:.0f}%')

    # 电机控制
    motor_control(ser, duty)
    if duty > 0:
        print('电机启动, 等15秒转速稳定...')
        time.sleep(15)
    else:
        print('电机静止采集')
        time.sleep(3)

    header = ['时间', 'RMS', '频率', '幅值', '温度', '运行时长', '状态'] + \
             [f'bin{i}' for i in range(16)]

    fname = f'{label}_duty{duty}_{datetime.now().strftime("%Y%m%d_%H%M%S")}.csv'
    with open(fname, 'w', newline='', encoding='utf-8-sig') as f:
        writer = csv.writer(f)
        writer.writerow(header)
        f.flush()

        start = time.time()
        count = 0
        while time.time() - start < duration:
            base = read_holding_regs(ser, 0, 7)
            spec1 = read_holding_regs(ser, 7, 8)
            spec2 = read_holding_regs(ser, 15, 8)
            if base and len(base) >= 7 and spec1 and len(spec1) >= 8 and spec2 and len(spec2) >= 8:
                row = [datetime.now().strftime('%H:%M:%S')] + base + spec1 + spec2
                writer.writerow(row)
                f.flush()
                count += 1
                if count % 10 == 0:
                    print(f'  {count}条 RMS={base[0]} 主频={base[1]}Hz')
            time.sleep(0.4)

    # 停电机
    motor_control(ser, 0)
    print(f'\n完成! {count}条 → {fname}')
    print('电机已停。现在可以:')
    print('  1. 换状态(松螺丝/贴胶带)后, 运行下一条命令')
    print('  2. 按回车关闭串口')
    input()
    ser.close()

if __name__ == '__main__':
    main()
