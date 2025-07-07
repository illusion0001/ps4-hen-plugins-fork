#include "syscall.h"
#include <stdarg.h>

// clang compatible implement of syscall
// https://github.com/john-tornblom/tiny-ps4-shell/blob/5ca90b7d2825e3e2c7f9fb1e81cf8949b37af563/kern_orbis.c#L39

extern int errno;

static long __syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
    unsigned long ret;
    char iserror;

    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    register long r9 __asm__("r9") = a6;

    __asm__ __volatile__(
        "syscall" : "=a"(ret), "=@ccc"(iserror), "+r"(r10), "+r"(r8), "+r"(r9) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");

    return iserror ? -ret : ret;
}

long orbis_syscall(long sysno, ...)
{
    int err;
    va_list args;
    long arg0, arg1, arg2, arg3, arg4, arg5;

    va_start(args, sysno);
    arg0 = va_arg(args, long);
    arg1 = va_arg(args, long);
    arg2 = va_arg(args, long);
    arg3 = va_arg(args, long);
    arg4 = va_arg(args, long);
    arg5 = va_arg(args, long);
    va_end(args);

    err = __syscall6(sysno, arg0, arg1, arg2, arg3, arg4, arg5);
    if (err < 0)
    {
        errno = -err;
        return -1;
    }

    return err;
}

int sys_dynlib_dlsym(const int module, const char* name, void* destination)
{
    return orbis_syscall(591, module, name, destination);
}

int sys_dynlib_load_prx(const char* name, int* idret)
{
    return orbis_syscall(594, name, 0, idret, 0);
}

int sys_dynlib_unload_prx(const int module)
{
    return orbis_syscall(595, module);
}
