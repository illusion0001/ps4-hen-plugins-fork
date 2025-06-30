#include "../../common/entry.h"

#include "../../common/plugin_common.h"
#include <stdint.h>

attr_public const char* g_pluginName = "plugin_loader";
attr_public const char* g_pluginDesc = "Plugin loader.";
attr_public const char* g_pluginAuth = "illusiony";
attr_public uint32_t g_pluginVersion = 0x00000100;  // 1.00

extern "C" attr_public int plugin_load(SceEntry* args, const void* atexit_handler)
{
    return 0;
}
