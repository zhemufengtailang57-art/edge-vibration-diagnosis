#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QSerialPortInfo>
#include <QSerialPort>
#include <QTimer>
#include <QLabel>
#include <QTableWidget>
#include <QTextEdit>
#include <QGroupBox>
#include <QSettings>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <QTabWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnect();
    void onPoll();
    void onReadReady();
    void onClearLog();
    void onToggleView();
    void onExportCSV();
    void onReadFaults();
    void onClearBlackbox();
    void readBlackboxRecord(int slave, int index, int total, int retryLeft = 2);

private:
    Ui::MainWindow *ui;

    QModbusRtuSerialClient *m_modbus = nullptr;
    QTimer *m_timer = nullptr;
    bool m_connected = false;
    bool m_pending = false;       // 等待回复中
    bool m_engineerView = true;   // 默认工程师视图
    QPushButton *m_btnConn = nullptr;
    QToolButton *m_btnView = nullptr;
    QTabWidget *m_tabs = nullptr;
    QTableWidget *m_faultTable = nullptr;

    // 数据缓存（导出用，最多保存3600条=1小时）
    struct DataPoint { qreal t; quint16 rms; quint16 freq; quint16 amp;
                       qint16 temp; quint32 uptime; quint16 status; };
    QVector<DataPoint> m_dataCache;
    static constexpr int MAX_CACHE = 3600;
    QComboBox *m_cbPort = nullptr;
    QComboBox *m_cbBaud = nullptr;
    QSpinBox *m_spSlave = nullptr;
    QSpinBox *m_spIntv = nullptr;
    int m_errCount = 0;
    int m_pollOk = 0;
    int m_pollFail = 0;

    // 核心卡片
    QLabel *m_cardRms, *m_cardFreq, *m_cardAmp;
    QLabel *m_cardTemp, *m_cardUptime, *m_cardStatus;
    QWidget *m_coreFrames[3];   // 核心卡片3个
    QWidget *m_secFrames[3];    // 次要卡片3个

    // 表 & 日志容器
    QGroupBox *m_gbReg, *m_gbLog;
    QTableWidget *m_table;
    QTextEdit *m_log;

    // 图表
    QLineSeries *m_seriesRms, *m_seriesFreq;
    QChart *m_chart;
    QChartView *m_chartView;
    QDateTimeAxis *m_axisX;
    QValueAxis *m_axisY, *m_axisY2;

    // 阈值告警
    QLabel *m_lblAlertRms, *m_lblAlertFreq;

    // 配置
    QSettings m_settings;

    void setupUI();
    void setupStyle();
    void setupChart();
    void updateUI(const QModbusDataUnit &unit);
    void appendLog(const QString &msg, const QString &color = "#8892A0");
    void saveSettings();
    void loadSettings();

    // 卡片构建
    QWidget* makeCoreCard(const QString &title, const QString &unit,
                          const QString &color, QLabel *&valOut);
    QWidget* makeSecCard(const QString &title, const QString &unit,
                         QLabel *&valOut);
};

#endif
