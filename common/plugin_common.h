#include "git_ver.h"

#define MAX_PATH_ 260

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

// just a copy of `debug_printf` but in final builds
#define debug_print_always(a, args...) printf("[%s] (%s:%d) " a, __FUNCTION__, __FILE__, __LINE__, ##args)

#if (__FINAL__) == 1
#define BUILD_TYPE "(Release)"
#define debug_printf(a, args...)
#else
#define BUILD_TYPE "(Debug)"
#define debug_printf debug_print_always
#endif

#define final_printf(a, args...) printf("(%s:%d) " a, __FILE__, __LINE__, ##args)
#define ffinal_printf debug_print_always

// Takes hardcoded input string 2 to strlen against during compile time.
// startsWith(input_1, "input 2");
#define startsWith(str1, str2) (strncmp(str1, str2, __builtin_strlen(str2)) == 0)
#define startsWithCase(str1, str2) (strncasecmp(str1, str2, __builtin_strlen(str2)) == 0)

// Writes null term to copied string
#define strncpy0(d, s, sz) strncpy(d, s, sz), d[sz - 1] = '\0'

#define _countof(a) sizeof(a) / sizeof(*a)
#define _countof_1(a) (_countof(a) - 1)
