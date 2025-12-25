# ELF 格式速查表

本文档提供 ELF 文件格式的快速参考，方便开发者查阅。

---

## 1. ELF 文件整体结构

```
+-------------------+
|    ELF Header     |  文件头 (64 字节)
+-------------------+
| Program Headers   |  程序头表 (运行时加载用)
+-------------------+
|                   |
|    Sections       |  各个节 (.text, .data, ...)
|                   |
+-------------------+
| Section Headers   |  节头表 (链接/调试用，可选)
+-------------------+
```

---

## 2. 三大核心结构对比

| 结构 | 用途 | 使用者 | 必需性 |
|------|------|--------|--------|
| **Program Headers** | 运行时加载 | 加载器 (loader) | 运行必需 |
| **Section Headers** | 链接/调试 | 链接器 (ld)、调试器 | 可 strip |
| **Dynamic Segment** | 动态链接信息 | 动态链接器 (ld.so) | 动态库必需 |

---

## 3. Program Header 类型 (p_type)

| 类型 | 值 | 说明 |
|------|-----|------|
| `PT_NULL` | 0 | 未使用 |
| `PT_LOAD` | 1 | 可加载段，需映射到内存 |
| `PT_DYNAMIC` | 2 | 动态链接信息 |
| `PT_INTERP` | 3 | 解释器路径 (如 `/lib64/ld-linux-x86-64.so.2`) |
| `PT_NOTE` | 4 | 附加信息 |
| `PT_PHDR` | 6 | 程序头表自身 |
| `PT_TLS` | 7 | 线程本地存储 |
| `PT_GNU_EH_FRAME` | 0x6474e550 | 异常处理帧 |
| `PT_GNU_STACK` | 0x6474e551 | 栈属性 (可执行性) |
| `PT_GNU_RELRO` | 0x6474e552 | 只读重定位段 |

### 段权限标志 (p_flags)

| 标志 | 值 | 含义 |
|------|-----|------|
| `PF_X` | 0x1 | 可执行 |
| `PF_W` | 0x2 | 可写 |
| `PF_R` | 0x4 | 可读 |

常见组合：`R--` (只读), `R-X` (代码), `RW-` (数据)

---

## 4. 常用 Section 类型

| 节名 | 类型 | 说明 |
|------|------|------|
| `.text` | SHT_PROGBITS | 可执行代码 |
| `.rodata` | SHT_PROGBITS | 只读数据 (字符串常量等) |
| `.data` | SHT_PROGBITS | 已初始化的可读写数据 |
| `.bss` | SHT_NOBITS | 未初始化数据 (不占文件空间) |
| `.symtab` | SHT_SYMTAB | 符号表 (完整，可 strip) |
| `.dynsym` | SHT_DYNSYM | 动态符号表 (运行时需要) |
| `.strtab` | SHT_STRTAB | 字符串表 |
| `.dynstr` | SHT_STRTAB | 动态字符串表 |
| `.rela.dyn` | SHT_RELA | 动态重定位表 |
| `.rela.plt` | SHT_RELA | PLT 重定位表 |
| `.plt` | SHT_PROGBITS | 过程链接表 |
| `.got` | SHT_PROGBITS | 全局偏移表 |
| `.got.plt` | SHT_PROGBITS | PLT 的 GOT 部分 |
| `.dynamic` | SHT_DYNAMIC | 动态链接信息 |
| `.init` | SHT_PROGBITS | 初始化代码 |
| `.fini` | SHT_PROGBITS | 终止代码 |
| `.init_array` | SHT_INIT_ARRAY | 构造函数指针数组 |
| `.fini_array` | SHT_FINI_ARRAY | 析构函数指针数组 |

---

## 5. Dynamic Segment 条目 (d_tag)

### 依赖与路径

| Tag | 说明 |
|-----|------|
| `DT_NEEDED` | 依赖的共享库名 |
| `DT_SONAME` | 共享库的 SO 名称 |
| `DT_RPATH` | 库搜索路径 (已废弃) |
| `DT_RUNPATH` | 库搜索路径 |

### 符号表相关

| Tag | 说明 |
|-----|------|
| `DT_SYMTAB` | 符号表地址 (.dynsym) |
| `DT_STRTAB` | 字符串表地址 (.dynstr) |
| `DT_STRSZ` | 字符串表大小 |
| `DT_SYMENT` | 符号表条目大小 |
| `DT_HASH` | ELF hash 表地址 |
| `DT_GNU_HASH` | GNU hash 表地址 |

### 重定位相关

| Tag | 说明 |
|-----|------|
| `DT_RELA` | RELA 重定位表地址 |
| `DT_RELASZ` | RELA 表大小 |
| `DT_RELAENT` | RELA 条目大小 |
| `DT_JMPREL` | PLT 重定位表地址 |
| `DT_PLTRELSZ` | PLT 重定位表大小 |
| `DT_PLTREL` | PLT 重定位类型 (DT_RELA) |
| `DT_PLTGOT` | PLT/GOT 地址 |

### 初始化/析构

| Tag | 说明 |
|-----|------|
| `DT_INIT` | 初始化函数地址 |
| `DT_FINI` | 析构函数地址 |
| `DT_INIT_ARRAY` | 构造函数数组地址 |
| `DT_INIT_ARRAYSZ` | 构造函数数组大小 |
| `DT_FINI_ARRAY` | 析构函数数组地址 |
| `DT_FINI_ARRAYSZ` | 析构函数数组大小 |

### 其他

| Tag | 说明 |
|-----|------|
| `DT_FLAGS` | 标志位 |
| `DT_FLAGS_1` | 扩展标志位 |
| `DT_DEBUG` | 调试信息 |
| `DT_NULL` | 结束标记 |

---

## 6. 重定位类型 (x86_64)

| 类型 | 值 | 计算公式 | 说明 |
|------|-----|----------|------|
| `R_X86_64_NONE` | 0 | - | 无操作 |
| `R_X86_64_64` | 1 | S + A | 绝对 64 位地址 |
| `R_X86_64_PC32` | 2 | S + A - P | 相对 32 位地址 |
| `R_X86_64_GOT32` | 3 | G + A | GOT 偏移 |
| `R_X86_64_PLT32` | 4 | L + A - P | PLT 相对地址 |
| `R_X86_64_COPY` | 5 | - | 复制符号 |
| `R_X86_64_GLOB_DAT` | 6 | S | GOT 条目 |
| `R_X86_64_JUMP_SLOT` | 7 | S | PLT 条目 |
| `R_X86_64_RELATIVE` | 8 | B + A | 相对于基址 |
| `R_X86_64_GOTPCREL` | 9 | G + GOT + A - P | GOT 相对地址 |

**符号说明**：
- `S` = 符号地址
- `A` = addend (附加值)
- `P` = 重定位位置地址
- `B` = 基址 (load_bias)
- `G` = GOT 中的偏移
- `L` = PLT 条目地址
- `GOT` = GOT 基址

---

## 7. 符号绑定与类型

### 符号绑定 (st_info 高 4 位)

| 绑定 | 值 | 说明 |
|------|-----|------|
| `STB_LOCAL` | 0 | 本地符号，不可见于外部 |
| `STB_GLOBAL` | 1 | 全局符号，可被其他模块引用 |
| `STB_WEAK` | 2 | 弱符号，可被同名全局符号覆盖 |

### 符号类型 (st_info 低 4 位)

| 类型 | 值 | 说明 |
|------|-----|------|
| `STT_NOTYPE` | 0 | 未指定类型 |
| `STT_OBJECT` | 1 | 数据对象 (变量) |
| `STT_FUNC` | 2 | 函数 |
| `STT_SECTION` | 3 | 节 |
| `STT_FILE` | 4 | 源文件名 |
| `STT_COMMON` | 5 | 公共块 |
| `STT_TLS` | 6 | TLS 数据 |

---

## 8. 常用命令速查

```bash
# 查看 ELF 头
readelf -h libdemo.so

# 查看程序头 (加载视图)
readelf -l libdemo.so

# 查看节头 (文件视图)
readelf -S libdemo.so

# 查看动态段
readelf -d libdemo.so

# 查看符号表
readelf -s libdemo.so    # 完整符号表
nm -D libdemo.so         # 动态符号

# 查看重定位表
readelf -r libdemo.so

# 反汇编
objdump -d libdemo.so

# 查看依赖库
ldd libdemo.so

# 十六进制查看
hexdump -C libdemo.so | head -20
xxd libdemo.so | head -20
```

---

## 9. ELF 魔数与标识

```
偏移  内容           说明
0x00  7F 45 4C 46   魔数: 0x7F 'E' 'L' 'F'
0x04  02            类型: 1=32位, 2=64位
0x05  01            字节序: 1=小端, 2=大端
0x06  01            ELF 版本
0x07  00            OS/ABI: 0=SYSV, 3=Linux
0x08  00 00 ...     填充 (8 字节)
0x10  03 00         文件类型: 2=EXEC, 3=DYN
0x12  3E 00         机器类型: 0x3E=x86_64
```

### 常见文件类型 (e_type)

| 类型 | 值 | 说明 |
|------|-----|------|
| `ET_NONE` | 0 | 无类型 |
| `ET_REL` | 1 | 可重定位文件 (.o) |
| `ET_EXEC` | 2 | 可执行文件 |
| `ET_DYN` | 3 | 共享库 (.so) 或 PIE 可执行文件 |
| `ET_CORE` | 4 | Core dump |

### 常见机器类型 (e_machine)

| 类型 | 值 | 说明 |
|------|-----|------|
| `EM_386` | 3 | x86 |
| `EM_ARM` | 40 | ARM |
| `EM_X86_64` | 62 | x86_64 |
| `EM_AARCH64` | 183 | ARM64 |

---

## 10. 动态链接流程

```
1. 内核加载可执行文件
        ↓
2. 读取 PT_INTERP，找到动态链接器 (ld.so)
        ↓
3. 加载动态链接器到内存
        ↓
4. 动态链接器接管，读取 PT_DYNAMIC
        ↓
5. 解析 DT_NEEDED，递归加载依赖库
        ↓
6. 处理所有重定位 (RELA, PLT)
        ↓
7. 调用 DT_INIT / DT_INIT_ARRAY (按依赖顺序)
        ↓
8. 跳转到程序入口点 (e_entry)
        ↓
9. 程序退出时调用 DT_FINI_ARRAY / DT_FINI
```

---

## 参考资料

- [ELF Specification](https://refspecs.linuxfoundation.org/elf/elf.pdf)
- [System V ABI - AMD64](https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf)
- `man elf` - Linux ELF 手册
- `/usr/include/elf.h` - ELF 头文件定义
