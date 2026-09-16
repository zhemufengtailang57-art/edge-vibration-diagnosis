# -*- coding: utf-8 -*-
"""黑匣子原始帧测试: 57600 下直接读写, 打印响应原始HEX
用法: 先关闭Qt上位机(释放COM口), 然后 python bb_raw_test.py
"""
import serial
import time
import sys

PORT = 'COM6'
BAUD = 57600

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc

def build_frame(func, payload):
    msg = bytes([0x01, func]) + payload
    c = crc16(msg)
    return msg + bytes([c & 0xFF, (c >> 8) & 0xFF])

def verify_frame(resp, expect_len):
    """校验响应CRC, 返回 (ok, 解析后的数据字节)"""
    if len(resp) < 4:
        return False, f'太短({len(resp)}字节)'
    if len(resp) != expect_len:
        return False, f'长度错: 期望{expect_len} 实收{len(resp)}'
    got = (resp[-1] << 8) | resp[-2]
    calc = crc16(resp[:-2])
    if got != calc:
        return False, f'CRC错: 帧尾={got:04X} 计算={calc:04X}'
    return True, 'OK'

def main():
    ser = serial.Serial(PORT, BAUD, timeout=0.3)
    print(f'打开 {PORT} @ {BAUD}')
    ser.reset_input_buffer()

    # 第1步: 读寄存器24 (总数)
    ser.write(build_frame(0x03, bytes([0x00, 0x18, 0x00, 0x01])))
    time.sleep(0.3)
    resp = ser.read_all()
    ok, info = verify_frame(resp, 7)
    print(f'读总数(24): 收到 {resp.hex(" ")} -> {info}')
    if not ok:
        print('第一步就失败, 停止')
        sys.exit(1)
    total = (resp[3] << 8) | resp[4]
    print(f'黑匣子总数 = {total}')

    # 第2步: 逐条读 index 0..9
    fail = 0
    for i in range(10):
        # 写寄存器25 = index
        ser.reset_input_buffer()
        ser.write(build_frame(0x06, bytes([0x00, 0x19, 0x00, i])))
        time.sleep(0.3)
        wresp = ser.read_all()
        wok, winfo = verify_frame(wresp, 8)
        print(f'[{i:2d}] 写25={i}: {wresp.hex(" ")} -> {winfo}')

        # 读寄存器26-33
        ser.reset_input_buffer()
        ser.write(build_frame(0x03, bytes([0x00, 0x1A, 0x00, 0x08])))
        time.sleep(0.5)
        rresp = ser.read_all()
        rok, rinfo = verify_frame(rresp, 21)
        print(f'[{i:2d}] 读26-33: {rresp.hex(" ")} -> {rinfo}')
        if not rok:
            fail += 1

    print(f'\n结果: 10条中 {fail} 条失败')
    ser.close()

if __name__ == '__main__':
    main()
