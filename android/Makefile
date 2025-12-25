# ============================================================================
# Android rootfs - Makefile
# ============================================================================
#
# 支持两种编译模式:
# 1. 本地编译 (用于测试): make native
# 2. Android 交叉编译: make android
#
# ============================================================================
# Makefile 基础语法说明:
# ============================================================================
#
# 1. 注释: 以 # 开头的行是注释
#
# 2. 变量定义:
#    VAR = value      # 递归展开变量 (使用时才展开, 可以引用后面定义的变量)
#    VAR := value     # 简单展开变量 (定义时立即展开)
#    VAR ?= value     # 条件赋值 (仅当变量未定义时才赋值)
#    VAR += value     # 追加赋值 (在现有值后面追加)
#
# 3. 变量引用:
#    $(VAR) 或 ${VAR}  # 引用变量的值
#
# 4. 规则格式:
#    目标: 依赖项1 依赖项2
#    [TAB]命令1
#    [TAB]命令2
#    注意: 命令前必须是 TAB 字符, 不能是空格!
#
# 5. 自动变量 (在规则的命令中使用):
#    $@  # 目标文件名
#    $<  # 第一个依赖项
#    $^  # 所有依赖项 (去重)
#    $?  # 所有比目标新的依赖项
#
# 6. 命令前缀:
#    @命令    # 静默执行, 不打印命令本身
#    -命令    # 忽略错误, 即使命令失败也继续执行
#
# ============================================================================

# ----------------------------------------------------------------------------
# 变量定义区域
# ----------------------------------------------------------------------------

# 源文件目录
# 使用 = 进行简单赋值, 这里定义了源代码和输出的目录路径
SRC_DIR = src
ROOTFS_DIR = android

# 输出目录
# $(OUT_DIR) 引用了上面定义的 OUT_DIR 变量
# 这展示了变量可以嵌套引用
OUT_DIR = out
NATIVE_OUT = $(OUT_DIR)/native      # 展开后为: out/native
ANDROID_OUT = $(OUT_DIR)/android    # 展开后为: out/android

# 源文件路径
# 通过组合变量来构建完整路径
DEMO_SRC = $(SRC_DIR)/demo.c        # 展开后为: src/demo.c
DEMO2_SRC = $(SRC_DIR)/demo2.c      # 展开后为: src/demo2.c
MAIN_SRC = $(SRC_DIR)/main.c        # 展开后为: src/main.c

# ============================================================================
# 本地编译配置 (用于快速测试)
# ============================================================================

NATIVE_CC = gcc                     # 本地编译器
NATIVE_CFLAGS = -Wall -fPIC         # 编译选项:
                                    #   -Wall: 开启所有警告
                                    #   -fPIC: 生成位置无关代码 (共享库必需)
NATIVE_LDFLAGS = -ldl               # 链接选项:
                                    #   -ldl: 链接动态加载库 (用于 dlopen 等)

# ============================================================================
# Android NDK 交叉编译配置
# ============================================================================

# ?= 是条件赋值运算符
# 含义: 如果 NDK_PATH 变量已经被设置 (比如通过环境变量或命令行),
#       则保持原值; 否则使用这里指定的默认值
# 用法示例:
#   make android                     # 使用默认路径
#   NDK_PATH=/my/ndk make android    # 使用自定义路径
#   export NDK_PATH=/my/ndk && make  # 通过环境变量设置
#NDK_PATH ?= /Users/nongjiawu/Library/Android/sdk/ndk/23.1.7779620
NDK_PATH ?= $HOME/sdk/ndk/23.1.7779620

# 目标架构配置
# 常见的 Android ABI:
#   arm64-v8a:    64位 ARM (现代手机主流)
#   armeabi-v7a:  32位 ARM (旧手机)
#   x86_64:       64位 x86 (模拟器)
#   x86:          32位 x86 (旧模拟器)
ANDROID_ABI ?= arm64-v8a
ANDROID_API ?= 21                   # 最低支持的 Android API 级别

# ----------------------------------------------------------------------------
# 条件语句 (ifeq / else ifeq / else / endif)
# ----------------------------------------------------------------------------
# 语法:
#   ifeq (值1, 值2)      # 如果值1等于值2
#   ifeq "值1" "值2"     # 另一种写法
#   ifneq (值1, 值2)     # 如果值1不等于值2
#   ifdef 变量名          # 如果变量已定义
#   ifndef 变量名         # 如果变量未定义
#
# 下面根据不同的 ANDROID_ABI 设置对应的工具链目标和架构

ifeq ($(ANDROID_ABI),arm64-v8a)
    # 64位 ARM 架构
    ANDROID_TARGET = aarch64-linux-android
    ANDROID_ARCH = arm64
else ifeq ($(ANDROID_ABI),armeabi-v7a)
    # 32位 ARM 架构
    ANDROID_TARGET = armv7a-linux-androideabi
    ANDROID_ARCH = arm
else ifeq ($(ANDROID_ABI),x86_64)
    # 64位 x86 架构 (主要用于模拟器)
    ANDROID_TARGET = x86_64-linux-android
    ANDROID_ARCH = x86_64
else ifeq ($(ANDROID_ABI),x86)
    # 32位 x86 架构
    ANDROID_TARGET = i686-linux-android
    ANDROID_ARCH = x86
endif

# NDK 工具链路径
# 这里展示了变量的多层嵌套引用
TOOLCHAIN = $(NDK_PATH)/toolchains/llvm/prebuilt/darwin-x86_64

# 构建完整的编译器路径
# $(ANDROID_TARGET)$(ANDROID_API) 会被拼接, 例如: aarch64-linux-android21
ANDROID_CC = $(TOOLCHAIN)/bin/$(ANDROID_TARGET)$(ANDROID_API)-clang

# Android 编译选项
ANDROID_CFLAGS = -Wall -fPIC -DANDROID   # -DANDROID: 定义 ANDROID 宏
ANDROID_LDFLAGS = -ldl

# ============================================================================
# 编译目标定义
# ============================================================================

# ----------------------------------------------------------------------------
# .PHONY 伪目标声明
# ----------------------------------------------------------------------------
# .PHONY 用于声明"伪目标" (phony targets)
#
# 什么是伪目标?
#   - 不对应实际文件的目标
#   - 每次调用都会执行, 不检查文件是否存在或更新
#
# 为什么需要 .PHONY?
#   假设当前目录有个文件叫 "clean", 如果不声明 .PHONY:
#   执行 make clean 时, make 会认为 clean 文件已存在且是最新的,
#   因此不会执行 clean 规则中的命令
#
# 声明为 .PHONY 后:
#   make 会忽略同名文件, 始终执行该目标的命令

.PHONY: all native android clean help

# ----------------------------------------------------------------------------
# 默认目标
# ----------------------------------------------------------------------------
# Makefile 中的第一个目标是默认目标
# 直接执行 make (不带参数) 时会构建这个目标
# 这里 all 依赖 native, 所以 make 等同于 make native

all: native

# ----------------------------------------------------------------------------
# help 目标 - 显示帮助信息
# ----------------------------------------------------------------------------
# @ 符号: 静默执行, 不打印命令本身, 只显示命令的输出
# 如果不加 @, 执行时会显示:
#   echo "Usage:"
#   Usage:
# 加了 @ 后只显示:
#   Usage:

help:
	@echo "Usage:"
	@echo "  make native       - Build for local testing"
	@echo "  make android      - Build for Android (requires NDK)"
	@echo "  make run          - Run native build locally"
	@echo "  make clean        - Clean build files"
	@echo ""
	@echo "Android options:"
	@echo "  ANDROID_ABI=arm64-v8a|armeabi-v7a|x86_64|x86"
	@echo "  NDK_PATH=/path/to/ndk"

# ============================================================================
# 本地编译规则
# ============================================================================

# ----------------------------------------------------------------------------
# native 目标
# ----------------------------------------------------------------------------
# 格式: 目标: 依赖项1 依赖项2 依赖项3
#
# native 目标依赖于三个文件:
#   1. $(NATIVE_OUT)/loader          - 主程序
#   2. $(NATIVE_OUT)/lib/libdemo.so  - 动态库1
#   3. $(NATIVE_OUT)/lib/libdemo2.so - 动态库2
#
# make 的工作原理:
#   1. 检查目标是否存在
#   2. 检查所有依赖项是否存在且是最新的
#   3. 如果依赖项不存在, 先构建依赖项
#   4. 如果依赖项比目标新, 重新构建目标

native: $(NATIVE_OUT)/loader $(NATIVE_OUT)/lib/libdemo.so $(NATIVE_OUT)/lib/libdemo2.so
	@echo ""
	@echo "Native build complete!"
	@echo "Run with: cd $(NATIVE_OUT) && ./loader"

# ----------------------------------------------------------------------------
# 目录创建规则
# ----------------------------------------------------------------------------
# 这个规则用于创建输出目录

$(NATIVE_OUT):
	mkdir -p $(NATIVE_OUT)/lib

# ----------------------------------------------------------------------------
# 动态库编译规则
# ----------------------------------------------------------------------------
# 完整格式: 目标: 普通依赖 | 顺序依赖
#
# | 符号后面是 "order-only prerequisites" (顺序依赖/仅顺序依赖)
#
# 普通依赖 vs 顺序依赖:
#   普通依赖: 如果依赖项更新了, 目标需要重新构建
#   顺序依赖: 只要求依赖项存在, 不关心它是否更新
#
# 这里 | $(NATIVE_OUT) 表示:
#   - 构建前确保 $(NATIVE_OUT) 目录存在
#   - 但目录的修改时间变化不会触发重新编译
#   (目录的修改时间会因为里面文件变化而改变, 如果用普通依赖会导致不必要的重编译)

$(NATIVE_OUT)/lib/libdemo.so: $(DEMO_SRC) | $(NATIVE_OUT)
	mkdir -p $(NATIVE_OUT)/lib
	$(NATIVE_CC) $(NATIVE_CFLAGS) -shared -o $@ $<
	# 上面这行展开后类似于:
	# gcc -Wall -fPIC -shared -o out/native/lib/libdemo.so src/demo.c
	#
	# 自动变量说明:
	#   $@ = out/native/lib/libdemo.so  (目标文件)
	#   $< = src/demo.c                  (第一个依赖项)
	#
	# -shared: 生成共享库 (.so 文件)

$(NATIVE_OUT)/lib/libdemo2.so: $(DEMO2_SRC) | $(NATIVE_OUT)
	mkdir -p $(NATIVE_OUT)/lib
	$(NATIVE_CC) $(NATIVE_CFLAGS) -shared -o $@ $<

# ----------------------------------------------------------------------------
# 主程序编译规则
# ----------------------------------------------------------------------------

$(NATIVE_OUT)/loader: $(MAIN_SRC) | $(NATIVE_OUT)
	$(NATIVE_CC) $(NATIVE_CFLAGS) -o $@ $< $(NATIVE_LDFLAGS)
	# 展开后: gcc -Wall -fPIC -o out/native/loader src/main.c -ldl

# ----------------------------------------------------------------------------
# run 目标 - 运行程序
# ----------------------------------------------------------------------------
# run 依赖于 native, 所以执行 make run 时会:
#   1. 先检查并构建 native 目标 (如果需要)
#   2. 然后执行 run 的命令

run: native
	@echo ""
	@echo "========== Running native build =========="
	cd $(NATIVE_OUT) && ./loader

# ============================================================================
# Android 交叉编译规则
# ============================================================================

# android 目标依赖于:
#   1. check-ndk (先检查 NDK 是否存在)
#   2. 三个输出文件

android: check-ndk $(ANDROID_OUT)/loader $(ANDROID_OUT)/lib/libdemo.so $(ANDROID_OUT)/lib/libdemo2.so
	@echo ""
	@echo "Android build complete for $(ANDROID_ABI)!"
	@echo "Files are in $(ANDROID_OUT)/"
	@echo ""
	@echo "To test on Android device:"
	@echo "  adb push $(ANDROID_OUT)/ /data/local/tmp/rootfs"
	@echo "  adb shell"
	@echo "  cd /data/local/tmp/rootfs && ./loader"

# ----------------------------------------------------------------------------
# NDK 检查规则
# ----------------------------------------------------------------------------
# 这个规则在命令中使用了 shell 脚本语法
#
# 注意: 每一行命令默认在独立的 shell 中执行
#      使用 \ 续行或 ; 可以在同一个 shell 中执行多条命令
#      或者像下面这样用 if [...]; then ... fi 写成一行

check-ndk:
	@if [ ! -d "$(NDK_PATH)" ]; then \
		echo "Error: NDK not found at $(NDK_PATH)"; \
		echo "Please set NDK_PATH environment variable"; \
		exit 1; \
	fi
	@if [ ! -f "$(ANDROID_CC)" ]; then \
		echo "Error: Compiler not found at $(ANDROID_CC)"; \
		echo "Check your NDK installation and ANDROID_API level"; \
		exit 1; \
	fi
	# 反斜杠 \ 用于续行, 将多行合并为一条命令
	# 这样 if-then-fi 才能在同一个 shell 中正确执行
	# 如果不用续行, 每行会在独立的 shell 中执行, if 语句会出错

$(ANDROID_OUT):
	mkdir -p $(ANDROID_OUT)/lib

$(ANDROID_OUT)/lib/libdemo.so: $(DEMO_SRC) | $(ANDROID_OUT)
	mkdir -p $(ANDROID_OUT)/lib
	$(ANDROID_CC) $(ANDROID_CFLAGS) -shared -o $@ $<

$(ANDROID_OUT)/lib/libdemo2.so: $(DEMO2_SRC) | $(ANDROID_OUT)
	mkdir -p $(ANDROID_OUT)/lib
	$(ANDROID_CC) $(ANDROID_CFLAGS) -shared -o $@ $<

$(ANDROID_OUT)/loader: $(MAIN_SRC) | $(ANDROID_OUT)
	$(ANDROID_CC) $(ANDROID_CFLAGS) -o $@ $< $(ANDROID_LDFLAGS)

# ============================================================================
# 打包 rootfs
# ============================================================================

rootfs: android
	@echo "Creating Android rootfs..."
	cp -r $(ROOTFS_DIR)/* $(ANDROID_OUT)/
	cp $(ANDROID_OUT)/loader $(ANDROID_OUT)/bin/
	cp $(ANDROID_OUT)/lib/*.so $(ANDROID_OUT)/lib/
	@echo "Rootfs created at $(ANDROID_OUT)/"

# ============================================================================
# 清理规则
# ============================================================================
# rm -rf 会递归删除目录及其所有内容
# 这就是为什么 clean 要声明为 .PHONY - 它不对应任何文件

clean:
	rm -rf $(OUT_DIR)
	@echo "Cleaned."

# ============================================================================
# 附录: Makefile 常用技巧速查
# ============================================================================
#
# 1. 通配符和模式:
#    *.c              # 匹配所有 .c 文件 (仅在规则中有效)
#    $(wildcard *.c)  # 在变量中使用通配符
#    %.o: %.c         # 模式规则: 所有 .o 文件依赖于同名 .c 文件
#
# 2. 字符串函数:
#    $(subst from,to,text)     # 字符串替换
#    $(patsubst %.c,%.o,files) # 模式替换
#    $(strip text)             # 去除首尾空白
#    $(findstring find,text)   # 查找字符串
#
# 3. 文件名函数:
#    $(dir path)       # 取目录部分: src/foo.c -> src/
#    $(notdir path)    # 取文件名: src/foo.c -> foo.c
#    $(basename path)  # 去掉后缀: src/foo.c -> src/foo
#    $(suffix path)    # 取后缀: src/foo.c -> .c
#
# 4. 调试技巧:
#    make -n           # 只打印命令, 不执行 (dry run)
#    make -p           # 打印所有规则和变量
#    make V=1          # 很多项目用这个显示详细输出
#    $(info 消息)      # 打印调试信息
#    $(warning 警告)   # 打印警告
#    $(error 错误)     # 打印错误并停止
#
# 5. 包含其他 Makefile:
#    include other.mk       # 包含文件, 文件不存在则报错
#    -include other.mk      # 包含文件, 文件不存在则忽略
#
# ============================================================================
