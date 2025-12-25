#ifndef LINKER_H
#define LINKER_H

#include <elf.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// SO 库信息结构（模仿 Android 的 soinfo）
typedef struct soinfo {
    char name[256];             // 库名

    // 加载信息
    void* base;                 // 加载基地址
    size_t size;                // 总映射大小
    void* load_bias;            // 加载偏移（实际加载地址 - 期望地址）

    // ELF 结构
    Elf64_Phdr* phdr;           // 程序头表（映射后的地址）
    size_t phnum;               // 程序头数量
    Elf64_Dyn* dynamic;         // 动态段

    // 符号表
    Elf64_Sym* symtab;          // 符号表
    const char* strtab;         // 字符串表
    size_t strtab_size;         // 字符串表大小

    // 哈希表（用于符号查找加速）
    uint32_t* hash;             // ELF hash
    uint32_t* gnu_hash;         // GNU hash（可选）

    // 重定位表
    Elf64_Rela* rela;           // RELA 重定位表
    size_t rela_count;          // RELA 条目数
    Elf64_Rela* plt_rela;       // PLT RELA 重定位表
    size_t plt_rela_count;      // PLT RELA 条目数

    // 初始化/析构函数
    void (*init_func)(void);    // DT_INIT
    void (*fini_func)(void);    // DT_FINI
    void (**init_array)(void);  // DT_INIT_ARRAY
    size_t init_array_count;    // DT_INIT_ARRAYSZ
    void (**fini_array)(void);  // DT_FINI_ARRAY
    size_t fini_array_count;    // DT_FINI_ARRAYSZ

    // 引用计数
    int ref_count;

    // 链表
    struct soinfo* next;
} soinfo_t;

// 全局链接器状态
typedef struct {
    soinfo_t* soinfo_list;      // 已加载库链表
    char error_msg[512];        // 错误信息
    bool has_error;             // 是否有错误
} linker_state_t;

// 初始化链接器
void linker_init(void);

// 加载共享库
soinfo_t* linker_load(const char* path);

// 卸载共享库
void linker_unload(soinfo_t* si);

// 查找符号
void* linker_find_symbol(soinfo_t* si, const char* name);

// 查找全局符号（在所有已加载库中查找）
void* linker_find_global_symbol(const char* name);

// 执行重定位
int linker_relocate(soinfo_t* si);

// 调用初始化函数
void linker_call_constructors(soinfo_t* si);

// 调用析构函数
void linker_call_destructors(soinfo_t* si);

// 设置错误信息
void linker_set_error(const char* fmt, ...);

// 获取错误信息
const char* linker_get_error(void);

// 清除错误
void linker_clear_error(void);

// 调试：打印 soinfo
void soinfo_print(soinfo_t* si);

#endif // LINKER_H
