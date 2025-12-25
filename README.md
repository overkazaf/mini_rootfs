# Mini Rootfs - 构建自定义动态链接环境

本项目演示如何构建一个最小化的 rootfs（根文件系统），实现动态库的加载与调用。项目包含两种实现方式：

1. **Android 方式**：使用系统提供的 `dlopen/dlsym` API
2. **Linux 方式**：从零实现一个自定义的 ELF 加载器（模仿 Android linker）

---

## 目录

- [1. 核心概念](#1-核心概念)
  - [1.1 什么是 Rootfs](#11-什么是-rootfs)
  - [1.2 ELF 文件格式](#12-elf-文件格式)
  - [1.3 动态链接原理](#13-动态链接原理)
- [2. 项目结构](#2-项目结构)
- [3. Android 方式：使用系统 dlopen](#3-android-方式使用系统-dlopen)
  - [3.1 创建共享库](#31-创建共享库)
  - [3.2 动态加载库](#32-动态加载库)
  - [3.3 构建与运行](#33-构建与运行)
- [4. Linux 方式：自定义 ELF 加载器](#4-linux-方式自定义-elf-加载器)
  - [4.1 ELF 解析器](#41-elf-解析器)
  - [4.2 链接器核心](#42-链接器核心)
  - [4.3 符号查找与重定位](#43-符号查找与重定位)
  - [4.4 实现 dlopen/dlsym](#44-实现-dlopendlsym)
- [5. 构建系统详解](#5-构建系统详解)
- [6. 实际运行演示](#6-实际运行演示)

---

## 1. 核心概念

### 1.1 什么是 Rootfs

Rootfs（Root Filesystem）是操作系统的根文件系统，包含系统运行所需的基本文件结构：

```
rootfs/
├── bin/           # 可执行文件
├── lib/           # 共享库 (.so 文件)
├── etc/           # 配置文件
└── ...
```

在嵌入式系统或容器环境中，我们经常需要构建最小化的 rootfs。本项目聚焦于 **动态库加载** 这一核心功能。

### 1.2 ELF 文件格式

ELF（Executable and Linkable Format）是 Linux/Android 系统使用的可执行文件格式。

#### ELF 文件结构

```
+-------------------+
|    ELF Header     |  <- 文件头，描述文件类型、架构等
+-------------------+
| Program Headers   |  <- 程序头表，描述如何加载到内存
+-------------------+
|                   |
|    Sections       |  <- 各个节（.text, .data, .rodata 等）
|                   |
+-------------------+
| Section Headers   |  <- 节头表，描述各节的属性
+-------------------+
```

#### 关键的程序头类型

| 类型 | 说明 |
|------|------|
| `PT_LOAD` | 可加载段，需要映射到内存 |
| `PT_DYNAMIC` | 动态链接信息 |
| `PT_INTERP` | 解释器路径（如 `/lib/ld-linux.so`）|
| `PT_GNU_RELRO` | 只读重定位段 |

#### 查看 ELF 信息

```bash
# 查看 ELF 头
readelf -h libdemo.so

# 查看程序头
readelf -l libdemo.so

# 查看节头
readelf -S libdemo.so

# 查看动态段
readelf -d libdemo.so

# 查看符号表
nm -D libdemo.so
```

### 1.3 动态链接原理

动态链接允许程序在运行时加载共享库，而不是在编译时静态链接。

#### 动态链接的关键步骤

```
1. 打开 ELF 文件
       ↓
2. 解析 ELF 头和程序头
       ↓
3. 将 PT_LOAD 段映射到内存
       ↓
4. 解析动态段 (PT_DYNAMIC)
       ↓
5. 执行重定位 (修正地址引用)
       ↓
6. 调用初始化函数 (constructor)
       ↓
7. 库可用，可调用导出函数
```

---

## 2. 项目结构

```
mini_rootfs/
├── android/                    # Android 方式实现
│   ├── Makefile               # 构建脚本
│   └── src/
│       ├── main.c             # 主程序（使用系统 dlopen）
│       ├── demo.c             # 示例共享库 1
│       └── demo2.c            # 示例共享库 2
│
├── linux/                      # Linux 方式实现（自定义 linker）
│   ├── Makefile               # 构建脚本
│   ├── lib/
│   │   ├── elf.h              # ELF 格式定义
│   │   ├── elf_parser.h       # ELF 解析器头文件
│   │   ├── linker.h           # 链接器头文件
│   │   ├── mini_dlfcn.h       # 自定义 dlopen API
│   │   └── log.h              # 日志系统
│   ├── src/
│   │   ├── elf_parser.c       # ELF 文件解析
│   │   ├── linker.c           # 核心链接器实现
│   │   ├── dlfcn.c            # dlopen/dlsym 实现
│   │   └── log.c              # 日志实现
│   └── test/
│       ├── main.c             # 测试主程序
│       └── test_lib.c         # 测试共享库
│
└── README.md                   # 本文档
```

---

## 3. Android 方式：使用系统 dlopen

这种方式使用系统提供的动态链接 API，简单直接。

### 3.1 创建共享库

共享库需要导出函数供外部调用。

**demo.c** - 示例共享库：

```c
#include <stdio.h>

/* 导出函数：打印欢迎信息 */
void demo_hello(void) {
    printf("[demo.so] Hello from demo shared library!\n");
}

/* 导出函数：加法计算 */
int demo_add(int a, int b) {
    printf("[demo.so] Calculating %d + %d\n", a, b);
    return a + b;
}

/* 导出函数：获取版本信息 */
const char* demo_version(void) {
    return "Demo Library v1.0";
}

/* 库加载时自动调用 (constructor 属性) */
__attribute__((constructor))
void demo_init(void) {
    printf("[demo.so] Library loaded! (constructor called)\n");
}

/* 库卸载时自动调用 (destructor 属性) */
__attribute__((destructor))
void demo_fini(void) {
    printf("[demo.so] Library unloading! (destructor called)\n");
}
```

#### 关键点说明

1. **`__attribute__((constructor))`**：标记函数为构造函数，在库加载时自动调用
2. **`__attribute__((destructor))`**：标记函数为析构函数，在库卸载时自动调用
3. **导出函数**：默认情况下，非 `static` 函数都会被导出

### 3.2 动态加载库

**main.c** - 使用 dlopen 加载库：

```c
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>      // dlopen, dlsym, dlclose, dlerror

/* 定义函数指针类型 */
typedef void (*func_void)(void);
typedef int (*func_int_int_int)(int, int);
typedef const char* (*func_str_void)(void);

int main(int argc, char *argv[]) {
    /* 1. 打开共享库 */
    void *handle = dlopen("./lib/libdemo.so", RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }

    /* 2. 查找符号（函数地址） */
    // 清除之前的错误
    dlerror();

    // 获取 demo_hello 函数
    func_void hello = (func_void)dlsym(handle, "demo_hello");
    char *error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "dlsym failed: %s\n", error);
        dlclose(handle);
        return 1;
    }

    // 获取 demo_add 函数
    func_int_int_int add = (func_int_int_int)dlsym(handle, "demo_add");

    // 获取 demo_version 函数
    func_str_void version = (func_str_void)dlsym(handle, "demo_version");

    /* 3. 调用函数 */
    hello();                          // 调用 demo_hello
    int result = add(10, 20);         // 调用 demo_add
    printf("Result: %d\n", result);
    printf("Version: %s\n", version()); // 调用 demo_version

    /* 4. 关闭库 */
    dlclose(handle);

    return 0;
}
```

#### dlopen 函数详解

```c
void *dlopen(const char *filename, int flags);
```

| 参数 | 说明 |
|------|------|
| `filename` | 库文件路径 |
| `flags` | 加载标志 |

常用 flags：

| Flag | 说明 |
|------|------|
| `RTLD_NOW` | 立即解析所有符号 |
| `RTLD_LAZY` | 延迟解析符号（首次调用时解析）|
| `RTLD_GLOBAL` | 符号可被其他库使用 |
| `RTLD_LOCAL` | 符号仅在本库可见 |

#### dlsym 函数详解

```c
void *dlsym(void *handle, const char *symbol);
```

| 参数 | 说明 |
|------|------|
| `handle` | dlopen 返回的句柄，或特殊值 |
| `symbol` | 符号名称（函数名或变量名）|

特殊 handle 值：

| 值 | 说明 |
|------|------|
| `RTLD_DEFAULT` | 在所有已加载库中查找 |
| `RTLD_NEXT` | 在当前库之后的库中查找 |

### 3.3 构建与运行

**Makefile 关键部分**：

```makefile
# 编译器配置
CC = gcc
CFLAGS = -Wall -fPIC      # -fPIC: 生成位置无关代码（共享库必需）
LDFLAGS = -ldl            # 链接 libdl（提供 dlopen 等函数）

# 编译共享库
lib/libdemo.so: src/demo.c
	$(CC) $(CFLAGS) -shared -o $@ $<
	# -shared: 生成共享库
	# $@: 目标文件 (lib/libdemo.so)
	# $<: 第一个依赖 (src/demo.c)

# 编译主程序
loader: src/main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
```

**运行示例**：

```bash
cd android
make native    # 编译
make run       # 运行

# 输出:
# [demo.so] Library loaded! (constructor called)
# [demo.so] Hello from demo shared library!
# [demo.so] Calculating 10 + 20
# Result: 30
# Version: Demo Library v1.0
# [demo.so] Library unloading! (destructor called)
```

---

## 4. Linux 方式：自定义 ELF 加载器

这种方式从零实现 ELF 加载器，深入理解动态链接原理。

### 4.1 ELF 解析器

**elf_parser.h** - ELF 文件结构定义：

```c
#include <elf.h>    // 系统 ELF 头文件，定义了 Elf64_Ehdr 等

/* ELF 文件句柄 */
typedef struct {
    int fd;                  // 文件描述符
    void* map_start;         // mmap 映射起始地址
    size_t map_size;         // 映射大小
    Elf64_Ehdr* ehdr;        // ELF 头
    Elf64_Phdr* phdr;        // 程序头表
    Elf64_Shdr* shdr;        // 节头表
    const char* shstrtab;    // 节名字符串表
} elf_file_t;
```

**elf_parser.c** - 解析 ELF 文件：

```c
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

/* 验证 ELF 头 */
int elf_validate_header(const Elf64_Ehdr* ehdr) {
    /* 检查 ELF 魔数: 0x7F 'E' 'L' 'F' */
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||    // 0x7F
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||    // 'E'
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||    // 'L'
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {    // 'F'
        return -1;  // 不是 ELF 文件
    }

    /* 检查是 64 位 */
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        return -1;
    }

    /* 检查是共享库或可执行文件 */
    if (ehdr->e_type != ET_DYN && ehdr->e_type != ET_EXEC) {
        return -1;
    }

    return 0;
}

/* 打开并映射 ELF 文件 */
int elf_open(const char* path, elf_file_t* elf) {
    struct stat st;

    /* 打开文件 */
    elf->fd = open(path, O_RDONLY);
    if (elf->fd < 0) return -1;

    /* 获取文件大小 */
    fstat(elf->fd, &st);
    elf->map_size = st.st_size;

    /* mmap 映射整个文件到内存 */
    elf->map_start = mmap(NULL, elf->map_size,
                          PROT_READ, MAP_PRIVATE,
                          elf->fd, 0);
    if (elf->map_start == MAP_FAILED) return -1;

    /* 解析 ELF 头 (文件开头就是 ELF 头) */
    elf->ehdr = (Elf64_Ehdr*)elf->map_start;
    if (elf_validate_header(elf->ehdr) < 0) return -1;

    /* 解析程序头表 */
    elf->phdr = (Elf64_Phdr*)((uint8_t*)elf->map_start + elf->ehdr->e_phoff);

    /* 解析节头表 */
    elf->shdr = (Elf64_Shdr*)((uint8_t*)elf->map_start + elf->ehdr->e_shoff);

    return 0;
}
```

### 4.2 链接器核心

**linker.h** - soinfo 结构（模仿 Android）：

```c
/* SO 库信息结构 (模仿 Android 的 soinfo) */
typedef struct soinfo {
    char name[256];             // 库名

    /* 加载信息 */
    void* base;                 // 加载基地址
    size_t size;                // 总映射大小
    void* load_bias;            // 加载偏移 (实际地址 - 期望地址)

    /* ELF 结构 */
    Elf64_Phdr* phdr;           // 程序头表
    size_t phnum;               // 程序头数量
    Elf64_Dyn* dynamic;         // 动态段

    /* 符号表 */
    Elf64_Sym* symtab;          // 符号表
    const char* strtab;         // 字符串表

    /* 哈希表 (用于符号查找加速) */
    uint32_t* hash;             // ELF hash
    uint32_t* gnu_hash;         // GNU hash

    /* 重定位表 */
    Elf64_Rela* rela;           // RELA 重定位表
    size_t rela_count;
    Elf64_Rela* plt_rela;       // PLT 重定位表
    size_t plt_rela_count;

    /* 初始化/析构函数 */
    void (*init_func)(void);    // DT_INIT
    void (*fini_func)(void);    // DT_FINI
    void (**init_array)(void);  // DT_INIT_ARRAY
    size_t init_array_count;
    void (**fini_array)(void);  // DT_FINI_ARRAY
    size_t fini_array_count;

    struct soinfo* next;        // 链表
} soinfo_t;
```

**linker.c** - 加载共享库：

```c
#include <sys/mman.h>

#define PAGE_SIZE 4096
#define PAGE_START(x) ((x) & ~(PAGE_SIZE - 1))
#define PAGE_END(x)   PAGE_START((x) + PAGE_SIZE - 1)

/* 加载共享库 */
soinfo_t* linker_load(const char* path) {
    elf_file_t elf;
    soinfo_t* si;

    /* 1. 打开 ELF 文件 */
    if (elf_open(path, &elf) < 0) return NULL;

    /* 2. 分配 soinfo */
    si = calloc(1, sizeof(soinfo_t));
    strncpy(si->name, path, sizeof(si->name) - 1);

    /* 3. 计算需要的内存大小 */
    size_t load_size = calculate_load_size(elf.phdr, elf.ehdr->e_phnum);

    /* 4. 预留地址空间 */
    si->base = mmap(NULL, load_size, PROT_NONE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (si->base == MAP_FAILED) goto error;

    /* 5. 计算加载偏移 */
    Elf64_Addr min_vaddr = find_min_vaddr(elf.phdr, elf.ehdr->e_phnum);
    si->load_bias = (void*)((uint8_t*)si->base - min_vaddr);

    /* 6. 映射每个 PT_LOAD 段 */
    int fd = open(path, O_RDONLY);
    for (size_t i = 0; i < elf.ehdr->e_phnum; i++) {
        Elf64_Phdr* phdr = &elf.phdr[i];
        if (phdr->p_type != PT_LOAD) continue;

        /* 计算映射地址和偏移 */
        Elf64_Addr seg_start = (Elf64_Addr)si->load_bias + phdr->p_vaddr;
        Elf64_Addr seg_page_start = PAGE_START(seg_start);

        /* 映射文件内容 */
        mmap((void*)seg_page_start,
             phdr->p_filesz + (seg_start - seg_page_start),
             elf_to_mmap_prot(phdr->p_flags),  // 转换权限
             MAP_PRIVATE | MAP_FIXED,
             fd, PAGE_START(phdr->p_offset));

        /* 处理 BSS 段 (p_memsz > p_filesz) */
        if (phdr->p_memsz > phdr->p_filesz) {
            /* 清零 BSS 部分 */
            memset((void*)(seg_start + phdr->p_filesz), 0,
                   phdr->p_memsz - phdr->p_filesz);
        }
    }
    close(fd);

    /* 7. 解析动态段 */
    parse_dynamic(si);

    /* 8. 执行重定位 */
    linker_relocate(si);

    elf_close(&elf);
    return si;

error:
    free(si);
    elf_close(&elf);
    return NULL;
}
```

### 4.3 符号查找与重定位

**符号查找** - 使用 ELF hash 或 GNU hash：

```c
/* ELF hash 函数 */
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

/* 使用 ELF hash 查找符号 */
void* linker_find_symbol(soinfo_t* si, const char* name) {
    if (!si->hash || !si->symtab || !si->strtab) return NULL;

    uint32_t nbucket = si->hash[0];  // bucket 数量
    uint32_t* bucket = &si->hash[2]; // bucket 数组
    uint32_t* chain = &si->hash[2 + nbucket]; // chain 数组

    uint32_t hash = elf_hash(name);

    /* 在 hash 链中查找 */
    for (uint32_t i = bucket[hash % nbucket]; i != 0; i = chain[i]) {
        Elf64_Sym* sym = &si->symtab[i];
        const char* sym_name = si->strtab + sym->st_name;

        if (strcmp(sym_name, name) == 0 && sym->st_shndx != SHN_UNDEF) {
            /* 找到符号，返回地址 */
            return (uint8_t*)si->load_bias + sym->st_value;
        }
    }
    return NULL;
}
```

**重定位处理** - 修正地址引用：

```c
/* 执行单个重定位 */
static int do_reloc(soinfo_t* si, Elf64_Rela* rela) {
    uint32_t type = ELF64_R_TYPE(rela->r_info);    // 重定位类型
    uint32_t sym_idx = ELF64_R_SYM(rela->r_info);  // 符号索引

    /* 计算重定位地址 */
    void* reloc_addr = (uint8_t*)si->load_bias + rela->r_offset;

    /* 查找符号地址 */
    void* sym_addr = NULL;
    if (sym_idx != 0) {
        Elf64_Sym* sym = &si->symtab[sym_idx];
        const char* sym_name = si->strtab + sym->st_name;
        sym_addr = linker_find_global_symbol(sym_name);
    }

    /* 根据类型执行重定位 */
    switch (type) {
        case R_X86_64_NONE:
            /* 无操作 */
            break;

        case R_X86_64_64:
            /* 绝对地址: S + A */
            *(uint64_t*)reloc_addr = (uint64_t)sym_addr + rela->r_addend;
            break;

        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT:
            /* GOT/PLT 条目: S */
            *(uint64_t*)reloc_addr = (uint64_t)sym_addr;
            break;

        case R_X86_64_RELATIVE:
            /* 相对地址: B + A (B = load_bias) */
            *(uint64_t*)reloc_addr = (uint64_t)si->load_bias + rela->r_addend;
            break;
    }
    return 0;
}

/* 执行所有重定位 */
int linker_relocate(soinfo_t* si) {
    /* 处理 RELA 重定位 */
    for (size_t i = 0; i < si->rela_count; i++) {
        do_reloc(si, &si->rela[i]);
    }

    /* 处理 PLT 重定位 */
    for (size_t i = 0; i < si->plt_rela_count; i++) {
        do_reloc(si, &si->plt_rela[i]);
    }

    return 0;
}
```

常见重定位类型（x86_64）：

| 类型 | 计算公式 | 说明 |
|------|----------|------|
| `R_X86_64_64` | S + A | 绝对 64 位地址 |
| `R_X86_64_GLOB_DAT` | S | GOT 条目 |
| `R_X86_64_JUMP_SLOT` | S | PLT 条目 |
| `R_X86_64_RELATIVE` | B + A | 相对于加载地址 |

其中：S = 符号地址，A = addend，B = load_bias

### 4.4 实现 dlopen/dlsym

**dlfcn.c** - 封装 dlopen API：

```c
#include "linker.h"

/* mini_dlopen - 加载共享库 */
void* mini_dlopen(const char* path, int flags) {
    (void)flags;  // 暂时忽略 flags

    /* 加载库 */
    soinfo_t* si = linker_load(path);
    if (!si) return NULL;

    /* 调用构造函数 */
    linker_call_constructors(si);

    return (void*)si;
}

/* mini_dlsym - 获取符号地址 */
void* mini_dlsym(void* handle, const char* symbol) {
    if (handle == MINI_RTLD_DEFAULT) {
        /* 在所有已加载库中查找 */
        return linker_find_global_symbol(symbol);
    }

    /* 在指定库中查找 */
    soinfo_t* si = (soinfo_t*)handle;
    return linker_find_symbol(si, symbol);
}

/* mini_dlclose - 关闭库 */
int mini_dlclose(void* handle) {
    soinfo_t* si = (soinfo_t*)handle;
    linker_unload(si);  // 调用析构函数并释放资源
    return 0;
}

/* mini_dlerror - 获取错误信息 */
const char* mini_dlerror(void) {
    return linker_get_error();
}
```

**调用构造/析构函数**：

```c
/* 调用构造函数 */
void linker_call_constructors(soinfo_t* si) {
    /* 1. 调用 DT_INIT */
    if (si->init_func) {
        si->init_func();
    }

    /* 2. 调用 DT_INIT_ARRAY */
    if (si->init_array) {
        for (size_t i = 0; i < si->init_array_count; i++) {
            if (si->init_array[i]) {
                si->init_array[i]();
            }
        }
    }
}

/* 调用析构函数 */
void linker_call_destructors(soinfo_t* si) {
    /* 1. 调用 DT_FINI_ARRAY (逆序) */
    if (si->fini_array) {
        for (size_t i = si->fini_array_count; i > 0; i--) {
            if (si->fini_array[i-1]) {
                si->fini_array[i-1]();
            }
        }
    }

    /* 2. 调用 DT_FINI */
    if (si->fini_func) {
        si->fini_func();
    }
}
```

---

## 5. 构建系统详解

### Android 构建

```bash
cd android
make native       # 本地编译
make run          # 运行测试
make clean        # 清理

# 交叉编译到 Android
export NDK_PATH=/path/to/ndk
make android ANDROID_ABI=arm64-v8a
```

### Linux 构建

```bash
cd linux
make              # 编译
make run          # 运行测试
make readelf      # 查看测试库的 ELF 信息
make nm           # 查看符号表
make clean        # 清理
```

---

## 6. 实际运行演示

### Android 方式运行结果

```
$ cd android && make run

==========================================
Android rootfs - Dynamic Library Loader
==========================================

Loading 2 libraries...

=== Loading library: ./lib/libdemo.so ===
[demo.so] Library loaded! (constructor called)
Successfully loaded: ./lib/libdemo.so

=== Loading library: ./lib/libdemo2.so ===
[demo2.so] Library loaded! (constructor called)
Successfully loaded: ./lib/libdemo2.so

--- Testing demo.so functions ---
Found function: demo_hello at 0x...
[demo.so] Hello from demo shared library!
Result: 10 + 20 = 30
Version: Demo Library v1.0

--- Testing demo2.so functions ---
[demo2.so] Message: Hello from main program!
Result: 6 * 7 = 42

=== Unloading libraries ===
[demo2.so] Library unloading! (destructor called)
[demo.so] Library unloading! (destructor called)

Program completed successfully!
```

### Linux 自定义 Linker 运行结果

```
$ cd linux && make run

===========================================
  Mini Linker - Android-style ELF Loader
===========================================

--- Analyzing ELF file ---
=== ELF Header ===
Type: Shared Object
Entry: 0x0
Program headers: 9
Section headers: 22

=== Program Headers ===
[ 0] PHDR         offset=0x00000040 vaddr=0x00000040 ...
[ 1] LOAD         offset=0x00000000 vaddr=0x00000000 ... flags=R--
[ 2] LOAD         offset=0x00001000 vaddr=0x00001000 ... flags=R-X
[ 3] LOAD         offset=0x00002000 vaddr=0x00002000 ... flags=RW-
[ 4] DYNAMIC      offset=0x00002e00 vaddr=0x00002e00 ...

--- Loading library ---
[linker] Loading: lib/test_lib.so
[linker] Base address: 0x7f..., load_bias: 0x7f...
[linker] Loaded segment: vaddr=0x0, memsz=0x470, flags=R--
[linker] Loaded segment: vaddr=0x1000, memsz=0x1c5, flags=R-X
[linker] Calling DT_INIT_ARRAY for lib/test_lib.so
[test_lib] Constructor called (count=1)

--- Testing functions ---
add(10, 20) = 30
multiply(6, 7) = 42
get_message() = "Hello from mini linker!"
[test_lib] Hello, Mini Linker!
factorial(5) = 120

--- Unloading library ---
[linker] Calling DT_FINI_ARRAY for lib/test_lib.so
[test_lib] Destructor called

===========================================
  Test completed successfully!
===========================================
```

---

## 总结

本项目展示了两种构建动态链接环境的方式：

| 特性 | Android 方式 | Linux 自定义 Linker |
|------|--------------|---------------------|
| 实现复杂度 | 低（使用系统 API）| 高（完整实现）|
| 代码量 | ~200 行 | ~1000 行 |
| 适用场景 | 快速开发 | 学习原理、特殊需求 |
| 依赖 | 系统 libdl | 无外部依赖 |
| 可定制性 | 低 | 高 |

通过这个项目，你可以深入理解：
- ELF 文件格式
- 动态链接的工作原理
- 符号查找与重定位机制
- 构造/析构函数的调用时机

这些知识对于理解 Android 系统的 linker、进行逆向工程、或者开发类似 LD_PRELOAD 的工具都非常有帮助。
