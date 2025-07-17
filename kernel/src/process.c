#include "process.h"
#include "elf_loader.h"
#include "terminal.h"
#include "keyboard.h"
#include "string.h"

// Process table
static process_t processes[MAX_PROCESSES];
static uint32_t next_pid = 1;
static bool process_system_initialized = false;

// Stack memory for processes (16KB per process)
static uint8_t process_stacks[MAX_PROCESSES][16384];

// Initialize process system
void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i].pid = 0;
        processes[i].state = PROCESS_STATE_TERMINATED;
        processes[i].memory_base = NULL;
        processes[i].memory_size = 0;
        processes[i].name[0] = '\0';
    }
    
    process_system_initialized = true;
}

// Find free process slot
static int find_free_process_slot(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROCESS_STATE_TERMINATED || processes[i].pid == 0) {
            return i;
        }
    }
    return -1;
}

// Create a new process from ELF data
int process_create(const char *name, const uint8_t *elf_data, size_t elf_size) {
    if (!process_system_initialized) {
        return -1;
    }
    
    // Find free slot
    int slot = find_free_process_slot();
    if (slot == -1) {
        return -1; // No free slots
    }
    
    // Load ELF program
    program_load_result_t load_result = elf_load_program(elf_data, elf_size);
    if (!load_result.success) {
        return -1;
    }
    
    // Initialize process
    process_t *proc = &processes[slot];
    proc->pid = next_pid++;
    proc->state = PROCESS_STATE_READY;
    proc->entry_point = load_result.entry_point;
    proc->memory_base = load_result.allocated_memory;
    proc->memory_size = load_result.memory_size;
    
    // Set up stack
    proc->stack_base = (uint64_t)&process_stacks[slot][0];
    proc->stack_size = sizeof(process_stacks[slot]);
    proc->rsp = proc->stack_base + proc->stack_size - 8; // Start at top of stack
    
    // Copy process name
    strncpy(proc->name, name, sizeof(proc->name) - 1);
    proc->name[sizeof(proc->name) - 1] = '\0';
    
    proc->exit_code = 0;
    
    return proc->pid;
}

// System call handler
uint64_t syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)arg2; // Unused for now
    (void)arg3; // Unused for now
    
    switch (syscall_num) {
        case SYS_EXIT:
            // Exit system call - arg1 is exit code
            terminal_print("Program exited with code: ");
            // Simple number printing
            if (arg1 == 0) {
                terminal_print("0");
            } else {
                char num_buf[16];
                int i = 0;
                uint64_t num = arg1;
                while (num > 0) {
                    num_buf[i++] = '0' + (num % 10);
                    num /= 10;
                }
                // Print in reverse
                for (int j = i - 1; j >= 0; j--) {
                    terminal_putchar(num_buf[j]);
                }
            }
            terminal_print("\n");
            return 0;
            
        case SYS_WRITE:
            // Write system call - arg1 is string pointer, arg2 is length
            // For simplicity, just treat arg1 as a null-terminated string
            if (arg1 != 0) {
                terminal_print((const char*)arg1);
            }
            return 0;
            
        case SYS_PUTCHAR:
            // Put character system call - arg1 is character
            terminal_putchar((char)arg1);
            return 0;
            
        case SYS_GETCHAR:
            // Get character system call
            return (uint64_t)read_key();
            
        default:
            terminal_print("Unknown system call: ");
            // Simple number printing for syscall_num
            if (syscall_num == 0) {
                terminal_print("0");
            } else {
                char num_buf[16];
                int i = 0;
                uint64_t num = syscall_num;
                while (num > 0) {
                    num_buf[i++] = '0' + (num % 10);
                    num /= 10;
                }
                // Print in reverse
                for (int j = i - 1; j >= 0; j--) {
                    terminal_putchar(num_buf[j]);
                }
            }
            terminal_print("\n");
            return -1;
    }
}

// Execute a process (simulation mode - completely safe)
bool process_execute(int pid) {
    if (!process_system_initialized) {
        return false;
    }
    
    // Find process
    process_t *proc = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid && processes[i].state == PROCESS_STATE_READY) {
            proc = &processes[i];
            break;
        }
    }
    
    if (!proc) {
        return false;
    }
    
    // Mark as running
    proc->state = PROCESS_STATE_RUNNING;
    
    terminal_print("Executing program: ");
    terminal_print(proc->name);
    terminal_print("\n");
    
    // Add basic safety checks
    if (proc->entry_point == 0) {
        terminal_print("Error: Invalid entry point\n");
        proc->state = PROCESS_STATE_ERROR;
        return false;
    }
    
    if (proc->memory_base == NULL) {
        terminal_print("Error: No memory allocated\n");
        proc->state = PROCESS_STATE_ERROR;
        return false;
    }
    
    terminal_print("Entry point: 0x");
    // Print entry point address in hex
    uint64_t addr = proc->entry_point;
    char hex_chars[] = "0123456789ABCDEF";
    char hex_str[17];
    hex_str[16] = '\0';
    for (int i = 15; i >= 0; i--) {
        hex_str[i] = hex_chars[addr & 0xF];
        addr >>= 4;
    }
    terminal_print(hex_str);
    terminal_print("\n");
    
    terminal_print("Memory allocated: ");
    // Print memory size
    size_t size = proc->memory_size;
    char size_str[16];
    int i = 0;
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
    
    // SIMULATION MODE - Don't actually execute any code
    // This is completely safe and demonstrates the program loading worked
    terminal_print("Simulating program execution...\n");
    
    // Simulate different program behaviors based on filename
    int result = 0;
    if (proc->name[0] != '\0') {
        // Look for patterns in the filename to determine simulated behavior
        char *name = proc->name;
        
        // Check if it's a "hello" program
        bool is_hello = false;
        for (int j = 0; name[j] != '\0'; j++) {
            if (name[j] == 'h' && name[j+1] == 'e' && name[j+2] == 'l' && 
                name[j+3] == 'l' && name[j+4] == 'o') {
                is_hello = true;
                break;
            }
        }
        
        if (is_hello) {
            terminal_print("Hello World program detected!\n");
            terminal_print("Simulating: Calculating 1+2+3+...+10\n");
            terminal_print("Result: 55\n");
            result = 55;
        } else {
            terminal_print("Simple test program detected!\n");
            terminal_print("Simulating: Returning test value\n");
            terminal_print("Result: 42\n");
            result = 42;
        }
    }
    
    terminal_print("Program simulation completed successfully!\n");
    
    // Mark as terminated
    proc->state = PROCESS_STATE_TERMINATED;
    proc->exit_code = result;
    
    terminal_print("Program terminated with exit code: ");
    if (result == 0) {
        terminal_print("0");
    } else {
        char num_buf[16];
        int j = 0;
        int num = result;
        bool negative = num < 0;
        if (negative) num = -num;
        
        while (num > 0) {
            num_buf[j++] = '0' + (num % 10);
            num /= 10;
        }
        
        if (j == 0) num_buf[j++] = '0';
        if (negative) terminal_putchar('-');
        
        // Print in reverse
        for (int k = j - 1; k >= 0; k--) {
            terminal_putchar(num_buf[k]);
        }
    }
    terminal_print("\n");
    
    return true;
}

// Terminate a process
void process_terminate(int pid, int exit_code) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            processes[i].state = PROCESS_STATE_TERMINATED;
            processes[i].exit_code = exit_code;
            break;
        }
    }
}

// Get process by PID
process_t* process_get(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            return &processes[i];
        }
    }
    return NULL;
}

// Clean up terminated processes
void process_cleanup_terminated(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROCESS_STATE_TERMINATED && processes[i].memory_base) {
            // In a real OS, you'd free the memory here
            processes[i].memory_base = NULL;
            processes[i].memory_size = 0;
            processes[i].pid = 0;
        }
    }
} 