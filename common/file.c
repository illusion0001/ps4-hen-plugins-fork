// https://github.com/bucanero/apollo-ps4/blob/461ee5d58653a82ab4b901041a3cc0b7026bfffb/source/util.c

#include "plugin_common.h"

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include "file.h"

#define TEMP_DIR "/user/temp"

int get_file_size(const char* file_path, uint64_t* size)
{
    struct stat stat_buf = {0};

    if (!file_path || !size)
    {
        return -1;
    }

    if (stat(file_path, &stat_buf) < 0)
    {
        return -1;
    }

    *size = stat_buf.st_size;

    return 0;
}

int read_file(const char* file_path, void* data, uint64_t size)
{
    FILE* fp;

    if (!file_path || !data)
    {
        return -1;
    }

    fp = fopen(file_path, "rb");
    if (!fp)
    {
        return -1;
    }

    memset(data, 0, size);

    if (fread(data, 1, size, fp) < 0)
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    return 0;
}

int write_file(const char* file_path, const void* data, uint64_t size)
{
    FILE* fp;

    if (!file_path || !data)
    {
        return -1;
    }

    fp = fopen(file_path, "wb");
    if (!fp)
    {
        return -1;
    }

    if (fwrite(data, 1, size, fp) < 0)
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    return 0;
}

// https://github.com/bucanero/apollo-ps4/blob/461ee5d58653a82ab4b901041a3cc0b7026bfffb/source/common.c#L37
int file_exists(const char* path)
{
    if (access(path, F_OK) == 0)
    {
        return 0;
    }
    return 1;
}

int touch(const char* path)
{
    FILE* s_FilePointer = fopen(path, "w");
    if (!s_FilePointer)
    {
        return 1;
    }
    fclose(s_FilePointer);
    return 0;
}

int touch_temp(const char* func)
{
    char path[260] = {};
    snprintf(path, sizeof(path), TEMP_DIR "/%s", func);
    const int r = touch(path);
    final_printf("[%s] %s return %d\n", __FUNCTION__, path, r);
    return r;
}

int file_exists_temp(const char* func)
{
    char path[260] = {};
    snprintf(path, sizeof(path), TEMP_DIR "/%s", func);
    const int r = file_exists(path);
    final_printf("[%s] %s return %d\n", __FUNCTION__, path, r);
    return r;
}

// https://github.com/bucanero/apollo-ps4/blob/461ee5d58653a82ab4b901041a3cc0b7026bfffb/source/saves.c#L190
char* endsWith(const char* a, const char* b)
{
    int al = strlen(a), bl = strlen(b);

    if (al < bl)
    {
        return NULL;
    }

    a += (al - bl);
    while (*a)
    {
        extern int toupper(int);
        if (toupper(*a++) != toupper(*b++))
        {
            return NULL;
        }
    }

    return (char*)(a - bl);
}
