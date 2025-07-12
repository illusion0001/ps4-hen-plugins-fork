#include "git_ver.h"

#define MAX_PATH_ 260

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

#if (__FINAL__) == 1
#define BUILD_TYPE "(Release)"
#define debug_printf(a, args...)
#else
#define BUILD_TYPE "(Debug)"
#define debug_printf(a, args...) printf("[%s] (%s:%d) " a, __func__, __FILE__, __LINE__, ##args)
#endif

#define final_printf(a, args...) printf("(%s:%d) " a, __FILE__, __LINE__, ##args)

// Takes hardcoded input string 2 to strlen against during compile time.
// startsWith(input_1, "input 2");
#define startsWith(str1, str2) (strncmp(str1, str2, __builtin_strlen(str2)) == 0)

// Writes null term to copied string
#define strncpy0(d, s, sz) strncpy(d, s, sz), d[sz - 1] = '\0'
