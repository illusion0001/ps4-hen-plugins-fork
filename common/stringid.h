#pragma once

// https://github.com/icemesh/StringId/blob/main/StringId64/main.c

#include <stdint.h>
#include <wchar.h>

#if defined(__cplusplus)
#define CXX_CONSTEXPR constexpr
#else
#define CXX_CONSTEXPR
#endif

static CXX_CONSTEXPR uint64_t ToStringId64A(const char* str)
{
    uint64_t base = 0xCBF29CE484222325;
    if (*str)
    {
        do
        {
            base = 0x100000001B3 * (base ^ *str++);
        } while (*str);
    }
    return base;
}

static CXX_CONSTEXPR uint64_t ToStringId64W(const wchar_t* str)
{
    uint64_t base = 0xCBF29CE484222325;
    if (*str)
    {
        do
        {
            base = 0x100000001B3 * (base ^ *str++);
        } while (*str);
    }
    return base;
}

#define SID(x) ToStringId64A(x)
#define wSID(x) ToStringId64W(x)
