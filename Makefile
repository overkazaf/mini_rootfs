# ============================================================================
# Mini Rootfs - 顶层 Makefile
# ============================================================================
#
# 统一编译入口，支持多平台构建
#
# 用法:
#   make linux          - 编译 Linux 平台 linker
#   make android        - 编译 Android 平台 (本地测试)
#   make android-cross  - 编译 Android 平台 (交叉编译)
#   make demo           - 编译 Socket 演示项目
#   make all            - 编译所有平台
#   make clean          - 清理所有构建文件
#
# ============================================================================

# 子目录
LINUX_DIR = linux
ANDROID_DIR = android
DEMO_DIR = demo

# Android 编译选项 (可通过命令行覆盖)
ANDROID_ABI ?= arm64-v8a
ANDROID_API ?= 21
NDK_PATH ?= $(HOME)/sdk/ndk/23.1.7779620

# 导出变量给子 Makefile
export ANDROID_ABI ANDROID_API NDK_PATH

# ============================================================================
# 主要目标
# ============================================================================

.PHONY: all linux android android-cross demo clean help
.PHONY: linux-run android-run demo-run
.PHONY: linux-clean android-clean demo-clean

# 默认目标
all: linux android demo
	@echo ""
	@echo "============================================"
	@echo "  所有平台编译完成!"
	@echo "============================================"

# ============================================================================
# Linux 平台
# ============================================================================

linux:
	@echo "========== 编译 Linux 平台 =========="
	$(MAKE) -C $(LINUX_DIR) all

linux-run: linux
	$(MAKE) -C $(LINUX_DIR) run

linux-clean:
	$(MAKE) -C $(LINUX_DIR) clean

# ============================================================================
# Android 平台
# ============================================================================

# 本地编译 (用于 macOS/Linux 上测试)
android:
	@echo "========== 编译 Android 平台 (native) =========="
	$(MAKE) -C $(ANDROID_DIR) native

android-run: android
	$(MAKE) -C $(ANDROID_DIR) run

# 交叉编译 (用于真机)
android-cross:
	@echo "========== 编译 Android 平台 (cross-compile) =========="
	@echo "Target ABI: $(ANDROID_ABI)"
	@echo "API Level: $(ANDROID_API)"
	$(MAKE) -C $(ANDROID_DIR) android

# ============================================================================
# Demo 项目
# ============================================================================

demo:
	@echo "========== 编译 Demo 项目 =========="
	$(MAKE) -C $(DEMO_DIR) all

demo-run-server: demo
	$(MAKE) -C $(DEMO_DIR) run-server

demo-run-client: demo
	$(MAKE) -C $(DEMO_DIR) run-client

demo-clean:
	$(MAKE) -C $(DEMO_DIR) clean

# ============================================================================
# 清理
# ============================================================================

clean: linux-clean android-clean demo-clean
	@echo ""
	@echo "所有平台已清理"

# ============================================================================
# 帮助
# ============================================================================

help:
	@echo "Mini Rootfs - 多平台构建系统"
	@echo ""
	@echo "编译目标:"
	@echo "  make all            - 编译所有平台"
	@echo "  make linux          - 编译 Linux 平台 linker"
	@echo "  make android        - 编译 Android 平台 (本地测试)"
	@echo "  make android-cross  - 编译 Android 平台 (交叉编译，需要 NDK)"
	@echo "  make demo           - 编译 Socket 演示项目"
	@echo ""
	@echo "运行目标:"
	@echo "  make linux-run      - 运行 Linux linker 测试"
	@echo "  make android-run    - 运行 Android native 测试"
	@echo "  make demo-run-server- 启动 demo 服务器"
	@echo "  make demo-run-client- 启动 demo 客户端"
	@echo ""
	@echo "清理目标:"
	@echo "  make clean          - 清理所有平台"
	@echo "  make linux-clean    - 清理 Linux 构建"
	@echo "  make android-clean  - 清理 Android 构建"
	@echo "  make demo-clean     - 清理 Demo 构建"
	@echo ""
	@echo "Android 交叉编译选项:"
	@echo "  ANDROID_ABI=arm64-v8a|armeabi-v7a|x86_64|x86 (默认: arm64-v8a)"
	@echo "  ANDROID_API=21|23|26|...                     (默认: 21)"
	@echo "  NDK_PATH=/path/to/ndk"
	@echo ""
	@echo "示例:"
	@echo "  make android-cross ANDROID_ABI=x86_64   # 为模拟器编译"
	@echo "  make android-cross NDK_PATH=/opt/ndk   # 指定 NDK 路径"
