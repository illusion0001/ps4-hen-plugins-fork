// https://github.com/bucanero/apollo-ps4/blob/461ee5d58653a82ab4b901041a3cc0b7026bfffb/source/util.c

#include "plugin_common.h"

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include "file.h"

#include "memory.h"

#define STRLEN_CONST(str) (_countof(str) - 1)

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

static bool validAscii(unsigned int c){
    return c < ' ' || c > '\x7e';
}

#define XML_LT "&lt;"
#define XML_GT "&gt;"
#define XML_AMP "&amp;"
#define XML_QUOT "&quot;"
#define XML_APOS "&apos;"
#define XML_NEWLINE "&#10;"
#define XML_CARRIAGE "&#13;"
#define XML_TAB "&#9;"

size_t calculate_xml_size(const char* input)
{
    size_t size = 0;
    const char* p = input;

    while (*p)
    {
        switch (*p)
        {
            case '<':
                size += STRLEN_CONST(XML_LT);
                break;
            case '>':
                size += STRLEN_CONST(XML_GT);
                break;
            case '&':
                size += STRLEN_CONST(XML_AMP);
                break;
            case '"':
                size += STRLEN_CONST(XML_QUOT);
                break;
            case '\'':
                size += STRLEN_CONST(XML_APOS);
                break;
            case '\n':
                size += STRLEN_CONST(XML_NEWLINE);
                break;
            case '\r':
                size += STRLEN_CONST(XML_CARRIAGE);
                break;
            case '\t':
                size += STRLEN_CONST(XML_TAB);
                break;
            default:
                if (validAscii((unsigned char)*p))
                {
                    size += 6;
                }
                else
                {
                    size += 1;
                }
                break;
        }
        p++;
    }

    return size + 1;
}

char* string_to_xml(const char* input)
{
    if (!input)
    {
        return NULL;
    }

    size_t output_size = calculate_xml_size(input);
    char* output = malloc(output_size);
    if (!output)
    {
        return NULL;
    }

    const char* src = input;
    char* dst = output;

    while (*src)
    {
        switch (*src)
        {
            case '<':
                strcpy(dst, XML_LT);
                dst += STRLEN_CONST(XML_LT);
                break;
            case '>':
                strcpy(dst, XML_GT);
                dst += STRLEN_CONST(XML_GT);
                break;
            case '&':
                strcpy(dst, XML_AMP);
                dst += STRLEN_CONST(XML_AMP);
                break;
            case '"':
                strcpy(dst, XML_QUOT);
                dst += STRLEN_CONST(XML_QUOT);
                break;
            case '\'':
                strcpy(dst, XML_APOS);
                dst += STRLEN_CONST(XML_APOS);
                break;
            case '\n':
                strcpy(dst, XML_NEWLINE);
                dst += STRLEN_CONST(XML_NEWLINE);
                break;
            case '\r':
                strcpy(dst, XML_CARRIAGE);
                dst += STRLEN_CONST(XML_CARRIAGE);
                break;
            case '\t':
                strcpy(dst, XML_TAB);
                dst += STRLEN_CONST(XML_TAB);
                break;
            default:
                if (validAscii((unsigned char)*src))
                {
                    sprintf(dst, "&#%d;", (unsigned char)*src);
                    int written = strlen(dst);
                    dst += written;
                }
                else
                {
                    *dst++ = *src;
                }
                break;
        }
        src++;
    }

    *dst = '\0';
    return output;
}

void xml_string_free(const char* s)
{
    free((void*)s);
}
