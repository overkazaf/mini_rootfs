# Mini Linker Makefile
#
# 此项目需要在 Linux 环境中编译运行

CC = gcc
CFLAGS = -Wall -Wextra -g -O2 -I $(INC_DIR)
LDFLAGS = -ldl

# 目录
SRC_DIR = src
INC_DIR = lib
TEST_DIR = test
LIB_DIR = lib

# 源文件
SRCS = $(SRC_DIR)/log.c \
       $(SRC_DIR)/elf_parser.c \
       $(SRC_DIR)/linker.c \
       $(SRC_DIR)/dlfcn.c

# 目标文件
OBJS = $(SRCS:.c=.o)

# 可执行文件
TARGET = mini_linker

# 测试库
TEST_LIB = $(LIB_DIR)/test_lib.so

# 默认目标
all: $(TARGET) $(TEST_LIB)

# 编译可执行文件
$(TARGET): $(OBJS) $(TEST_DIR)/main.o
	$(CC) $(LDFLAGS) -o $@ $^

# 编译源文件
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TEST_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 编译测试共享库
$(TEST_LIB): $(TEST_DIR)/test_lib.c
	@mkdir -p $(LIB_DIR)
	$(CC) -shared -fPIC -o $@ $<

# 运行测试
run: all
	./$(TARGET) $(TEST_LIB)

# 调试运行
debug: all
	gdb -ex "run $(TEST_LIB)" ./$(TARGET)

# 使用 valgrind 检查内存
valgrind: all
	valgrind --leak-check=full ./$(TARGET) $(TEST_LIB)

# 清理
clean:
	rm -f $(OBJS) $(TEST_DIR)/*.o $(TARGET) $(TEST_LIB)

# 完全清理
distclean: clean
	rm -rf $(LIB_DIR)

# 查看测试库的 ELF 信息
readelf: $(TEST_LIB)
	readelf -a $(TEST_LIB)

# 查看测试库的符号
nm: $(TEST_LIB)
	nm -D $(TEST_LIB)

# 查看测试库的反汇编
objdump: $(TEST_LIB)
	objdump -d $(TEST_LIB)

# 帮助
help:
	@echo "Mini Linker - Android-style ELF Loader"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build linker and test library (default)"
	@echo "  run       - Build and run test"
	@echo "  debug     - Build and run with gdb"
	@echo "  valgrind  - Run with valgrind memory check"
	@echo "  clean     - Remove object files and binaries"
	@echo "  distclean - Remove all generated files"
	@echo "  readelf   - Show ELF info of test library"
	@echo "  nm        - Show symbols of test library"
	@echo "  objdump   - Disassemble test library"
	@echo "  help      - Show this help"
	@echo ""
	@echo "Note: This project must be built on Linux (ELF-based system)"

.PHONY: all clean distclean run debug valgrind readelf nm objdump help
