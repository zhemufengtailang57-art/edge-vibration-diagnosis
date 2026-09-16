"""
Modbus-RTU 振动监测上位机 (PyQt5)
功能: 通过RS485读取F407振动传感器数据, 实时显示
"""

import sys
import struct
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
    QHBoxLayout, QLabel, QLineEdit, QPushButton, QComboBox, QGroupBox,
    QGridLayout, QStatusBar, QMessageBox)
from PyQt5.QtCore import QTimer, Qt
from PyQt5.QtGui import QFont, QColor, QPalette
import serial
import serial.tools.list_ports
import time

# ==================== Modbus 协议 ====================

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1: crc = (crc >> 1) ^ 0xA001
            else: crc >>= 1
    return crc

def read_holding_regs(ser, slave, addr, count, timeout=0.3):
    """读保持寄存器, 返回寄存器值列表或None"""
    req = bytes([slave, 0x03, (addr >> 8) & 0xFF, addr & 0xFF,
                 (count >> 8) & 0xFF, count & 0xFF])
    crc = crc16(req)
    req += bytes([crc & 0xFF, (crc >> 8) & 0xFF])

    ser.reset_input_buffer()
    ser.write(req)
    time.sleep(0.15)
    r = ser.read(5 + count * 2 + 10)

    if len(r) < 5:
        return None

    # 找帧头 [slave][0x03][byte_cnt]
    for i in range(len(r) - 2):
        if r[i] == slave and r[i+1] == 0x03:
            byte_cnt = r[i+2]
            expected = 3 + byte_cnt + 2
            if i + expected <= len(r):
                frame = r[i:i+expected]
                dp = frame[:-2]
                cr = frame[-1] << 8 | frame[-2]
                if crc16(dp) == cr:
                    regs = []
                    for j in range(byte_cnt // 2):
                        regs.append((frame[3+j*2] << 8) | frame[3+j*2+1])
                    return regs
    return None

# ==================== 主窗口 ====================

class ModbusMonitor(QMainWindow):
    def __init__(self):
        super().__init__()
        self.ser = None
        self.polling = False
        self.timer = QTimer()
        self.timer.timeout.connect(self.poll)
        self.initUI()

    def initUI(self):
        self.setWindowTitle("振动监测 Modbus 上位机")
        self.setMinimumSize(520, 420)

        cw = QWidget()
        self.setCentralWidget(cw)
        layout = QVBoxLayout(cw)

        # ===== 连接设置 =====
        gbConn = QGroupBox("串口设置")
        gConn = QGridLayout(gbConn)

        gConn.addWidget(QLabel("串口:"), 0, 0)
        self.cbPort = QComboBox()
        self.refreshPorts()
        gConn.addWidget(self.cbPort, 0, 1)

        gConn.addWidget(QLabel("波特率:"), 0, 2)
        self.cbBaud = QComboBox()
        self.cbBaud.addItems(["115200", "9600"])
        self.cbBaud.setCurrentText("115200")
        gConn.addWidget(self.cbBaud, 0, 3)

        gConn.addWidget(QLabel("从站ID:"), 0, 4)
        self.leSlave = QLineEdit("1")
        self.leSlave.setMaximumWidth(50)
        gConn.addWidget(self.leSlave, 0, 5)

        self.btnConnect = QPushButton("连接")
        self.btnConnect.clicked.connect(self.toggleConnect)
        gConn.addWidget(self.btnConnect, 0, 6)

        layout.addWidget(gbConn)

        # ===== 数据显示 =====
        gbData = QGroupBox("实时数据")
        gData = QGridLayout(gbData)

        self.labels = {}
        regs = [
            (0, "振动RMS", "mG"),
            (1, "主频", "Hz"),
            (2, "峰值幅值", "mG"),
            (3, "温度", "°C"),
            (4, "运行时间", "s"),
            (5, "系统状态", ""),
        ]

        fontVal = QFont("Consolas", 20, QFont.Bold)
        fontUnit = QFont("Microsoft YaHei", 11)

        for i, (idx, name, unit) in enumerate(regs):
            row, col = divmod(i, 3)
            frame = QGroupBox(name)
            fl = QVBoxLayout(frame)
            val = QLabel("--")
            val.setFont(fontVal)
            val.setAlignment(Qt.AlignCenter)
            val.setStyleSheet("color: #00aa00;")
            fl.addWidget(val)
            if unit:
                u = QLabel(unit)
                u.setFont(fontUnit)
                u.setAlignment(Qt.AlignCenter)
                u.setStyleSheet("color: #888;")
                fl.addWidget(u)
            gData.addWidget(frame, row, col)
            self.labels[idx] = val

        layout.addWidget(gbData)

        # ===== 状态栏 =====
        self.statusBar = QStatusBar()
        self.setStatusBar(self.statusBar)
        self.lblStatus = QLabel("未连接")
        self.statusBar.addWidget(self.lblStatus)

        # 轮询间隔
        gbPoll = QGroupBox("轮询设置")
        gP = QHBoxLayout(gbPoll)
        gP.addWidget(QLabel("间隔(ms):"))
        self.leInterval = QLineEdit("500")
        self.leInterval.setMaximumWidth(80)
        gP.addWidget(self.leInterval)
        gP.addStretch()
        layout.addWidget(gbPoll)

    def refreshPorts(self):
        self.cbPort.clear()
        for p in serial.tools.list_ports.comports():
            self.cbPort.addItem(f"{p.device} - {p.description}", p.device)

    def toggleConnect(self):
        if self.polling:
            self.stopPoll()
        else:
            self.startPoll()

    def startPoll(self):
        port = self.cbPort.currentData()
        baud = int(self.cbBaud.currentText())
        slave = int(self.leSlave.text())

        try:
            self.ser = serial.Serial(port, baud, timeout=0.3)
            self.slave = slave
            self.polling = True
            self.btnConnect.setText("断开")
            self.lblStatus.setText(f"已连接 {port} @ {baud}")
            self.lblStatus.setStyleSheet("color: green;")
            interval = int(self.leInterval.text())
            self.timer.start(interval)
        except Exception as e:
            QMessageBox.critical(self, "错误", f"串口打开失败:\n{e}")

    def stopPoll(self):
        self.polling = False
        self.timer.stop()
        if self.ser:
            self.ser.close()
            self.ser = None
        self.btnConnect.setText("连接")
        self.lblStatus.setText("未连接")
        self.lblStatus.setStyleSheet("color: red;")

    def poll(self):
        if not self.polling or not self.ser:
            return

        try:
            regs = read_holding_regs(self.ser, self.slave, 0, 7)
            if regs is None:
                return

            # 注册器0: RMS
            self.labels[0].setText(f"{regs[0]}")
            self.labels[0].setStyleSheet(f"color: {'red' if regs[0] > 50 else '#00aa00'};")

            # 注册器1: 频率
            self.labels[1].setText(f"{regs[1]}")
            self.labels[1].setStyleSheet(f"color: {'orange' if regs[1] > 100 else '#00aa00'};")

            # 注册器2: 幅值
            self.labels[2].setText(f"{regs[2]}")

            # 注册器3: 温度
            temp = regs[3]
            if temp > 0:
                self.labels[3].setText(f"{temp/10:.1f}")
            else:
                self.labels[3].setText(f"{regs[3]}")

            # 注册器4+5: 运行时间
            uptime = (regs[4] << 16) | regs[5]
            h, m, s = uptime // 3600, (uptime % 3600) // 60, uptime % 60
            self.labels[4].setText(f"{h:02d}:{m:02d}:{s:02d}")

            # 注册器6: 状态
            st = regs[6]
            stText = {0: "正常", 1: "预警", 2: "故障"}.get(st, f"未知({st})")
            stColor = {0: "#00aa00", 1: "orange", 2: "red"}.get(st, "#888")
            self.labels[5].setText(stText)
            self.labels[5].setStyleSheet(f"color: {stColor};")

        except Exception as e:
            self.lblStatus.setText(f"通信错误: {e}")
            self.lblStatus.setStyleSheet("color: red;")

    def closeEvent(self, event):
        self.stopPoll()
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    window = ModbusMonitor()
    window.show()
    sys.exit(app.exec_())
