#pragma once

#include <assert.h>

struct SceEntry
{
    int argc;
    int pad;
    const char* argv[];
};

static_assert(__builtin_offsetof(struct SceEntry, argv) == 0x8, "");
