#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// File system constants - reduced for boot stability
#define MAX_FILES 16          // Reduced from 64
#define MAX_FILENAME_LENGTH 32 // Reduced from 64  
#define MAX_FILE_SIZE 1024     // Reduced from 4096
#define MAX_PATH_LENGTH 128    // Reduced from 256

// File types
typedef enum {
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY
} file_type_t;

// File structure
typedef struct {
    char name[MAX_FILENAME_LENGTH];
    file_type_t type;
    size_t size;
    uint8_t data[MAX_FILE_SIZE];
    bool used;
    uint32_t created_time;
    uint32_t modified_time;
} file_t;

// Directory entry
typedef struct {
    char name[MAX_FILENAME_LENGTH];
    file_type_t type;
    size_t size;
} dir_entry_t;

// File system functions
void fs_init(void);
bool fs_create_file(const char *name, file_type_t type);
bool fs_delete_file(const char *name);
file_t* fs_find_file(const char *name);
bool fs_write_file(const char *name, const uint8_t *data, size_t size);
bool fs_read_file(const char *name, uint8_t *buffer, size_t *size);
int fs_list_files(dir_entry_t *entries, int max_entries);
bool fs_file_exists(const char *name);
size_t fs_get_free_space(void);
size_t fs_get_used_space(void);

#endif 