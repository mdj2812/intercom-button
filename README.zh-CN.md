# ESP32-S3 桌面对讲按键

[home-intercom](https://github.com/mdj2812/home-intercom) 系统的配套硬件——按下录音，松开广播到目标房间的小爱音箱。

## 演示

![演示](docs/demo.gif)

*按住 → 录音 → 松开 → 书房音箱播放。[高清视频](docs/demo.mp4)*

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
    "server_scheme": "http",
    "server_host": "homeassistant.local",
    "server_port": 8123,
    "pins": [4, 5, 12, 13]
}
```

| 字段 | 说明 |
|------|------|
| `server_scheme` | 可信局域网使用 `http`，远端 HA 使用 `https`。HTTPS 流量已加密，但当前固件暂不验证服务器证书。 |
| `server_host` | Home Assistant 的 IP（Docker 模式填 Docker 主机 IP） |
| `server_port` | HA 集成用 `8123`，Docker 旧模式用 `8764` |
| `pins` | 要初始化的硬件 GPIO。省略则使用编译期 `{4,5,12,13}`。房间目标来自 hello `buttons`，不写在这个文件里。 |

复制 `data/config.example.json` 为 `data/config.json` 并填入你的设置。`data/config.json` 已加入 `.gitignore`——WiFi 凭证不会泄露。

设备用 Wi-Fi MAC（`X-Device-ID`）表明身份。WiFi 连上后会 `GET /api/home_intercom/config` 拉取 `sample_rate` / `max_record_secs`（服务器不可达时使用编译期 16000 Hz / 60 秒），然后 `POST /api/home_intercom/devices/hello`（首次信任注册）。hello 的 `buttons` 对象是 GPIO→房间映射（home-intercom#78）；空 `{}` 表示未配置，保留上次 NVS。空闲时每 10 秒再 hello 一次以刷新 HA 的 `last_seen` / Online，并在不重启的情况下应用 PWA 改的映射。然后再向 `/api/home_intercom/device/record` 上传。ESP32 上不再保存 Home Assistant 令牌。未知或已吊销的 MAC 会收到 HTTP 403；丢失的设备可在 HA 后台吊销。心跳若收到 revoked/pending，会回到橙色等待。

**多按键部署**：烧录一次固件。哪个 GPIO 是哪个按键写在 `pins` 里。在 Home Intercom PWA 里把每个 GPIO 绑到房间。改引脚后执行 `pio run -e esp32-s3-devkitc-1 -t uploadfs`。

## 快速开始

第一块板：USB `make flash`，再 `make flashfs`。之后升级：把 GitHub 的 `.bin` 和 `.sig` 留在 Release 上，由 Home Intercom **Update** 走局域网 OTA。`.sig` 在设备上校验，不是用来现场签名的。详见 [GitHub Release](#github-release) 和 [局域网 OTA](#局域网-ota)。

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

USB 用于首次安装和恢复。会写入 bootloader、本仓库的 8MB OTA 分区表，以及 factory 分区里的应用。GitHub 上的 `.sig` **不会**在 USB 烧录时使用。

1. 用 USB 连接 ESP32-S3
2. `make flash`（或 `make docker-flash`）
3. 编辑好 `data/config.json` 后执行 `make flashfs`

空白板请从与目标版本一致的 git tag 编译烧录，这样 bootloader、分区表和应用是一套的。LittleFS（`config.json`）从不随 GitHub Release 发布。

### GitHub Release

每个 [GitHub Release](https://github.com/mdj2812/intercom-button/releases) 会发布：

| 资源 | 含义 |
|------|------|
| `intercom-button-vX.Y.Z.bin` | 应用镜像（factory 分区，地址 `0x10000`） |
| `intercom-button-vX.Y.Z.bin.sig` | 对该 `.bin` 的 SHA-256 做 ECDSA secp256r1 签名后的 64 字节（原始 r、s 各 32 字节） |

CI 用仓库密钥 `OTA_PRIVATE_KEY` **签名** `.bin`。对应的**公钥**编译进固件（`src/ota_keys.h`）。使用者不需要私钥。板子**不会**给固件签名；OTA 只**校验**已经签好的 `.sig`。

**用 Release 的 `.bin` 做 USB 烧录：** 只烧 `.bin`，不要把 `.sig` 写进 Flash。这会覆盖 `0x10000` 的 factory 应用，并假定芯片上已有本仓库的分区表（此前做过一次 `make flash`）。

```bash
esptool.py --chip esp32s3 --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 8MB \
  0x10000 intercom-button-vX.Y.Z.bin
```

然后用自己的配置执行 `make flashfs`。

### 局域网 OTA

ESP32 的 OTA 客户端**只走 HTTP**，不能直接下 GitHub 资源（HTTPS）。[home-intercom](https://github.com/mdj2812/home-intercom) 会把最新的 GitHub `.bin` 和 `.sig` 缓存下来，在局域网提供：

- `GET /api/home_intercom/firmware`（带 `X-Checksum-SHA256`）
- `GET /api/home_intercom/firmware.sig`

在 Home Intercom PWA 里对该设备点 **Update** 会缓存镜像，并让下一次 hello 带上 `"ota": true`。空闲中的按键随后会：

1. 下载 `.sig`（正好 64 字节；没有则跳过 ECDSA）
2. 下载 `.bin` 并计算哈希
3. 若有 SHA-256 头则必须一致
4. 若已拿到 `.sig`，用**当前正在运行的固件**里的公钥做 ECDSA 校验
5. 写入空闲 OTA 分区并重启（烧录过程中 LED 为橙色常亮）

校验和或 `.sig` 不对会中止，继续跑旧镜像。没有 `.sig` 时仍可用 SHA-256 烧录（连校验和头也没有则只打警告）。

重启后，60 秒内成功解析 `/devices/hello`（`ok` / `pending` / `revoked`）会把新镜像标为有效。按任意键或串口输入 `confirm` 是快捷方式。超时会回滚。空闲时串口输入 `ota` 也会从同一组局域网 URL 下载。

**公钥轮换：** 若板子上一次升级之后 `ota_keys.h` 已更换，GitHub 的 `.sig` 会 ECDSA 失败，需要先用 USB（或无签名 OTA）装一次带新公钥的镜像。

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
| 🟢 绿色 | 就绪（WiFi 已连接、已注册、空闲） |
| 🔴 红色闪烁 | WiFi 断开 |
| 🟠 橙色闪烁 | 正在向服务器注册（`/devices/hello`） |
| 🟠 橙色常亮 | 局域网 OTA 下载 / 烧录中 |
| 🔵 蓝色 | 录音中 |
| ⚪ 白色闪烁 | 上传中 |
| 🟢 闪 4 次 | 上传成功 |
| 🔴 闪 4 次 | 上传失败 |

### 录音行为

- **按住说话**：按下按键 → 录音，松开 → 发送
- **最小录音时长**：500ms（更短的轻触被丢弃）
- **最大录音时长**：来自 `GET /api/home_intercom/config`（hello 会再带一次）；编译期回退 60 秒
- **格式**：16-bit PCM WAV、16kHz 单声道（服务器 `sample_rate`，默认 16000）

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
│   ├── test_audio_recorder/ # 定时器 ISR / ADC / PSRAM 录音测试
│   ├── test_firmware_main/  # setup()/loop() 状态机（宿主 mocks）
│   ├── test_config_manager/ # 配置解析测试
│   ├── test_http_uploader/  # HTTP 上传测试
│   ├── test_device_id/      # MAC 身份测试
│   ├── test_device_hello/   # /devices/hello 注册测试
│   ├── test_server_config/  # GET /config 音频设置测试
│   ├── test_wifi_manager/   # WiFi 管理测试
│   ├── test_button_manager/ # 按键管理测试
│   └── test_room_target_store/ # 房间存储测试
└── src/
    ├── main.cpp             # 状态机：IDLE→RECORDING→UPLOADING
    ├── config.h             # 引脚定义
    ├── config_manager.h/cpp # JSON 配置加载器 (LittleFS)
    ├── wifi_manager.h/cpp   # 非阻塞 WiFi + 自动重连
    ├── audio_recorder.h/cpp # 16kHz 定时器 ISR → PSRAM → WAV
    ├── http_uploader.h/cpp  # POST /device/record?target=<room>
    ├── device_id.h/cpp      # STA MAC → X-Device-ID
    ├── device_hello.h/cpp   # POST /devices/hello 注册
    ├── server_config.h/cpp  # GET /config 音频设置
    ├── button_manager.h/cpp # 多按键 GPIO 矩阵 + 消抖
    ├── room_target_store.h/cpp # NVS 房间目标存储
    ├── ota_keys.h           # 编译进固件的 ECDSA 公钥（仅校验）
    ├── ota_manager.h/cpp    # 局域网 OTA 下载，SHA-256 + ECDSA 校验
    └── consts.hpp           # 共享常量
```

## 编译环境

本项目有**两个 PlatformIO 环境**——使用时务必指定：

| 环境 | 平台 | 用途 |
|------|------|------|
| `esp32-s3-devkitc-1` | xtensa-esp32s3 | 固件——编译、烧录、监视 |
| `native` | x86_64 Linux | 单元测试——宿主机运行，无需 ESP32 |

不加 `-e` 时 `pio run` 会编译**两个**环境。原生测试会编译 `src/`，但不包括 `main.cpp`（该文件被 `test_firmware_main` 引入，以便 Unity 调用 `setup()`/`loop()`）。

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
make compiledb     # 生成 clangd 用的 compile_commands.json（走开发 Docker 镜像）
```

宿主机上的 clangd 看不到镜像里的 `/root/.platformio`。`make compiledb` 会把固件用到的包拷到 `.clangd-pio`，生成 `compile_commands.json` 并改写路径。安装 **clangd** 扩展并关掉 Microsoft C/C++ IntelliSense，然后重新加载窗口。`platformio.ini` 或镜像更新后再跑一次 `make compiledb`。

CI 强制格式检查——违反格式规则的 PR 会通不过 `format` 任务。

## 调试

### 串口监视器

```bash
make monitor
```

固件会打印每次状态转换：

```text
=== ESP32-S3 Intercom Button ===
[cfg] Loaded: server=https://ha.example.com:443 wifi=MyWiFi pins=4
[wifi] Connecting to MyWiFi...
[wifi] Connected
[audio] Buffer: 960000 samples (60 sec), PSRAM free: 7654 KB
[audio] Ready — ISR @ 16000 Hz
[main] Setup complete — ready.
[main] Recording...
[audio] Stopped — 48000 samples (3.0s)
[upload] POST https://ha.example.com:443/api/home_intercom/device/record?target=study (96044 bytes) attempt 1/3
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
| 上传成功但没声音 | 房间键值不对 | 确认 PWA 里的 GPIO→房间映射（hello `buttons`）与扬声器一致。未分配的 GPIO 不会上传。hello `{}` 空对象保留上次 NVS。 |
| 配置不加载 | LittleFS 未烧录 | 运行 `make flashfs` 上传文件系统 |
| PSRAM 分配警告 | 板子变体不匹配 | 检查 `platformio.ini` 中 `board_build.psram_type = opi` |
| `[ota] FAILED: ECDSA signature invalid` | 当前镜像公钥不一致，或 `.sig` 与 `.bin` 不匹配 | 用对应 tag USB 烧一次（或无 `.sig` 的 OTA）；不要把 `.sig` 写进芯片 |
| `[ota] SHA-256 mismatch` | 缓存固件与 `X-Checksum-SHA256` 不一致 | 再点一次 PWA **Update**，让 home-intercom 重新缓存 GitHub `.bin` / `.sig` |
| 新镜像约 60 秒后回滚 | hello 没有确认这次启动 | 保持面板在局域网让 hello 成功，或串口 `confirm` / 按一下键 |
| PWA Update 后面板没有动作 | GitHub 资源是 HTTPS，ESP32 不会自己去拉 | 确认 home-intercom 已缓存该 Release，且下一次 hello 带 `"ota": true` |

### 查看编译详情

```bash
# 显示内存使用
make size

# 清理后重新编译
make clean && make
```

## 开源许可

MIT
