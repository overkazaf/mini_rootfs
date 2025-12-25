#ifndef ELF_PARSER_H
#define ELF_PARSER_H

#include <elf.h>
#include <stdint.h>
#include <stddef.h>

// ELF 文件信息结构
typedef struct {
    int fd;                     // 文件描述符
    void* map_start;            // mmap 起始地址
    size_t map_size;            // mmap 大小
    Elf64_Ehdr* ehdr;           // ELF 头
    Elf64_Phdr* phdr;           // 程序头表
    Elf64_Shdr* shdr;           // 节头表
    const char* shstrtab;       // 节字符串表
} elf_file_t;

// 解析 ELF 文件
int elf_open(const char* path, elf_file_t* elf);

// 关闭 ELF 文件
void elf_close(elf_file_t* elf);

// 验证 ELF 头
int elf_validate_header(const Elf64_Ehdr* ehdr);

// 查找程序头
Elf64_Phdr* elf_find_phdr(elf_file_t* elf, uint32_t type);

// 查找节
Elf64_Shdr* elf_find_section(elf_file_t* elf, const char* name);

// 获取节数据
void* elf_get_section_data(elf_file_t* elf, Elf64_Shdr* shdr);

// 打印 ELF 信息（调试用）
void elf_print_info(elf_file_t* elf);

#endif // ELF_PARSER_H
