#pragma once

long orbis_syscall(long sysno, ...);
int sys_dynlib_dlsym(int module, const char* name, void* destination);
int sys_dynlib_load_prx(const char* name, int* idret);
int sys_dynlib_unload_prx(const int module);
