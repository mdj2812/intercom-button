# ────────────────────────────────────────────────────
# ESP32-S3 Intercom Button — Convenience Makefile
#
# 用法:
#   make             编译固件
#   make flash       编译 + 烧录
#   make flashfs     上传 LittleFS 配置
#   make monitor     串口监视器
#   make test        运行单元测试（不需要 ESP32）
#   make check       静态分析
#   make clean       清理构建产物
#   make size        显示固件体积
#   make format      自动格式化代码
#   make format-check 检查代码格式（CI 用）
#
# 通过 Docker 运行（替代裸机 pio）：
#   make docker-shell   进入 Docker 开发环境
#   make docker-build   容器内编译
#   make docker-flash   容器内烧录
# ────────────────────────────────────────────────────

ENV      := esp32-s3-devkitc-1
PIO      := pio

.PHONY: all build flash flashfs monitor test check clean size format format-check \
        docker-build docker-flash docker-flashfs docker-test docker-check docker-monitor docker-shell

# ── 裸机命令 ────────────────────────────────────────

all: build

build:
	$(PIO) run -e $(ENV)

flash:
	$(PIO) run -e $(ENV) -t upload

flashfs:
	$(PIO) run -e $(ENV) -t uploadfs

monitor:
	$(PIO) device monitor

test:
	$(PIO) test -e native -v

# ── Coverage ────────────────────────────────────────
test-coverage:
	rm -rf coverage .pio/build/native/coverage
	$(PIO) test -e native -v
	lcov --capture \
	    --directory .pio/build/native \
	    --output-file coverage.info \
	    --rc lcov_branch_coverage=1
	lcov --remove coverage.info '*/ArduinoJson/*' '*/Unity/*' '*/unity_config*' \
	    --output-file coverage_filtered.info
	genhtml coverage_filtered.info \
	    --output-directory coverage \
	    --branch-coverage \
	    --title "ESP32 Intercom Button — Coverage"
	@echo ""
	@echo "=== Coverage report: coverage/index.html ==="
	@lcov --summary coverage_filtered.info 2>/dev/null | grep -E "lines|functions"
	@echo ""

check:
	$(PIO) check -e $(ENV) --fail-on-defect=high --skip-packages

clean:
	$(PIO) run -e $(ENV) -t clean

size:
	$(PIO) run -e $(ENV) -t size

# ── Code formatting ──────────────────────────────────
format:
	clang-format -i src/*.cpp src/*.h $$(find test -name '*.cpp')

format-check:
	clang-format --dry-run -Werror src/*.cpp src/*.h $$(find test -name '*.cpp')

# ── Docker 命令（通过 dev.sh，自动处理镜像拉取和 USB 设备挂载）─

docker-shell:
	./docker/dev.sh

docker-build:
	./docker/dev.sh $(PIO) run -e $(ENV)

docker-flash:
	./docker/dev.sh $(PIO) run -e $(ENV) -t upload

docker-flashfs:
	./docker/dev.sh $(PIO) run -e $(ENV) -t uploadfs

docker-test:
	./docker/dev.sh $(PIO) test -e native -v

docker-test-coverage:
	./docker/dev.sh make test-coverage

docker-check:
	./docker/dev.sh $(PIO) check -e $(ENV) --fail-on-defect=high --skip-packages

docker-monitor:
	./docker/dev.sh $(PIO) device monitor
