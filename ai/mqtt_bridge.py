
# --- 数据文件与脚本同目录，保证从任意路径运行都能找到数据 ---
import os as _os
_os.chdir(_os.path.dirname(_os.path.abspath(__file__)))

import paho.mqtt.client as mqtt
import json
import time
import socket
from http.server import HTTPServer, BaseHTTPRequestHandler

# 最新数据缓存
latest = {"rms": 0, "freq": 0, "amp": 0, "temp": 0, "up": 0, "st": 0,
          "rms_max": 0, "freq_max": 0, "time": "", "samples": 0}
history = []

def on_connect(client, userdata, flags, rc, props=None):
    print(f"MQTT Broker connected, rc={rc}")
    client.subscribe("vibration/data")

def on_message(client, userdata, msg):
    global latest, history
    try:
        data = json.loads(msg.payload.decode())
        data.setdefault("rms", 0); data.setdefault("freq", 0)
        data.setdefault("amp", 0); data.setdefault("temp", 0)
        data.setdefault("up", 0); data.setdefault("st", 0)
        rms = data.get("rms", 0)
        freq = data.get("freq", 0)
        latest = data
        latest["rms_max"] = max(latest.get("rms_max", 0), rms)
        latest["freq_max"] = max(latest.get("freq_max", 0), freq)
        latest["time"] = time.strftime("%H:%M:%S")
        latest["samples"] = latest.get("samples", 0) + 1
        history.append({"rms": rms, "freq": freq, "time": latest["time"], "st": data.get("st", 0)})
        if len(history) > 100:
            history.pop(0)
    except Exception as e:
        print(f"Parse error: {e}")

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/" or self.path == "":
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            with open(r"phone_monitor.html", "rb") as f:
                self.wfile.write(f.read())
            return
        if self.path == "/api/latest":
            self.send_json([latest])
        elif self.path == "/api/history":
            self.send_json(history)
        elif self.path == "/api/search":
            self.send_json(["rms", "freq", "amp", "temp", "st", "rms_max", "freq_max", "samples"])
        else:
            self.send_response(404)
            self.end_headers()

    def send_json(self, data):
        body = json.dumps(data).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        pass

# MQTT连接
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message
client.connect("127.0.0.1", 1884, 60)
client.loop_start()

# HTTP服务
server = HTTPServer(("0.0.0.0", 8080), Handler)

# 找WLAN/WiFi网卡的IP（跳过回环127、虚拟169.254、VMware 198.x）
local_ip = "127.0.0.1"
hn = socket.gethostname()
try:
    for info in socket.getaddrinfo(hn, None, socket.AF_INET):
        ip = info[4][0]
        if not ip.startswith("127.") and not ip.startswith("169.254") and not ip.startswith("198."):
            local_ip = ip
            break
except:
    pass

print("=" * 50)
print(f"  手机网页 -> http://{local_ip}:8080")
print(f"  手机Grafana -> http://{local_ip}:3030")
print(f"  电脑网页 -> http://127.0.0.1:8080")
print(f"  电脑Grafana -> http://127.0.0.1:3030")
print("=" * 50)
server.serve_forever()
