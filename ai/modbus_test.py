import serial, time

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1: crc = (crc >> 1) ^ 0xA001
            else: crc >>= 1
    return crc

req = bytes([0x01, 0x03, 0x00, 0x00, 0x00, 0x07])
crc = crc16(req)
req += bytes([crc & 0xFF, (crc >> 8) & 0xFF])

print('TX:', req.hex())

s = serial.Serial('COM7', 9600, timeout=5)
s.reset_input_buffer()
s.write(req)
time.sleep(2)

r = bytearray()
while s.in_waiting:
    r += s.read(s.in_waiting)

print(f'RX {len(r)} bytes: {r.hex() if r else "TIMEOUT"}')

if r and len(r) >= 5:
    dp = r[:-2]
    cr = r[-1] << 8 | r[-2]
    cc = crc16(dp)
    print(f'CRC: rx=0x{cr:04X} calc=0x{cc:04X} {"OK" if cr==cc else "FAIL"}')
    if cr == cc:
        bc = r[2]
        for i in range(bc // 2):
            val = (r[3+i*2] << 8) | r[3+i*2+1]
            print(f'  Reg[{i}] = {val}')

s.close()
