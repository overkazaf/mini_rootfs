/**
 * main.c - Mini Linker 测试程序
 *
 * 使用自定义的 mini_dlopen/mini_dlsym 加载共享库
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mini_dlfcn.h"
#include "linker.h"
#include "elf_parser.h"
#include "log.h"

// 函数指针类型定义
typedef int (*add_func)(int, int);
typedef int (*multiply_func)(int, int);
typedef const char* (*get_message_func)(void);
typedef void (*print_hello_func)(const char*);
typedef int (*factorial_func)(int);

void print_usage(const char* prog) {
    printf("Usage: %s <shared_library.so>\n", prog);
    printf("\nExample:\n");
    printf("  %s lib/test_lib.so\n", prog);
}

int main(int argc, char* argv[]) {
    const char* lib_path;

    // 初始化日志系统
    log_init();

    LOG_INFO("===========================================\n");
    LOG_INFO("  Mini Linker - Android-style ELF Loader\n");
    LOG_INFO("===========================================\n");

    // 解析参数
    if (argc < 2) {
        lib_path = "lib/test_lib.so";
        LOG_INFO("No library specified, using default: %s\n", lib_path);
    } else {
        lib_path = argv[1];
    }

    // 初始化链接器
    linker_init();

    // 可选: 先分析 ELF 文件
    LOG_INFO("--- Analyzing ELF file ---\n");
    elf_file_t elf;
    if (elf_open(lib_path, &elf) == 0) {
        elf_print_info(&elf);
        elf_close(&elf);
    }

    // 使用 mini_dlopen 加载库
    LOG_INFO("--- Loading library ---\n");
    void* handle = mini_dlopen(lib_path, MINI_RTLD_NOW);
    if (!handle) {
        LOG_ERROR("Failed to load library: %s\n", mini_dlerror());
        return 1;
    }

    // 打印 soinfo 信息
    LOG_INFO("--- Library info ---\n");
    soinfo_print((soinfo_t*)handle);

    // 查找并调用函数
    LOG_INFO("--- Testing functions ---\n");

    // 测试 add
    LOG_INFO("Looking up symbol: add\n");
    add_func add = (add_func)mini_dlsym(handle, "add");
    if (add) {
        int result = add(10, 20);
        LOG_INFO("add(10, 20) = %d\n", result);
    } else {
        LOG_ERROR("Failed to find 'add': %s\n", mini_dlerror());
    }

    // 测试 multiply
    LOG_INFO("Looking up symbol: multiply\n");
    multiply_func multiply = (multiply_func)mini_dlsym(handle, "multiply");
    if (multiply) {
        int result = multiply(6, 7);
        LOG_INFO("multiply(6, 7) = %d\n", result);
    } else {
        LOG_ERROR("Failed to find 'multiply': %s\n", mini_dlerror());
    }

    // 测试 get_message
    LOG_INFO("Looking up symbol: get_message\n");
    get_message_func get_message = (get_message_func)mini_dlsym(handle, "get_message");
    if (get_message) {
        const char* msg = get_message();
        LOG_INFO("get_message() = \"%s\"\n", msg);
    } else {
        LOG_ERROR("Failed to find 'get_message': %s\n", mini_dlerror());
    }

    // 测试 print_hello
    LOG_INFO("Looking up symbol: print_hello\n");
    print_hello_func print_hello = (print_hello_func)mini_dlsym(handle, "print_hello");
    if (print_hello) {
        LOG_INFO("Calling print_hello(\"Mini Linker\"):\n");
        print_hello("Mini Linker");
    } else {
        LOG_ERROR("Failed to find 'print_hello': %s\n", mini_dlerror());
    }

    // 测试 factorial
    LOG_INFO("Looking up symbol: factorial\n");
    factorial_func factorial = (factorial_func)mini_dlsym(handle, "factorial");
    if (factorial) {
        LOG_INFO("factorial(5) = %d\n", factorial(5));
        LOG_INFO("factorial(10) = %d\n", factorial(10));
    } else {
        LOG_ERROR("Failed to find 'factorial': %s\n", mini_dlerror());
    }

    // 测试全局变量
    LOG_INFO("Looking up symbol: global_counter\n");
    int* counter = (int*)mini_dlsym(handle, "global_counter");
    if (counter) {
        LOG_INFO("global_counter = %d\n", *counter);
        *counter = 100;
        LOG_INFO("global_counter (after modification) = %d\n", *counter);
    } else {
        LOG_ERROR("Failed to find 'global_counter': %s\n", mini_dlerror());
    }

    // 测试未定义符号
    LOG_INFO("Looking up undefined symbol (expect error)\n");
    void* undefined = mini_dlsym(handle, "undefined_symbol");
    if (!undefined) {
        LOG_WARN("Expected error for undefined symbol: %s\n", mini_dlerror());
    }

    LOG_INFO("--- Unloading library ---\n");
    mini_dlclose(handle);

    LOG_INFO("===========================================\n");
    LOG_INFO("  Test completed successfully!\n");
    LOG_INFO("===========================================\n");

    return 0;
}
