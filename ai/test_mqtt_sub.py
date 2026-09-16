import paho.mqtt.client as mqtt
import time, json

msg_count = 0
def on_msg(client, userdata, msg):
    global msg_count
    msg_count += 1
    data = json.loads(msg.payload.decode())
    print(f'#{msg_count}: rms={data.get("rms",0)} freq={data.get("freq",0)}')

c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
c.on_message = on_msg
c.connect('127.0.0.1', 1883, 60)
c.subscribe('vibration/data')
c.loop_start()
time.sleep(3)
c.loop_stop()
c.disconnect()
print(f'Total: {msg_count} messages')
