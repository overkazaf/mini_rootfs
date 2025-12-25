# 常见问题 (FAQ)

本文档收集动态链接器开发过程中的常见问题和解答。

---

## Q1: 为什么加载器要扫描所有 PHDR 来找最小虚拟地址？编译器不能按顺序排列吗？

### 简短回答

现代链接器（GCC/ld）**确实会按顺序排列**，但加载器仍需扫描，主要出于健壮性考虑。

### 现实情况：大多数 ELF 确实是有序的

```bash
# 查看一个典型的 .so 文件
readelf -l /lib/x86_64-linux-gnu/libc.so.6 | grep LOAD
```

通常输出：
```
LOAD  0x000000  0x0000000000000000  ...  R--
LOAD  0x020000  0x0000000000020000  ...  R-X
LOAD  0x180000  0x0000000000180000  ...  RW-
```

第一个 PT_LOAD 就是最小 vaddr。

### 为什么加载器还要扫描？

| 原因 | 说明 |
|------|------|
| **ELF 规范不强制** | 只说"描述可加载段"，没规定顺序 |
| **防御性编程** | 不能信任输入，恶意/损坏文件可能乱序 |
| **链接器脚本** | 用户自定义脚本可能打乱顺序 |
| **特殊工具链** | 嵌入式/老旧工具链可能不保证顺序 |
| **扫描开销极小** | 通常只有 3-5 个 PHDR，O(n) 可忽略 |

### 为什么 ELF 规范不强制顺序？

ELF 设计于 1980s 末，追求**最大灵活性**：

```
设计哲学："机制与策略分离" - 规范定义结构，不限制使用方式
```

- 不同架构可能有不同需求
- 允许链接器优化自由度
- 避免过度约束导致兼容性问题

### 实际优化

现代加载器（glibc ld.so）有优化：

```c
// glibc 实际做法：假设第一个 PT_LOAD 是最小的，但仍然验证
for (i = 0; i < phnum; i++) {
    if (phdr[i].p_type == PT_LOAD) {
        if (first_load == NULL) {
            first_load = &phdr[i];  // 快速路径
        }
        // 仍然跟踪 min/max 用于验证
    }
}
```

### 总结

- **Q: 为什么不强制顺序？**
  - A: 规范追求灵活性，且扫描成本可忽略

- **Q: 编译器没考虑到？**
  - A: 考虑到了，现代工具链会排序，但加载器为了健壮性仍需验证

本质上是**信任边界**问题：编译器可以保证顺序，但加载器不能假设所有输入都来自"正常"编译器。

---

## Q2: load_bias 是什么？为什么需要它？

### 简短回答

`load_bias` 是实际加载地址与 ELF 文件中期望地址的差值。

### 详细解释

```
load_bias = 实际基址 - 期望基址 (min_vaddr)
```

**为什么需要？**

1. **地址无关代码 (PIC)**：共享库编译时不知道会被加载到哪个地址
2. **ASLR**：系统随机化加载地址，每次运行地址不同
3. **地址冲突**：多个库可能期望相同地址，需要重定位

**使用场景**：

```c
// ELF 中记录的地址
Elf64_Addr file_addr = sym->st_value;  // 例如: 0x1000

// 实际运行时地址
void* runtime_addr = (void*)((char*)load_bias + file_addr);
```

---

## Q3: 为什么有 .symtab 和 .dynsym 两个符号表？

### 简短回答

- `.dynsym`：运行时需要，包含导出/导入符号，不可 strip
- `.symtab`：调试用，包含所有符号（含本地），可 strip

### 对比

| 特性 | .symtab | .dynsym |
|------|---------|---------|
| 用途 | 调试、分析 | 动态链接 |
| 包含符号 | 所有（含 static） | 仅导出/导入 |
| 可否 strip | 可以 | 不可以 |
| 运行时需要 | 否 | 是 |

### 示例

```bash
# 查看动态符号（运行时需要）
nm -D libdemo.so

# 查看所有符号（含调试）
nm libdemo.so

# strip 后 .symtab 消失，.dynsym 保留
strip libdemo.so
nm libdemo.so        # 无输出
nm -D libdemo.so     # 仍有输出
```

---

## Q4: RELA 和 REL 重定位的区别？

### 简短回答

- `REL`：addend 存储在被重定位的位置
- `RELA`：addend 显式存储在重定位条目中

### 结构对比

```c
// REL (无显式 addend)
typedef struct {
    Elf64_Addr r_offset;
    Elf64_Xword r_info;
} Elf64_Rel;

// RELA (有显式 addend)
typedef struct {
    Elf64_Addr r_offset;
    Elf64_Xword r_info;
    Elf64_Sxword r_addend;  // 显式存储
} Elf64_Rela;
```

### 平台使用

| 架构 | 使用类型 |
|------|----------|
| x86 (32-bit) | REL |
| x86_64 | RELA |
| ARM32 | REL |
| AArch64 | RELA |

**x86_64 使用 RELA 的原因**：指令长度可变，从指令中提取 addend 很复杂。

---

## Q5: PLT 和 GOT 是什么关系？

### 简短回答

- **GOT (Global Offset Table)**：存储全局变量和函数的实际地址
- **PLT (Procedure Linkage Table)**：函数调用的跳板，实现延迟绑定

### 调用流程

```
首次调用 printf():

代码 -> PLT[printf] -> GOT[printf] -> 解析器 -> 找到真实地址 -> 更新 GOT
                           ↓
                      (首次指向解析器)

后续调用 printf():

代码 -> PLT[printf] -> GOT[printf] -> printf 真实地址
                           ↓
                      (已更新为真实地址)
```

### 为什么需要 PLT？

1. **延迟绑定**：程序启动更快，只解析实际调用的函数
2. **位置无关**：代码段可以是只读的，地址修改在 GOT（数据段）

---

## Q6: 为什么 constructor 在 dlopen 时调用，而不是在 main 之前？

### 简短回答

对于动态加载的库（dlopen），constructor 在加载时调用；对于程序启动时加载的库，constructor 在 main 之前调用。

### 时序

```
程序启动:
1. 内核加载可执行文件
2. ld.so 加载依赖库
3. 调用所有库的 constructors (按依赖顺序)
4. 调用 main()

dlopen 加载:
1. main() 运行中
2. 调用 dlopen("libfoo.so")
3. 加载库，执行重定位
4. 调用该库的 constructors  <-- 此时调用
5. dlopen 返回
```

### 顺序保证

- **依赖库先初始化**：如果 A 依赖 B，B 的 constructor 先调用
- **析构顺序相反**：B 的 destructor 后调用

---

## Q7: 如何调试动态链接问题？

### 常用方法

```bash
# 1. 查看库搜索路径
LD_DEBUG=libs ./program

# 2. 查看符号解析
LD_DEBUG=symbols ./program

# 3. 查看所有调试信息
LD_DEBUG=all ./program

# 4. 查看依赖
ldd ./program
ldd ./libdemo.so

# 5. 查看 RPATH/RUNPATH
readelf -d ./program | grep -E 'RPATH|RUNPATH'

# 6. 查看未解析符号
nm -u ./libdemo.so
```

### 常见问题

| 错误 | 原因 | 解决 |
|------|------|------|
| `cannot open shared object` | 找不到库 | 设置 LD_LIBRARY_PATH 或 RPATH |
| `undefined symbol` | 符号未导出或库顺序错 | 检查链接顺序，nm -D 查看导出 |
| `version not found` | 符号版本不匹配 | 更新库或重新编译 |
| `SIGFPE/SIGSEGV in init` | constructor 崩溃 | GDB 调试 constructor |

---

*持续更新中...*
