#include "string.h"

// Calculate length of a string
size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

// Compare two strings
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

// Copy string
char *strcpy(char *dest, const char *src) {
    char *start = dest;
    while ((*dest++ = *src++));
    return start;
}

// Copy up to n characters from string
char *strncpy(char *dest, const char *src, size_t n) {
    char *start = dest;
    while (n && (*dest++ = *src++)) {
        n--;
    }
    while (n--) {
        *dest++ = '\0';
    }
    return start;
}

// Compare first n characters of two strings
int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return (unsigned char)*s1 - (unsigned char)*s2;
}

// Concatenate two strings
char *strcat(char *dest, const char *src) {
    char *ptr = dest;
    while (*ptr) ptr++;  // Find end of dest
    while ((*ptr++ = *src++));  // Copy src to end of dest
    return dest;
}

// Convert integer to string
void int_to_string(int value, char *buffer) {
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    char temp[12];  // Enough for 32-bit int
    int i = 0;
    int is_negative = 0;
    
    if (value < 0) {
        is_negative = 1;
        value = -value;
    }
    
    // Convert digits in reverse order
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    // Add negative sign if needed
    if (is_negative) {
        temp[i++] = '-';
    }
    
    // Reverse the string
    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}