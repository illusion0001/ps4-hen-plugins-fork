#include "../../common/entry.h"

#include <stdint.h>
#include <stdio.h>
#include "../../common/notify.h"
#include "../../common/plugin_common.h"
#include "../../common/function_ptr.h"
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <pthread.h>
#include <unistd.h>

#include "memory.h"
#include "mono.h"

#include "patches.h"

#include "hen_settings.inc.c"
#include "../../common/file.h"

attr_public const char* g_pluginName = "plugin_mono";
attr_public const char* g_pluginDesc = "Demonstrate usage of CXX in module. Based from OpenOrbis `library_example`";
attr_public const char* g_pluginAuth = "illusiony";
attr_public const char* g_pluginVersion = "Git Commit: " GIT_COMMIT
                                          "\n"
                                          "Git Branch: " GIT_VER
                                          "\n"
                                          "Git Commit Number: " GIT_NUM_STR
                                          "\n"
                                          "Built: " BUILD_DATE;

static void open_console(void)
{
    extern int open(const char*, int);
    extern int close(int);
    const int O_WRONLY = 2;
    const int stdout_ = 1;
    const int stderr_ = 2;
    const int fd = open("/dev/console", O_WRONLY);
    if (fd > 0)
    {
        dup2(fd, stdout_);
        dup2(fd, stderr_);
        close(fd);
    }
}

void PrintTimeTick(void)
{
    static bool once = false;
    if (!once)
    {
        Root_Domain = mono_get_root_domain();
        if (Root_Domain)
        {
            void* app_exe = mono_get_image("/app0/psm/Application/app.exe");
            if (app_exe)
            {
                App_Exe = app_exe;
                void (*createDevKitPanel)(void*) = (void*)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.TopMenu", "AreaManager", "createDevKitPanel", 0);
                printf("createDevKitPanel 0x%p\n", createDevKitPanel);
                if (createDevKitPanel)
                {
                    createDevKitPanel(Mono_Get_InstanceEx(App_Exe, "Sce.Vsh.ShellUI.TopMenu", "AreaManager", "Instance"));
                }
            }
            UploadNewCorelibStreamReader();
            UploadNewPkgInstallerPath(App_Exe);
            UploadOnBranch(App_Exe);
        }
        once = true;
    }
}

static int sceKernelGetHwSerialNumber_hook(char* buf)
{
    if (buf)
    {
        const int sz = 18;
        memset(buf, 0, sz);
        memset(buf, '0', sz - 1);
    }
    return 0;
}

static void PatchDebugSettings(void)
{
    uintptr_t sceKernelGetUtokenStoreModeForRcmgr = 0;
    uintptr_t sceKernelGetDebugMenuModeForRcmgr = 0;
    sceKernelDlsym(0x2001, "sceKernelGetUtokenStoreModeForRcmgr", (void**)&sceKernelGetUtokenStoreModeForRcmgr);
    sceKernelDlsym(0x2001, "sceKernelGetDebugMenuModeForRcmgr", (void**)&sceKernelGetDebugMenuModeForRcmgr);
    static const uint8_t m[] = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3};
    const int pid = getpid();
    if (sceKernelGetUtokenStoreModeForRcmgr)
    {
        sys_proc_rw(pid, sceKernelGetUtokenStoreModeForRcmgr, m, sizeof(m), 1);
    }
    if (sceKernelGetDebugMenuModeForRcmgr)
    {
        sys_proc_rw(pid, sceKernelGetDebugMenuModeForRcmgr, m, sizeof(m), 1);
    }
}

attr_public int plugin_load(struct SceEntry* args)
{
    mkdir(BASE_PATH, 0777);
    mkdir(SHELLUI_DATA_PATH, 0777);
    write_file(SHELLUI_HEN_SETTINGS, data_hen_settings_xml, data_hen_settings_xml_len);
    open_console();
    printf("====\n\nHello from mono module\n\n====\n");
    if (file_exists_temp(g_pluginName) == 0)
    {
        Notify("", "Attempted to load %s again! Did it crash previously?\n", g_pluginName);
        return 0;
    }
    touch_temp(g_pluginName);
    puts(g_pluginVersion);
    struct OrbisKernelModuleInfo info = {0};
    info.size = sizeof(info);
    const int r = sceKernelGetModuleInfo(0, &info);
    if (r == 0)
    {
        UploadEventProxyCall(&info);
        int app_exe = sceKernelLoadStartModule("/app0/psm/Application/app.exe.sprx", 0, 0, 0, 0, 0);
        printf("app_exe 0x%x\n", app_exe);
        if (app_exe > 0)
        {
            struct OrbisKernelModuleInfo info_app = {0};
            info_app.size = sizeof(info_app);
            const int r = sceKernelGetModuleInfo(app_exe, &info_app);
            if (r == 0)
            {
                UploadRegStrCall(&info, &info_app);
                UploadRemotePlayPatch(&info_app);
                UploadShellUICheck();
                UploadBgftFixup(&info);
                UploadFinishBootEffectCode(&info_app);
            }
        }
    }
    uintptr_t sceKernelGetHwSerialNumber = 0;
    sceKernelDlsym(0x2001, "sceKernelGetHwSerialNumber", (void**)&sceKernelGetHwSerialNumber);
    if (sceKernelGetHwSerialNumber)
    {
        WriteJump64(sceKernelGetHwSerialNumber, (uintptr_t)sceKernelGetHwSerialNumber_hook);
    }
    UploadDebugSettingsPatch();
    return 0;
}

