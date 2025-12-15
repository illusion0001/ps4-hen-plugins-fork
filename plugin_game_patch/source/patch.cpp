#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

extern "C"
{
#include "../../common/memory.h"
}
#include "../../common/plugin_common.h"

char* unescape(const char* s)
{
    size_t len = strlen(s);
    char* unescaped_str = (char*)malloc(len + 1);
    if (!unescaped_str)
    {
        return nullptr;
    }
    uint32_t i, j;
    for (i = 0, j = 0; s[i] != '\0'; i++, j++)
    {
        if (s[i] == '\\')
        {
            i++;
            switch (s[i])
            {
                case 'n':
                    unescaped_str[j] = '\n';
                    break;
                case '0':
                    unescaped_str[j] = '\0';
                    break;
                case 't':
                    unescaped_str[j] = '\t';
                    break;
                case 'r':
                    unescaped_str[j] = '\r';
                    break;
                case '\\':
                    unescaped_str[j] = '\\';
                    break;
                case 'x':
                {
                    char hex_string[3] = {0};
                    uint32_t val = 0;
                    hex_string[0] = s[++i];
                    hex_string[1] = s[++i];
                    hex_string[2] = '\0';
                    if (sscanf(hex_string, "%x", &val) != 1)
                    {
                        final_printf("Invalid hex escape sequence: %s\n", hex_string);
                        val = '?';
                    }
                    unescaped_str[j] = (char)val;
                }
                break;
                default:
                    unescaped_str[j] = s[i];
                    break;
            }
        }
        else
        {
            unescaped_str[j] = s[i];
        }
    }
    unescaped_str[j] = '\0';
    return unescaped_str;
}

// valid hex look up table.
const uint8_t hex_lut[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

uint8_t* hexstrtochar2(const char* hexstr, size_t* size)
{
    uint32_t str_len = strlen(hexstr);
    size_t data_len = ((str_len + 1) / 2) * sizeof(uint8_t);
    *size = (str_len) * sizeof(uint8_t);
    if (!*size)
    {
        return nullptr;
    }
    uint8_t* data = (uint8_t*)malloc(*size);
    if (!data)
    {
        return nullptr;
    }
    uint32_t j = 0;  // hexstr position
    uint32_t i = 0;  // data position

    if (str_len % 2 == 1)
    {
        data[i] = (uint8_t)(hex_lut[0] << 4) | hex_lut[(uint8_t)hexstr[j]];
        j = ++i;
    }

    for (; j < str_len; j += 2, i++)
    {
        data[i] = (uint8_t)(hex_lut[(uint8_t)hexstr[j]] << 4) |
                  hex_lut[(uint8_t)hexstr[j + 1]];
    }

    *size = data_len;
    return data;
}

bool hex_prefix(const char* str)
{
    return (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'));
}

// http://www.cse.yorku.ca/~oz/hash.html
constexpr uint64_t djb2_hash(const char* str)
{
    uint64_t hash = 5381;
    uint32_t c = 0;
    while ((c = *str++))
    {
        hash = hash * 33 ^ c;
    }
    return hash;
}

static void sys_proc_rw_l(const uint64_t addr, const void* data, const size_t len)
{
    static int pid = 0;
    if (!pid)
    {
        pid = getpid();
    }
    final_printf("addr 0x%lx\n", addr);
    sys_proc_rw(pid, addr, data, len, 1);
}

uint64_t patch_hash_calc(const char* title, const char* name, const char* app_ver, const char* title_id, const char* elf)
{
    uint64_t output_hash = 0;
    char hash_str[256] = {0};
    snprintf(hash_str, sizeof(hash_str), "%s%s%s%s%s", title, name, app_ver, title_id, elf);
    output_hash = djb2_hash(hash_str);
    debug_printf("input: \"%s\"\n", hash_str);
    debug_printf("output: 0x%016lx\n", output_hash);
    return output_hash;
}

void patch_data1(const char* patch_type_str, uint64_t addr, const char* value, uint32_t source_size, uint64_t jump_target)
{
    uint64_t patch_type = djb2_hash(patch_type_str);
    switch (patch_type)
    {
        case djb2_hash("byte"):
        case djb2_hash("mask_byte"):
        {
            uint8_t real_value = 0;
            if (hex_prefix(value))
            {
                real_value = strtol(value, NULL, 16);
            }
            else
            {
                real_value = strtol(value, NULL, 10);
            }
            sys_proc_rw_l(addr, &real_value, sizeof(real_value));
            break;
        }
        case djb2_hash("bytes16"):
        case djb2_hash("mask_bytes16"):
        {
            uint16_t real_value = 0;
            if (hex_prefix(value))
            {
                real_value = strtol(value, NULL, 16);
            }
            else
            {
                real_value = strtol(value, NULL, 10);
            }
            sys_proc_rw_l(addr, &real_value, sizeof(real_value));
            break;
        }
        case djb2_hash("bytes32"):
        case djb2_hash("mask_bytes32"):
        {
            uint32_t real_value = 0;
            if (hex_prefix(value))
            {
                real_value = strtol(value, NULL, 16);
            }
            else
            {
                real_value = strtol(value, NULL, 10);
            }
            sys_proc_rw_l(addr, &real_value, sizeof(real_value));
            break;
        }
        case djb2_hash("bytesize_t"):
        case djb2_hash("mask_bytesize_t"):
        {
            size_t real_value = 0;
            if (hex_prefix(value))
            {
                real_value = strtoll(value, NULL, 16);
            }
            else
            {
                real_value = strtoll(value, NULL, 10);
            }
            sys_proc_rw_l(addr, &real_value, sizeof(real_value));
            break;
        }
        case djb2_hash("bytes"):
        case djb2_hash("mask"):
        case djb2_hash("mask_bytes"):
        {
            size_t bytearray_size = 0;
            uint8_t* bytearray = hexstrtochar2(value, &bytearray_size);
            if (!bytearray)
            {
                break;
            }
            sys_proc_rw_l(addr, bytearray, bytearray_size);
            free(bytearray);
            break;
        }
        case djb2_hash("float32"):
        case djb2_hash("mask_float32"):
        {
            float real_value = 0;
            real_value = strtod(value, NULL);
            sys_proc_rw_l(addr, &real_value, sizeof(real_value));
            break;
        }
        case djb2_hash("float64"):
        case djb2_hash("mask_float64"):
        {
            double real_value = 0;
            real_value = strtod(value, NULL);
            sys_proc_rw_l(addr, &real_value, sizeof(real_value));
            break;
        }
        case djb2_hash("utf8"):
        case djb2_hash("mask_utf8"):
        {
            char* new_str = unescape(value);
            if (!new_str)
            {
                break;
            }
            uint64_t char_len = strlen(new_str);
            sys_proc_rw_l(addr, (void*)new_str, char_len + 1);  // get null
            free(new_str);
            break;
        }
        case djb2_hash("utf16"):
        case djb2_hash("mask_utf16"):
        {
            char* new_str = unescape(value);
            if (!new_str)
            {
                break;
            }
            for (uint32_t i = 0; new_str[i] != '\x00'; i++)
            {
                uint8_t val_ = new_str[i];
                uint8_t value_[2] = {val_, 0x00};
                sys_proc_rw_l(addr, value_, sizeof(value_));
                addr = addr + 2;
            }
            uint8_t value_[2] = {0x00, 0x00};
            sys_proc_rw_l(addr, value_, sizeof(value_));
            free(new_str);
            break;
        }
        case djb2_hash("mask_jump32"):
        {
            constexpr uint32_t MAX_PATTERN_LENGTH = 256;
            if (source_size < 5)
            {
                final_printf("Can't create code cave with size less than 32 bit jump!\n");
                break;
            }
            if (source_size > MAX_PATTERN_LENGTH)
            {
                final_printf("Can't create code cave with size more than %u bytes!\n", MAX_PATTERN_LENGTH);
                break;
            }
            uint8_t nop_bytes[MAX_PATTERN_LENGTH];
            memset(nop_bytes, 0x90, sizeof(nop_bytes));
            sys_proc_rw_l(addr, nop_bytes, source_size);
            size_t bytearray_size = 0;
            uint8_t* bytearray = hexstrtochar2(value, &bytearray_size);
            if (!bytearray)
            {
                break;
            }
            uint64_t code_cave_end = jump_target + bytearray_size;
            uint8_t jump_32[5] = {0xe9, 0x00, 0x00, 0x00, 0x00};
            int32_t target_jmp = (int32_t)(jump_target - addr - sizeof(jump_32));
            int32_t target_return = (int32_t)(addr) - (code_cave_end);
            sys_proc_rw_l(jump_target, bytearray, bytearray_size);
            sys_proc_rw_l(addr, jump_32, sizeof(jump_32));
            sys_proc_rw_l(addr + 1, &target_jmp, sizeof(target_jmp));
            sys_proc_rw_l(jump_target + bytearray_size, jump_32, sizeof(jump_32));
            sys_proc_rw_l(code_cave_end + 1, &target_return, sizeof(target_return));
            free(bytearray);
            break;
        }
        case djb2_hash("patchCall"):
        case djb2_hash("mask_patchCall"):
        {
            uint8_t call_bytes[5] = {0};
            memcpy(call_bytes, (void*)addr, sizeof(call_bytes));
            if (call_bytes[0] == 0xe8 || call_bytes[0] == 0xe9)
            {
                int32_t branch_target = *(int32_t*)(call_bytes + 1);
                if (branch_target)
                {
                    uintptr_t branched_call = addr + branch_target + sizeof(call_bytes);
                    final_printf("0x%016lx: 0x%08x -> 0x%016lx\n", addr, branch_target, branched_call);
                    size_t bytearray_size = 0;
                    uint8_t* bytearray = hexstrtochar2(value, &bytearray_size);
                    if (!bytearray)
                    {
                        break;
                    }
                    sys_proc_rw_l(branched_call, bytearray, bytearray_size);
                    free(bytearray);
                }
            }
            break;
        }
        default:
        {
            final_printf("Patch type: '%s (#%.16lx) not found or unsupported\n", patch_type_str, patch_type);
            final_printf("Patch data:\n");
            final_printf("      Address: 0x%lx\n", addr);
            final_printf("      Value: %s\n", value);
            final_printf("      Jump Size: %u\n", source_size);
            final_printf("      Jump Target: 0x%lx\n", jump_target);
            break;
        }
    }
}
