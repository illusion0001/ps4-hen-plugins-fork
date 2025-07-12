#pragma once

#define JOIN_HELPER(x, y) x##y
#define JOIN(x, y) JOIN_HELPER(x, y)
#define TYPE_PAD(size) char JOIN(_pad_, __COUNTER__)[size]
#define TYPE_BEGIN(name, size) \
    name                       \
    {                          \
        union                  \
        {                      \
            TYPE_PAD(size)
#define TYPE_END(...) \
    }                 \
    ;                 \
    }                 \
    __VA_ARGS__

#define TYPE_FIELD(field, offset) \
    struct                        \
    {                             \
        TYPE_PAD(offset);         \
        field;                    \
    }
