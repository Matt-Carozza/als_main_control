#ifndef STRING_TYPE_H_
#define STRING_TYPE_H_

#include <stdio.h>

typedef struct {
    char data[128];
    size_t length;
} String128;

String128 String128_create(char* data, size_t length);
void String128_copy(String128* dest, char* src, size_t src_length);

#endif // STRING_TYPE_H_