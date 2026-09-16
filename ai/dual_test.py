import serial, time, threading

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1: crc = (crc >> 1) ^ 0xA001
            else: crc >>= 1
    return crc

log = serial.Serial('COM5', 115200, timeout=0.1)
mb = serial.Serial('COM7', 9600, timeout=2)

log_lines = []
done = False

def read_log():
    buf = b''
    while not done:
        try:
            r = log.read(256)
            if r:
                buf += r
        except:
            pass
    log_lines.append(buf)

t = threading.Thread(target=read_log, daemon=True)
t.start()

req = bytes([0x01, 0x03, 0x00, 0x00, 0x00, 0x07])
crc = crc16(req)
req += bytes([crc & 0xFF, (crc >> 8) & 0xFF])

print('TX:', req.hex())
time.sleep(0.5)

for attempt in range(5):
    mb.reset_input_buffer()
    mb.write(req)
    time.sleep(0.6)
    r = mb.read(64)
    if r:
        print(f'Modbus RX: {r.hex()}')
        dp = r[:-2]
        cr = r[-1]<<8|r[-2]
        cc = crc16(dp)
        if cr == cc:
            print('CRC OK!')
            bc = r[2]
            for i in range(bc//2):
                v = (r[3+i*2]<<8)|r[3+i*2+1]
                print(f'  Reg{i}={v}')
        break
    else:
        print(f'#{attempt+1} TIMEOUT')

time.sleep(1)
done = True
t.join(timeout=2)

buf = log_lines[0] if log_lines else b''
text = buf.decode('latin-1', errors='replace')
print('\n--- MB Logs ---')
for line in text.split('\n'):
    if 'MB' in line or 'CRC' in line:
        print(line.strip()[:130])

log.close()
mb.close()
