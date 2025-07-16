#include "filesystem.h"
#include "../string.h"

// Global file system storage
static file_t files[MAX_FILES];
static bool fs_initialized = false;

// Simple time counter (since we don't have real time yet)
static uint32_t current_time = 0;

// Forward declaration
static void fs_create_default_files(void);

// Initialize the file system
void fs_init(void) {
    if (fs_initialized) return;
    
    // Clear all files
    for (int i = 0; i < MAX_FILES; i++) {
        files[i].used = false;
        files[i].name[0] = '\0';
        files[i].type = FILE_TYPE_REGULAR;
        files[i].size = 0;
        files[i].created_time = 0;
        files[i].modified_time = 0;
    }
    
    fs_initialized = true;
    
    // Create default files after initialization is complete
    fs_create_default_files();
}

// Create default files (separate function to avoid boot issues)
static void fs_create_default_files(void) {
    // Create welcome file
    if (fs_create_file("welcome.txt", FILE_TYPE_REGULAR)) {
        fs_write_file("welcome.txt", (uint8_t*)"Welcome to DEA OS!\nType 'help' for commands.\n", 45);
    }
    
    // Create readme file
    if (fs_create_file("readme.txt", FILE_TYPE_REGULAR)) {
        fs_write_file("readme.txt", (uint8_t*)"DEA OS File System\n\nCommands:\n- ls\n- cat\n- touch\n- rm\n- write\n- df\n", 70);
    }
}

// Find a free file slot
static int find_free_slot(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            return i;
        }
    }
    return -1;
}

// Create a new file
bool fs_create_file(const char *name, file_type_t type) {
    if (!fs_initialized) return false;
    
    // Check if file already exists
    if (fs_find_file(name) != NULL) {
        return false;
    }
    
    // Find free slot
    int slot = find_free_slot();
    if (slot == -1) {
        return false;
    }
    
    // Create the file
    files[slot].used = true;
    strcpy(files[slot].name, name);
    files[slot].type = type;
    files[slot].size = 0;
    files[slot].created_time = ++current_time;
    files[slot].modified_time = current_time;
    
    return true;
}

// Delete a file
bool fs_delete_file(const char *name) {
    if (!fs_initialized) return false;
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            files[i].used = false;
            files[i].name[0] = '\0';
            files[i].size = 0;
            return true;
        }
    }
    return false;
}

// Find a file by name
file_t* fs_find_file(const char *name) {
    if (!fs_initialized) return NULL;
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            return &files[i];
        }
    }
    return NULL;
}

// Write data to a file
bool fs_write_file(const char *name, const uint8_t *data, size_t size) {
    if (!fs_initialized) return false;
    
    if (size > MAX_FILE_SIZE) {
        return false;
    }
    
    file_t *file = fs_find_file(name);
    if (file == NULL) {
        // Create file if it doesn't exist
        if (!fs_create_file(name, FILE_TYPE_REGULAR)) {
            return false;
        }
        file = fs_find_file(name);
    }
    
    // Copy data
    for (size_t i = 0; i < size; i++) {
        file->data[i] = data[i];
    }
    file->size = size;
    file->modified_time = ++current_time;
    
    return true;
}

// Read data from a file
bool fs_read_file(const char *name, uint8_t *buffer, size_t *size) {
    if (!fs_initialized) return false;
    
    file_t *file = fs_find_file(name);
    if (file == NULL) {
        return false;
    }
    
    // Copy data
    for (size_t i = 0; i < file->size; i++) {
        buffer[i] = file->data[i];
    }
    *size = file->size;
    
    return true;
}

// List all files
int fs_list_files(dir_entry_t *entries, int max_entries) {
    if (!fs_initialized) return 0;
    
    int count = 0;
    
    for (int i = 0; i < MAX_FILES && count < max_entries; i++) {
        if (files[i].used) {
            strcpy(entries[count].name, files[i].name);
            entries[count].type = files[i].type;
            entries[count].size = files[i].size;
            count++;
        }
    }
    
    return count;
}

// Check if file exists
bool fs_file_exists(const char *name) {
    return fs_find_file(name) != NULL;
}

// Get free space
size_t fs_get_free_space(void) {
    if (!fs_initialized) return 0;
    
    int free_slots = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            free_slots++;
        }
    }
    
    return free_slots * MAX_FILE_SIZE;
}

// Get used space
size_t fs_get_used_space(void) {
    if (!fs_initialized) return 0;
    
    size_t used = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            used += files[i].size;
        }
    }
    
    return used;
} 