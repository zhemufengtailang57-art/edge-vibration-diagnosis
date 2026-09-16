import serial, time

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1: crc = (crc >> 1) ^ 0xA001
            else: crc >>= 1
    return crc

# Try with RTS enabled
s = serial.Serial('COM6', 115200, timeout=1)
s.rts = True   # Enable RTS
time.sleep(0.1)

req = bytes([0x01, 0x03, 0x00, 0x00, 0x00, 0x07])
crc = crc16(req)
req += bytes([crc & 0xFF, (crc >> 8) & 0xFF])

for attempt in range(15):
    print(f'#{attempt+1}...', end=' ', flush=True)
    time.sleep(0.5)
    s.reset_input_buffer()
    s.write(req)
    time.sleep(0.8)
    r = s.read(64)
    if r and len(r) >= 5:
        dp = r[:-2]
        cr_r = r[-1] << 8 | r[-2]
        cr_c = crc16(dp)
        if cr_r == cr_c:
            print(f'OK! {r.hex()}')
            bc = r[2]
            names = ['RMS_mG','Freq_Hz','Amp_mG','Temp_x10','UpH','UpL','Status']
            for i in range(bc//2):
                v = (r[3+i*2]<<8)|r[3+i*2+1]
                print(f'  {names[i]}={v}')
            break
        else:
            print(f'CRC FAIL')
    else:
        print('TIMEOUT')

s.close()
