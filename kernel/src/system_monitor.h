#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// System memory information structure
typedef struct {
    uint64_t total_memory;      // Total system memory in bytes
    uint64_t used_memory;       // Used memory in bytes  
    uint64_t free_memory;       // Free memory in bytes
    uint64_t buffer_memory;     // Buffer memory in bytes
    uint64_t cache_memory;      // Cache memory in bytes
    float usage_percentage;     // Memory usage as percentage
} memory_info_t;

// CPU usage information structure  
typedef struct {
    float current_usage;        // Current CPU usage percentage
    float average_usage;        // Average CPU usage over time
    uint64_t idle_time;         // Time spent idle
    uint64_t active_time;       // Time spent active
    uint32_t frequency;         // CPU frequency in MHz (if available)
    uint32_t core_count;        // Number of CPU cores
} cpu_info_t;

// System monitoring functions
void sysmon_init(void);
void sysmon_update(void);
bool sysmon_get_memory_info(memory_info_t *info);
bool sysmon_get_cpu_info(cpu_info_t *info);

// Memory monitoring functions
uint64_t sysmon_get_total_memory(void);
uint64_t sysmon_get_used_memory(void);
uint64_t sysmon_get_free_memory(void);
float sysmon_get_memory_usage_percent(void);

// CPU monitoring functions  
float sysmon_get_cpu_usage_percent(void);
void sysmon_sample_cpu_usage(void);
uint32_t sysmon_get_cpu_frequency(void);

// Utility functions
void sysmon_format_bytes(uint64_t bytes, char *buffer, size_t buffer_size);
void sysmon_format_percentage(float percentage, char *buffer, size_t buffer_size);

// Real memory allocation tracking functions
void sysmon_track_allocation(uint64_t addr, uint64_t bytes);
void sysmon_track_deallocation(uint64_t addr);
void sysmon_get_allocation_stats(uint32_t *total_allocs, uint32_t *active_allocs, uint64_t *peak_usage);

// Constants
#define SYSMON_SAMPLE_HISTORY 10  // Number of samples to keep for averaging
#define SYSMON_UPDATE_INTERVAL_MS 1000  // Update interval in milliseconds

#endif // SYSTEM_MONITOR_H 