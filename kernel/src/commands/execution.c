#include "execution.h"
#include "../shell.h"
#include "../process.h"
#include "../elf_loader.h"
#include "../terminal.h"
#include "../string.h"
#include "../fs/filesystem.h"

// Forward declaration for memcpy (defined in main.c)
void *memcpy(void *restrict dest, const void *restrict src, size_t n);

// Simple test program that just returns a value
// This is safe and doesn't use system calls that could crash the OS

// Create a simple ELF wrapper for our test program
static void create_test_elf(uint8_t *buffer, size_t *size) {
    // Create a much simpler program that doesn't use system calls
    // This program just returns a value without making any system calls
    
    static const uint8_t simple_program[] = {
        // Simple program that just returns 42
        0x48, 0xc7, 0xc0, 0x2a, 0x00, 0x00, 0x00,  // mov rax, 42
        0xc3,                                       // ret
    };
    
    // This is a simplified ELF creation - in practice you'd use proper tools
    elf64_header_t *header = (elf64_header_t*)buffer;
    
    // ELF header
    header->e_ident[0] = 0x7F;
    header->e_ident[1] = 'E';
    header->e_ident[2] = 'L';
    header->e_ident[3] = 'F';
    header->e_ident[4] = 2;  // 64-bit
    header->e_ident[5] = 1;  // Little endian
    header->e_ident[6] = 1;  // Current version
    header->e_ident[7] = 0;  // System V ABI
    
    for (int i = 8; i < 16; i++) {
        header->e_ident[i] = 0;
    }
    
    header->e_type = 2;      // Executable
    header->e_machine = 0x3E; // x86_64
    header->e_version = 1;
    header->e_entry = 0x400000; // Entry point
    header->e_phoff = sizeof(elf64_header_t); // Program header offset
    header->e_shoff = 0;
    header->e_flags = 0;
    header->e_ehsize = sizeof(elf64_header_t);
    header->e_phentsize = sizeof(elf64_phdr_t);
    header->e_phnum = 1;     // One program header
    header->e_shentsize = 0;
    header->e_shnum = 0;
    header->e_shstrndx = 0;
    
    // Program header
    elf64_phdr_t *phdr = (elf64_phdr_t*)(buffer + sizeof(elf64_header_t));
    phdr->p_type = 1;        // PT_LOAD
    phdr->p_flags = 5;       // Read + Execute
    phdr->p_offset = sizeof(elf64_header_t) + sizeof(elf64_phdr_t);
    phdr->p_vaddr = 0x400000;
    phdr->p_paddr = 0x400000;
    phdr->p_filesz = sizeof(simple_program);
    phdr->p_memsz = sizeof(simple_program);
    phdr->p_align = 0x1000;
    
    // Copy program data
    memcpy(buffer + phdr->p_offset, simple_program, sizeof(simple_program));
    
    *size = phdr->p_offset + sizeof(simple_program);
}

// Create a more interesting test program that does some computation
static void create_hello_elf(uint8_t *buffer, size_t *size) {
    // This program does some computation and returns a result
    // It demonstrates loops and arithmetic without system calls
    
    static const uint8_t hello_program[] = {
        // Count from 1 to 10 and return the sum (55)
        0x48, 0x31, 0xc0,                           // xor rax, rax (clear rax)
        0x48, 0x31, 0xc9,                           // xor rcx, rcx (clear rcx)
        0x48, 0xc7, 0xc1, 0x01, 0x00, 0x00, 0x00,  // mov rcx, 1
        
        // Loop: add rcx to rax, increment rcx, compare with 11
        0x48, 0x01, 0xc8,                           // add rax, rcx (loop_start)
        0x48, 0xff, 0xc1,                           // inc rcx
        0x48, 0x83, 0xf9, 0x0b,                     // cmp rcx, 11
        0x75, 0xf6,                                 // jne loop_start (jump back 10 bytes)
        
        0xc3,                                       // ret (return sum in rax)
    };
    
    // Create ELF structure
    elf64_header_t *header = (elf64_header_t*)buffer;
    
    // ELF header
    header->e_ident[0] = 0x7F;
    header->e_ident[1] = 'E';
    header->e_ident[2] = 'L';
    header->e_ident[3] = 'F';
    header->e_ident[4] = 2;  // 64-bit
    header->e_ident[5] = 1;  // Little endian
    header->e_ident[6] = 1;  // Current version
    header->e_ident[7] = 0;  // System V ABI
    
    for (int i = 8; i < 16; i++) {
        header->e_ident[i] = 0;
    }
    
    header->e_type = 2;      // Executable
    header->e_machine = 0x3E; // x86_64
    header->e_version = 1;
    header->e_entry = 0x400000; // Entry point
    header->e_phoff = sizeof(elf64_header_t); // Program header offset
    header->e_shoff = 0;
    header->e_flags = 0;
    header->e_ehsize = sizeof(elf64_header_t);
    header->e_phentsize = sizeof(elf64_phdr_t);
    header->e_phnum = 1;     // One program header
    header->e_shentsize = 0;
    header->e_shnum = 0;
    header->e_shstrndx = 0;
    
    // Program header
    elf64_phdr_t *phdr = (elf64_phdr_t*)(buffer + sizeof(elf64_header_t));
    phdr->p_type = 1;        // PT_LOAD
    phdr->p_flags = 5;       // Read + Execute
    phdr->p_offset = sizeof(elf64_header_t) + sizeof(elf64_phdr_t);
    phdr->p_vaddr = 0x400000;
    phdr->p_paddr = 0x400000;
    phdr->p_filesz = sizeof(hello_program);
    phdr->p_memsz = sizeof(hello_program);
    phdr->p_align = 0x1000;
    
    // Copy program data
    memcpy(buffer + phdr->p_offset, hello_program, sizeof(hello_program));
    
    *size = phdr->p_offset + sizeof(hello_program);
}

// Execute command - load and run a program from filesystem
void cmd_exec(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: exec <filename>\n");
        terminal_print("Example: exec hello.elf\n");
        terminal_print("Note: Programs are executed in safe simulation mode\n");
        return;
    }
    
    terminal_print("DEBUG: Starting execution of: ");
    terminal_print(args);
    terminal_print("\n");
    
    // Try to read file from filesystem
    uint8_t file_buffer[4096];
    size_t file_size;
    
    terminal_print("DEBUG: Reading file from filesystem...\n");
    if (!fs_read_file(args, file_buffer, &file_size)) {
        terminal_print("Error: Cannot read file '");
        terminal_print(args);
        terminal_print("'\n");
        terminal_print("Use 'ls' to see available files\n");
        return;
    }
    
    terminal_print("DEBUG: File read successfully, size: ");
    // Print file size
    char size_str[16];
    int i = 0;
    size_t size = file_size;
    if (size == 0) {
        size_str[i++] = '0';
    } else {
        while (size > 0) {
            size_str[i++] = '0' + (size % 10);
            size /= 10;
        }
    }
    size_str[i] = '\0';
    // Print in reverse
    for (int j = i - 1; j >= 0; j--) {
        terminal_putchar(size_str[j]);
    }
    terminal_print(" bytes\n");
    
    // Create process
    terminal_print("DEBUG: Creating process...\n");
    int pid = process_create(args, file_buffer, file_size);
    if (pid < 0) {
        terminal_print("Error: Failed to create process\n");
        return;
    }
    
    terminal_print("DEBUG: Process created with PID: ");
    // Print PID
    char pid_str[16];
    i = 0;
    int p = pid;
    while (p > 0) {
        pid_str[i++] = '0' + (p % 10);
        p /= 10;
    }
    if (i == 0) pid_str[i++] = '0';
    pid_str[i] = '\0';
    // Print in reverse
    for (int j = i - 1; j >= 0; j--) {
        terminal_putchar(pid_str[j]);
    }
    terminal_print("\n");
    
    // Execute process
    terminal_print("DEBUG: Starting process execution...\n");
    if (!process_execute(pid)) {
        terminal_print("Error: Failed to execute process\n");
        return;
    }
    
    terminal_print("DEBUG: Process execution completed\n");
    
    // Clean up
    terminal_print("DEBUG: Cleaning up terminated processes...\n");
    process_cleanup_terminated();
    terminal_print("DEBUG: Cleanup completed\n");
}

// Load command - just load without executing
void cmd_load(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: load <filename>\n");
        return;
    }
    
    uint8_t file_buffer[4096];
    size_t file_size;
    
    if (!fs_read_file(args, file_buffer, &file_size)) {
        terminal_print("Error: Cannot read file '");
        terminal_print(args);
        terminal_print("'\n");
        return;
    }
    
    int pid = process_create(args, file_buffer, file_size);
    if (pid < 0) {
        terminal_print("Error: Failed to load program\n");
        return;
    }
    
    terminal_print("Program loaded successfully with PID: ");
    // Simple number printing
    char num_buf[16];
    int i = 0;
    int num = pid;
    while (num > 0) {
        num_buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    if (i == 0) num_buf[i++] = '0';
    
    // Print in reverse
    for (int j = i - 1; j >= 0; j--) {
        terminal_putchar(num_buf[j]);
    }
    terminal_print("\n");
}

// PS command - list processes
void cmd_ps(const char *args) {
    (void)args; // Unused
    
    terminal_print("PID  Name                 State\n");
    terminal_print("---  -------------------  ---------\n");
    
    for (int i = 1; i < 100; i++) { // Check reasonable PID range
        process_t *proc = process_get(i);
        if (proc && proc->pid != 0) {
            // Print PID
            char num_buf[16];
            int j = 0;
            int num = proc->pid;
            while (num > 0) {
                num_buf[j++] = '0' + (num % 10);
                num /= 10;
            }
            if (j == 0) num_buf[j++] = '0';
            
            // Pad to 3 characters
            for (int k = j; k < 3; k++) {
                terminal_putchar(' ');
            }
            // Print in reverse
            for (int k = j - 1; k >= 0; k--) {
                terminal_putchar(num_buf[k]);
            }
            
            terminal_print("  ");
            
            // Print name (padded to 20 chars)
            int name_len = strlen(proc->name);
            terminal_print(proc->name);
            for (int k = name_len; k < 20; k++) {
                terminal_putchar(' ');
            }
            
            terminal_print(" ");
            
            // Print state
            switch (proc->state) {
                case PROCESS_STATE_READY:
                    terminal_print("READY");
                    break;
                case PROCESS_STATE_RUNNING:
                    terminal_print("RUNNING");
                    break;
                case PROCESS_STATE_TERMINATED:
                    terminal_print("TERMINATED");
                    break;
                case PROCESS_STATE_ERROR:
                    terminal_print("ERROR");
                    break;
                default:
                    terminal_print("UNKNOWN");
                    break;
            }
            
            terminal_print("\n");
        }
    }
}

// Kill command - terminate a process
void cmd_kill(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: kill <pid>\n");
        return;
    }
    
    // Simple string to integer conversion
    int pid = 0;
    for (int i = 0; args[i] != '\0'; i++) {
        if (args[i] >= '0' && args[i] <= '9') {
            pid = pid * 10 + (args[i] - '0');
        } else {
            terminal_print("Error: Invalid PID\n");
            return;
        }
    }
    
    process_t *proc = process_get(pid);
    if (!proc || proc->pid == 0) {
        terminal_print("Error: Process not found\n");
        return;
    }
    
    process_terminate(pid, -1);
    terminal_print("Process terminated\n");
}

// Compile command - create a simple test program
void cmd_compile(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: compile <output_filename>\n");
        terminal_print("Creates a simple test ELF program\n");
        terminal_print("Execution is simulated (safe mode) - no actual code execution\n");
        return;
    }
    
    // Create a test ELF file
    uint8_t elf_buffer[1024];
    size_t elf_size;
    
    create_test_elf(elf_buffer, &elf_size);
    
    // Write to filesystem
    if (!fs_write_file(args, elf_buffer, elf_size)) {
        terminal_print("Error: Failed to write ELF file\n");
        return;
    }
    
    terminal_print("Test program compiled and saved as: ");
    terminal_print(args);
    terminal_print("\n");
    terminal_print("Note: Execution will be simulated for safety\n");
    terminal_print("Run with: exec ");
    terminal_print(args);
    terminal_print("\n");
}

// Hello command - create a more interesting program
void cmd_hello(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: hello <output_filename>\n");
        terminal_print("Creates a Hello World program\n");
        terminal_print("Execution is simulated (safe mode) - no actual code execution\n");
        return;
    }
    
    // Create a Hello World ELF file
    uint8_t elf_buffer[1024];
    size_t elf_size;
    
    create_hello_elf(elf_buffer, &elf_size);
    
    // Write to filesystem
    if (!fs_write_file(args, elf_buffer, elf_size)) {
        terminal_print("Error: Failed to write ELF file\n");
        return;
    }
    
    terminal_print("Hello World program compiled and saved as: ");
    terminal_print(args);
    terminal_print("\n");
    terminal_print("Note: Execution will be simulated for safety\n");
    terminal_print("Run with: exec ");
    terminal_print(args);
    terminal_print("\n");
}

// Register execution commands
void register_execution_commands(void) {
    register_command("exec", cmd_exec, "Execute a program (simulation mode)", "exec <filename>", "Execution");
    register_command("load", cmd_load, "Load a program without executing", "load <filename>", "Execution");
    register_command("ps", cmd_ps, "List all processes", "ps", "Execution");
    register_command("kill", cmd_kill, "Terminate a process", "kill <pid>", "Execution");
    register_command("compile", cmd_compile, "Create a simple test program", "compile <filename>", "Development");
    register_command("hello", cmd_hello, "Create a Hello World program", "hello <filename>", "Development");
} 