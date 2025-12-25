/*
 * main.c - Dynamic library loader for Android rootfs
 *
 * 演示如何加载多个 .so 文件并调用其中的函数
 */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

/* 函数指针类型定义 */
typedef void (*func_void)(void);
typedef int (*func_int_int_int)(int, int);
typedef const char* (*func_str_void)(void);
typedef void (*func_void_str)(const char*);
typedef int (*func_int_str)(const char*);

/* 库句柄结构体 - 用于管理多个 so */
typedef struct {
    void *handle;
    const char *path;
    const char *name;
} LibHandle;

/* 加载单个 so 文件 */
void* load_library(const char *lib_path) {
    printf("\n=== Loading library: %s ===\n", lib_path);

    void *handle = dlopen(lib_path, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "Error loading %s: %s\n", lib_path, dlerror());
        return NULL;
    }

    printf("Successfully loaded: %s\n", lib_path);
    return handle;
}

/* 获取函数地址 */
void* get_function(void *handle, const char *func_name) {
    dlerror();  /* 清除之前的错误 */

    void *func = dlsym(handle, func_name);
    char *error = dlerror();

    if (error != NULL) {
        fprintf(stderr, "Error getting symbol '%s': %s\n", func_name, error);
        return NULL;
    }

    printf("Found function: %s at %p\n", func_name, func);
    return func;
}

/* 卸载库 */
void unload_library(void *handle, const char *name) {
    printf("\n=== Unloading library: %s ===\n", name);
    if (handle) {
        dlclose(handle);
    }
}

/* 测试 demo.so */
void test_demo_so(void *handle) {
    printf("\n--- Testing demo.so functions ---\n");

    /* 调用 demo_hello */
    func_void hello = (func_void)get_function(handle, "demo_hello");
    if (hello) {
        hello();
    }

    /* 调用 demo_add */
    func_int_int_int add = (func_int_int_int)get_function(handle, "demo_add");
    if (add) {
        int result = add(10, 20);
        printf("Result: 10 + 20 = %d\n", result);
    }

    /* 调用 demo_version */
    func_str_void version = (func_str_void)get_function(handle, "demo_version");
    if (version) {
        printf("Version: %s\n", version());
    }
}

/* 测试 demo2.so */
void test_demo2_so(void *handle) {
    printf("\n--- Testing demo2.so functions ---\n");

    /* 调用 demo2_print */
    func_void_str print_func = (func_void_str)get_function(handle, "demo2_print");
    if (print_func) {
        print_func("Hello from main program!");
    }

    /* 调用 demo2_strlen */
    func_int_str strlen_func = (func_int_str)get_function(handle, "demo2_strlen");
    if (strlen_func) {
        int len = strlen_func("Android rootfs");
        printf("Length result: %d\n", len);
    }

    /* 调用 demo2_multiply */
    func_int_int_int multiply = (func_int_int_int)get_function(handle, "demo2_multiply");
    if (multiply) {
        int result = multiply(6, 7);
        printf("Result: 6 * 7 = %d\n", result);
    }
}

/* 批量加载多个 so 文件 */
int load_multiple_libraries(const char *lib_paths[], int count, LibHandle *handles) {
    int loaded = 0;

    printf("\n========================================\n");
    printf("Loading %d libraries...\n", count);
    printf("========================================\n");

    for (int i = 0; i < count; i++) {
        handles[i].path = lib_paths[i];
        handles[i].name = lib_paths[i];  /* 简化处理，使用路径作为名称 */
        handles[i].handle = load_library(lib_paths[i]);

        if (handles[i].handle) {
            loaded++;
        }
    }

    printf("\nLoaded %d/%d libraries successfully.\n", loaded, count);
    return loaded;
}

/* 卸载所有库 */
void unload_all_libraries(LibHandle *handles, int count) {
    printf("\n========================================\n");
    printf("Unloading all libraries...\n");
    printf("========================================\n");

    for (int i = count - 1; i >= 0; i--) {
        if (handles[i].handle) {
            unload_library(handles[i].handle, handles[i].name);
        }
    }
}

int main(int argc, char *argv[]) {
    printf("==========================================\n");
    printf("Android rootfs - Dynamic Library Loader\n");
    printf("==========================================\n");

    /* 定义要加载的 so 文件列表 */
    const char *lib_paths[] = {
        "./lib/libdemo.so",
        "./lib/libdemo2.so"
    };
    int lib_count = sizeof(lib_paths) / sizeof(lib_paths[0]);

    /* 如果命令行提供了参数，使用命令行参数作为库路径 */
    if (argc > 1) {
        lib_paths[0] = argv[1];
        lib_count = 1;

        if (argc > 2) {
            lib_paths[1] = argv[2];
            lib_count = 2;
        }
    }

    /* 创建库句柄数组 */
    LibHandle handles[10];  /* 最多支持 10 个库 */
    memset(handles, 0, sizeof(handles));

    /* 批量加载库 */
    int loaded = load_multiple_libraries(lib_paths, lib_count, handles);

    if (loaded == 0) {
        fprintf(stderr, "\nNo libraries loaded. Exiting.\n");
        return 1;
    }

    /* 测试各个库的函数 */
    printf("\n========================================\n");
    printf("Testing library functions...\n");
    printf("========================================\n");

    for (int i = 0; i < lib_count; i++) {
        if (!handles[i].handle) continue;

        /* 根据库名称调用相应的测试函数 */
        if (strstr(handles[i].path, "libdemo2")) {
            test_demo2_so(handles[i].handle);
        } else if (strstr(handles[i].path, "libdemo")) {
            test_demo_so(handles[i].handle);
        }
    }

    /* 卸载所有库 */
    unload_all_libraries(handles, lib_count);

    printf("\n==========================================\n");
    printf("Program completed successfully!\n");
    printf("==========================================\n");

    return 0;
}
