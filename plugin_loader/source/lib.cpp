#include "../../common/entry.h"

#include "../../common/plugin_common.h"
#include <stdint.h>

extern "C"
{
    attr_public const char* g_pluginName = "plugin_loader";
    attr_public const char* g_pluginDesc = "Plugin loader.";
    attr_public const char* g_pluginAuth = "illusiony";
    attr_public const char* g_pluginVersion = "Git Commit: " GIT_COMMIT
                                              "\n"
                                              "Git Branch: " GIT_VER
                                              "\n"
                                              "Git Commit Number: " GIT_NUM_STR
                                              "\n"
                                              "Built: " BUILD_DATE;  // 1.00
    attr_public int plugin_load(SceEntry* args, const void* atexit_handler)
    {
        return 0;
    }
    attr_public int plugin_unload(SceEntry* args, const void* atexit_handler)
    {
        return 0;
    }
}
