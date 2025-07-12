#include "../../common/entry.h"
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "../../common/plugin_common.h"
#include "../../common/notify.h"
#include "../../common/function_ptr.h"
#include "../../common/file.h"
#include "appinfo.h"
#include "local_appinfo.h"
#include "memory.h"

attr_public const char* g_pluginName = "plugin_shellcore";
attr_public const char* g_pluginDesc = "Component of game patch plugin.";
attr_public const char* g_pluginAuth = "illusiony";
attr_public const char* g_pluginVersion = "Git Commit: " GIT_COMMIT
                                          "\n"
                                          "Git Branch: " GIT_VER
                                          "\n"
                                          "Git Commit Number: " GIT_NUM_STR
                                          "\n"
                                          "Built: " BUILD_DATE;

uiTYPEDEF_FUNCTION_PTR(int, chmodRootDirForBug86309_Original, const void* param_1, const void* param_2, const int pid);

static int chmodRootDirForBug86309_Hook(const void* param_1, const void* param_2, const int app_pid, struct appinfo* my_param_4)
{
    if (0)
    {
        Notify("",
               "pid %d\n"
               "m_titleid %s\n"
               "m_category %s\n"
               "m_appver %s\n",
               app_pid,
               my_param_4->m_titleid,
               my_param_4->m_category,
               my_param_4->m_appver);
    }
    struct disk_appinfo diskinfo = {0};
    diskinfo.pid = app_pid;
    strncpy0(diskinfo.m_titleid, my_param_4->m_titleid, sizeof(diskinfo.m_titleid));
    strncpy0(diskinfo.m_category, my_param_4->m_category, sizeof(diskinfo.m_category));
    strncpy0(diskinfo.m_appver, my_param_4->m_appver, sizeof(diskinfo.m_appver));
    char filep[260] = {0};
    snprintf(filep, _countof(filep) - 1, "/user" TEMP_INFO_PATH "/%s/" TEMP_INFO_FILE, my_param_4->m_titleid);
    write_file(filep, &diskinfo, sizeof(diskinfo));
    return chmodRootDirForBug86309_Original.ptr(param_1, param_2, app_pid);
}

static int __attribute__((naked)) chmodRootDirForBug86309(const void* param_1, const void* param_2, const int pid)
{
    __asm
    {
        mov rcx,rax;
        jmp chmodRootDirForBug86309_Hook;
    }
}

static int RunPost(const void* mp, const uint64_t ms)
{
    // Unpatch ourselves by invaliding pattern match on next frame.
    const uintptr_t caller = PatternScan(mp, ms, "e8 ? ? ? ? 48 89 df e8 ? ? ? ? 48 89 df e8 ? ? ? ? e8 ? ? ? ? 31 c0", 21);
    if (caller)
    {
        sys_proc_memset(getpid(), caller, 0x90, 5);
        return 0;
    }
    return 1;
}

static void RunPatch(void)
{
    struct OrbisKernelModuleInfo info = {0};
    info.size = sizeof(info);
    const int r = sceKernelGetModuleInfo(0, &info);
    if (r == 0)
    {
        const void* mm_p = info.segmentInfo[0].address;
        const uint64_t mm_s = info.segmentInfo[0].size;
        // checked only on 9.00
        uintptr_t chmodEntry = PatternScan(mm_p, mm_s, "8b b0 88 00 00 00 44 89 f2 e8 ? ? ? ?", 9);
        if (!chmodEntry)
        {
            // checked only on 5.00
            chmodEntry = PatternScan(mm_p, mm_s, "8b b0 88 00 00 00 e8 ? ? ? ?", 6);
        }
        if (chmodEntry)
        {
            Make32to64Jmp((uintptr_t)mm_p, mm_s, chmodEntry, (uintptr_t)chmodRootDirForBug86309, 5, true, &chmodRootDirForBug86309_Original.addr);
            if (RunPost(mm_p, mm_s) == 0)
            {
                Notify("", "Shellcore code installed for application info");
            }
        }
        else
        {
            Notify("",
                   "Can't find chmodRootDirForBug86309 pattern!\n"
                   "Game info will not be available to plugins.\n");
        }
    }
}

attr_public int plugin_load(struct SceEntry* args)
{
    static int once = 0;
    if (!once)
    {
        RunPatch();
        once = 1;
    }
    return 0;
}
