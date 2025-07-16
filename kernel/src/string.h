#ifndef STRING_H
#define STRING_H

#include <stddef.h>

// String functions
size_t strlen(const char *str);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcat(char *dest, const char *src);
void int_to_string(int value, char *buffer);

#endif // STRING_H 