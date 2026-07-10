# ESP32-S3 桌面对讲按键

[home-intercom](https://github.com/mdj2812/home-intercom) 系统的配套硬件——按下录音，松开广播到目标房间的小爱音箱。

## 硬件

| 组件 | 说明 |
|------|------|
| **MCU** | ESP32-S3-DevKitC (WROOM-1 N8) |
| **麦克风** | MAX9814 驻极体麦克风模块，带 AGC（约 ¥5-6） |
| **按键** | 任意自复位按键（约 ¥2） |
| **BOM 总计** | ~¥8（MCU 已有） |

### 接线

| ESP32-S3 | MAX9814 | 按键 |
|----------|---------|------|
| GPIO1 (ADC1_CH0) | OUT | — |
| 3.3V | VCC | — |
| GND | GND | 一脚 |
| GPIO4 | — | 另一脚 → GND |

MAX9814 增益：将 GAIN 焊盘接地获得 50dB（桌面使用推荐）。

### 设备配置

配置存储在 LittleFS（`data/config.json`）中，不在源码里。同一份固件适配所有房间——只需上传不同的配置文件。

```json
{
    "wifi_ssid": "你的WiFi",
    "wifi_password": "你的密码",
    "server_host": "192.168.99.10",
    "server_port": 8764,
    "room": "study",
    "sample_rate": 16000,
    "max_record_secs": 60
}
```

房间键值（来自 `rooms.json`）：`study`、`living`、`cinema`、`bedroom`，或 `all` 广播所有房间。

复制 `data/config.example.json` 为 `data/config.json` 并填入你的设置。`data/config.json` 已加入 `.gitignore`——凭证不会泄露。

**多按键部署**：烧录一次固件，然后每个设备修改 `data/config.json`（改 `room`）后执行 `pio run -e esp32-s3-devkitc-1 -t uploadfs`。

## 快速开始

### 使用 Make（推荐）

```bash
make            # 编译固件
make flash      # 编译 + USB 烧录
make flashfs    # 上传 LittleFS 配置 (data/config.json)
make monitor    # 串口监视器
make test       # 运行单元测试（无需硬件）
make check      # 静态分析
make clean      # 清理编译产物
make format     # 自动格式化代码
make size       # 显示固件内存使用
```

### Docker

```bash
# 拉取镜像并进入交互式 shell
./docker/dev.sh

# 通过 Docker 执行单次命令
make docker-build     # 容器内编译
make docker-flash     # 容器内烧录（自动挂载 USB）
make docker-flashfs   # 上传 LittleFS 配置
make docker-test      # 容器内单元测试
make docker-shell     # 容器内交互式 shell
```

### 裸机（无 Docker）

同样的 `make` 命令——只需安装 [PlatformIO Core](https://platformio.org/install/cli)：

```bash
pip install platformio
make flash
```

## 命令

| 命令 | 功能 |
|------|------|
| `make` | 编译固件 |
| `make flash` | 编译 + USB 烧录 |
| `make flashfs` | 上传 LittleFS 配置 (`data/config.json`) |
| `make monitor` | 打开串口监视器 |
| `make test` | 在宿主机运行单元测试（无需 ESP32） |
| `make check` | 静态分析 (cppcheck) |
| `make clean` | 清理编译产物 |
| `make format` | 用 clang-format 自动格式化代码 |
| `make format-check` | 检查格式（CI 用） |
| `make size` | 显示固件内存使用 |

Docker 变体：加 `docker-` 前缀（如 `make docker-build`、`make docker-flash`）。

原生 PlatformIO 命令（如果你喜欢）：

| 命令 | 功能 |
|------|------|
| `pio run -e esp32-s3-devkitc-1` | 编译固件 |
| `pio run -e esp32-s3-devkitc-1 -t upload` | 编译 + 烧录 |
| `pio run -e esp32-s3-devkitc-1 -t uploadfs` | 上传 LittleFS 配置 |
| `pio run -e esp32-s3-devkitc-1 -t clean` | 清理编译产物 |
| `pio test -e native` | 运行单元测试 |
| `pio check -e esp32-s3-devkitc-1` | 静态分析 |

### 首次 USB 烧录

1. 用 USB-C 连接 ESP32-S3
2. `make flash`
3. `make flashfs`（编辑好 `data/config.json` 后）

### Docker 专用

```bash
# 交互式开发 shell
./docker/dev.sh

# 便捷 make 目标（自动处理镜像和 USB）
make docker-build
make docker-flash
make docker-test
make docker-shell

# 本地重建 Docker 镜像
./docker/dev.sh -b
```

## 架构

```text
┌──────────────┐   16kHz ADC     ┌───────────────┐   WAV POST   ┌──────────────┐
│  MAX9814 Mic │ ──────────────→ │  ESP32-S3     │ ───────────→ │ Flask Server │
│  (模拟信号)   │   GPIO1 (ADC)   │  PSRAM 缓冲区  │  /record     │  :8764       │
└──────────────┘                 │  WiFi STA      │              └──────┬───────┘
                                 │  WS2812 LED    │                     │
┌──────────────┐                 │  按键           │              ┌──────▼───────┐
│   按键       │ ──────────────→ │  (低电平有效)   │              │  Home        │
│   (GPIO4)    │   中断 + 上拉   └───────────────┘              │  Assistant   │
└──────────────┘                                                 │  play_media  │
                                                                 └──────────────┘
```

### LED 状态

| 颜色 | 含义 |
|------|------|
| 🟢 绿色 | 就绪（WiFi 已连接，空闲） |
| 🔴 红色闪烁 | WiFi 断开 |
| 🔵 蓝色 | 录音中 |
| ⚪ 白色闪烁 | 上传中 |
| 🟢 闪 4 次 | 上传成功 |
| 🔴 闪 4 次 | 上传失败 |

### 录音行为

- **按住说话**：按下按键 → 录音，松开 → 发送
- **最小录音时长**：500ms（更短的轻触被丢弃）
- **最大录音时长**：在 `data/config.json` 中配置（`max_record_secs`，默认 60 秒）
- **格式**：16-bit PCM WAV、16kHz 单声道

## 项目结构

```text
intercom-button/
├── .clang-format            # C++ 代码风格规则
├── .editorconfig            # 编辑器设置
├── Makefile                 # 便捷命令
├── platformio.ini           # PlatformIO 项目配置
├── .github/
│   └── workflows/ci.yml     # CI 流水线（编译、检查、测试、格式、覆盖率）
├── docker/
│   ├── Dockerfile           # 自包含开发镜像
│   ├── .docker-image        # 镜像版本
│   └── dev.sh               # 一键开发容器
├── data/
│   ├── config.example.json  # 模板（已提交）
│   └── config.json          # 你的设置（gitignore，上传到 LittleFS）
├── test/
│   ├── mocks/               # 模拟 Arduino/ESP 头文件
│   ├── test_config_manager/ # 配置解析测试
│   ├── test_http_uploader/  # HTTP 上传测试
│   ├── test_wifi_manager/   # WiFi 管理测试
│   ├── test_button_manager/ # 按键管理测试
│   └── test_room_target_store/ # 房间存储测试
└── src/
    ├── main.cpp             # 状态机：IDLE→RECORDING→UPLOADING
    ├── config.h             # 引脚定义
    ├── config_manager.h/cpp # JSON 配置加载器 (LittleFS)
    ├── wifi_manager.h/cpp   # 非阻塞 WiFi + 自动重连
    ├── audio_recorder.h/cpp # 16kHz 定时器 ISR → PSRAM → WAV
    ├── http_uploader.h/cpp  # POST /record?target=<room>
    ├── button_manager.h/cpp # 多按键 GPIO 矩阵 + 消抖
    ├── room_target_store.h/cpp # NVS 房间目标存储
    └── consts.hpp           # 共享常量
```

## 编译环境

本项目有**两个 PlatformIO 环境**——使用时务必指定：

| 环境 | 平台 | 用途 |
|------|------|------|
| `esp32-s3-devkitc-1` | xtensa-esp32s3 | 固件——编译、烧录、监视 |
| `native` | x86_64 Linux | 单元测试——宿主机运行，无需 ESP32 |

不加 `-e` 时 `pio run` 会编译**两个**环境。原生环境设置了 `src_filter = -<src/*>`，只编译测试文件——绝不编译依赖 Arduino 的源码。

```bash
# ✅ 正确
make                    # 或：pio run -e esp32-s3-devkitc-1
make test               # 或：pio test -e native

# ❌ 错误——会尝试同时编译原生环境，导致 Arduino.h 报错
pio run                 # （除非你指定 -e）
```

## 依赖

| 库 | 版本 | 用途 |
|-----|------|------|
| Adafruit NeoPixel | ^1.12.0 | WS2812 RGB LED 控制 |
| ArduinoJson | ^6.21.0 | JSON 配置解析 |
| Arduino-ESP32 | ~3.20017.0 | HAL、WiFi、HTTPClient、LittleFS |
| ESP32-S3 工具链 | 8.4.0+2021r2 | Xtensa + RISC-V 编译器 |

全部由 PlatformIO 管理——无需手动安装。

## 代码风格

C++ 代码遵循 [`.clang-format`](.clang-format)（基于 LLVM，4 空格缩进，120 字符宽度）。编辑器设置在 [`.editorconfig`](.editorconfig)。

```bash
make format        # 自动格式化所有源文件
make format-check  # 检查格式但不修改（CI）
```

CI 强制格式检查——违反格式规则的 PR 会通不过 `format` 任务。

## 调试

### 串口监视器

```bash
make monitor
```

固件会打印每次状态转换：

```text
=== ESP32-S3 Intercom Button ===
[cfg] Loaded: room=study server=192.168.99.10:8764 wifi=MyWiFi
[wifi] Connecting to MyWiFi...
[wifi] Connected
[audio] Buffer: 960000 samples (60 sec), PSRAM free: 7654 KB
[audio] Ready — ISR @ 16000 Hz
[main] Setup complete — ready.
[main] Recording...
[audio] Stopped — 48000 samples (3.0s)
[upload] POST http://192.168.99.10:8764/record?target=study (96044 bytes) attempt 1/3
[upload] OK: {"ok":true,"rooms_sent":1,...}
[main] Upload OK (1725 ms)
```

### 常见问题

| 现象 | 可能原因 | 解决 |
|------|---------|------|
| 红色 LED 闪烁 | WiFi 连接不上 | 检查 `data/config.json` 中的 SSID/密码，用 `make flashfs` 重新上传 |
| 烧录失败 | USB 端口不对 | `pio device list`，然后 `make flash`（自动检测） |
| 录音但不上传 | 服务器不可达 | 检查 `data/config.json` 中的 `server_host` |
| 上传超时（ESP32 报失败但声音已播放） | HA 响应太慢 | 已知良性问题——音频已送达，重试逻辑会处理 |
| 上传成功但没声音 | 房间键值不对 | 确认 `data/config.json` 中的 `room` 与 `rooms.json` 的键一致 |
| 配置不加载 | LittleFS 未烧录 | 运行 `make flashfs` 上传文件系统 |
| PSRAM 分配警告 | 板子变体不匹配 | 检查 `platformio.ini` 中 `board_build.psram_type = opi` |

### 查看编译详情

```bash
# 显示内存使用
make size

# 清理后重新编译
make clean && make
```

## 开源许可

MIT
