#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QSpinBox>
#include <QMessageBox>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QSplitter>
#include <QHeaderView>
#include <QFont>
#include <QDateTime>
#include <QSerialPort>
#include <QFrame>
#include <QToolTip>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>

// ==================== 工业配色常量 ====================
#define CLR_BG_MAIN    "#1A1D22"
#define CLR_BG_PANEL   "#23272E"
#define CLR_BG_CORE    "#282E38"
#define CLR_BG_HEADER  "#2C323B"
#define CLR_BG_ROW_ALT "#252B33"
#define CLR_TITLE      "#B0B8C4"
#define CLR_SUB        "#8892A0"
#define CLR_VALUE      "#FFFFFF"
#define CLR_GREEN      "#39D077"
#define CLR_ORANGE     "#FF9800"
#define CLR_RED        "#FF5757"
#define CLR_BLUE       "#5C9CEF"
#define CLR_BTN_GRAY   "#404854"
#define CLR_BORDER     "#2E3239"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
    , m_settings("ModbusVibMonitor", "config")
{
    ui->setupUi(this);
    setupStyle();
    setupUI();
    loadSettings();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::onPoll);
}

MainWindow::~MainWindow()
{
    saveSettings();
    if (m_modbus) { m_modbus->disconnectDevice(); delete m_modbus; }
    delete ui;
}

// ==================== 全局工业配色 ====================

void MainWindow::setupStyle()
{
    qApp->setStyleSheet(QString(R"(
        * { font-family: "Microsoft YaHei","Segoe UI"; font-size:12px; }
        QMainWindow { background-color: %1; }
        QGroupBox {
            color: %2; border: 1px solid %3; border-radius: 4px;
            margin-top: 8px; padding-top: 14px; font-size:11px; font-weight:bold;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }
        QPushButton { border:none; padding:6px 18px; border-radius:3px; font-weight:bold; }
        QPushButton#btnConnect {
            background-color: %4; color: white; padding: 7px 32px; font-size:13px;
            min-width: 90px;
        }
        QPushButton#btnConnect:hover { background-color: #3ddb7d; }
        QPushButton#btnConnected {
            background-color: %5; color: white; padding: 7px 32px; font-size:13px;
            min-width: 90px;
        }
        QPushButton#btnConnected:hover { background-color: #ff7070; }
        QToolButton { background-color:%6; color:%7; border:1px solid %3;
                      padding:4px 12px; border-radius:3px; }
        QToolButton:hover { background-color: #3a404b; }
        QToolButton:checked { background-color: #2a3f5f; color: %8; }
        QSpinBox { padding-right: 18px; min-width: 70px; }
        QComboBox, QSpinBox, QLineEdit {
            background-color: %9; color: #ddd; border:1px solid %3; padding:4px 8px; border-radius:2px;
        }
        QComboBox::drop-down { border:none; }
        QComboBox QAbstractItemView { background-color:%9; color:#ddd; selection-background-color:%4; }
        QTableWidget {
            background-color: %10; color: #ccc; border:1px solid %3;
            gridline-color: #2d3038; selection-background-color:%4; font-size:11px;
        }
        QTableWidget::item { padding:2px 8px; }
        QHeaderView::section { background-color:%11; color:%7; border:none; padding:3px 8px; font-size:10px; }
        QTextEdit { background-color:#15171d; color:#7a7; border:1px solid %3;
                    font-family:"Consolas",monospace; font-size:10px; }
        QMenuBar { background-color:%9; color:#bbb; border-bottom:1px solid #2e3239; }
        QMenuBar::item:selected { background-color:%4; }
        QMenu { background-color:%9; color:#bbb; border:1px solid %3; }
        QMenu::item:selected { background-color:%4; }
        QStatusBar { background-color:#13151a; color:%7; border-top:1px solid %3; font-size:10px; }
        QSplitter::handle { background-color:%3; width:1px; }
    )")
    .arg(CLR_BG_MAIN)    // %1  main bg
    .arg(CLR_TITLE)      // %2  groupbox title
    .arg(CLR_BORDER)     // %3  border
    .arg(CLR_GREEN)      // %4  green
    .arg(CLR_RED)        // %5  red
    .arg(CLR_BTN_GRAY)   // %6  toolbutton bg
    .arg(CLR_SUB)        // %7  sub text
    .arg(CLR_BLUE)       // %8  checked blue
    .arg(CLR_BG_PANEL)   // %9  input bg
    .arg("#1F2228")      // %10 table bg
    .arg(CLR_BG_HEADER)); // %11 header
}

// ==================== 主界面 ====================

void MainWindow::setupUI()
{
    setWindowTitle("振动监测 Modbus 上位机  v1.0");
    resize(1220, 760);
    setMinimumSize(960, 600);

    QWidget *cw = new QWidget(this);
    setCentralWidget(cw);
    QVBoxLayout *mainV = new QVBoxLayout(cw);
    mainV->setContentsMargins(10, 4, 10, 6);
    mainV->setSpacing(0);

    // ===== 菜单 =====
    QMenu *mFile = menuBar()->addMenu("文件(&F)");
    mFile->addAction("导出 CSV...", this, &MainWindow::onExportCSV);
    mFile->addSeparator();
    mFile->addAction("退出(&Q)", this, &QWidget::close);

    QMenu *mView = menuBar()->addMenu("视图(&V)");
    mView->addAction("操作员/工程师切换", this, &MainWindow::onToggleView);

    QMenu *mHelp = menuBar()->addMenu("帮助(&H)");
    mHelp->addAction("关于", this, [this](){
        QMessageBox::about(this, "关于",
            "振动监测 Modbus 上位机 v1.0\n\n"
            "STM32F407 + RS485 Modbus-RTU + Qt 6\n"
            "工业级振动数据采集 | 2026.08");
    });

    // ===== 顶部控制栏 =====
    QFrame *topBar = new QFrame;
    topBar->setStyleSheet(QString("QFrame { background-color:%1; border-radius:4px; padding:4px; }").arg(CLR_BG_PANEL));
    topBar->setFixedHeight(46);
    QHBoxLayout *topL = new QHBoxLayout(topBar);
    topL->setContentsMargins(12,0,12,0); topL->setSpacing(10);

    auto lbl = [](const QString &t) -> QLabel* {
        QLabel *l = new QLabel(t);
        l->setStyleSheet("color:#8892A0; font-size:11px; background:transparent;");
        return l;
    };

    topL->addWidget(lbl("串口"));
    m_cbPort = new QComboBox; m_cbPort->setMinimumWidth(100);
    for (const auto &info : QSerialPortInfo::availablePorts())
        m_cbPort->addItem(info.portName(), info.portName());
    topL->addWidget(m_cbPort);

    topL->addWidget(lbl("波特率"));
    m_cbBaud = new QComboBox;
    m_cbBaud->addItems({"115200","57600","9600"}); m_cbBaud->setCurrentText("115200");
    topL->addWidget(m_cbBaud);

    topL->addWidget(lbl("从站"));
    m_spSlave = new QSpinBox;
    m_spSlave->setRange(1,247); m_spSlave->setValue(1); m_spSlave->setFixedWidth(80);
    topL->addWidget(m_spSlave);

    topL->addWidget(lbl("间隔(ms)"));
    m_spIntv = new QSpinBox;
    m_spIntv->setRange(500,5000); m_spIntv->setValue(1000); m_spIntv->setSingleStep(100); m_spIntv->setFixedWidth(100);
    topL->addWidget(m_spIntv);

    topL->addStretch();

    m_btnConn = new QPushButton("●  连 接");
    m_btnConn->setObjectName("btnConn");
    connect(m_btnConn, &QPushButton::clicked, this, &MainWindow::onConnect);
    topL->addWidget(m_btnConn);

    m_btnView = new QToolButton;
    m_btnView->setText("操作员视图");
    connect(m_btnView, &QToolButton::clicked, this, &MainWindow::onToggleView);
    topL->addWidget(m_btnView);

    mainV->addWidget(topBar);
    mainV->addSpacing(8);

    // ===== 中部：左侧卡片+表 | 右侧曲线 =====
    QSplitter *splitter = new QSplitter(Qt::Horizontal);

    // -- 左侧 --
    QWidget *leftPanel = new QWidget;
    QVBoxLayout *leftV = new QVBoxLayout(leftPanel);
    leftV->setContentsMargins(0,0,6,0); leftV->setSpacing(6);

    // 核心指标（大）
    QGroupBox *gbCore = new QGroupBox("核心振动指标");
    QGridLayout *coreGrid = new QGridLayout(gbCore);
    coreGrid->setVerticalSpacing(8); coreGrid->setHorizontalSpacing(10);

    m_coreFrames[0] = makeCoreCard("振动 RMS", "mG", CLR_GREEN,  m_cardRms);
    m_coreFrames[1] = makeCoreCard("主频", "Hz", CLR_ORANGE, m_cardFreq);
    m_coreFrames[2] = makeCoreCard("峰值幅值", "mG", CLR_BLUE,  m_cardAmp);

    coreGrid->addWidget(m_coreFrames[0], 0, 0);
    coreGrid->addWidget(m_coreFrames[1], 0, 1);
    coreGrid->addWidget(m_coreFrames[2], 0, 2);

    // 告警指示条
    QHBoxLayout *alertRow = new QHBoxLayout;
    m_lblAlertRms = new QLabel("RMS 阈值: 50 mG");
    m_lblAlertRms->setStyleSheet("color:#555; font-size:10px;");
    m_lblAlertFreq = new QLabel("频率阈值: 100 Hz");
    m_lblAlertFreq->setStyleSheet("color:#555; font-size:10px;");
    alertRow->addWidget(m_lblAlertRms);
    alertRow->addWidget(m_lblAlertFreq);
    alertRow->addStretch();
    coreGrid->addLayout(alertRow, 1, 0, 1, 3);

    leftV->addWidget(gbCore);

    // 次要参数（小）
    QHBoxLayout *secRow = new QHBoxLayout; secRow->setSpacing(8);
    m_secFrames[0] = makeSecCard("温度", "°C", m_cardTemp);
    m_secFrames[1] = makeSecCard("运行时长", "", m_cardUptime);
    m_secFrames[2] = makeSecCard("系统状态", "", m_cardStatus);
    secRow->addWidget(m_secFrames[0]); secRow->addWidget(m_secFrames[1]); secRow->addWidget(m_secFrames[2]);
    leftV->addLayout(secRow);

    // 寄存器表（4列）
    m_gbReg = new QGroupBox("Modbus 寄存器");
    QVBoxLayout *regV = new QVBoxLayout(m_gbReg);
    m_table = new QTableWidget(8, 4);
    m_table->setObjectName("regTable");
    m_table->setHorizontalHeaderLabels({"地址","参数","原始值","换算值"});
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 50);
    m_table->setColumnWidth(1, 100);
    m_table->setColumnWidth(2, 70);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setStyleSheet(QString(
        "QTableWidget { alternate-background-color: %1; }").arg(CLR_BG_ROW_ALT));

    struct { const char *addr; const char *name; } regs[] = {
        {"40001","振动RMS (mG)"}, {"40002","主频 (Hz)"}, {"40003","峰值幅值 (mG)"},
        {"40004","温度 (x10°C)"}, {"40005","运行时间H"}, {"40006","运行时间L"},
        {"40007","系统状态"},     {"40008","错误计数"}
    };
    for (int i = 0; i < 8; i++) {
        m_table->setItem(i, 0, new QTableWidgetItem(regs[i].addr));
        m_table->setItem(i, 1, new QTableWidgetItem(regs[i].name));
        m_table->setItem(i, 2, new QTableWidgetItem("--"));
        m_table->setItem(i, 3, new QTableWidgetItem("--"));
        m_table->item(i, 0)->setForeground(QColor("#555"));
        m_table->item(i, 1)->setForeground(QColor("#888"));
        m_table->item(i, 2)->setForeground(QColor(CLR_GREEN));
        m_table->item(i, 3)->setForeground(QColor(CLR_GREEN));
    }
    regV->addWidget(m_table);
    leftV->addWidget(m_gbReg);

    splitter->addWidget(leftPanel);

    // -- 右侧：实时曲线 --
    QGroupBox *gbChart = new QGroupBox("实时振动趋势");
    QVBoxLayout *chartV = new QVBoxLayout(gbChart);
    chartV->setContentsMargins(2,2,2,2);

    setupChart();
    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setStyleSheet(QString("background-color: %1; border: none;").arg(CLR_BG_MAIN));
    m_chartView->setRubberBand(QChartView::RectangleRubberBand);
    chartV->addWidget(m_chartView);

    splitter->addWidget(gbChart);
    splitter->setSizes({320, 870});

    // ===== 标签页容器 =====
    m_tabs = new QTabWidget;
    m_tabs->setStyleSheet(QString(
        "QTabWidget::pane { border: 1px solid %1; background: %2; }"
        "QTabBar::tab { background: #23272E; color: %3; padding: 8px 24px; border: 1px solid %1; "
        "  border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background: #282E38; color: %4; font-weight: bold; }"
        "QTabBar::tab:hover { background: #2a3039; }")
        .arg(CLR_BORDER, CLR_BG_MAIN, CLR_SUB, CLR_GREEN));

    m_tabs->addTab(splitter, "实时监测");

    // -- 故障记录页 --
    QWidget *faultPage = new QWidget;
    QVBoxLayout *faultV = new QVBoxLayout(faultPage);
    faultV->setContentsMargins(4,8,4,4);

    QHBoxLayout *faultBar = new QHBoxLayout;
    faultBar->addWidget(new QLabel("<b style='color:#B0B8C4'>F407 黑匣子故障记录</b>"));
    faultBar->addStretch();
    QPushButton *btnRead = new QPushButton("读取黑匣子");
    btnRead->setStyleSheet(QString("background-color:%1; color:white; padding:6px 18px;").arg(CLR_GREEN));
    connect(btnRead, &QPushButton::clicked, this, &MainWindow::onReadFaults);
    faultBar->addWidget(btnRead);

    QPushButton *btnClearBb = new QPushButton("清空记录");
    btnClearBb->setStyleSheet(QString("background-color:%1; color:white; padding:6px 18px;").arg(CLR_RED));
    connect(btnClearBb, &QPushButton::clicked, this, &MainWindow::onClearBlackbox);
    faultBar->addWidget(btnClearBb);
    faultV->addLayout(faultBar);

    m_faultTable = new QTableWidget(0, 7);
    m_faultTable->setHorizontalHeaderLabels({"序号","时间(s)","RMS(mG)","频率(Hz)","幅值(mG)","温度(°C)","状态"});
    m_faultTable->horizontalHeader()->setStretchLastSection(true);
    m_faultTable->verticalHeader()->setVisible(false);
    m_faultTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_faultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_faultTable->setAlternatingRowColors(true);
    m_faultTable->setStyleSheet(QString(
        "QTableWidget { alternate-background-color: %1; }").arg(CLR_BG_ROW_ALT));
    faultV->addWidget(m_faultTable);

    QLabel *lblFaultHint = new QLabel("<span style='color:#555'>点击「读取黑匣子」从F407 W25Q64 Flash 读取历史故障记录</span>");
    lblFaultHint->setStyleSheet("padding:4px;");
    faultV->addWidget(lblFaultHint);

    m_tabs->addTab(faultPage, "故障记录");
    mainV->addWidget(m_tabs, 1);

    // ===== 底部日志（可折叠） =====
    m_gbLog = new QGroupBox("通信日志");
    m_gbLog->setMaximumHeight(110);
    QVBoxLayout *logV = new QVBoxLayout(m_gbLog);
    logV->setContentsMargins(4,2,4,2);
    QHBoxLayout *logBar = new QHBoxLayout;
    logBar->addStretch();
    QPushButton *btnClear = new QPushButton("清空"); btnClear->setMaximumWidth(50);
    btnClear->setStyleSheet("background-color:#333; font-size:10px; padding:1px 8px;");
    connect(btnClear, &QPushButton::clicked, this, &MainWindow::onClearLog);
    logBar->addWidget(btnClear);
    m_log = new QTextEdit; m_log->setReadOnly(true); m_log->setMaximumHeight(76);
    logV->addLayout(logBar); logV->addWidget(m_log);
    mainV->addWidget(m_gbLog);

    statusBar()->showMessage("就绪  |  串口未连接");
}

// ==================== 卡片构建函数 ====================

QWidget* MainWindow::makeCoreCard(const QString &title, const QString &unit,
                                  const QString &color, QLabel *&valOut)
{
    QFrame *bg = new QFrame;
    bg->setStyleSheet(QString(
        "QFrame { background-color:%1; border-radius:4px; }").arg(CLR_BG_CORE));
    QVBoxLayout *vl = new QVBoxLayout(bg);
    vl->setContentsMargins(6,2,6,2); vl->setSpacing(0);
    QLabel *t = new QLabel(title);
    t->setStyleSheet(QString("color:%1; font-size:9px; background:transparent;").arg(CLR_TITLE));
    t->setAlignment(Qt::AlignCenter);
    vl->addWidget(t);
    valOut = new QLabel("--");
    valOut->setFont(QFont("Consolas", 26, QFont::Bold));
    valOut->setAlignment(Qt::AlignCenter);
    valOut->setStyleSheet(QString("color:%1; background:transparent;").arg(color));
    vl->addWidget(valOut);
    QLabel *u = new QLabel(unit);
    u->setStyleSheet(QString("color:%1; font-size:9px; background:transparent;").arg(CLR_SUB));
    u->setAlignment(Qt::AlignCenter);
    vl->addWidget(u);
    return bg;
}

QWidget* MainWindow::makeSecCard(const QString &title, const QString &unit, QLabel *&valOut)
{
    QFrame *bg = new QFrame;
    bg->setStyleSheet(QString("QFrame { background-color:%1; border-radius:3px; }").arg(CLR_BG_PANEL));
    QVBoxLayout *vl = new QVBoxLayout(bg);
    vl->setContentsMargins(6,0,6,0); vl->setSpacing(0);
    QLabel *t = new QLabel(title);
    t->setStyleSheet(QString("color:%1; font-size:8px; background:transparent;").arg(CLR_SUB));
    t->setAlignment(Qt::AlignCenter);
    vl->addWidget(t);
    valOut = new QLabel("--");
    valOut->setFont(QFont("Consolas", 14, QFont::Bold));
    valOut->setAlignment(Qt::AlignCenter);
    valOut->setStyleSheet("color:#ddd; background:transparent;");
    vl->addWidget(valOut);
    if (!unit.isEmpty()) {
        QLabel *u = new QLabel(unit);
        u->setStyleSheet("color:#555; font-size:8px; background:transparent;");
        u->setAlignment(Qt::AlignCenter);
        vl->addWidget(u);
    }
    return bg;
}

// ==================== 实时曲线 ====================

void MainWindow::setupChart()
{
    m_seriesRms = new QLineSeries;
    m_seriesRms->setName("RMS (mG)");
    QPen penRms(QColor(CLR_GREEN)); penRms.setWidth(3);
    m_seriesRms->setPen(penRms);

    m_seriesFreq = new QLineSeries;
    m_seriesFreq->setName("频率 (Hz)");
    QPen penFreq(QColor(CLR_ORANGE)); penFreq.setWidth(3);
    m_seriesFreq->setPen(penFreq);

    m_chart = new QChart;
    m_chart->addSeries(m_seriesRms);
    m_chart->addSeries(m_seriesFreq);
    m_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignTop);
    m_chart->setBackgroundBrush(QBrush(QColor(CLR_BG_MAIN)));
    m_chart->legend()->setLabelColor(QColor(CLR_TITLE));
    m_chart->legend()->setMarkerShape(QLegend::MarkerShapeRectangle);
    m_chart->setMargins(QMargins(0,0,0,0));

    // X轴
    m_axisX = new QDateTimeAxis;
    m_axisX->setFormat("HH:mm:ss");
    m_axisX->setLabelsColor(QColor(CLR_SUB));
    m_axisX->setGridLineColor(QColor("#252830"));
    m_axisX->setTitleVisible(false);
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_seriesRms->attachAxis(m_axisX);
    m_seriesFreq->attachAxis(m_axisX);

    // 左Y轴 (RMS)
    m_axisY = new QValueAxis;
    m_axisY->setLabelsColor(QColor(CLR_GREEN));
    m_axisY->setGridLineColor(QColor("#252830"));
    m_axisY->setTitleText("mG");
    m_axisY->setTitleBrush(QBrush(QColor(CLR_GREEN)));
    m_axisY->setRange(0, 50);
    m_axisY->setLabelFormat("%d");
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    m_seriesRms->attachAxis(m_axisY);

    // 右Y轴 (频率)
    m_axisY2 = new QValueAxis;
    m_axisY2->setLabelsColor(QColor(CLR_ORANGE));
    m_axisY2->setGridLineVisible(false);
    m_axisY2->setTitleText("Hz");
    m_axisY2->setTitleBrush(QBrush(QColor(CLR_ORANGE)));
    m_axisY2->setRange(0, 100);
    m_axisY2->setLabelFormat("%d");
    m_chart->addAxis(m_axisY2, Qt::AlignRight);
    m_seriesFreq->attachAxis(m_axisY2);

    // 悬浮提示
    connect(m_seriesRms, &QLineSeries::hovered, this, [this](const QPointF &pt, bool) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(pt.x()));
        QToolTip::showText(QCursor::pos(),
            QString("时间: %1\nRMS: %2 mG").arg(dt.toString("HH:mm:ss.zzz")).arg(pt.y(), 0, 'f', 1));
    });
    connect(m_seriesFreq, &QLineSeries::hovered, this, [this](const QPointF &pt, bool) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(pt.x()));
        QToolTip::showText(QCursor::pos(),
            QString("时间: %1\n频率: %2 Hz").arg(dt.toString("HH:mm:ss.zzz")).arg(pt.y(), 0, 'f', 1));
    });
}

// ==================== 连接 ====================

void MainWindow::onConnect()
{
    if (m_connected) {
        m_timer->stop();
        if (m_modbus) { m_modbus->disconnectDevice(); delete m_modbus; m_modbus = nullptr; }
        m_connected = false;
        m_btnConn->setText("●  连 接");
        m_btnConn->setObjectName("btnConnect");
        statusBar()->showMessage("已断开");
        appendLog("已断开", CLR_SUB);
        return;
    }

    QString port = m_cbPort->currentData().toString();
    int baud = m_cbBaud->currentText().toInt();
    int slave = m_spSlave->value();
    int interval = m_spIntv->value();

    if (port.isEmpty()) { QMessageBox::warning(this, "错误", "未检测到串口"); return; }
    if (slave < 1 || slave > 247) { QMessageBox::warning(this, "错误", "从站ID范围 1-247"); return; }

    m_modbus = new QModbusRtuSerialClient(this);
    m_modbus->setConnectionParameter(QModbusDevice::SerialPortNameParameter, port);
    m_modbus->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, baud);
    m_modbus->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, QSerialPort::Data8);
    m_modbus->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, QSerialPort::OneStop);
    m_modbus->setConnectionParameter(QModbusDevice::SerialParityParameter, QSerialPort::NoParity);
    m_modbus->setTimeout(3000);   /* 3000ms: 清空黑匣子需擦3个扇区, 可达1秒以上 */
    m_modbus->setNumberOfRetries(0);
    /* USB-RS485(CH340)按USB轮询周期(1ms)分包上传, 长帧被拆成多段,
     * 段间间隙+Windows调度抖动可达数ms, 默认帧间容忍(390us)会提前判帧结束→CRC错。
     * 加大到20ms: 本系统帧间隔最小100ms, 20ms容忍不会造成帧粘连 */
    m_modbus->setInterFrameDelay(20000);

    if (!m_modbus->connectDevice()) {
        QMessageBox::critical(this, "连接失败", m_modbus->errorString());
        delete m_modbus; m_modbus = nullptr;
        return;
    }

    m_connected = true;
    m_errCount = 0; m_pollOk = 0; m_pollFail = 0;
    m_seriesRms->clear(); m_seriesFreq->clear();
    m_dataCache.clear();

    m_btnConn->setText("■  断 开");
    m_btnConn->setObjectName("btnConnected");
    statusBar()->showMessage(QString("已连接 %1 @ %2  |  从站 #%3  |  间隔 %4ms").arg(port).arg(baud).arg(slave).arg(interval));
    appendLog(QString("已连接 %1 @ %2 baud, 从站ID=%3, 间隔=%4ms").arg(port).arg(baud).arg(slave).arg(interval), CLR_GREEN);
    m_timer->start(interval);
    saveSettings();
}

// ==================== 轮询 ====================

void MainWindow::onPoll()
{
    if (!m_modbus || !m_connected || m_pending) return;
    int slave = m_spSlave->value();
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, 0, 7);
    if (auto *reply = m_modbus->sendReadRequest(unit, slave)) {
        if (!reply->isFinished()) {
            m_pending = true;
            connect(reply, &QModbusReply::finished, this, &MainWindow::onReadReady);
        } else {
            delete reply;
        }
    }
}

void MainWindow::onReadReady()
{
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply) return;
    m_pending = false;

    if (reply->error() == QModbusDevice::NoError) {
        m_errCount = 0;
        m_pollOk++;
        updateUI(reply->result());
    } else {
        m_errCount++;
        m_pollFail++;
        if (m_errCount == 10)
            appendLog(QString("连续 %1 次超时 - 可能离线").arg(m_errCount), CLR_RED);
        m_table->item(7, 2)->setText(QString::number(m_errCount));
        m_table->item(7, 3)->setText(QString::number(m_errCount));
    }

    statusBar()->showMessage(
        QString("已连接  |  成功: %1  |  失败: %2").arg(m_pollOk).arg(m_pollFail));
    reply->deleteLater();
}

// ==================== 更新UI ====================

void MainWindow::updateUI(const QModbusDataUnit &unit)
{
    if (unit.valueCount() < 7) return;

    quint16 rms    = unit.value(0);
    quint16 freq   = unit.value(1);
    quint16 amp    = unit.value(2);
    qint16  temp   = static_cast<qint16>(unit.value(3));
    quint32 uptime = (static_cast<quint32>(unit.value(4)) << 16) | unit.value(5);
    quint16 status = unit.value(6);

    // 核心卡片
    bool rmsAlert = rms > 50;
    m_cardRms->setText(QString::number(rms));
    m_cardRms->setStyleSheet(QString("color:%1; background:transparent;").arg(rmsAlert ? CLR_RED : CLR_GREEN));

    bool freqAlert = freq > 100;
    m_cardFreq->setText(QString::number(freq));
    m_cardFreq->setStyleSheet(QString("color:%1; background:transparent;").arg(freqAlert ? CLR_RED : CLR_ORANGE));

    m_cardAmp->setText(QString::number(amp));
    m_cardAmp->setStyleSheet(QString("color:%1; background:transparent;").arg(CLR_BLUE));

    // 告警指示
    m_lblAlertRms->setStyleSheet(QString("color:%1; font-size:10px;").arg(rmsAlert ? CLR_RED : "#555"));
    m_lblAlertFreq->setStyleSheet(QString("color:%1; font-size:10px;").arg(freqAlert ? CLR_RED : "#555"));

    // 次要卡片
    m_cardTemp->setText(temp > 0 ? QString::number(temp / 10.0, 'f', 1) : "--");
    int h = uptime / 3600, m = (uptime % 3600) / 60, s = uptime % 60;
    m_cardUptime->setText(QString("%1:%2:%3").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0')));
    static const char *st[]  = {"正常","螺丝松动","不平衡"};
    static const char *sc[]  = {CLR_GREEN, CLR_ORANGE, CLR_RED};
    bool stAlert = (status >= 1);
    m_cardStatus->setText(status < 3 ? st[status] : "?");
    m_cardStatus->setStyleSheet(QString("color:%1; background:%2; font-size:16px; padding:2px 8px; border-radius:3px;")
        .arg(stAlert ? "#fff" : CLR_GREEN)
        .arg(stAlert ? (status==2?CLR_RED:CLR_ORANGE) : "transparent"));

    // 寄存器表（4列：地址|名称|原始|换算）
    auto setReg = [&](int row, const QString &raw, const QString &conv = "", const QString &color = CLR_GREEN) {
        m_table->item(row, 2)->setText(raw);
        m_table->item(row, 2)->setForeground(QColor(color));
        m_table->item(row, 3)->setText(conv.isEmpty() ? raw : conv);
        m_table->item(row, 3)->setForeground(QColor(color));
    };

    setReg(0, QString::number(rms), "", rmsAlert ? CLR_RED : CLR_GREEN);
    setReg(1, QString::number(freq), "", freqAlert ? CLR_ORANGE : CLR_GREEN);
    setReg(2, QString::number(amp));
    setReg(3, QString::number(temp), temp > 0 ? QString::number(temp/10.0,'f',1) : "--");
    setReg(4, QString::number(unit.value(4)), "", CLR_SUB);
    setReg(5, QString::number(unit.value(5)), QString::number(uptime), CLR_SUB);
    setReg(6, status < 3 ? st[status] : "?", status < 3 ? sc[status] : CLR_SUB);
    setReg(7, QString::number(m_errCount), "", m_errCount > 0 ? CLR_RED : CLR_GREEN);

    // 曲线
    qreal now = QDateTime::currentMSecsSinceEpoch();
    m_seriesRms->append(now, rms);
    m_seriesFreq->append(now, freq);

    qreal cutoff = now - 30000;
    while (m_seriesRms->count() > 0 && m_seriesRms->at(0).x() < cutoff)
        m_seriesRms->removePoints(0, 1);
    while (m_seriesFreq->count() > 0 && m_seriesFreq->at(0).x() < cutoff)
        m_seriesFreq->removePoints(0, 1);

    m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(cutoff)),
                      QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(now)));

    qreal maxRms = 50, maxFreq = 100;
    for (auto &pt : m_seriesRms->points()) if (pt.y() > maxRms) maxRms = pt.y();
    for (auto &pt : m_seriesFreq->points()) if (pt.y() > maxFreq) maxFreq = pt.y();
    m_axisY->setRange(0, maxRms * 1.5);
    m_axisY2->setRange(0, maxFreq * 1.5);

    // 缓存数据（最多3600条≈1小时@1s间隔）
    if (m_dataCache.size() < MAX_CACHE)
        m_dataCache.append({now, rms, freq, amp, temp, uptime, status});
}

// ==================== 配置持久化 ====================

void MainWindow::saveSettings()
{
    m_settings.setValue("port", m_cbPort->currentData().toString());
    m_settings.setValue("baud", m_cbBaud->currentText());
    m_settings.setValue("slave", m_spSlave->value());
    m_settings.setValue("interval", m_spIntv->value());
}

void MainWindow::loadSettings()
{
    QString port = m_settings.value("port", "").toString();
    int idx = m_cbPort->findData(port);
    if (idx >= 0) m_cbPort->setCurrentIndex(idx);

    QString baud = m_settings.value("baud", "115200").toString();
    m_cbBaud->setCurrentText(baud);

    m_spSlave->setValue(m_settings.value("slave", 1).toInt());
    m_spIntv->setValue(m_settings.value("interval", 1000).toInt());
}

// ==================== 视图切换 ====================

void MainWindow::onToggleView()
{
    m_engineerView = !m_engineerView;
    bool eng = m_engineerView;

    m_gbReg->setVisible(eng);
    m_gbLog->setVisible(eng);

    // 卡片尺寸：操作员紧凑 / 工程师放大
    int corePad = eng ? 10 : 6;
    int coreFont = eng ? 30 : 26;
    int secPad   = eng ? 8 : 6;
    int secFont  = eng ? 16 : 14;

    for (auto *w : m_coreFrames) {
        auto *vl = qobject_cast<QVBoxLayout *>(w->layout());
        if (vl) vl->setContentsMargins(corePad, eng ? 4 : 2, corePad, eng ? 4 : 2);
        // 找数字标签
        auto *val = w->findChild<QLabel *>();
        if (val && val->font().pixelSize() < 0)
            val->setFont(QFont("Consolas", coreFont, QFont::Bold));
    }

    for (auto *w : m_secFrames) {
        auto *vl = qobject_cast<QVBoxLayout *>(w->layout());
        if (vl) vl->setContentsMargins(secPad, 0, secPad, 0);
    }

    // 操作员视图：左侧更窄，图表更大
    QSplitter *sp = findChild<QSplitter *>();
    if (sp) sp->setSizes(eng ? QList<int>{320, 870} : QList<int>{280, 910});

    m_btnView->setText(eng ? "操作员视图" : "工程师视图");
    appendLog(eng ? "切换到工程师视图" : "切换到操作员视图", CLR_BLUE);
}

// ==================== 日志 ====================

void MainWindow::appendLog(const QString &msg, const QString &color)
{
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_log->append(QString("<span style='color:#444'>[%1]</span> "
        "<span style='color:%2'>%3</span>").arg(ts, color, msg));
}

void MainWindow::onExportCSV()
{
    if (m_dataCache.isEmpty()) {
        appendLog("无数据可导出，请先连接并采集", CLR_ORANGE);
        return;
    }

    QString defaultName = QString("振动数据_%1.csv")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    QString path = QFileDialog::getSaveFileName(this, "导出CSV", defaultName,
                                                 "CSV 文件 (*.csv);;所有文件 (*)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendLog("文件写入失败: " + path, CLR_RED);
        return;
    }

    QTextStream out(&f);
    out << "\xEF\xBB\xBF";  // UTF-8 BOM（Excel不乱码）

    out << "时间,RMS(mG),频率(Hz),幅值(mG),温度(°C),运行时长(s),状态\n";

    static const char *st[] = {"正常","螺丝松动","不平衡"};
    for (const auto &dp : m_dataCache) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(dp.t));
        out << dt.toString("yyyy-MM-dd HH:mm:ss") << ","
            << dp.rms << ","
            << dp.freq << ","
            << dp.amp << ","
            << (dp.temp > 0 ? QString::number(dp.temp / 10.0, 'f', 1) : "0.0") << ","
            << dp.uptime << ","
            << (dp.status < 3 ? st[dp.status] : "未知") << "\n";
    }

    f.close();
    appendLog(QString("已导出 %1 条记录 → %2").arg(m_dataCache.size()).arg(path), CLR_GREEN);
    statusBar()->showMessage(QString("导出完成: %1 条记录").arg(m_dataCache.size()));
}

void MainWindow::onClearBlackbox()
{
    if (!m_modbus || !m_connected) {
        appendLog("请先连接设备", CLR_ORANGE);
        return;
    }

    // 写寄存器6 = 0xCC00 → F407清空黑匣子
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, 6, 1);
    unit.setValue(0, 0xCC00);
    if (auto *reply = m_modbus->sendWriteRequest(unit, m_spSlave->value())) {
        connect(reply, &QModbusReply::finished, this, [this]() {
            auto *r = qobject_cast<QModbusReply *>(sender());
            if (!r) return;
            if (r->error() != QModbusDevice::NoError) {
                appendLog("清空黑匣子失败: " + r->errorString(), CLR_RED);
            } else {
                appendLog("黑匣子已清空 (0 条记录)", CLR_GREEN);
                m_faultTable->setRowCount(0);
            }
            r->deleteLater();
        });
    }
}

void MainWindow::onReadFaults()
{
    if (!m_modbus || !m_connected) {
        appendLog("请先连接设备", CLR_ORANGE);
        return;
    }

    // 暂停轮询, 避免请求穿插
    m_timer->stop();

    int slave = m_spSlave->value();

    // 第1步: 读黑匣子总数 (寄存器24)
    QModbusDataUnit countUnit(QModbusDataUnit::HoldingRegisters, 24, 1);
    if (auto *reply = m_modbus->sendReadRequest(countUnit, slave)) {
        if (reply->isFinished()) {
            delete reply;
            appendLog("读取黑匣子失败", CLR_RED);
            return;
        }
        connect(reply, &QModbusReply::finished, this, [this, slave](void) {
            auto *r = qobject_cast<QModbusReply *>(sender());
            if (!r) return;
            if (r->error() != QModbusDevice::NoError) {
                appendLog("读取黑匣子总数失败: " + r->errorString(), CLR_RED);
                r->deleteLater();
                return;
            }
            int total = r->result().value(0);
            r->deleteLater();
            if (total == 0) {
                appendLog("黑匣子无记录", CLR_SUB);
                m_faultTable->setRowCount(0);
                return;
            }
            appendLog(QString("黑匣子共 %1 条记录, 开始读取...").arg(total), CLR_GREEN);

            // 清空表格
            m_faultTable->setRowCount(0);

            // 第2步: 逐条读取, 间隔100ms避免RS485收发冲突
            QTimer::singleShot(100, this, [this, slave, total]() {
                readBlackboxRecord(slave, 0, total);
            });
        });
    }
}

void MainWindow::readBlackboxRecord(int slave, int index, int total, int retryLeft)
{
    if (index >= total) {
        appendLog(QString("黑匣子读取完成, 共 %1 条").arg(total), CLR_GREEN);
        // 恢复轮询
        int interval = m_spIntv->value();
        m_timer->start(interval);
        return;
    }

    // 写寄存器25 = index
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 25, 1);
    writeUnit.setValue(0, index);
    if (auto *reply = m_modbus->sendWriteRequest(writeUnit, slave)) {
        if (reply->isFinished()) {
            reply->deleteLater();
            if (retryLeft > 0)
                QTimer::singleShot(100, this, [this, slave, index, total, retryLeft]() {
                    readBlackboxRecord(slave, index, total, retryLeft - 1);
                });
            else
                QTimer::singleShot(100, this, [this, slave, index, total]() {
                    readBlackboxRecord(slave, index + 1, total);
                });
            return;
        }
        connect(reply, &QModbusReply::finished, this, [this, slave, index, total, retryLeft](void) {
            auto *w = qobject_cast<QModbusReply *>(sender());
            if (!w) return;
            if (w->error() != QModbusDevice::NoError) {
                appendLog(QString("写索引 %1 失败%2").arg(index).arg(retryLeft > 0 ? ", 重试" : ", 跳过"), CLR_RED);
                w->deleteLater();
                if (retryLeft > 0)
                    QTimer::singleShot(100, this, [this, slave, index, total, retryLeft]() {
                        readBlackboxRecord(slave, index, total, retryLeft - 1);
                    });
                else
                    QTimer::singleShot(100, this, [this, slave, index, total]() {
                        readBlackboxRecord(slave, index + 1, total);
                    });
                return;
            }
            w->deleteLater();

            // 等100ms让RS485收发切换充分完成, 再发读请求 (间隔太紧会丢帧开头字节)
            QTimer::singleShot(100, this, [this, slave, index, total, retryLeft]() {
            QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, 26, 8);
            if (auto *r2 = m_modbus->sendReadRequest(readUnit, slave)) {
                if (r2->isFinished()) {
                    r2->deleteLater();
                    if (retryLeft > 0)
                        QTimer::singleShot(100, this, [this, slave, index, total, retryLeft]() {
                            readBlackboxRecord(slave, index, total, retryLeft - 1);
                        });
                    else
                        QTimer::singleShot(100, this, [this, slave, index, total]() {
                            readBlackboxRecord(slave, index + 1, total);
                        });
                    return;
                }
                connect(r2, &QModbusReply::finished, this, [this, slave, index, total, retryLeft](void) {
                    auto *r = qobject_cast<QModbusReply *>(sender());
                    if (!r) return;
                    if (r->error() != QModbusDevice::NoError) {
                        appendLog(QString("读索引 %1 失败%2").arg(index).arg(retryLeft > 0 ? ", 重试" : ", 跳过"), CLR_RED);
                        r->deleteLater();
                        if (retryLeft > 0)
                            QTimer::singleShot(100, this, [this, slave, index, total, retryLeft]() {
                                readBlackboxRecord(slave, index, total, retryLeft - 1);
                            });
                        else
                            QTimer::singleShot(100, this, [this, slave, index, total]() {
                                readBlackboxRecord(slave, index + 1, total);
                            });
                        return;
                    }

                    QModbusDataUnit unit = r->result();
                    r->deleteLater();

                    quint32 timestamp = (quint32(unit.value(0)) << 16) | unit.value(1);
                    quint16 rms   = unit.value(2);
                    quint16 freq  = unit.value(3);
                    quint16 amp   = unit.value(4);
                    qint16  temp  = qint16(unit.value(5));
                    quint16 bin   = unit.value(6);
                    quint16 st    = unit.value(7);

                    int row = m_faultTable->rowCount();
                    m_faultTable->insertRow(row);
                    m_faultTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
                    m_faultTable->setItem(row, 1, new QTableWidgetItem(QString::number(timestamp)));
                    m_faultTable->setItem(row, 2, new QTableWidgetItem(QString::number(rms)));
                    m_faultTable->setItem(row, 3, new QTableWidgetItem(QString::number(freq)));
                    m_faultTable->setItem(row, 4, new QTableWidgetItem(QString::number(amp)));
                    m_faultTable->setItem(row, 5, new QTableWidgetItem(temp > 0 ? QString::number(temp / 10.0, 'f', 1) : "--"));
                    m_faultTable->setItem(row, 6, new QTableWidgetItem(st == 2 ? "不平衡" : st == 1 ? "螺丝松动" : "正常"));

                    // 继续下一条, 延时100ms
                    QTimer::singleShot(100, this, [this, slave, index, total]() {
                        readBlackboxRecord(slave, index + 1, total);
                    });
                });
            }
            });
        });
    }
}

void MainWindow::onClearLog() { m_log->clear(); }
