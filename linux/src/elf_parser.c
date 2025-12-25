#include "elf_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

// 验证 ELF 头
int elf_validate_header(const Elf64_Ehdr* ehdr) {
    // 检查 ELF 魔数
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        fprintf(stderr, "Error: Not an ELF file\n");
        return -1;
    }

    // 检查 64 位
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "Error: Not a 64-bit ELF\n");
        return -1;
    }

    // 检查小端序
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        fprintf(stderr, "Error: Not little-endian\n");
        return -1;
    }

    // 检查是共享库或可执行文件
    if (ehdr->e_type != ET_DYN && ehdr->e_type != ET_EXEC) {
        fprintf(stderr, "Error: Not a shared library or executable\n");
        return -1;
    }

    // 检查是 x86_64 架构
    if (ehdr->e_machine != EM_X86_64) {
        fprintf(stderr, "Error: Not x86_64 architecture\n");
        return -1;
    }

    return 0;
}

// 打开并映射 ELF 文件
int elf_open(const char* path, elf_file_t* elf) {
    struct stat st;

    memset(elf, 0, sizeof(elf_file_t));

    // 打开文件
    elf->fd = open(path, O_RDONLY);
    if (elf->fd < 0) {
        perror("open");
        return -1;
    }

    // 获取文件大小
    if (fstat(elf->fd, &st) < 0) {
        perror("fstat");
        close(elf->fd);
        return -1;
    }
    elf->map_size = st.st_size;

    // 映射文件
    elf->map_start = mmap(NULL, elf->map_size, PROT_READ, MAP_PRIVATE, elf->fd, 0);
    if (elf->map_start == MAP_FAILED) {
        perror("mmap");
        close(elf->fd);
        return -1;
    }

    // 解析 ELF 头
    elf->ehdr = (Elf64_Ehdr*)elf->map_start;
    if (elf_validate_header(elf->ehdr) < 0) {
        munmap(elf->map_start, elf->map_size);
        close(elf->fd);
        return -1;
    }

    // 解析程序头表
    if (elf->ehdr->e_phoff != 0) {
        elf->phdr = (Elf64_Phdr*)((uint8_t*)elf->map_start + elf->ehdr->e_phoff);
    }

    // 解析节头表
    if (elf->ehdr->e_shoff != 0) {
        elf->shdr = (Elf64_Shdr*)((uint8_t*)elf->map_start + elf->ehdr->e_shoff);

        // 获取节字符串表
        if (elf->ehdr->e_shstrndx != SHN_UNDEF) {
            Elf64_Shdr* shstrtab_hdr = &elf->shdr[elf->ehdr->e_shstrndx];
            elf->shstrtab = (const char*)((uint8_t*)elf->map_start + shstrtab_hdr->sh_offset);
        }
    }

    return 0;
}

// 关闭 ELF 文件
void elf_close(elf_file_t* elf) {
    if (elf->map_start && elf->map_start != MAP_FAILED) {
        munmap(elf->map_start, elf->map_size);
    }
    if (elf->fd >= 0) {
        close(elf->fd);
    }
    memset(elf, 0, sizeof(elf_file_t));
    elf->fd = -1;
}

// 查找程序头
Elf64_Phdr* elf_find_phdr(elf_file_t* elf, uint32_t type) {
    if (!elf->phdr) return NULL;

    for (size_t i = 0; i < elf->ehdr->e_phnum; i++) {
        if (elf->phdr[i].p_type == type) {
            return &elf->phdr[i];
        }
    }
    return NULL;
}

// 查找节
Elf64_Shdr* elf_find_section(elf_file_t* elf, const char* name) {
    if (!elf->shdr || !elf->shstrtab) return NULL;

    for (size_t i = 0; i < elf->ehdr->e_shnum; i++) {
        const char* sec_name = elf->shstrtab + elf->shdr[i].sh_name;
        if (strcmp(sec_name, name) == 0) {
            return &elf->shdr[i];
        }
    }
    return NULL;
}

// 获取节数据
void* elf_get_section_data(elf_file_t* elf, Elf64_Shdr* shdr) {
    if (!shdr) return NULL;
    return (uint8_t*)elf->map_start + shdr->sh_offset;
}

// 打印 ELF 信息
void elf_print_info(elf_file_t* elf) {
    printf("=== ELF Header ===\n");
    printf("Type: %s\n", elf->ehdr->e_type == ET_DYN ? "Shared Object" : "Executable");
    printf("Machine: x86_64\n");
    printf("Entry: 0x%lx\n", (unsigned long)elf->ehdr->e_entry);
    printf("Program headers: %d\n", elf->ehdr->e_phnum);
    printf("Section headers: %d\n", elf->ehdr->e_shnum);

    printf("\n=== Program Headers ===\n");
    if (elf->phdr) {
        for (size_t i = 0; i < elf->ehdr->e_phnum; i++) {
            Elf64_Phdr* ph = &elf->phdr[i];
            const char* type_name;
            switch (ph->p_type) {
                case PT_NULL:    type_name = "NULL"; break;
                case PT_LOAD:    type_name = "LOAD"; break;
                case PT_DYNAMIC: type_name = "DYNAMIC"; break;
                case PT_INTERP:  type_name = "INTERP"; break;
                case PT_NOTE:    type_name = "NOTE"; break;
                case PT_PHDR:    type_name = "PHDR"; break;
                case PT_GNU_EH_FRAME: type_name = "GNU_EH_FRAME"; break;
                case PT_GNU_STACK:    type_name = "GNU_STACK"; break;
                case PT_GNU_RELRO:    type_name = "GNU_RELRO"; break;
                default:         type_name = "OTHER"; break;
            }
            printf("[%2zu] %-12s offset=0x%08lx vaddr=0x%08lx filesz=0x%06lx memsz=0x%06lx flags=%c%c%c\n",
                   i, type_name,
                   (unsigned long)ph->p_offset,
                   (unsigned long)ph->p_vaddr,
                   (unsigned long)ph->p_filesz,
                   (unsigned long)ph->p_memsz,
                   (ph->p_flags & PF_R) ? 'R' : '-',
                   (ph->p_flags & PF_W) ? 'W' : '-',
                   (ph->p_flags & PF_X) ? 'X' : '-');
        }
    }

    printf("\n=== Sections ===\n");
    if (elf->shdr && elf->shstrtab) {
        for (size_t i = 0; i < elf->ehdr->e_shnum; i++) {
            Elf64_Shdr* sh = &elf->shdr[i];
            const char* name = elf->shstrtab + sh->sh_name;
            printf("[%2zu] %-20s addr=0x%08lx size=0x%06lx\n",
                   i, name,
                   (unsigned long)sh->sh_addr,
                   (unsigned long)sh->sh_size);
        }
    }
}
