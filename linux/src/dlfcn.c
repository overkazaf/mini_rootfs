#include "mini_dlfcn.h"
#include "linker.h"
#include <stdio.h>
#include <string.h>

// dlopen - 加载共享库
void* mini_dlopen(const char* path, int flags) {
    (void)flags;  // 目前忽略 flags

    if (!path) {
        linker_set_error("dlopen: path is NULL");
        return NULL;
    }

    // 加载库
    soinfo_t* si = linker_load(path);
    if (!si) {
        return NULL;
    }

    // 调用构造函数
    linker_call_constructors(si);

    return (void*)si;
}

// dlsym - 获取符号地址
void* mini_dlsym(void* handle, const char* symbol) {
    if (!symbol) {
        linker_set_error("dlsym: symbol is NULL");
        return NULL;
    }

    if (handle == MINI_RTLD_DEFAULT) {
        // 在所有已加载库中查找
        void* addr = linker_find_global_symbol(symbol);
        if (!addr) {
            linker_set_error("dlsym: symbol not found: %s", symbol);
        }
        return addr;
    }

    if (handle == MINI_RTLD_NEXT) {
        // TODO: 实现 RTLD_NEXT
        linker_set_error("dlsym: RTLD_NEXT not implemented");
        return NULL;
    }

    // 在指定库中查找
    soinfo_t* si = (soinfo_t*)handle;
    void* addr = linker_find_symbol(si, symbol);

    if (!addr) {
        linker_set_error("dlsym: symbol not found in %s: %s", si->name, symbol);
    }

    return addr;
}

// dlclose - 关闭共享库
int mini_dlclose(void* handle) {
    if (!handle) {
        linker_set_error("dlclose: invalid handle");
        return -1;
    }

    soinfo_t* si = (soinfo_t*)handle;
    linker_unload(si);
    return 0;
}

// dlerror - 获取错误信息
const char* mini_dlerror(void) {
    return linker_get_error();
}
