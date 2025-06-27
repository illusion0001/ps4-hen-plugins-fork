#pragma once

#include <stdint.h>

int get_file_size(const char* file_path, uint64_t* size);
int read_file(const char* file_path, void* data, uint64_t size);
int write_file(const char* file_path, const void* data, uint64_t size);
int file_exists(const char* path);
int touch(const char* path);
int touch_temp(const char* func);
int file_exists_temp(const char* func);
