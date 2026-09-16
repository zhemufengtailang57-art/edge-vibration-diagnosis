Write-Host "=== 振动监测 WiFi 启动 ===" -ForegroundColor Green
Write-Host ""
Write-Host "[1/2] Mosquitto..."
Start-Process "C:\Program Files\mosquitto\mosquitto.exe" -ArgumentList "-v","-c","$PSScriptRoot\mosquitto.conf"
Write-Host "[2/2] MQTT Bridge..."
Start-Process python -ArgumentList "$PSScriptRoot\mqtt_bridge.py"
Write-Host ""
Write-Host "OK! Grafana: http://127.0.0.1:3030" -ForegroundColor Cyan
Write-Host "手机网页: 看 Bridge 窗口打印的地址" -ForegroundColor Cyan
Read-Host "按回车关闭"
