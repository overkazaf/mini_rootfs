# ==========================================
# Socket 通信教学示例
# ==========================================
#
# 这是一个独立的演示项目，展示:
# 1. TCP Socket 服务器/客户端的实现
# 2. 自定义二进制协议设计
# 3. 网络编程基础概念
#
# 用法:
#   make           - 编译 server 和 client
#   make run-server - 启动服务器
#   make run-client - 启动交互式客户端
#

# ========== 配置 ==========
CC = gcc
CFLAGS = -Wall -g -Isrc
SRC_DIR = src
OUT_DIR = out

# 目标文件
SERVER = $(OUT_DIR)/server
CLIENT = $(OUT_DIR)/client

# 默认端口
PORT ?= 8888

# ========== 目标 ==========
.PHONY: all clean run-server run-client test help

all: $(SERVER) $(CLIENT)
	@echo ""
	@echo "============================================"
	@echo "  编译完成!"
	@echo "============================================"
	@echo ""
	@echo "文件位置:"
	@echo "  服务器: $(SERVER)"
	@echo "  客户端: $(CLIENT)"
	@echo ""
	@echo "快速开始:"
	@echo "  终端 1: make run-server"
	@echo "  终端 2: make run-client"
	@echo ""
	@echo "或者直接运行:"
	@echo "  终端 1: ./out/server -p $(PORT)"
	@echo "  终端 2: ./out/client -h 127.0.0.1 -p $(PORT) -i"
	@echo ""

help:
	@echo "Socket 通信教学示例"
	@echo ""
	@echo "编译命令:"
	@echo "  make            - 编译 server 和 client"
	@echo "  make clean      - 清理编译文件"
	@echo ""
	@echo "运行命令:"
	@echo "  make run-server - 启动服务器 (端口 $(PORT))"
	@echo "  make run-client - 启动交互式客户端"
	@echo "  make test       - 快速测试 (ping + echo)"
	@echo ""
	@echo "修改端口:"
	@echo "  make run-server PORT=9999"
	@echo "  make run-client PORT=9999"
	@echo ""
	@echo "客户端用法:"
	@echo "  ./out/client -h <host> -p <port> -i        # 交互模式"
	@echo "  ./out/client -c \"echo Hello\"               # 单命令模式"
	@echo "  ./out/client -c \"add 10 20\"                # 计算命令"

# ========== 创建输出目录 ==========
$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# ========== 编译规则 ==========
$(SERVER): $(SRC_DIR)/server.c $(SRC_DIR)/protocol.h | $(OUT_DIR)
	@echo "编译服务器..."
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/server.c
	@echo "  -> $(SERVER)"

$(CLIENT): $(SRC_DIR)/client.c $(SRC_DIR)/protocol.h | $(OUT_DIR)
	@echo "编译客户端..."
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/client.c
	@echo "  -> $(CLIENT)"

# ========== 运行命令 ==========
run-server: $(SERVER)
	@echo ""
	@echo "启动服务器 (端口: $(PORT))..."
	@echo "按 Ctrl+C 停止"
	@echo ""
	./$(SERVER) -p $(PORT)

run-client: $(CLIENT)
	@echo ""
	@echo "启动客户端 (连接到 127.0.0.1:$(PORT))..."
	@echo ""
	./$(CLIENT) -h 127.0.0.1 -p $(PORT) -i

# ========== 测试命令 ==========
test: $(CLIENT)
	@echo "快速测试..."
	@echo ""
	./$(CLIENT) -h 127.0.0.1 -p $(PORT) -c "ping"
	@echo ""
	./$(CLIENT) -h 127.0.0.1 -p $(PORT) -c "echo Hello World"
	@echo ""
	./$(CLIENT) -h 127.0.0.1 -p $(PORT) -c "time"
	@echo ""
	./$(CLIENT) -h 127.0.0.1 -p $(PORT) -c "add 100 200"

# ========== 清理 ==========
clean:
	rm -rf $(OUT_DIR)
	@echo "已清理."
