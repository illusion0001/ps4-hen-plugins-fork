#pragma once

#define uINIT_FUNCTION_PTR(func_name) \
    union func_name##_union func_name = {.addr = 0}

#define uTYPEDEF_FUNCTION_PTR(ret_type, func_name, ...) \
    typedef ret_type (*func_name##_ptr)(__VA_ARGS__);   \
    union func_name##_union                             \
    {                                                   \
        func_name##_ptr ptr;                            \
        uintptr_t addr;                                 \
        void* v;                                        \
    };                                                  \
    extern union func_name##_union func_name

#define uiTYPEDEF_FUNCTION_PTR(ret_type, func_name, ...)     \
    uTYPEDEF_FUNCTION_PTR(ret_type, func_name, __VA_ARGS__); \
    uINIT_FUNCTION_PTR(func_name)
