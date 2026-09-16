import serial, time

for baud in [9600, 115200]:
    for rts in [False, True]:
        for dtr in [False, True]:
            try:
                s = serial.Serial('COM7', baud, timeout=0.5)
                s.rts = rts
                s.dtr = dtr
                time.sleep(0.1)
                s.reset_input_buffer()
                s.write(b'\x41\x42\x43')
                time.sleep(0.3)
                r = s.read(32)
                status = 'OK' if r else 'TIMEOUT'
                hexdata = r.hex() if r else ''
                print(f'BAUD={baud} RTS={rts} DTR={dtr}: {status} {hexdata}')
                s.close()
            except Exception as e:
                print(f'BAUD={baud} RTS={rts} DTR={dtr}: ERROR {e}')
