/**
 * test_lib.c - 测试共享库
 *
 * 编译命令:
 *   gcc -shared -fPIC -o lib/test_lib.so test/test_lib.c
 */

#include <stdio.h>

// 全局变量
static int g_init_count = 0;
static const char* g_message = "Hello from mini linker!";

// 构造函数 - 库加载时调用
__attribute__((constructor))
static void test_lib_init(void) {
    g_init_count++;
    printf("[test_lib] Constructor called (count=%d)\n", g_init_count);
}

// 析构函数 - 库卸载时调用
__attribute__((destructor))
static void test_lib_fini(void) {
    printf("[test_lib] Destructor called\n");
}

// 导出函数: 加法
int add(int a, int b) {
    return a + b;
}

// 导出函数: 乘法
int multiply(int a, int b) {
    return a * b;
}

// 导出函数: 获取消息
const char* get_message(void) {
    return g_message;
}

// 导出函数: 打印消息
void print_hello(const char* name) {
    printf("[test_lib] Hello, %s!\n", name);
}

// 导出函数: 计算阶乘
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

// 导出全局变量
int global_counter = 42;
