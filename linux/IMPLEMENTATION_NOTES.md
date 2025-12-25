# Mini Linker 实现关键步骤

## 概述

本项目实现了一个简化版的 Android bionic linker，能够：
1. 解析 ELF 文件格式
2. 将共享库加载到内存
3. 执行符号解析和重定位
4. 提供 dlopen/dlsym/dlclose API

## 核心流程

```
mini_dlopen()
    │
    ├── elf_open()           # 1. 打开并映射 ELF 文件
    │   ├── 验证 ELF 魔数
    │   ├── 检查 64 位 x86_64
    │   └── 解析 ELF 头/程序头/节头
    │
    ├── linker_load()        # 2. 加载到内存
    │   ├── 计算加载大小 (遍历 PT_LOAD)
    │   ├── mmap 预留内存空间
    │   ├── 计算 load_bias
    │   ├── mmap 各 PT_LOAD 段
    │   └── 处理 BSS 段
    │
    ├── parse_dynamic()      # 3. 解析动态段
    │   ├── DT_SYMTAB → 符号表
    │   ├── DT_STRTAB → 字符串表
    │   ├── DT_HASH   → 符号哈希表
    │   ├── DT_RELA   → 重定位表
    │   └── DT_INIT   → 初始化函数
    │
    ├── linker_relocate()    # 4. 执行重定位
    │   ├── R_X86_64_RELATIVE  → B + A
    │   ├── R_X86_64_GLOB_DAT  → S
    │   ├── R_X86_64_JUMP_SLOT → S
    │   └── R_X86_64_64        → S + A
    │
    └── call_constructors()  # 5. 调用初始化函数
        ├── DT_INIT
        └── DT_INIT_ARRAY
```

## 关键数据结构

### 1. soinfo_t - 共享库信息 (linker.h:12)

```c
typedef struct soinfo {
    char name[256];         // 库名
    void* base;             // 加载基地址
    size_t size;            // 映射大小
    void* load_bias;        // 加载偏移 = base - 期望地址

    Elf64_Phdr* phdr;       // 程序头表
    Elf64_Dyn* dynamic;     // 动态段
    Elf64_Sym* symtab;      // 符号表
    const char* strtab;     // 字符串表
    uint32_t* hash;         // ELF 哈希表

    Elf64_Rela* rela;       // RELA 重定位表
    Elf64_Rela* plt_rela;   // PLT 重定位表

    void (*init_func)(void);    // 构造函数
    void (**init_array)(void);  // 构造函数数组
} soinfo_t;
```

### 2. ELF 结构关系

```
ELF 文件
├── Elf64_Ehdr (ELF 头)
│   ├── e_phoff → 程序头表偏移
│   ├── e_shoff → 节头表偏移
│   └── e_entry → 入口点
│
├── Elf64_Phdr[] (程序头表)
│   ├── PT_LOAD    → 可加载段
│   ├── PT_DYNAMIC → 动态链接信息
│   └── PT_GNU_RELRO → 只读重定位
│
└── Elf64_Dyn[] (动态段)
    ├── DT_SYMTAB  → 符号表地址
    ├── DT_STRTAB  → 字符串表地址
    ├── DT_RELA    → 重定位表地址
    └── DT_HASH    → 符号哈希表
```

## 关键算法

### 1. 计算加载大小 (linker.c:55)

```c
// 遍历所有 PT_LOAD 段，找到最小和最大虚拟地址
for (PT_LOAD segments) {
    min_vaddr = min(min_vaddr, p_vaddr)
    max_vaddr = max(max_vaddr, p_vaddr + p_memsz)
}
// 页对齐
load_size = PAGE_END(max_vaddr) - PAGE_START(min_vaddr)
```

### 2. Load Bias 计算 (linker.c:285)

```c
// load_bias = 实际加载地址 - ELF 中指定的虚拟地址
si->load_bias = si->base - min_vaddr;

// 使用 load_bias 转换 ELF 地址到实际地址
actual_addr = load_bias + elf_vaddr;
```

### 3. 重定位处理 (linker.c:186)

```c
// 重定位地址
reloc_addr = load_bias + rela->r_offset;

switch (type) {
    case R_X86_64_RELATIVE:
        // 相对地址: B + A
        *reloc_addr = load_bias + rela->r_addend;
        break;

    case R_X86_64_GLOB_DAT:
    case R_X86_64_JUMP_SLOT:
        // GOT/PLT: 符号地址
        *reloc_addr = symbol_address;
        break;

    case R_X86_64_64:
        // 绝对地址: S + A
        *reloc_addr = symbol_address + rela->r_addend;
        break;
}
```

### 4. 符号查找 (linker.c:129)

```c
// 使用 ELF hash 加速查找
hash = elf_hash(symbol_name);
bucket_idx = hash % nbucket;

for (i = bucket[bucket_idx]; i != 0; i = chain[i]) {
    sym = &symtab[i];
    if (strcmp(strtab + sym->st_name, symbol_name) == 0) {
        return load_bias + sym->st_value;
    }
}
```

## 内存布局

```
高地址
┌─────────────────────┐
│     BSS 段          │ ← 零初始化数据 (mmap ANONYMOUS)
├─────────────────────┤
│     .data 段        │ ← 可读写数据
├─────────────────────┤
│     .rodata 段      │ ← 只读数据
├─────────────────────┤
│     .text 段        │ ← 可执行代码
├─────────────────────┤
│     ELF 头/程序头   │
└─────────────────────┘ ← base (load_bias + min_vaddr)
低地址
```

## 与 Android Bionic Linker 的对比

| 功能 | Android Linker | Mini Linker |
|------|----------------|-------------|
| ELF 解析 | ✓ | ✓ |
| mmap 加载 | ✓ | ✓ |
| 重定位 | 完整 | 基本类型 |
| 符号查找 | GNU hash + ELF hash | ELF hash |
| dlopen/dlsym | ✓ | ✓ |
| 命名空间 | ✓ | ✗ |
| TLS | ✓ | ✗ |
| IFUNC | ✓ | ✗ |
| 版本符号 | ✓ | ✗ |
| 依赖加载 | ✓ | ✗ |

## 使用方法

```bash
# 在 Linux 环境中
cd mini_rootfs
make          # 编译
make run      # 运行测试
```

## 扩展方向

1. **依赖加载**: 解析 DT_NEEDED，递归加载依赖库
2. **GNU Hash**: 实现更快的 GNU hash 查找
3. **TLS 支持**: 实现线程本地存储
4. **命名空间**: 实现 Android 风格的 linker namespace
5. **RELR 重定位**: 支持压缩的相对重定位格式
