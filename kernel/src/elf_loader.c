#include "elf_loader.h"
#include "string.h"

// Forward declarations for memory functions (defined in main.c)
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);

// Simple memory allocator for programs (you'll want to improve this)
static uint8_t program_memory[1024 * 1024]; // 1MB for programs
static size_t program_memory_offset = 0;

// Simple malloc-like function for program memory
static void* program_malloc(size_t size) {
    // Align to 8 bytes
    size = (size + 7) & ~7;
    
    if (program_memory_offset + size > sizeof(program_memory)) {
        return NULL;
    }
    
    void *ptr = &program_memory[program_memory_offset];
    program_memory_offset += size;
    return ptr;
}

// Validate ELF header
bool elf_validate_header(const elf64_header_t *header) {
    // Check ELF magic bytes
    if (header->e_ident[0] != ELF_MAGIC_0 ||
        header->e_ident[1] != ELF_MAGIC_1 ||
        header->e_ident[2] != ELF_MAGIC_2 ||
        header->e_ident[3] != ELF_MAGIC_3) {
        return false;
    }
    
    // Check for 64-bit ELF
    if (header->e_ident[4] != ELF_CLASS_64) {
        return false;
    }
    
    // Check for little-endian
    if (header->e_ident[5] != ELF_DATA_LSB) {
        return false;
    }
    
    // Check for executable type
    if (header->e_type != ELF_TYPE_EXEC) {
        return false;
    }
    
    // Check for x86_64 architecture
    if (header->e_machine != ELF_MACHINE_X86_64) {
        return false;
    }
    
    return true;
}

// Load ELF program into memory
program_load_result_t elf_load_program(const uint8_t *elf_data, size_t size) {
    program_load_result_t result = {0};
    
    // Check minimum size for ELF header
    if (size < sizeof(elf64_header_t)) {
        strcpy(result.error_msg, "File too small for ELF header");
        return result;
    }
    
    // Parse ELF header
    const elf64_header_t *header = (const elf64_header_t*)elf_data;
    
    if (!elf_validate_header(header)) {
        strcpy(result.error_msg, "Invalid ELF header");
        return result;
    }
    
    // Check if we have program headers
    if (header->e_phnum == 0) {
        strcpy(result.error_msg, "No program headers found");
        return result;
    }
    
    // Calculate total memory needed for all LOAD segments
    size_t total_memory_needed = 0;
    uint64_t lowest_addr = 0xFFFFFFFFFFFFFFFF;
    uint64_t highest_addr = 0;
    
    for (uint16_t i = 0; i < header->e_phnum; i++) {
        const elf64_phdr_t *phdr = (const elf64_phdr_t*)(elf_data + header->e_phoff + i * header->e_phentsize);
        
        if (phdr->p_type == PT_LOAD) {
            if (phdr->p_vaddr < lowest_addr) {
                lowest_addr = phdr->p_vaddr;
            }
            if (phdr->p_vaddr + phdr->p_memsz > highest_addr) {
                highest_addr = phdr->p_vaddr + phdr->p_memsz;
            }
        }
    }
    
    total_memory_needed = highest_addr - lowest_addr;
    
    // Allocate memory for the program
    void *program_base = program_malloc(total_memory_needed);
    if (!program_base) {
        strcpy(result.error_msg, "Failed to allocate memory for program");
        return result;
    }
    
    // Clear allocated memory
    memset(program_base, 0, total_memory_needed);
    
    // Load all LOAD segments
    for (uint16_t i = 0; i < header->e_phnum; i++) {
        const elf64_phdr_t *phdr = (const elf64_phdr_t*)(elf_data + header->e_phoff + i * header->e_phentsize);
        
        if (phdr->p_type == PT_LOAD) {
            // Calculate destination address relative to our allocated base
            uint8_t *dest = (uint8_t*)program_base + (phdr->p_vaddr - lowest_addr);
            
            // Copy file data
            if (phdr->p_filesz > 0) {
                memcpy(dest, elf_data + phdr->p_offset, phdr->p_filesz);
            }
            
            // Zero remaining memory if needed (BSS section)
            if (phdr->p_memsz > phdr->p_filesz) {
                memset(dest + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
            }
        }
    }
    
    // Success!
    result.success = true;
    result.entry_point = header->e_entry - lowest_addr + (uint64_t)program_base;
    result.allocated_memory = program_base;
    result.memory_size = total_memory_needed;
    strcpy(result.error_msg, "Program loaded successfully");
    
    return result;
}

// Unload program (simple version)
void elf_unload_program(program_load_result_t *result) {
    if (result->allocated_memory) {
        // In a real OS, you'd free the memory here
        // For now, we just mark it as unloaded
        result->allocated_memory = NULL;
        result->memory_size = 0;
        result->success = false;
    }
} 