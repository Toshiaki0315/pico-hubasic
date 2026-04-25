#pragma once
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the SD card and file system (FatFS)
void hal_sdcard_init();

// File operations
void* hal_file_open(const char* path, const char* mode);
void hal_file_close(void* file);
size_t hal_file_read(void* buffer, size_t size, size_t count, void* file);
size_t hal_file_write(const void* buffer, size_t size, size_t count, void* file);
char* hal_file_gets(char* str, int n, void* file);
int hal_file_printf(void* file, const char* format, ...);

// Directory operations
void* hal_dir_open(const char* path);
void hal_dir_close(void* dir);
const char* hal_dir_read(void* dir);

// File management
int hal_file_remove(const char* path);
int hal_file_rename(const char* old_path, const char* new_path);

// Helper to normalize paths like "0:TEST.BAS" or "CAS:TEST.BAS"
void hal_sdcard_resolve_path(const char* input_path, char* resolved_path, size_t max_len);

#ifdef __cplusplus
}
#endif
