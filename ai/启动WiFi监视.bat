@echo off
chcp 65001 >nul
echo === Vibration Monitor WiFi Startup ===
echo.
echo [1/2] Starting Mosquitto...
start "" "C:\Program Files\mosquitto\mosquitto.exe" -v -c %~dp0mosquitto.conf
echo [2/3] Starting MQTT Bridge...
start "" python %~dp0mqtt_bridge.py
echo [3/3] Starting Grafana...
start "" /min cmd /c "D:\GrafanaLabs\grafana\bin\grafana.exe server --homepath D:\GrafanaLabs\grafana"
echo.
echo Done! Grafana: http://127.0.0.1:3030
echo Phone: check Bridge window for URL
pause
