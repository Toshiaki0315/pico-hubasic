#include "hal_sdcard.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

void hal_sdcard_resolve_path(const char* input_path, char* resolved_path, size_t max_len) {
    const char* p = input_path;
    // Strip "0:" prefix
    if (strncmp(p, "0:", 2) == 0) {
        p += 2;
    }
    // Strip "CAS:" prefix
    else if (strncmp(p, "CAS:", 4) == 0) {
        p += 4;
    }
    
    strncpy(resolved_path, p, max_len - 1);
    resolved_path[max_len - 1] = '\0';
}

#if __has_include("pico/stdlib.h")
// ---------------------------------------------------------
// Pico Target Implementations (Stub for future FatFS)
// ---------------------------------------------------------

void hal_sdcard_init() {
    // TODO: Initialize SPI for SD Card and mount FatFS
}

void* hal_file_open(const char* path, const char* mode) { return NULL; }
void hal_file_close(void* file) {}
size_t hal_file_read(void* buffer, size_t size, size_t count, void* file) { return 0; }
size_t hal_file_write(const void* buffer, size_t size, size_t count, void* file) { return 0; }
char* hal_file_gets(char* str, int n, void* file) { return NULL; }

int hal_file_printf(void* file, const char* format, ...) {
    return -1;
}

void* hal_dir_open(const char* path) { return NULL; }
void hal_dir_close(void* dir) {}
const char* hal_dir_read(void* dir) { return NULL; }

int hal_file_remove(const char* path) { return -1; }
int hal_file_rename(const char* old_path, const char* new_path) { return -1; }

#else
// ---------------------------------------------------------
// Host Target Implementations (using standard C library)
// ---------------------------------------------------------
#include <dirent.h>

void hal_sdcard_init() {
    // No initialization needed for host
}

void* hal_file_open(const char* path, const char* mode) {
    char resolved[256];
    hal_sdcard_resolve_path(path, resolved, sizeof(resolved));
    return fopen(resolved, mode);
}

void hal_file_close(void* file) {
    if (file) fclose((FILE*)file);
}

size_t hal_file_read(void* buffer, size_t size, size_t count, void* file) {
    if (!file) return 0;
    return fread(buffer, size, count, (FILE*)file);
}

size_t hal_file_write(const void* buffer, size_t size, size_t count, void* file) {
    if (!file) return 0;
    return fwrite(buffer, size, count, (FILE*)file);
}

char* hal_file_gets(char* str, int n, void* file) {
    if (!file) return NULL;
    return fgets(str, n, (FILE*)file);
}

int hal_file_printf(void* file, const char* format, ...) {
    if (!file) return -1;
    va_list args;
    va_start(args, format);
    int ret = vfprintf((FILE*)file, format, args);
    va_end(args);
    return ret;
}

void* hal_dir_open(const char* path) {
    char resolved[256];
    hal_sdcard_resolve_path(path, resolved, sizeof(resolved));
    return opendir(resolved);
}

void hal_dir_close(void* dir) {
    if (dir) closedir((DIR*)dir);
}

const char* hal_dir_read(void* dir) {
    if (!dir) return NULL;
    struct dirent* entry = readdir((DIR*)dir);
    if (!entry) return NULL;
    return entry->d_name;
}

int hal_file_remove(const char* path) {
    char resolved[256];
    hal_sdcard_resolve_path(path, resolved, sizeof(resolved));
    return remove(resolved);
}

int hal_file_rename(const char* old_path, const char* new_path) {
    char resolved_old[256];
    char resolved_new[256];
    hal_sdcard_resolve_path(old_path, resolved_old, sizeof(resolved_old));
    hal_sdcard_resolve_path(new_path, resolved_new, sizeof(resolved_new));
    return rename(resolved_old, resolved_new);
}

#endif
