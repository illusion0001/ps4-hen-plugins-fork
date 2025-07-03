#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "entry.h"
#include "plugin_common.h"
#include "notify.h"
#include "path.h"

attr_public const char* g_pluginName = "bootloader";
attr_public const char* g_pluginDesc = "Bootloader plugin.";
attr_public const char* g_pluginAuth = "illusiony";
attr_public const char* g_pluginVersion = "Git Commit: " GIT_COMMIT
                                          "\n"
                                          "Git Branch: " GIT_VER
                                          "\n"
                                          "Git Commit Number: " GIT_NUM_STR
                                          "\n"
                                          "Built: " BUILD_DATE;

int32_t attr_public plugin_load(struct SceEntry* args, const void* atexit_handler)
{
    final_printf("%s Plugin Started.\n", g_pluginName);
    final_printf("%s\n", g_pluginVersion);
    final_printf("Plugin Author(s): %s\n", g_pluginAuth);
    // `sceKernelLoadStartModule` will do all the crt startup work
    // but module_start doesn't do anything there so resolve the `plugin_load` symbol and start it
    static const char loader_path[] = PRX_LOADER_PATH;
    const int m = sceKernelLoadStartModule(loader_path, 0, 0, 0, 0, 0);
    if (m > 0)
    {
        int32_t (*load)(struct SceEntry*) = NULL;
        static const char loader_sym[] = "plugin_load";
        sceKernelDlsym(m, loader_sym, (void**)&load);
        int32_t (*unload)(struct SceEntry*) = NULL;
        static const char unload_sym[] = "plugin_unload";
        sceKernelDlsym(m, unload_sym, (void**)&unload);
        if (load)
        {
            if (load(args) && unload)
            {
                unload(args);
                const int unload = sceKernelStopUnloadModule(m, 0, 0, 0, 0, 0);
                final_printf("Unload result 0x%08x\n", unload);
            }
        }
        else
        {
            Notify(TEX_ICON_SYSTEM,
                   "Failed to resolve\n"
                   "%s\n"
                   "from module\n"
                   "%s\n",
                   loader_sym,
                   loader_path);
        }
    }
    else
    {
        Notify(TEX_ICON_SYSTEM,
               "Failed to load loader module\n"
               "%s\n"
               "return code 0x%08x\n",
               loader_path,
               m);
    }
    return 0;
}
