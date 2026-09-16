from pymodbus.client import ModbusSerialClient
c=ModbusSerialClient(port='COM7',baudrate=115200,bytesize=8,parity='N',stopbits=1,timeout=0.5)
c.connect()
rr=c.read_holding_registers(address=0,count=7,device_id=1)
if not rr.isError(): print("OK:", rr.registers)
else: print("ERR:", rr)
c.close()
