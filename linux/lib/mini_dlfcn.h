#ifndef MINI_DLFCN_H
#define MINI_DLFCN_H

// dlopen 标志
#define MINI_RTLD_LAZY     0x0001  // 延迟绑定
#define MINI_RTLD_NOW      0x0002  // 立即绑定
#define MINI_RTLD_LOCAL    0x0000  // 符号不导出
#define MINI_RTLD_GLOBAL   0x0100  // 符号全局可见

// 特殊句柄
#define MINI_RTLD_DEFAULT  ((void*)0)   // 默认搜索
#define MINI_RTLD_NEXT     ((void*)-1)  // 下一个匹配

// dlopen - 加载共享库
// path: 库路径
// flags: 加载标志
// 返回: 库句柄，失败返回 NULL
void* mini_dlopen(const char* path, int flags);

// dlsym - 获取符号地址
// handle: dlopen 返回的句柄
// symbol: 符号名
// 返回: 符号地址，失败返回 NULL
void* mini_dlsym(void* handle, const char* symbol);

// dlclose - 关闭共享库
// handle: dlopen 返回的句柄
// 返回: 成功返回 0，失败返回非 0
int mini_dlclose(void* handle);

// dlerror - 获取错误信息
// 返回: 错误字符串，无错误返回 NULL
const char* mini_dlerror(void);

#endif // MINI_DLFCN_H
