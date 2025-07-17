#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ELF64 Header structure (simplified)
typedef struct {
    uint8_t  e_ident[16];     // ELF identification
    uint16_t e_type;          // Object file type
    uint16_t e_machine;       // Machine type
    uint32_t e_version;       // Object file version
    uint64_t e_entry;         // Entry point address
    uint64_t e_phoff;         // Program header offset
    uint64_t e_shoff;         // Section header offset
    uint32_t e_flags;         // Processor-specific flags
    uint16_t e_ehsize;        // ELF header size
    uint16_t e_phentsize;     // Program header entry size
    uint16_t e_phnum;         // Number of program header entries
    uint16_t e_shentsize;     // Section header entry size
    uint16_t e_shnum;         // Number of section header entries
    uint16_t e_shstrndx;      // Section header string table index
} __attribute__((packed)) elf64_header_t;

// ELF64 Program Header
typedef struct {
    uint32_t p_type;          // Segment type
    uint32_t p_flags;         // Segment flags
    uint64_t p_offset;        // Segment file offset
    uint64_t p_vaddr;         // Segment virtual address
    uint64_t p_paddr;         // Segment physical address
    uint64_t p_filesz;        // Segment size in file
    uint64_t p_memsz;         // Segment size in memory
    uint64_t p_align;         // Segment alignment
} __attribute__((packed)) elf64_phdr_t;

// ELF Constants
#define ELF_MAGIC_0     0x7F
#define ELF_MAGIC_1     'E'
#define ELF_MAGIC_2     'L'
#define ELF_MAGIC_3     'F'
#define ELF_CLASS_64    2
#define ELF_DATA_LSB    1
#define ELF_TYPE_EXEC   2
#define ELF_MACHINE_X86_64  0x3E

// Program header types
#define PT_NULL         0
#define PT_LOAD         1
#define PT_DYNAMIC      2
#define PT_INTERP       3

// Program execution result
typedef struct {
    bool success;
    uint64_t entry_point;
    void *allocated_memory;
    size_t memory_size;
    char error_msg[128];
} program_load_result_t;

// Function declarations
bool elf_validate_header(const elf64_header_t *header);
program_load_result_t elf_load_program(const uint8_t *elf_data, size_t size);
void elf_unload_program(program_load_result_t *result);

#endif 