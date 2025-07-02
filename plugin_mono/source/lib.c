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

attr_public const char* g_pluginName = "plugin_example";
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

uiTYPEDEF_FUNCTION_PTR(void, PrintTimeTick_Original, void* MonoStr);

static void PrintTimeTick(void* MonoStr)
{
    extern const char* ScePsmMonoStringToUtf8(void* param_1);
    extern void* mono_pe_file_open(const char* f, const void* s);
    extern void* mono_image_open_from_data(const void* data, uint32_t data_len, bool need_copy, uint32_t* status);
    const char* msg = ScePsmMonoStringToUtf8(MonoStr);
    static bool once = false;
    if (msg && !once && strstr(msg, "Shell UI is Ready"))
    {
        Notify("", msg);
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
        mono_free(msg);
        once = true;
    }
    PrintTimeTick_Original.ptr(MonoStr);
}

uiTYPEDEF_FUNCTION_PTR(int, sceSysmoduleLoadModuleInternal_original, const uint32_t m);

static int sceSysmoduleLoadModuleInternal_hook(const uint32_t m)
{
    const int ret = sceSysmoduleLoadModuleInternal_original.ptr(m);
    printf("request 0x%08x ret %d\n", m, ret);
    if (m == 0x80000030 && ret == 0)
    {
        printf("PSM Handle %d\n", ret);
        static const char mod_name[] = "libScePsm.sprx";
        const int m = sceKernelLoadStartModule(mod_name, 0, 0, 0, 0, 0);
        static bool once = false;
        if (!once && m > 0)
        {
            printf("load start %s %d\n", mod_name, m);
            struct OrbisKernelModuleInfo moduleInfo = {0};
            moduleInfo.size = sizeof(moduleInfo);
            int ret2 = sceKernelGetModuleInfo(m, &moduleInfo);
            final_printf("ret 0x%x\n", ret2);
            final_printf("module %d\n", m);
            final_printf("name: %s\n", moduleInfo.name);
            const void* mm_p = moduleInfo.segmentInfo[0].address;
            const uint32_t mm_s = moduleInfo.segmentInfo[0].size;
            PatchInternalCallList((uintptr_t)mm_p, mm_s, "Sce.PlayStation.Core.Runtime.DiagnosticsNative::DebugPrintTimeTick(string)", &PrintTimeTick_Original.addr, (uintptr_t)PrintTimeTick);
            once = true;
        }
    }
    return ret;
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
    const uintptr_t* p = findSymbolPtrInEboot("libSceSysmodule.sprx", "sceSysmoduleLoadModuleInternal");
    if (p)
    {
        sceSysmoduleLoadModuleInternal_original.addr = *p;
    }

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
            }
        }
    }
    const uintptr_t new_func = (uintptr_t)sceSysmoduleLoadModuleInternal_hook;
    printf("findSymbolPtrInEboot 0x%p\n", p);
    printf("p read 0x%lx\n", *p);
    sys_proc_rw(getpid(), (uintptr_t)p, &new_func, sizeof(new_func), 1);
    printf("p read 0x%lx\n", *p);
    uintptr_t sceKernelGetHwSerialNumber = 0;
    sceKernelDlsym(0x2001, "sceKernelGetHwSerialNumber", (void**)&sceKernelGetHwSerialNumber);
    if (sceKernelGetHwSerialNumber)
    {
        WriteJump64(sceKernelGetHwSerialNumber, (uintptr_t)sceKernelGetHwSerialNumber_hook);
    }
    UploadDebugSettingsPatch();
    return 0;
}

