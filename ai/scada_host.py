#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
工业振动检测与故障预测系统 — SCADA上位机
通信: Modbus-RTU (USB-485)
风格: 工业深蓝
"""

import sys, time
from datetime import datetime

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QLabel, QPushButton,
    QComboBox, QVBoxLayout, QHBoxLayout, QGridLayout,
    QTableWidget, QFrame, QTabWidget
)
from PyQt5.QtCore import QTimer, QThread, pyqtSignal, Qt, QRectF
from PyQt5.QtGui import (
    QFont, QColor, QPalette, QPainter, QPen, QBrush
)
import pyqtgraph as pg

try:
    from pymodbus.client import ModbusSerialClient
    HAS_MODBUS = True
except ImportError:
    HAS_MODBUS = False

# ============ 配色 ============
BG      = '#0d1b2a'
PANEL   = '#152238'
ACCENT  = '#2980b9'
TEXT    = '#d4dce6'
SUB     = '#7f95ad'
GREEN   = '#27ae60'
YELLOW  = '#f39c12'
RED     = '#e74c3c'
BLUE    = '#3498db'

# ============ 通信线程 ============
class ModbusThread(QThread):
    data_ready = pyqtSignal(dict)

    def __init__(self, port='COM3', baud=115200):
        super().__init__()
        self.port = port
        self.baud = baud
        self.running = False
        self.client = None
        self.frame_count = 0

    def run(self):
        if not HAS_MODBUS:
            return
        self.client = ModbusSerialClient(
            port=self.port, baudrate=self.baud,
            bytesize=8, parity='N', stopbits=1, timeout=0.5
        )
        if not self.client.connect():
            print("Modbus fail:", self.port)
            return
        self.running = True
        while self.running:
            try:
                rr = self.client.read_holding_registers(0, 7, slave=1)
                if not rr.isError():
                    self.frame_count += 1
                    d = dict(
                        rms_mg=rr.registers[0], freq_hz=rr.registers[1],
                        amp_mg=rr.registers[2], temp_x10=rr.registers[3],
                        uph=rr.registers[4], upl=rr.registers[5],
                        st=rr.registers[6]
                    )
                    self.data_ready.emit(d)
            except:
                pass
            time.sleep(0.5)

    def stop(self):
        self.running = False
        if self.client:
            self.client.close()

# ============ 圆形仪表 ============
class Gauge(QWidget):
    def __init__(self, title='', unit='', mx=100, wv=70, dv=90, sz=100):
        super().__init__()
        self.val = 0
        self.title = title
        self.unit = unit
        self.mx = mx
        self.wv = wv
        self.dv = dv
        self.setMinimumSize(sz, sz+5)
        self.setMaximumSize(sz+20, sz+20)

    def setValue(self, v):
        self.val = v
        self.update()

    def paintEvent(self, e):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w = self.width()
        h = self.height()
        r = min(w, h) // 2 - 10
        cx = w // 2
        cy = h // 2 + 2

        # 背景弧
        p.setPen(QPen(QColor('#1e3750'), 8))
        p.drawArc(QRectF(cx-r, cy-r, r*2, r*2), 225*16, 90*16)

        # 有值弧
        ratio = min(abs(self.val) / self.mx, 1.0)
        if ratio < self.wv / self.mx:
            c = QColor(GREEN)
        elif ratio < self.dv / self.mx:
            c = QColor(YELLOW)
        else:
            c = QColor(RED)
        p.setPen(QPen(c, 6))
        p.drawArc(QRectF(cx-r, cy-r, r*2, r*2), 225*16, -int(90*ratio)*16)

        # 数值 (大字)
        p.setFont(QFont('Arial', 16, QFont.Bold))
        p.setPen(QColor(TEXT))
        p.drawText(QRectF(cx-40, cy-22, 80, 24), Qt.AlignmentFlag.AlignCenter,
                   f'{self.val:.0f}')

        # 单位 (小字, 数值下方)
        p.setFont(QFont('Arial', 7))
        p.setPen(QColor(SUB))
        p.drawText(QRectF(cx-20, cy+2, 40, 12), Qt.AlignmentFlag.AlignCenter,
                   self.unit)

        # 标题 (底部)
        p.setFont(QFont('Microsoft YaHei', 8))
        p.drawText(QRectF(0, h-15, w, 14), Qt.AlignmentFlag.AlignCenter,
                   self.title)
        p.end()

# ============ 状态灯 ============
class Lamp(QWidget):
    def __init__(self, label=''):
        super().__init__()
        self.lb = label
        self.st = 0
        self.setFixedSize(110, 26)

    def setStatus(self, s):
        self.st = s
        self.update()

    def paintEvent(self, e):
        p = QPainter(self)
        cs = [QColor(GREEN), QColor(YELLOW), QColor(RED)]
        ls = ['正常', '预警', '故障']
        p.setBrush(QBrush(cs[self.st]))
        p.setPen(QColor(0, 0, 0, 0))
        p.drawEllipse(4, 5, 14, 14)
        p.setPen(QColor(TEXT))
        p.setFont(QFont('Microsoft YaHei', 9))
        p.drawText(22, 0, 85, 26, Qt.AlignmentFlag.AlignVCenter,
                   f'{self.lb}: {ls[self.st]}')
        p.end()

# ============ 数值显示 ============
class ValBox(QWidget):
    def __init__(self, label='', unit='', color=BLUE):
        super().__init__()
        self.lb = label
        self.un = unit
        self.co = color
        self.vl = '--'
        self.setMinimumSize(90, 55)

    def setValue(self, v):
        self.vl = v
        self.update()

    def paintEvent(self, e):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        p.setPen(QColor(SUB))
        p.setFont(QFont('Microsoft YaHei', 8))
        p.drawText(QRectF(0, 2, self.width(), 14), Qt.AlignmentFlag.AlignCenter,
                   self.lb)
        p.setPen(QColor(self.co))
        p.setFont(QFont('Arial', 24, QFont.Bold))
        p.drawText(QRectF(0, 16, self.width()-15, 34), Qt.AlignmentFlag.AlignRight,
                   self.vl)
        p.setPen(QColor(SUB))
        p.setFont(QFont('Arial', 8))
        p.drawText(QRectF(self.width()-12, 36, 15, 14), Qt.AlignmentFlag.AlignRight,
                   self.un)
        p.end()

# ============ 主窗口 ============
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle('工业振动检测与故障预测系统')
        self.setMinimumSize(1100, 700)
        self.modbus = None
        self.td = []
        self.vd = []
        self.setup_ui()
        self.timer = QTimer()
        self.timer.timeout.connect(self.tick)
        self.timer.start(1000)

    def setup_ui(self):
        c = QWidget()
        self.setCentralWidget(c)
        c.setStyleSheet('background:' + BG + ';')
        L = QVBoxLayout(c)
        L.setSpacing(4)

        # ---- 顶栏 ----
        tb = QHBoxLayout()
        s = f'color:{SUB}; background:{PANEL}; border:1px solid {ACCENT}; padding:3px;'
        # 串口
        lb = QLabel('串口 ')
        lb.setStyleSheet('color:' + SUB)
        tb.addWidget(lb)
        self.cpo = QComboBox()
        self.cpo.addItems(['COM3', 'COM4', 'COM5', 'COM6', 'COM7', 'COM8'])
        self.cpo.setStyleSheet(s)
        tb.addWidget(self.cpo)
        # 波特率
        lb2 = QLabel(' 波特率 ')
        lb2.setStyleSheet('color:' + SUB)
        tb.addWidget(lb2)
        self.cba = QComboBox()
        self.cba.addItems(['115200', '9600', '19200'])
        self.cba.setStyleSheet(s)
        tb.addWidget(self.cba)
        # 连接
        self.bc = QPushButton('连接')
        self.bc.clicked.connect(self.toggle)
        self.bc.setStyleSheet(
            'background:' + ACCENT + '; color:white; padding:4px 14px;'
            'border:none; border-radius:3px; font-weight:bold;'
        )
        tb.addWidget(self.bc)
        tb.addSpacing(10)
        self.lc = QLabel('未连接')
        self.lc.setStyleSheet('color:' + SUB)
        tb.addWidget(self.lc)
        self.lf = QLabel('帧:0')
        self.lf.setStyleSheet('color:' + SUB)
        tb.addWidget(self.lf)
        tb.addStretch()
        self.lt = QLabel()
        self.lt.setStyleSheet('color:' + SUB)
        tb.addWidget(self.lt)
        L.addLayout(tb)

        # ---- 标签页 ----
        self.tabs = QTabWidget()
        self.tabs.addTab(self.make_main(), '主监视屏')
        self.tabs.addTab(self.make_fault(), '故障记录')
        L.addWidget(self.tabs)

        # ---- 底栏 ----
        self.sb = QLabel('就绪 | 等待 USB-RS485 连接...')
        self.sb.setStyleSheet('background:' + PANEL + '; color:' + SUB + '; padding:3px;')
        L.addWidget(self.sb)

    def make_main(self):
        w = QWidget()
        g = QGridLayout(w)
        g.setSpacing(6)
        g.setColumnStretch(0, 1)
        g.setColumnStretch(1, 1)
        g.setColumnStretch(2, 1)
        g.setColumnStretch(3, 2)

        # 仪表行 (小尺寸)
        s = 100  # gauge size
        self.gr = Gauge('RMS 振动', 'mG', 5000, 1000, 2000, sz=s)
        g.addWidget(self.make_frame(self.gr), 0, 0)
        self.gf = Gauge('主频', 'Hz', 500, 300, 400, sz=s)
        g.addWidget(self.make_frame(self.gf), 0, 1)
        self.ga = Gauge('峰值幅值', 'mG', 5000, 1500, 3000, sz=s)
        g.addWidget(self.make_frame(self.ga), 0, 2)

        # 右侧报警面板 (焦点)
        ap = QFrame()
        ap.setStyleSheet('background:' + PANEL + '; border-radius:6px;')
        al = QVBoxLayout(ap)
        al.setContentsMargins(10, 8, 10, 8)
        al.setSpacing(6)

        self.alarm_label = QLabel('系统状态')
        self.alarm_label.setFont(QFont('Microsoft YaHei', 13, QFont.Bold))
        self.alarm_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        al.addWidget(self.alarm_label)

        self.alarm_big = QLabel('● 正常')
        self.alarm_big.setFont(QFont('Microsoft YaHei', 28, QFont.Bold))
        self.alarm_big.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.alarm_big.setStyleSheet('color:' + GREEN + ';')
        al.addWidget(self.alarm_big)

        # 温度+运行
        hh = QHBoxLayout()
        self.vt = ValBox('温度', 'C', BLUE)
        hh.addWidget(self.vt)
        self.vu = ValBox('运行', '', '#888')
        hh.addWidget(self.vu)
        al.addLayout(hh)

        # 状态灯
        self.ls = Lamp('系统')
        al.addWidget(self.ls)
        self.lv = Lamp('振动')
        al.addWidget(self.lv)
        self.lt_lamp = Lamp('温度')
        al.addWidget(self.lt_lamp)
        self.lm = Lamp('电机')
        al.addWidget(self.lm)
        al.addStretch()
        g.addWidget(ap, 0, 3)

        # 频谱 (第2行左)
        self.ps = pg.PlotWidget(title='实时频谱')
        self.ps.setLabel('left', '幅值')
        self.ps.setLabel('bottom', 'Hz')
        self.ps.showGrid(x=True, y=True)
        self.ps.setBackground(PANEL)
        self.ps.setMinimumHeight(140)
        self.ps.hideButtons()
        g.addWidget(self.ps, 1, 0, 1, 2)

        # 温度趋势 (第2行右)
        self.pt = pg.PlotWidget(title='温度趋势')
        self.pt.hideButtons()
        self.pt.setLabel('left', 'C')
        self.pt.showGrid(x=True, y=True)
        self.pt.setBackground(PANEL)
        self.pt.setMinimumHeight(140)
        self.ct = self.pt.plot(pen=pg.mkPen(BLUE, width=2))
        g.addWidget(self.pt, 1, 2, 1, 2)

        # 趋势图 (第3行全宽)
        self.pv = pg.PlotWidget(title='振动 RMS 实时趋势')
        self.pv.hideButtons()
        self.pv.setLabel('left', 'mG')
        self.pv.setLabel('bottom', 's')
        self.pv.showGrid(x=True, y=True)
        self.pv.setBackground(PANEL)
        self.pv.setMinimumHeight(130)
        self.cv = self.pv.plot(pen=pg.mkPen(YELLOW, width=2))
        g.addWidget(self.pv, 2, 0, 1, 4)

        g.setRowStretch(0, 2)
        g.setRowStretch(1, 3)
        g.setRowStretch(2, 4)
        return w

    def make_frame(self, w):
        f = QFrame()
        f.setStyleSheet('background:' + PANEL + '; border-radius:4px;')
        l = QVBoxLayout(f)
        l.setContentsMargins(6, 4, 6, 4)
        l.addWidget(w)
        return f

    def make_fault(self):
        w = QWidget()
        l = QVBoxLayout(w)
        br = QHBoxLayout()
        self.bk = QPushButton('清空黑匣子(Modbus写寄存器)')
        self.bk.clicked.connect(self.clr_bb)
        br.addWidget(self.bk)
        br.addStretch()
        l.addLayout(br)
        self.ft = QTableWidget(0, 6)
        self.ft.setHorizontalHeaderLabels(
            ['时间', 'RMS(mG)', '频率(Hz)', '幅值(mG)', '温度(C)', '状态']
        )
        self.ft.horizontalHeader().setStretchLastSection(True)
        self.ft.setStyleSheet(
            'QTableWidget{background:' + PANEL + '; gridline-color:#333;}'
        )
        l.addWidget(self.ft)
        return w

    # ========== 回调 ==========
    def toggle(self):
        if self.modbus and self.modbus.running:
            self.modbus.stop()
            self.modbus = None
            self.bc.setText('连接')
            self.lc.setText('未连接')
            self.lc.setStyleSheet('color:' + SUB)
            self.sb.setText('已断开')
        else:
            port = self.cpo.currentText()
            baud = int(self.cba.currentText())
            self.modbus = ModbusThread(port, baud)
            self.modbus.data_ready.connect(self.on_data)
            self.modbus.start()
            self.bc.setText('断开')
            self.lc.setText('已连接')
            self.lc.setStyleSheet('color:' + GREEN)
            self.sb.setText('已连接 ' + port + ' @ ' + str(baud))

    def on_data(self, d):
        rms_mg = d['rms_mg']
        freq_hz = d['freq_hz']
        amp_mg = d['amp_mg']
        temp_x10 = d['temp_x10']
        up = (d['uph'] << 16) | d['upl']
        st = d['st']
        tc = temp_x10 / 10.0
        self.gr.setValue(rms_mg)
        self.gf.setValue(freq_hz)
        self.ga.setValue(amp_mg)
        self.vt.setValue(f'{tc:.1f}')
        h, m = divmod(up, 3600)
        m, s = divmod(m, 60)
        self.vu.setValue(f'{h:02d}:{m:02d}')
        s2 = 2 if st >= 2 else (1 if st >= 1 else 0)
        # 大字告警
        if s2 == 0:
            self.alarm_big.setText('● 正常')
            self.alarm_big.setStyleSheet('color:' + GREEN + ';')
        elif s2 == 1:
            self.alarm_big.setText('▲ 预警')
            self.alarm_big.setStyleSheet('color:' + YELLOW + ';')
        else:
            self.alarm_big.setText('■ 故障!')
            self.alarm_big.setStyleSheet('color:' + RED + ';')
        self.ls.setStatus(s2)
        self.lv.setStatus(s2)
        ts = 2 if tc > 60 else (1 if tc > 45 else 0)
        self.lt_lamp.setStatus(ts)
        self.lm.setStatus(s2)
        t = time.time()
        self.vd.append((t, rms_mg))
        if len(self.vd) > 300:
            self.vd = self.vd[-300:]
        self.cv.setData([x[0] for x in self.vd], [x[1] for x in self.vd])
        self.td.append((t, tc))
        if len(self.td) > 200:
            self.td = self.td[-200:]
        self.ct.setData([x[0] for x in self.td], [x[1] for x in self.td])
        if self.modbus:
            self.lf.setText('帧:' + str(self.modbus.frame_count))

    def clr_bb(self):
        if self.modbus and self.modbus.client:
            try:
                self.modbus.client.write_register(6, 0xCC00, slave=1)
            except:
                pass

    def tick(self):
        self.lt.setText(datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

    def closeEvent(self, e):
        if self.modbus:
            self.modbus.stop()
        e.accept()

# ============ 入口 ============
if __name__ == '__main__':
    app = QApplication(sys.argv)
    app.setStyle('Fusion')
    # 暗色主题
    p = QPalette()
    p.setColor(QPalette.Window, QColor(13, 27, 42))
    p.setColor(QPalette.WindowText, QColor(212, 220, 230))
    p.setColor(QPalette.Base, QColor(21, 34, 56))
    p.setColor(QPalette.Text, QColor(212, 220, 230))
    p.setColor(QPalette.Button, QColor(30, 45, 70))
    p.setColor(QPalette.ButtonText, QColor(212, 220, 230))
    app.setPalette(p)
    win = MainWindow()
    win.show()
    sys.exit(app.exec_())
