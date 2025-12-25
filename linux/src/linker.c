/**
 * =============================================================================
 * linker.c - 自定义动态链接器实现
 * =============================================================================
 *
 * 本文件实现了一个最小化的 ELF64 动态链接器，模仿 Android 的 linker 设计。
 * 主要功能包括：
 *   1. 加载共享库 (.so) 到内存
 *   2. 解析 ELF 格式和动态段
 *   3. 符号查找（支持 ELF hash 和 GNU hash）
 *   4. 执行重定位（修正地址引用）
 *   5. 调用构造/析构函数
 *
 * 动态链接的基本流程：
 *   ┌─────────────────┐
 *   │  打开 ELF 文件   │
 *   └────────┬────────┘
 *            ▼
 *   ┌─────────────────┐
 *   │ 解析 ELF 头     │  验证魔数、架构等
 *   └────────┬────────┘
 *            ▼
 *   ┌─────────────────┐
 *   │ 映射 PT_LOAD 段 │  使用 mmap 映射到内存
 *   └────────┬────────┘
 *            ▼
 *   ┌─────────────────┐
 *   │ 解析动态段      │  获取符号表、重定位表等
 *   └────────┬────────┘
 *            ▼
 *   ┌─────────────────┐
 *   │ 执行重定位      │  修正所有地址引用
 *   └────────┬────────┘
 *            ▼
 *   ┌─────────────────┐
 *   │ 调用构造函数    │  DT_INIT, DT_INIT_ARRAY
 *   └─────────────────┘
 *
 * =============================================================================
 */

#define _GNU_SOURCE
#include "linker.h"
#include "elf_parser.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dlfcn.h>  /* 用于 dlsym(RTLD_DEFAULT, ...) 从系统库查找符号 */

/* =============================================================================
 * 全局状态
 * =============================================================================
 */

/*
 * 全局链接器状态
 * - soinfo_list: 已加载库的链表头
 * - error_msg: 最近一次错误信息
 * - has_error: 是否有错误发生
 */
static linker_state_t g_linker = {0};

/* =============================================================================
 * 内存页对齐宏
 * =============================================================================
 *
 * 现代操作系统以"页"为单位管理内存，通常每页 4KB (4096 字节)。
 * mmap 等系统调用要求地址和大小必须页对齐。
 *
 * 示例：假设 PAGE_SIZE = 4096 (0x1000)
 *   PAGE_START(0x1234) = 0x1000  (向下对齐到页边界)
 *   PAGE_END(0x1234)   = 0x2000  (向上对齐到页边界)
 *   PAGE_OFFSET(0x1234) = 0x234  (页内偏移)
 */
#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE - 1))                    /* 0xFFFFF000 */
#define PAGE_START(x) ((x) & PAGE_MASK)                 /* 向下对齐 */
#define PAGE_END(x) PAGE_START((x) + PAGE_SIZE - 1)     /* 向上对齐 */
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)               /* 页内偏移 */

/* =============================================================================
 * 错误处理函数
 * =============================================================================
 */

/**
 * linker_init - 初始化链接器
 *
 * 清零全局状态结构体，准备开始加载库。
 * 应该在程序开始时调用一次。
 */
void linker_init(void) {
    memset(&g_linker, 0, sizeof(g_linker));
}

/**
 * linker_set_error - 设置错误信息
 * @fmt: printf 风格的格式字符串
 * @...: 可变参数
 *
 * 类似于 dlerror() 的实现，保存最近一次的错误信息。
 * 使用 vsnprintf 防止缓冲区溢出。
 */
void linker_set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_linker.error_msg, sizeof(g_linker.error_msg), fmt, args);
    va_end(args);
    g_linker.has_error = true;
}

/**
 * linker_get_error - 获取错误信息
 *
 * 返回最近一次的错误信息，并清除错误状态。
 * 这与标准 dlerror() 的行为一致：每次调用后错误会被清除。
 *
 * 返回: 错误信息字符串，如果没有错误则返回 NULL
 */
const char* linker_get_error(void) {
    if (g_linker.has_error) {
        g_linker.has_error = false;
        return g_linker.error_msg;
    }
    return NULL;
}

/**
 * linker_clear_error - 清除错误状态
 *
 * 手动清除错误标志和错误信息。
 */
void linker_clear_error(void) {
    g_linker.has_error = false;
    g_linker.error_msg[0] = '\0';
}

/* =============================================================================
 * 辅助函数
 * =============================================================================
 */

/**
 * calculate_load_size - 计算加载所有段需要的内存大小
 * @phdr: 程序头表
 * @phnum: 程序头数量
 *
 * 遍历所有 PT_LOAD 段，找到最小和最大的虚拟地址，
 * 从而计算出需要预留的连续内存空间大小。
 *
 * ELF 文件中的 PT_LOAD 段布局示例：
 *
 *   虚拟地址空间:
 *   0x0000 ┌──────────────────┐ ◄── min_vaddr
 *          │   PT_LOAD (R--)  │  .text, .rodata
 *   0x1000 ├──────────────────┤
 *          │   PT_LOAD (R-X)  │  代码段
 *   0x2000 ├──────────────────┤
 *          │   PT_LOAD (RW-)  │  .data, .bss
 *   0x3000 └──────────────────┘ ◄── max_vaddr
 *
 *   需要的内存大小 = max_vaddr - min_vaddr = 0x3000
 *
 * 返回: 页对齐后的总大小，如果没有可加载段则返回 0
 */
static size_t calculate_load_size(Elf64_Phdr* phdr, size_t phnum) {
    Elf64_Addr min_vaddr = (Elf64_Addr)-1;  /* 初始化为最大值 */
    Elf64_Addr max_vaddr = 0;

    /* 遍历所有程序头，只关心 PT_LOAD 类型 */
    for (size_t i = 0; i < phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;

        /* 更新最小虚拟地址 */
        if (phdr[i].p_vaddr < min_vaddr) {
            min_vaddr = phdr[i].p_vaddr;
        }

        /* 更新最大虚拟地址 (段起始 + 内存大小) */
        Elf64_Addr end = phdr[i].p_vaddr + phdr[i].p_memsz;
        if (end > max_vaddr) {
            max_vaddr = end;
        }
    }

    /* 没有找到可加载段 */
    if (min_vaddr > max_vaddr) {
        return 0;
    }

    /* 页对齐：向下对齐起始地址，向上对齐结束地址 */
    min_vaddr = PAGE_START(min_vaddr);
    max_vaddr = PAGE_END(max_vaddr);

    return max_vaddr - min_vaddr;
}

/**
 * elf_to_mmap_prot - 将 ELF 权限标志转换为 mmap 权限标志
 * @p_flags: ELF 程序头中的权限标志 (PF_R, PF_W, PF_X)
 *
 * ELF 和 mmap 使用不同的权限定义：
 *   ELF:  PF_R=0x4, PF_W=0x2, PF_X=0x1
 *   mmap: PROT_READ=0x1, PROT_WRITE=0x2, PROT_EXEC=0x4
 *
 * 返回: mmap 兼容的权限标志
 */
static int elf_to_mmap_prot(uint32_t p_flags) {
    int prot = 0;
    if (p_flags & PF_R) prot |= PROT_READ;
    if (p_flags & PF_W) prot |= PROT_WRITE;
    if (p_flags & PF_X) prot |= PROT_EXEC;
    return prot;
}

/* =============================================================================
 * 动态段解析
 * =============================================================================
 */

/**
 * parse_dynamic - 解析动态段 (PT_DYNAMIC)
 * @si: 共享库信息结构体
 *
 * 动态段包含了动态链接所需的所有信息，是一个 Elf64_Dyn 数组，
 * 每个条目包含一个标签 (d_tag) 和一个值 (d_un)。
 *
 * 重要的动态段标签：
 *
 *   标签           | 说明
 *   ---------------|------------------------------------------
 *   DT_SYMTAB      | 符号表地址
 *   DT_STRTAB      | 字符串表地址（符号名等）
 *   DT_STRSZ       | 字符串表大小
 *   DT_HASH        | ELF hash 表地址（用于符号查找加速）
 *   DT_GNU_HASH    | GNU hash 表地址（更快的符号查找）
 *   DT_RELA        | RELA 重定位表地址
 *   DT_RELASZ      | RELA 重定位表大小
 *   DT_JMPREL      | PLT 重定位表地址
 *   DT_PLTRELSZ    | PLT 重定位表大小
 *   DT_INIT        | 初始化函数地址
 *   DT_FINI        | 析构函数地址
 *   DT_INIT_ARRAY  | 初始化函数数组地址
 *   DT_FINI_ARRAY  | 析构函数数组地址
 *
 * 返回: 成功返回 0，失败返回 -1
 */
static int parse_dynamic(soinfo_t* si) {
    if (!si->dynamic) {
        linker_set_error("No dynamic section");
        return -1;
    }

    /*
     * 遍历动态段数组，直到遇到 DT_NULL 结束标记。
     * 每个条目的 d_un 是一个联合体，可能是地址 (d_ptr) 或值 (d_val)。
     * 这里的地址是相对于文件开头的偏移，需要加上 load_bias 得到实际内存地址。
     */
    for (Elf64_Dyn* d = si->dynamic; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {
            /* ============ 符号表相关 ============ */
            case DT_SYMTAB:
                /* 符号表：包含所有符号的定义 */
                si->symtab = (Elf64_Sym*)((uint8_t*)si->load_bias + d->d_un.d_ptr);
                break;

            case DT_STRTAB:
                /* 字符串表：存储符号名称等字符串 */
                si->strtab = (const char*)((uint8_t*)si->load_bias + d->d_un.d_ptr);
                break;

            case DT_STRSZ:
                /* 字符串表大小 */
                si->strtab_size = d->d_un.d_val;
                break;

            /* ============ 哈希表（用于加速符号查找）============ */
            case DT_HASH:
                /*
                 * ELF hash 表结构：
                 *   [0]: nbucket (bucket 数量)
                 *   [1]: nchain  (chain 数量，等于符号数量)
                 *   [2...nbucket+1]: bucket 数组
                 *   [nbucket+2...]: chain 数组
                 */
                si->hash = (uint32_t*)((uint8_t*)si->load_bias + d->d_un.d_ptr);
                break;

            case DT_GNU_HASH:
                /*
                 * GNU hash 表：比 ELF hash 更快，使用 bloom filter
                 * 结构更复杂，但查找效率更高
                 */
                si->gnu_hash = (uint32_t*)((uint8_t*)si->load_bias + d->d_un.d_ptr);
                break;

            /* ============ 重定位表 ============ */
            case DT_RELA:
                /* RELA 重定位表：用于修正数据引用 */
                si->rela = (Elf64_Rela*)((uint8_t*)si->load_bias + d->d_un.d_ptr);
                break;

            case DT_RELASZ:
                /* RELA 表大小，除以条目大小得到条目数 */
                si->rela_count = d->d_un.d_val / sizeof(Elf64_Rela);
                break;

            case DT_JMPREL:
                /* PLT 重定位表：用于修正函数调用 */
                si->plt_rela = (Elf64_Rela*)((uint8_t*)si->load_bias + d->d_un.d_ptr);
                break;

            case DT_PLTRELSZ:
                /* PLT 重定位表大小 */
                si->plt_rela_count = d->d_un.d_val / sizeof(Elf64_Rela);
                break;

            /* ============ 初始化/析构函数 ============ */
            case DT_INIT:
                /* 单个初始化函数（旧式，现在较少使用）*/
                si->init_func = (void(*)(void))((uint8_t*)si->load_bias + d->d_un.d_ptr);
                break;

            case DT_FINI:
                /* 单个析构函数 */
                si->fini_func = (void(*)(void))((uint8_t*)si->load_bias + d->d_un.d_ptr);
                break;

            case DT_INIT_ARRAY:
                /* 初始化函数数组（现代编译器使用这个）*/
                si->init_array = (void(**)(void))((uint8_t*)si->load_bias + d->d_un.d_ptr);
                break;

            case DT_INIT_ARRAYSZ:
                /* 初始化函数数组大小 */
                si->init_array_count = d->d_un.d_val / sizeof(void*);
                break;

            case DT_FINI_ARRAY:
                /* 析构函数数组 */
                si->fini_array = (void(**)(void))((uint8_t*)si->load_bias + d->d_un.d_ptr);
                break;

            case DT_FINI_ARRAYSZ:
                /* 析构函数数组大小 */
                si->fini_array_count = d->d_un.d_val / sizeof(void*);
                break;
        }
    }

    /* 符号表和字符串表是必需的 */
    if (!si->symtab || !si->strtab) {
        linker_set_error("Missing symbol table or string table");
        return -1;
    }

    return 0;
}

/* =============================================================================
 * 符号查找
 * =============================================================================
 */

/**
 * get_symbol_count - 获取符号数量
 * @si: 共享库信息
 *
 * 从 ELF hash 表获取符号数量。
 * ELF hash 表的 nchain 字段等于符号数量。
 *
 * 返回: 符号数量，如果没有 hash 表则返回一个默认值
 */
static size_t get_symbol_count(soinfo_t* si) {
    if (si->hash) {
        /* ELF hash 结构: [nbucket, nchain, ...] */
        /* nchain 等于符号数量 */
        return si->hash[1];
    }
    /* 没有 hash 表时使用默认上限 */
    return 256;
}

/**
 * elf_hash - 计算 ELF hash 值
 * @name: 符号名称
 *
 * 这是 ELF 规范定义的标准 hash 算法。
 * 用于在 ELF hash 表中快速定位符号。
 *
 * 算法步骤：
 *   1. 初始化 h = 0
 *   2. 对每个字符：h = (h << 4) + char
 *   3. 如果 h 的高 4 位非零：h ^= (高4位 >> 24)
 *   4. 清除高 4 位
 *
 * 返回: 32 位 hash 值
 */
static uint32_t elf_hash(const char* name) {
    uint32_t h = 0, g;
    const unsigned char* s = (const unsigned char*)name;

    while (*s) {
        h = (h << 4) + *s++;
        if ((g = h & 0xf0000000) != 0) {
            h ^= g >> 24;
        }
        h &= ~g;
    }
    return h;
}

/**
 * gnu_hash - 计算 GNU hash 值
 * @name: 符号名称
 *
 * GNU hash 算法，比 ELF hash 更快。
 * 使用 DJB hash 变体：h = h * 33 + c
 *
 * 返回: 32 位 hash 值
 */
static uint32_t gnu_hash(const char* name) {
    uint32_t h = 5381;  /* 魔数初始值 */
    const unsigned char* s = (const unsigned char*)name;

    while (*s) {
        h = (h << 5) + h + *s++;  /* h * 33 + c */
    }
    return h;
}

/**
 * gnu_lookup - 使用 GNU hash 查找符号
 * @si: 共享库信息
 * @name: 符号名称
 *
 * GNU hash 表结构比 ELF hash 更复杂但更高效：
 *
 *   +----------------+
 *   | nbuckets       |  bucket 数量
 *   | symoffset      |  第一个符号的索引
 *   | bloom_size     |  bloom filter 大小
 *   | bloom_shift    |  bloom filter 移位量
 *   +----------------+
 *   | bloom[0..n-1]  |  bloom filter 数组
 *   +----------------+
 *   | buckets[0..n-1]|  bucket 数组
 *   +----------------+
 *   | chains[...]    |  hash chain 数组
 *   +----------------+
 *
 * 查找步骤：
 *   1. 计算 hash
 *   2. 检查 bloom filter（快速排除不存在的符号）
 *   3. 查找 bucket
 *   4. 遍历 chain 直到找到匹配
 *
 * 返回: 符号指针，未找到返回 NULL
 */
static Elf64_Sym* gnu_lookup(soinfo_t* si, const char* name) {
    if (!si->gnu_hash) return NULL;

    /* 解析 GNU hash 头部 */
    uint32_t* gnu = si->gnu_hash;
    uint32_t nbuckets = gnu[0];
    uint32_t symoffset = gnu[1];
    uint32_t bloom_size = gnu[2];
    uint32_t bloom_shift = gnu[3];

    /* 定位各个数组 */
    uint64_t* bloom = (uint64_t*)&gnu[4];
    uint32_t* buckets = (uint32_t*)&bloom[bloom_size];
    uint32_t* chain = &buckets[nbuckets];

    uint32_t h1 = gnu_hash(name);

    /*
     * Bloom filter 检查
     * Bloom filter 是一种概率数据结构，可以快速判断元素"肯定不存在"
     * 如果两个位都被设置，符号可能存在；否则符号肯定不存在
     */
    uint64_t word = bloom[(h1 / 64) % bloom_size];
    uint64_t mask = (1ULL << (h1 % 64)) | (1ULL << ((h1 >> bloom_shift) % 64));
    if ((word & mask) != mask) {
        return NULL;  /* 符号肯定不存在 */
    }

    /* 查找 bucket */
    uint32_t n = buckets[h1 % nbuckets];
    if (n == 0) {
        return NULL;  /* bucket 为空 */
    }

    /* 遍历 chain */
    do {
        Elf64_Sym* sym = &si->symtab[n];
        uint32_t h2 = chain[n - symoffset];

        /*
         * 比较 hash 的高 31 位
         * 最低位用作 chain 结束标记，所以只比较高 31 位
         */
        if (((h1 ^ h2) >> 1) == 0) {
            const char* sym_name = si->strtab + sym->st_name;
            if (strcmp(sym_name, name) == 0) {
                return sym;  /* 找到匹配 */
            }
        }

        /* 检查是否是 chain 的最后一个（最低位为 1 表示结束）*/
        if (h2 & 1) break;
        n++;
    } while (1);

    return NULL;
}

/**
 * linker_find_symbol - 在指定库中查找符号
 * @si: 共享库信息
 * @name: 符号名称
 *
 * 查找策略：
 *   1. 优先使用 GNU hash（更快）
 *   2. 其次使用 ELF hash
 *   3. 最后使用线性搜索（作为后备）
 *
 * 返回: 符号地址，未找到返回 NULL
 */
void* linker_find_symbol(soinfo_t* si, const char* name) {
    if (!si || !si->symtab || !si->strtab) {
        return NULL;
    }

    Elf64_Sym* sym = NULL;

    /* ============ 方法 1: GNU hash 查找 ============ */
    if (si->gnu_hash) {
        sym = gnu_lookup(si, name);
        if (sym && sym->st_shndx != SHN_UNDEF) {
            /*
             * 检查符号绑定类型：
             * STB_GLOBAL: 全局符号，可以被其他库引用
             * STB_WEAK: 弱符号，可以被强符号覆盖
             */
            unsigned char bind = ELF64_ST_BIND(sym->st_info);
            if (bind == STB_GLOBAL || bind == STB_WEAK) {
                return (uint8_t*)si->load_bias + sym->st_value;
            }
        }
    }

    /* ============ 方法 2: ELF hash 查找 ============ */
    if (si->hash) {
        uint32_t nbucket = si->hash[0];
        uint32_t* bucket = &si->hash[2];
        uint32_t* chain = &si->hash[2 + nbucket];

        uint32_t hash = elf_hash(name);

        /*
         * ELF hash 查找过程：
         *   1. 用 hash % nbucket 找到起始 bucket
         *   2. 沿着 chain 遍历，直到 chain[i] == 0
         */
        for (uint32_t i = bucket[hash % nbucket]; i != 0; i = chain[i]) {
            sym = &si->symtab[i];
            const char* sym_name = si->strtab + sym->st_name;

            if (strcmp(sym_name, name) == 0) {
                if (sym->st_shndx == SHN_UNDEF) {
                    continue;  /* 未定义符号，继续搜索 */
                }
                unsigned char bind = ELF64_ST_BIND(sym->st_info);
                if (bind == STB_GLOBAL || bind == STB_WEAK) {
                    return (uint8_t*)si->load_bias + sym->st_value;
                }
            }
        }
    }

    /* ============ 方法 3: 线性搜索（后备方案）============ */
    if (!si->hash && !si->gnu_hash) {
        size_t sym_count = get_symbol_count(si);
        for (size_t i = 0; i < sym_count; i++) {
            sym = &si->symtab[i];
            if (sym->st_name == 0) continue;

            const char* sym_name = si->strtab + sym->st_name;
            if (strcmp(sym_name, name) == 0) {
                if (sym->st_shndx == SHN_UNDEF) {
                    continue;
                }
                unsigned char bind = ELF64_ST_BIND(sym->st_info);
                if (bind == STB_GLOBAL || bind == STB_WEAK) {
                    return (uint8_t*)si->load_bias + sym->st_value;
                }
            }
        }
    }

    return NULL;
}

/**
 * linker_find_global_symbol - 在所有已加载库中查找符号
 * @name: 符号名称
 *
 * 查找顺序：
 *   1. 首先在我们自己加载的库中查找
 *   2. 然后通过系统的 dlsym(RTLD_DEFAULT) 查找系统库
 *
 * 这样可以让加载的库调用 libc 函数（如 printf）。
 *
 * 返回: 符号地址，未找到返回 NULL
 */
void* linker_find_global_symbol(const char* name) {
    /* 先在我们加载的库中查找 */
    for (soinfo_t* si = g_linker.soinfo_list; si != NULL; si = si->next) {
        void* sym = linker_find_symbol(si, name);
        if (sym) return sym;
    }

    /*
     * 从系统库中查找
     * RTLD_DEFAULT 表示在默认搜索范围内查找
     * 这允许加载的 .so 调用 libc 函数
     */
    void* sym = dlsym(RTLD_DEFAULT, name);
    if (sym) {
        return sym;
    }

    return NULL;
}

/* =============================================================================
 * 重定位
 * =============================================================================
 */

/**
 * do_reloc - 执行单个重定位
 * @si: 共享库信息
 * @rela: 重定位条目
 *
 * 重定位是动态链接的核心步骤之一。
 * 当共享库被加载到内存中时，其代码和数据中的某些地址引用需要被修正，
 * 因为库的实际加载地址可能与编译时预期的地址不同。
 *
 * RELA 重定位条目结构：
 *   struct Elf64_Rela {
 *       Elf64_Addr r_offset;   // 需要修正的位置（相对于段起始）
 *       Elf64_Xword r_info;    // 符号索引和重定位类型
 *       Elf64_Sxword r_addend; // 加数
 *   };
 *
 * r_info 的解析：
 *   - 高 32 位: 符号索引 (ELF64_R_SYM)
 *   - 低 32 位: 重定位类型 (ELF64_R_TYPE)
 *
 * 返回: 成功返回 0，失败返回 -1
 */
static int do_reloc(soinfo_t* si, Elf64_Rela* rela) {
    /* 提取重定位类型和符号索引 */
    uint32_t type = ELF64_R_TYPE(rela->r_info);
    uint32_t sym_idx = ELF64_R_SYM(rela->r_info);

    /* 计算需要修正的内存地址 */
    void* reloc_addr = (uint8_t*)si->load_bias + rela->r_offset;
    void* sym_addr = NULL;

    /* 如果有符号索引，查找符号地址 */
    if (sym_idx != 0) {
        Elf64_Sym* sym = &si->symtab[sym_idx];
        const char* sym_name = si->strtab + sym->st_name;

        /*
         * 符号查找策略：
         * 1. 如果符号在当前库中已定义（st_shndx != SHN_UNDEF），使用本地定义
         * 2. 否则在全局范围（其他库和系统库）中查找
         */
        if (sym->st_shndx != SHN_UNDEF) {
            sym_addr = (uint8_t*)si->load_bias + sym->st_value;
        } else {
            sym_addr = linker_find_global_symbol(sym_name);
        }

        /* 如果非弱符号找不到，记录警告但继续执行 */
        if (!sym_addr && ELF64_ST_BIND(sym->st_info) != STB_WEAK) {
            LOG_WARN("Cannot find symbol: %s\n", sym_name);
            /* 允许继续，某些符号可能是可选的 */
        }
    }

    /*
     * 根据重定位类型执行修正
     *
     * x86_64 常见重定位类型：
     *
     *   类型                | 计算公式    | 说明
     *   --------------------|-------------|----------------------------------
     *   R_X86_64_NONE       | -           | 无操作
     *   R_X86_64_64         | S + A       | 64 位绝对地址
     *   R_X86_64_GLOB_DAT   | S           | GOT 条目（全局数据）
     *   R_X86_64_JUMP_SLOT  | S           | PLT 条目（函数调用）
     *   R_X86_64_RELATIVE   | B + A       | 相对于加载基址
     *   R_X86_64_COPY       | -           | 复制符号内容
     *
     *   其中: S = 符号地址, A = addend, B = load_bias
     */
    switch (type) {
        case R_X86_64_NONE:
            /* 无操作，占位符 */
            break;

        case R_X86_64_64:
            /*
             * 绝对地址重定位: S + A
             * 用于直接引用符号地址的情况
             * 例如: static void* ptr = &some_func;
             */
            *(uint64_t*)reloc_addr = (uint64_t)sym_addr + rela->r_addend;
            break;

        case R_X86_64_GLOB_DAT:
            /*
             * 全局数据偏移表 (GOT) 条目: S
             * 用于访问全局变量
             * GOT 中存储变量的实际地址
             */
            *(uint64_t*)reloc_addr = (uint64_t)sym_addr;
            break;

        case R_X86_64_JUMP_SLOT:
            /*
             * 过程链接表 (PLT) 条目: S
             * 用于函数调用
             * PLT 中存储函数的实际地址
             */
            *(uint64_t*)reloc_addr = (uint64_t)sym_addr;
            break;

        case R_X86_64_RELATIVE:
            /*
             * 相对地址重定位: B + A
             * 这是最常见的重定位类型
             * 用于与位置无关代码 (PIC) 中的地址计算
             * 不需要符号查找，只需加上加载偏移
             */
            *(uint64_t*)reloc_addr = (uint64_t)si->load_bias + rela->r_addend;
            break;

        case R_X86_64_COPY:
            /*
             * 复制重定位
             * 将符号的内容复制到目标位置
             * 主要用于可执行文件中的全局变量
             */
            if (sym_addr) {
                Elf64_Sym* sym = &si->symtab[sym_idx];
                memcpy(reloc_addr, sym_addr, sym->st_size);
            }
            break;

        default:
            LOG_WARN("Unsupported relocation type: %u\n", type);
            break;
    }

    return 0;
}

/**
 * linker_relocate - 执行所有重定位
 * @si: 共享库信息
 *
 * 处理两种类型的重定位表：
 *   1. RELA (.rela.dyn): 数据重定位
 *   2. PLT RELA (.rela.plt): 函数调用重定位
 *
 * 返回: 成功返回 0，失败返回 -1
 */
int linker_relocate(soinfo_t* si) {
    /* 处理 RELA 重定位（数据引用）*/
    if (si->rela) {
        for (size_t i = 0; i < si->rela_count; i++) {
            if (do_reloc(si, &si->rela[i]) < 0) {
                return -1;
            }
        }
    }

    /* 处理 PLT 重定位（函数调用）*/
    if (si->plt_rela) {
        for (size_t i = 0; i < si->plt_rela_count; i++) {
            if (do_reloc(si, &si->plt_rela[i]) < 0) {
                return -1;
            }
        }
    }

    return 0;
}

/* =============================================================================
 * 库加载与卸载
 * =============================================================================
 */

/**
 * linker_load - 加载共享库
 * @path: 共享库文件路径
 *
 * 这是链接器的核心函数，完整的加载流程如下：
 *
 *   1. 打开并解析 ELF 文件
 *   2. 分配 soinfo 结构体
 *   3. 计算需要的内存大小
 *   4. 预留地址空间 (PROT_NONE)
 *   5. 映射每个 PT_LOAD 段
 *   6. 处理 BSS 段
 *   7. 解析动态段
 *   8. 执行重定位
 *   9. 添加到已加载库列表
 *
 * 返回: 成功返回 soinfo 指针，失败返回 NULL
 */
soinfo_t* linker_load(const char* path) {
    elf_file_t elf;
    soinfo_t* si = NULL;
    int fd = -1;

    LOG("[linker] Loading: %s\n", path);

    /* ============ 步骤 1: 打开 ELF 文件 ============ */
    if (elf_open(path, &elf) < 0) {
        linker_set_error("Failed to open: %s", path);
        return NULL;
    }

    /* ============ 步骤 2: 分配 soinfo ============ */
    si = (soinfo_t*)calloc(1, sizeof(soinfo_t));
    if (!si) {
        linker_set_error("Out of memory");
        elf_close(&elf);
        return NULL;
    }

    strncpy(si->name, path, sizeof(si->name) - 1);
    si->phdr = elf.phdr;
    si->phnum = elf.ehdr->e_phnum;

    /* ============ 步骤 3: 计算加载大小 ============ */
    size_t load_size = calculate_load_size(elf.phdr, elf.ehdr->e_phnum);
    if (load_size == 0) {
        linker_set_error("No loadable segments");
        goto error;
    }
    si->size = load_size;

    /* ============ 步骤 4: 找到最小虚拟地址 ============ */
    Elf64_Addr min_vaddr = (Elf64_Addr)-1;
    for (size_t i = 0; i < elf.ehdr->e_phnum; i++) {
        if (elf.phdr[i].p_type == PT_LOAD && elf.phdr[i].p_vaddr < min_vaddr) {
            min_vaddr = elf.phdr[i].p_vaddr;
        }
    }
    min_vaddr = PAGE_START(min_vaddr);

    /* ============ 步骤 5: 预留地址空间 ============ */
    /*
     * 使用 MAP_ANONYMOUS 预留一块连续的虚拟地址空间
     * PROT_NONE: 初始时不可访问
     * 后续会用 MAP_FIXED 在这块区域内映射实际内容
     *
     * 这种两步映射的好处：
     * 1. 确保所有段在连续的地址空间内
     * 2. 可以精确控制每个段的权限
     */
    si->base = mmap(NULL, load_size, PROT_NONE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (si->base == MAP_FAILED) {
        linker_set_error("mmap failed");
        goto error;
    }

    /*
     * 计算加载偏移 (load_bias)
     *
     * load_bias = 实际加载地址 - ELF 中指定的虚拟地址
     *
     * 例如：
     *   ELF 中 min_vaddr = 0x0
     *   实际加载到 si->base = 0x7f0000000000
     *   则 load_bias = 0x7f0000000000 - 0x0 = 0x7f0000000000
     *
     * 后续所有地址计算都需要加上这个偏移
     */
    si->load_bias = (void*)((uint8_t*)si->base - min_vaddr);

    LOG("[linker] Base address: %p, load_bias: %p\n", si->base, si->load_bias);

    /* ============ 步骤 6: 打开文件用于 mmap ============ */
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        linker_set_error("Failed to open file for mmap");
        goto error;
    }

    /* ============ 步骤 7: 映射每个 PT_LOAD 段 ============ */
    for (size_t i = 0; i < elf.ehdr->e_phnum; i++) {
        Elf64_Phdr* phdr = &elf.phdr[i];
        if (phdr->p_type != PT_LOAD) continue;

        /*
         * 计算映射参数
         *
         * 段在内存中的布局：
         *
         *   seg_page_start    seg_start                seg_file_end  seg_end
         *         │              │                          │           │
         *         ▼              ▼                          ▼           ▼
         *   ┌─────┬──────────────┬──────────────────────────┬───────────┐
         *   │     │  文件内容    │        文件内容          │   BSS    │
         *   │ pad │  (filesz)    │                          │  (zero)   │
         *   └─────┴──────────────┴──────────────────────────┴───────────┘
         *         ◄──────────────────── memsz ────────────────────────────►
         *
         * p_filesz: 文件中的大小
         * p_memsz: 内存中的大小（可能大于 filesz，多出的是 BSS）
         */
        Elf64_Addr seg_start = (Elf64_Addr)si->load_bias + phdr->p_vaddr;
        Elf64_Addr seg_end = seg_start + phdr->p_memsz;
        Elf64_Addr seg_page_start = PAGE_START(seg_start);
        Elf64_Addr seg_page_end = PAGE_END(seg_end);
        Elf64_Addr seg_file_end = seg_start + phdr->p_filesz;

        Elf64_Off file_start = phdr->p_offset;
        Elf64_Off file_page_start = PAGE_START(file_start);

        /* 映射文件内容 */
        void* seg_addr = mmap((void*)seg_page_start,
                              seg_file_end - seg_page_start,
                              elf_to_mmap_prot(phdr->p_flags),
                              MAP_PRIVATE | MAP_FIXED,
                              fd, file_page_start);
        if (seg_addr == MAP_FAILED) {
            linker_set_error("Failed to mmap segment");
            goto error;
        }

        /*
         * 处理 BSS 段
         *
         * 如果 memsz > filesz，说明有 BSS 段（未初始化的全局变量）
         * BSS 段需要被清零
         */
        if (phdr->p_memsz > phdr->p_filesz) {
            /* 清零 BSS 部分 */
            Elf64_Addr bss_start = seg_file_end;
            Elf64_Addr bss_page_start = PAGE_END(bss_start);

            /* 清零文件末尾到页边界的部分 */
            if (bss_start < bss_page_start) {
                memset((void*)bss_start, 0, bss_page_start - bss_start);
            }

            /* 如果 BSS 跨越多个页，需要分配匿名内存 */
            if (seg_page_end > bss_page_start) {
                void* bss_addr = mmap((void*)bss_page_start,
                                      seg_page_end - bss_page_start,
                                      elf_to_mmap_prot(phdr->p_flags),
                                      MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS,
                                      -1, 0);
                if (bss_addr == MAP_FAILED) {
                    linker_set_error("Failed to mmap BSS");
                    goto error;
                }
            }
        }

        LOG("[linker] Loaded segment: vaddr=0x%lx, memsz=0x%lx, flags=%c%c%c\n",
               (unsigned long)phdr->p_vaddr,
               (unsigned long)phdr->p_memsz,
               (phdr->p_flags & PF_R) ? 'R' : '-',
               (phdr->p_flags & PF_W) ? 'W' : '-',
               (phdr->p_flags & PF_X) ? 'X' : '-');
    }

    close(fd);
    fd = -1;

    /* ============ 步骤 8: 查找并保存重要段的地址 ============ */
    for (size_t i = 0; i < elf.ehdr->e_phnum; i++) {
        if (elf.phdr[i].p_type == PT_PHDR) {
            /* 程序头表在内存中的地址 */
            si->phdr = (Elf64_Phdr*)((uint8_t*)si->load_bias + elf.phdr[i].p_vaddr);
        } else if (elf.phdr[i].p_type == PT_DYNAMIC) {
            /* 动态段在内存中的地址 */
            si->dynamic = (Elf64_Dyn*)((uint8_t*)si->load_bias + elf.phdr[i].p_vaddr);
        }
    }

    /* 如果没有 PT_PHDR，使用计算的地址 */
    if (!si->phdr) {
        si->phdr = (Elf64_Phdr*)((uint8_t*)si->load_bias + elf.ehdr->e_phoff);
    }

    /* ============ 步骤 9: 解析动态段 ============ */
    if (parse_dynamic(si) < 0) {
        goto error;
    }

    /* ============ 步骤 10: 执行重定位 ============ */
    if (linker_relocate(si) < 0) {
        goto error;
    }

    /* ============ 步骤 11: 添加到已加载库列表 ============ */
    si->ref_count = 1;
    si->next = g_linker.soinfo_list;
    g_linker.soinfo_list = si;

    elf_close(&elf);

    LOG("[linker] Successfully loaded: %s\n", path);
    return si;

error:
    if (fd >= 0) close(fd);
    if (si) {
        if (si->base && si->base != MAP_FAILED) {
            munmap(si->base, si->size);
        }
        free(si);
    }
    elf_close(&elf);
    return NULL;
}

/**
 * linker_unload - 卸载共享库
 * @si: 共享库信息
 *
 * 卸载步骤：
 *   1. 减少引用计数
 *   2. 如果引用计数为 0：
 *      a. 调用析构函数
 *      b. 从链表中移除
 *      c. 解除内存映射
 *      d. 释放 soinfo 结构
 */
void linker_unload(soinfo_t* si) {
    if (!si) return;

    si->ref_count--;
    if (si->ref_count > 0) return;

    /* 调用析构函数 */
    linker_call_destructors(si);

    /* 从链表中移除 */
    soinfo_t** p = &g_linker.soinfo_list;
    while (*p && *p != si) {
        p = &(*p)->next;
    }
    if (*p) {
        *p = si->next;
    }

    /* 释放内存 */
    if (si->base) {
        munmap(si->base, si->size);
    }
    free(si);
}

/* =============================================================================
 * 构造函数和析构函数
 * =============================================================================
 */

/**
 * is_valid_func_ptr - 检查函数指针是否有效
 * @ptr: 函数指针
 *
 * 某些情况下，init_array 中可能包含无效的指针（如 NULL 或 -1）
 * 调用这些无效指针会导致崩溃，需要过滤掉。
 *
 * 返回: 有效返回 1，无效返回 0
 */
static int is_valid_func_ptr(void* ptr) {
    /* NULL 和 -1 都是无效的 */
    if (ptr == NULL || ptr == (void*)-1) {
        return 0;
    }
    return 1;
}

/**
 * linker_call_constructors - 调用构造函数
 * @si: 共享库信息
 *
 * 构造函数的调用顺序：
 *   1. DT_INIT（单个初始化函数，旧式）
 *   2. DT_INIT_ARRAY（初始化函数数组，按顺序调用）
 *
 * 在 C 代码中，使用 __attribute__((constructor)) 标记的函数
 * 会被放入 .init_array 节中。
 *
 * 示例：
 *   __attribute__((constructor))
 *   void my_init(void) {
 *       printf("Library initialized!\n");
 *   }
 */
void linker_call_constructors(soinfo_t* si) {
    if (!si) return;

    /* 调用 DT_INIT */
    if (is_valid_func_ptr((void*)si->init_func)) {
        LOG("[linker] Calling DT_INIT for %s\n", si->name);
        si->init_func();
    }

    /* 调用 DT_INIT_ARRAY */
    if (si->init_array && si->init_array_count > 0) {
        LOG("[linker] Calling DT_INIT_ARRAY (%zu entries) for %s\n",
               si->init_array_count, si->name);
        for (size_t i = 0; i < si->init_array_count; i++) {
            if (is_valid_func_ptr((void*)si->init_array[i])) {
                LOG("[linker] Calling init_array[%zu] at %p\n", i, (void*)si->init_array[i]);
                si->init_array[i]();
            }
        }
    }
}

/**
 * linker_call_destructors - 调用析构函数
 * @si: 共享库信息
 *
 * 析构函数的调用顺序（与构造函数相反）：
 *   1. DT_FINI_ARRAY（逆序调用）
 *   2. DT_FINI（单个析构函数）
 *
 * 在 C 代码中，使用 __attribute__((destructor)) 标记的函数
 * 会被放入 .fini_array 节中。
 *
 * 示例：
 *   __attribute__((destructor))
 *   void my_fini(void) {
 *       printf("Library unloaded!\n");
 *   }
 */
void linker_call_destructors(soinfo_t* si) {
    if (!si) return;

    /* 调用 DT_FINI_ARRAY（逆序）*/
    if (si->fini_array && si->fini_array_count > 0) {
        LOG("[linker] Calling DT_FINI_ARRAY (%zu entries) for %s\n",
               si->fini_array_count, si->name);
        for (size_t i = si->fini_array_count; i > 0; i--) {
            if (is_valid_func_ptr((void*)si->fini_array[i-1])) {
                LOG("[linker] Calling fini_array[%zu] at %p\n", i-1, (void*)si->fini_array[i-1]);
                si->fini_array[i-1]();
            }
        }
    }

    /* 调用 DT_FINI */
    if (is_valid_func_ptr((void*)si->fini_func)) {
        LOG("[linker] Calling DT_FINI for %s\n", si->name);
        si->fini_func();
    }
}

/* =============================================================================
 * 调试辅助函数
 * =============================================================================
 */

/**
 * soinfo_print - 打印 soinfo 信息
 * @si: 共享库信息
 *
 * 用于调试，打印共享库的所有关键信息。
 */
void soinfo_print(soinfo_t* si) {
    if (!si) return;

    printf("\n=== soinfo: %s ===\n", si->name);
    printf("Base: %p\n", si->base);
    printf("Size: 0x%zx\n", si->size);
    printf("Load bias: %p\n", si->load_bias);
    printf("Phdr: %p (%zu entries)\n", (void*)si->phdr, si->phnum);
    printf("Dynamic: %p\n", (void*)si->dynamic);
    printf("Symtab: %p\n", (void*)si->symtab);
    printf("Strtab: %p (size: %zu)\n", si->strtab, si->strtab_size);
    printf("Hash: %p\n", (void*)si->hash);
    printf("GNU hash: %p\n", (void*)si->gnu_hash);
    printf("Rela: %p (%zu entries)\n", (void*)si->rela, si->rela_count);
    printf("PLT Rela: %p (%zu entries)\n", (void*)si->plt_rela, si->plt_rela_count);
    printf("Init: %p\n", (void*)si->init_func);
    printf("Fini: %p\n", (void*)si->fini_func);
    printf("Init array: %p (%zu entries)\n", (void*)si->init_array, si->init_array_count);
    printf("Fini array: %p (%zu entries)\n", (void*)si->fini_array, si->fini_array_count);
    printf("Ref count: %d\n", si->ref_count);
}
