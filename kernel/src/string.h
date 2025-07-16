#ifndef STRING_H
#define STRING_H

#include <stddef.h>

// String utility functions
size_t strlen(const char *str);
char *strcpy(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

#endif 