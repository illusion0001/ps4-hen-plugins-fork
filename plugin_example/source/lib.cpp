#include <stdio.h>
#include <string.h>
#include <iostream>

#include <orbis/libkernel.h>

#include "../../common/plugin_common.h"
#include "../../common/notify.h"

attr_public const char* g_pluginName = "plugin_example";
attr_public const char* g_pluginDesc = "Demonstrate usage of CXX in module. Based from OpenOrbis `library_example`";
attr_public const char* g_pluginAuth = "illusiony";
attr_public uint32_t g_pluginVersion = 0x00000100;  // 1.00

extern "C" int plugin_load(int* argc, const char** argv)
{
    // https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/63c0be5ffff09fbaebebc6b9a738d150e2da0205/samples/library_example/library_example/lib.cpp
    // Just a copy of example for now
    char str[128] = {0};
    snprintf(str, sizeof(str),
             "Hi I'm from the library!\n"
             "You passed: %d args\n"
             "Built: " __TIME__ " " __DATE__,
             argc ? *argc : -1);
    std::string stdstr = str;
    printf("%s\n", str);
    printf("std::string %s\n", stdstr.c_str());
    sceKernelDebugOutText(0, str);
    sceKernelDebugOutText(0, "\r\n");
    Notify(TEX_ICON_SYSTEM, stdstr.c_str());
    return 0;
}
