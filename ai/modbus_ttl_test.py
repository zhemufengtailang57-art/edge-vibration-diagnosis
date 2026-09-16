"""
Modbus-RTU 直连测试脚本 (TTL串口, 不用RS485)
用法: 先看设备管理器里 USB-TTL 的 COM 号, 改下面 PORT 的值, 然后运行
"""
import serial
import struct
import time

# ========== 改这里 ==========
PORT = 'COM7'  # USB-TTL 的 COM 号
# ===========================

# Modbus CRC16 计算
def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

# 构建读保持寄存器请求: 站号1, 起始地址0, 读7个
SLAVE = 1
FUNC = 0x03
START_ADDR = 0
QUANTITY = 7

req = bytearray([SLAVE, FUNC, (START_ADDR >> 8) & 0xFF, START_ADDR & 0xFF,
                  (QUANTITY >> 8) & 0xFF, QUANTITY & 0xFF])
crc = crc16(req)
req.append(crc & 0xFF)
req.append((crc >> 8) & 0xFF)

print(f"发送: {' '.join(f'{b:02X}' for b in req)}")

try:
    ser = serial.Serial(PORT, baudrate=115200, bytesize=8, parity='N', stopbits=1, timeout=1)
    print(f"串口 {PORT} 已打开")

    ser.write(req)
    print("已发送, 等待回复...")

    time.sleep(0.1)

    resp = ser.read(64)
    if resp:
        print(f"收到 {len(resp)} 字节: {' '.join(f'{b:02X}' for b in resp)}")

        if len(resp) >= 5:
            # 验证 CRC
            data_part = resp[:-2]
            crc_rcv = resp[-1] << 8 | resp[-2]
            crc_calc = crc16(data_part)
            if crc_rcv == crc_calc:
                print("CRC 校验通过 ✓")

                # 解析寄存器值
                byte_count = resp[2]
                print(f"\n====== 保持寄存器 (0x03) 共 {byte_count//2} 个 ======")
                for i in range(byte_count // 2):
                    offset = 3 + i * 2
                    val = (resp[offset] << 8) | resp[offset + 1]
                    names = ['振动RMS(mG)', '主频(Hz)', '峰值幅值(mG)',
                             '温度(x10°C)', '运行时间H', '运行时间L', '状态']
                    name = names[i] if i < len(names) else f"寄存器{i}"
                    print(f"  寄存器[{i}] {name}: {val}")
            else:
                print(f"CRC 不匹配: 收到=0x{crc_rcv:04X}, 计算=0x{crc_calc:04X}")
        else:
            print(f"回复太短 ({len(resp)} 字节)")
    else:
        print("未收到回复 (超时)")
        print("\n可能原因:")
        print("1. COM口号不对 → 去设备管理器确认")
        print("2. TX/RX 接反了 → 交换试试")
        print("3. F407没上电或代码没烧")

finally:
    if 'ser' in dir():
        ser.close()
    print("\n串口已关闭")
