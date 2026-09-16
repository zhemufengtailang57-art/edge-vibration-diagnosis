# 工业设备振动监测与边缘故障诊断系统

> 基于 STM32F407 + FreeRTOS 的电机故障边缘诊断终端 —— **推理全程本地化，数据不出厂区**

针对电机、风机等旋转设备的不平衡、基座松动等常见故障，传统云端诊断方案存在网络依赖与数据外泄风险，无法满足军工及高端制造客户"数据不出厂区"的要求。本项目把 FFT 特征提取、神经网络推理、故障存储全部放在 MCU 端完成，仅上报诊断结论，原始振动数据不出设备。

---

## 一、关键指标

| 指标 | 数值 | 说明 |
|------|------|------|
| 振动采样率 | 1 kHz | TIM4 + DMA 双缓冲，不占用 CPU |
| FFT | 128 点，7.81 Hz 分辨率 | 手写基-2 算法 |
| 模型结构 | MLP `5 → 16 → 8 → 3` | 259 个参数 |
| 模型体积 | **259 B**（int8） / 1036 B（FP32） | 对称量化，压缩 75% |
| 量化精度损失 | **0.00 个百分点** | 测试集 96.2% → 96.2% |
| 单帧推理延迟 | < 1 ms | 占 200 ms 任务周期 < 0.5% |
| 数据集 | 388 组（131 正常 / 122 松动 / 135 不平衡） | 自建，3 类工况 |
| 5 折交叉验证 | **97.7% ± 2.5%** | 独立测试集 96.2% |
| 黑匣子容量 | 500 条 × 16 B | W25Q64 SPI Flash，断电不丢失 |

---

## 二、系统架构

```
                    ┌──────────────────────────────────────────────────┐
                    │            STM32F407ZGT6 @ 168 MHz               │
                    │            FreeRTOS (CMSIS-OS v2)                │
                    │                                                  │
   ADXL345 ──SPI1───┤ sensorTask  [AboveNormal, 4 KB]                  │
   1 kHz 振动        │   ├── TIM4 + DMA 双缓冲 @ 1 kHz                  │
   (5.25 MHz)       │   ├── 128 点 FFT → 7.81 Hz 分辨率                 │
                    │   ├── 5 维鲁棒特征 (RMS + bin1/2/6/7)             │
                    │   └── int8 MLP 推理 (5→16→8→3, 259 参数)          │
                    │                                                  │
   W25Q64 ──SPI2───┤ 事件型黑匣子：仅在工况变化时落盘 (600 ms 防抖)      │
   黑匣子            │                                                  │
   (2.625 MHz)      │ modbusTask  [High, 2 KB] ──USART3──RS485──▶ 上位机 │
                    │ displayTask [Normal, 2 KB] ──I2C1──▶ SSD1306 OLED│
   ESP32 ──USART1──┤ watchdogTask[BelowNormal] + TIM4 喂狗 (IWDG)      │
   MQTT 无线         │                                                  │
   (备用通道)        │ 日志 USART2 ──▶ 串口调试                          │
                    └──────────────────────────────────────────────────┘
```

### 任务与优先级

| 任务 | 优先级 | 栈 | 职责 |
|------|--------|-----|------|
| `modbusTask` | **High** | 2 KB | Modbus 收发，响应发送期间不被打断 |
| `sensorTask` | AboveNormal | 4 KB | 采样 → FFT → 特征 → 推理 → 黑匣子 |
| `displayTask` | Normal | 2 KB | OLED 刷新 |
| `watchdogTask` | BelowNormal | 1 KB | 系统健康监控 |

`spi2Mutex` 保护 Flash 黑匣子的多任务访问；TIM4 中断承担 1 kHz 采样节拍与看门狗喂狗。

---

## 三、目录结构

```
.
├── firmware/          STM32F407 固件 (Keil MDK + STM32CubeMX)
│   ├── Core/Src/      应用层与驱动层
│   │   ├── main.c             任务调度、OLED 布局、事件触发落盘
│   │   ├── adxl345_dma.c      ADXL345 驱动 + DMA 双缓冲采样
│   │   ├── fft_analyzer.c     128 点 FFT 与特征提取
│   │   ├── ai_classifier.c    int8 MLP 手写推理引擎
│   │   ├── modbus_slave.c     Modbus-RTU 从站协议栈
│   │   ├── w25q64.c           SPI Flash 驱动 + 黑匣子
│   │   ├── ssd1306.c          OLED 驱动 (含中文字库)
│   │   ├── mqtt_simple.c      轻量 MQTT 客户端
│   │   └── log.c              分级日志 (vTaskSuspendAll 保护)
│   ├── Core/Inc/ai_model.h    int8 量化权重 (自动生成)
│   ├── MDK-ARM/               Keil 工程文件
│   └── Vibration_F407.ioc     CubeMX 配置 (可重新生成工程)
│
├── host-qt/           Qt 6 上位机 (CMake)
│   ├── mainwindow.cpp         波形显示 / 频谱分析 / 诊断可视化 / 黑匣子回放
│   └── CMakeLists.txt
│
├── ai/                Python 全流程 (采集 → 训练 → 量化 → 导出)
│   ├── collect_data.py        串口采集振动数据
│   ├── train_simple.py        最终部署模型训练 (5 维鲁棒特征)
│   ├── train_final2.py        17 维对比实验
│   ├── quantize_test.py       int8 对称量化验证
│   ├── export_simple.py       导出 C 头文件 → firmware/Core/Inc/ai_model.h
│   ├── cross_speed_test.py    跨转速泛化实验 (见已知限制)
│   ├── dataset (*.csv)        388 组振动特征数据
│   └── reports (*.txt)        各阶段实验报告
│
├── esp32/             ESP32 无线通道 (Arduino)
└── docs/              开发日志 / 调试踩坑记录 / AI 研发记录
```

---

## 四、技术栈

| 层面 | 选型 |
|------|------|
| 主控 | STM32F407ZGT6（Cortex-M4 @168 MHz, FPU） |
| 实时内核 | FreeRTOS（CMSIS-OS v2 封装） |
| 传感器 | ADXL345 三轴加速度计（SPI1, 5.25 MHz） |
| 存储 | W25Q64 SPI Flash（SPI2, 2.625 MHz） |
| 显示 | SSD1306 OLED 128×64（I2C1, 含 16×16 中文字库） |
| 通信 | RS485 / Modbus-RTU（USART3）、ESP32 + MQTT（USART1） |
| 算法 | 手写基-2 FFT、轻量 MLP、int8 对称量化 |
| 上位机 | Qt 6 / C++（QModbusRtuSerialClient） |
| 工具链 | Keil MDK 5 + STM32CubeMX / CMake / Python 3 |

---

## 五、快速开始

### 1. 固件

```bash
# 方式 A：直接用 Keil 打开工程
firmware/MDK-ARM/Vibration_F407.uvprojx

# 方式 B：用 CubeMX 从 .ioc 重新生成（会覆盖 Core/ 下 CubeMX 托管的代码）
firmware/Vibration_F407.ioc
```

> **注意**：`Core/Inc/ai_model.h` 由 `ai/export_simple.py` 自动生成，重新生成工程时不要覆盖。
> 烧录后波特率固定 **115200 8N1**（RS485 自动方向模块要求，详见调试踩坑记录）。

### 2. 上位机

```bash
cd host-qt
cmake -B build -DCMAKE_PREFIX_PATH=<Qt6 安装路径>
cmake --build build
```

### 3. AI 全流程（复现训练与量化）

```bash
cd ai
pip install numpy scipy pyserial

python train_simple.py      # 训练 5 维鲁棒特征模型 → mlp_simple.npz
                            # 输出：5 折交叉验证 97.7%±2.5%，测试集 96.2%

python quantize_test.py     # int8 对称量化验证 → quant_report.txt
                            # 输出：精度损失 0.00 个百分点，权重 1036B → 259B

python export_simple.py     # 导出 C 头文件 → ../firmware/Core/Inc/ai_model.h
```

脚本会自动切到所在目录，数据文件与脚本同目录，可在任意路径下运行。

### 4. 无线通道（可选）

```bash
cd ai
启动WiFi监视.bat             # 启动 mosquitto + MQTT 桥接
# 手机浏览器打开 phone_monitor.html 即可远程查看
```

---

## 六、Modbus-RTU 寄存器映射

从站地址 `0x01`，支持功能码 `0x03`（读保持寄存器）、`0x06`（写单寄存器）。

### 保持寄存器

| 地址 | 名称 | 说明 |
|------|------|------|
| 0 | `VIB_RMS` | 振动 RMS（mG） |
| 1 | `VIB_FREQ` | 主频（Hz） |
| 2 | `VIB_AMP` | 峰值幅值（mG） |
| 3 | `TEMP` | 温度 ×10（°C） |
| 4–5 | `UPTIME` | 运行时间（秒，32 位，高字在前） |
| 6 | `STATUS` | 0=正常 / 1=预警 / 2=故障；**写 `0xCC00` 清空黑匣子** |
| 7–22 | `SPEC[16]` | 16 个频点能量，频点 *i* = *i* × 7.81 Hz |
| 23 | `MOTOR` | 电机控制：0=停，1~999=占空比 |
| 24 | `BB_COUNT` | 黑匣子记录总数（只读） |
| 25 | `BB_READ_IDX` | 写 N → 第 N 条记录装入 26~33 |
| 26–33 | `BB_REC[8]` | 记录内容：时间戳 / 工况 / RMS / 频率 / 温度 / 置信度 |

### 输入寄存器（0x04）

| 地址 | 名称 |
|------|------|
| 0 | `RMS_RAW`（LSB） |
| 1 | `FREQ_BIN`（FFT 峰值 bin） |

---

## 七、黑匣子设计

采用 **事件型**而非定时型落盘：

- **触发条件**：推理工况发生变化（`stable_class != prev_stable`）且持续 600 ms 防抖确认
- **记录格式**：16 字节/条，共 500 条，环形覆盖
- **断电保护**：数据存于 W25Q64，上电后可远程回放
- **读取接口**：上位机写记录索引 → 读寄存器 26~33，带 2 次重试

> **为什么是事件型？** 定时型在设备长期稳定运行时会产生大量重复记录，500 条容量几天就写满。事件型只记录"工况发生变化"的关键时刻，同样的容量可覆盖数月。

---

## 八、数据集与模型

### 数据采集

通过串口以 200 ms 周期采集运行中的电机振动特征，覆盖 **3 类工况 × 5 档转速**（60/70/80/90/99% 占空比），共 19 组采集文件。

最终训练集为 **388 组**（60% 转速下）：

| 工况 | 样本数 | RMS 均值 (mG) | 主频均值 (Hz) |
|------|--------|---------------|---------------|
| 正常 | 131 | 339 | 25 |
| 螺丝松动 | 122 | 415 | 55 |
| 转子不平衡 | 135 | 1908 | 78 |

三类故障在 RMS 与主频上区分度明显，这也是最终选用 **5 维鲁棒特征**（RMS + 4 个关键频点）而非全部 17 维的原因。

### 为什么不是 17 维？

初版用 RMS + 全部 16 个 FFT 频点（17 维）训练，单转速下准确率很高（98.4%），但**跨转速泛化极差**（详见 `ai/cross_speed_report.txt`，70% 转速下松动识别率跌到 0%）。

原因：全频谱特征把「转速」本身编码进了特征（转速不同则能量峰位置整体平移），模型学到的是"转速指纹"而非"故障特征"。改用 RMS + 4 个与转速无关的鲁棒频点后，单工况精度略降但特征物理意义明确、可解释。

---

## 九、设计取舍与已知限制

诚实说明边界，避免过度宣称：

1. **模型为单工况模型**：388 组数据均在 60% 转速下采集，跨转速泛化能力有限。若要覆盖全转速范围，需要采集全转速数据并做数据增强或域自适应。
2. **数据集规模有限**：388 组样本、单一电机测试台，97.7% 的交叉验证精度不代表工业现场的泛化性能。
3. **黑匣子容量**：500 条记录，事件触发下可覆盖数月，但高频工况切换场景仍可能写满。
4. **RS485 波特率锁定 115200**：受自动方向模块的 DE 撤销超时限制，降速到 57600 会导致帧尾被截断（见调试踩坑记录）。
5. **ESP32 无线通道为辅助**：有线 Modbus 为主链路，无线通道用于远程查看，不作为控制通路。

---

## 十、文档

`docs/` 目录下为项目过程的完整记录：

| 文档 | 内容 |
|------|------|
| `项目开发日志.docx` | Day 1–13 完整开发时间线 |
| `调试踩坑记录.docx` | 26 个真实调试案例，按"现象→排查→根因→解决→教训"五段式整理 |
| `AI研发记录.md` | 从零讲解神经网络与 int8 量化，含 17 维特征失败复盘 |
| `CubeMX配置速查手册.docx` | 时钟树、外设、中断优先级配置速查 |
| `软件交接文档.md` | 模块职责、关键接口、遗留问题 |

---

## 十一、License

本项目为个人学习与作品展示用途。第三方组件版权归各自所有者：

- `firmware/Drivers/` — STMicroelectronics HAL 库（BSD-3-Clause）
- `firmware/Middlewares/` — FreeRTOS（MIT）
- `firmware/Drivers/CMSIS/` — ARM CMSIS（Apache-2.0）
