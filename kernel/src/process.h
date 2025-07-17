#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "elf_loader.h"

// Process states
typedef enum {
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_TERMINATED,
    PROCESS_STATE_ERROR
} process_state_t;

// System call numbers
#define SYS_EXIT        1
#define SYS_WRITE       2
#define SYS_READ        3
#define SYS_PUTCHAR     4
#define SYS_GETCHAR     5

// Process Control Block (simplified)
typedef struct {
    uint32_t pid;                    // Process ID
    process_state_t state;           // Current state
    uint64_t entry_point;            // Program entry point
    void *memory_base;               // Base address of allocated memory
    size_t memory_size;              // Size of allocated memory
    uint64_t rsp;                    // Stack pointer
    uint64_t stack_base;             // Stack base address
    size_t stack_size;               // Stack size
    char name[64];                   // Process name
    int exit_code;                   // Exit code when terminated
} process_t;

// Maximum number of processes
#define MAX_PROCESSES 8

// Function declarations
void process_init(void);
int process_create(const char *name, const uint8_t *elf_data, size_t elf_size);
bool process_execute(int pid);
void process_terminate(int pid, int exit_code);
process_t* process_get(int pid);
void process_cleanup_terminated(void);

// System call handler
uint64_t syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

#endif 