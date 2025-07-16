#include "filesystem.h"
#include "../fs/filesystem.h"
#include "../terminal.h"
#include "../string.h"

// Helper function to print file size
void print_file_size(size_t size) {
    if (size < 1024) {
        terminal_print("    ");
        // Simple number to string conversion
        char buf[16];
        int i = 0;
        if (size == 0) {
            buf[i++] = '0';
        } else {
            int temp = size;
            while (temp > 0) {
                buf[i++] = '0' + (temp % 10);
                temp /= 10;
            }
            // Reverse the string
            for (int j = 0; j < i / 2; j++) {
                char temp = buf[j];
                buf[j] = buf[i - 1 - j];
                buf[i - 1 - j] = temp;
            }
        }
        buf[i] = '\0';
        terminal_print(buf);
        terminal_print("B");
    } else {
        terminal_print("   ");
        char buf[16];
        int kb = size / 1024;
        int i = 0;
        if (kb == 0) {
            buf[i++] = '0';
        } else {
            int temp = kb;
            while (temp > 0) {
                buf[i++] = '0' + (temp % 10);
                temp /= 10;
            }
            // Reverse the string
            for (int j = 0; j < i / 2; j++) {
                char temp = buf[j];
                buf[j] = buf[i - 1 - j];
                buf[i - 1 - j] = temp;
            }
        }
        buf[i] = '\0';
        terminal_print(buf);
        terminal_print("KB");
    }
}

// Helper function to print file type
void print_file_type(int type) {
    if (type == FILE_TYPE_DIRECTORY) {
        terminal_print(" DIR  ");
    } else {
        terminal_print(" FILE ");
    }
}

// List files command
void cmd_ls(const char *args) {
    (void)args; // Unused parameter
    
    dir_entry_t entries[MAX_FILES];
    int count = fs_list_files(entries, MAX_FILES);
    
    if (count == 0) {
        terminal_print("No files found.\n");
        return;
    }
    
    terminal_print("Files:\n");
    terminal_print("TYPE  SIZE   NAME\n");
    terminal_print("----  ----   ----\n");
    
    for (int i = 0; i < count; i++) {
        print_file_type(entries[i].type);
        print_file_size(entries[i].size);
        terminal_print("  ");
        terminal_print(entries[i].name);
        terminal_print("\n");
    }
}

// Cat command - display file contents
void cmd_cat(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: cat <filename>\n");
        return;
    }
    
    uint8_t buffer[MAX_FILE_SIZE];
    size_t size;
    
    if (fs_read_file(args, buffer, &size)) {
        // Print file contents
        for (size_t i = 0; i < size; i++) {
            terminal_putchar(buffer[i]);
        }
        if (size > 0 && buffer[size - 1] != '\n') {
            terminal_putchar('\n');
        }
    } else {
        terminal_print("Error: File '");
        terminal_print(args);
        terminal_print("' not found.\n");
    }
}

// Remove file command
void cmd_rm(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: rm <filename>\n");
        return;
    }
    
    if (fs_delete_file(args)) {
        terminal_print("File '");
        terminal_print(args);
        terminal_print("' deleted.\n");
    } else {
        terminal_print("Error: File '");
        terminal_print(args);
        terminal_print("' not found.\n");
    }
}

// Touch command - create empty file
void cmd_touch(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: touch <filename>\n");
        return;
    }
    
    if (fs_file_exists(args)) {
        terminal_print("File '");
        terminal_print(args);
        terminal_print("' already exists.\n");
        return;
    }
    
    if (fs_create_file(args, FILE_TYPE_REGULAR)) {
        terminal_print("File '");
        terminal_print(args);
        terminal_print("' created.\n");
    } else {
        terminal_print("Error: Could not create file '");
        terminal_print(args);
        terminal_print("'.\n");
    }
}

// Disk free command - show filesystem statistics
void cmd_df(const char *args) {
    (void)args; // Unused parameter
    
    size_t used = fs_get_used_space();
    size_t free = fs_get_free_space();
    size_t total = used + free;
    
    terminal_print("Filesystem Usage:\n");
    terminal_print("Total space: ");
    print_file_size(total);
    terminal_print("\nUsed space:  ");
    print_file_size(used);
    terminal_print("\nFree space:  ");
    print_file_size(free);
    terminal_print("\n");
    
    // Calculate percentage
    if (total > 0) {
        int percentage = (used * 100) / total;
        terminal_print("Usage: ");
        char buf[4];
        int i = 0;
        if (percentage == 0) {
            buf[i++] = '0';
        } else {
            int temp = percentage;
            while (temp > 0) {
                buf[i++] = '0' + (temp % 10);
                temp /= 10;
            }
            // Reverse the string
            for (int j = 0; j < i / 2; j++) {
                char temp = buf[j];
                buf[j] = buf[i - 1 - j];
                buf[i - 1 - j] = temp;
            }
        }
        buf[i] = '\0';
        terminal_print(buf);
        terminal_print("%\n");
    }
}

// Write command - write text to file
void cmd_write(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: write <filename> <text>\n");
        return;
    }
    
    // Find the first space to separate filename from content
    const char *space = args;
    while (*space && *space != ' ') {
        space++;
    }
    
    if (*space == '\0') {
        terminal_print("Usage: write <filename> <text>\n");
        return;
    }
    
    // Extract filename
    char filename[MAX_FILENAME_LENGTH];
    size_t filename_len = space - args;
    if (filename_len >= MAX_FILENAME_LENGTH) {
        terminal_print("Error: Filename too long.\n");
        return;
    }
    
    for (size_t i = 0; i < filename_len; i++) {
        filename[i] = args[i];
    }
    filename[filename_len] = '\0';
    
    // Skip spaces after filename
    while (*space == ' ') {
        space++;
    }
    
    // Write content to file
    if (fs_write_file(filename, (const uint8_t*)space, strlen(space))) {
        terminal_print("Content written to '");
        terminal_print(filename);
        terminal_print("'.\n");
    } else {
        terminal_print("Error: Could not write to file '");
        terminal_print(filename);
        terminal_print("'.\n");
    }
}

// Register filesystem commands
void register_filesystem_commands(void) {
    register_command("ls", cmd_ls, "List files and directories", "ls", "Filesystem");
    register_command("cat", cmd_cat, "Display file contents", "cat <filename>", "Filesystem");
    register_command("rm", cmd_rm, "Remove a file", "rm <filename>", "Filesystem");
    register_command("touch", cmd_touch, "Create an empty file", "touch <filename>", "Filesystem");
    register_command("write", cmd_write, "Write text to a file", "write <filename> <text>", "Filesystem");
    register_command("df", cmd_df, "Show filesystem usage", "df", "Filesystem");
} 