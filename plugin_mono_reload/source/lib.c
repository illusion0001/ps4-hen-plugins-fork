#include "../../common/entry.h"

#include <stdint.h>
#include <stdio.h>
#include "../../common/memory.h"
#include "../../common/notify.h"
#include "../../common/plugin_common.h"

// #include "../../common/HDE/HDE64.h"
#include "../../common/function_ptr.h"

#include <unistd.h>

#include "mono.h"

attr_public const char* g_pluginName = "plugin_example";
attr_public const char* g_pluginDesc = "Demonstrate usage of CXX in module. Based from OpenOrbis `library_example`";
attr_public const char* g_pluginAuth = "illusiony";
attr_public uint32_t g_pluginVersion = 0x00000100;  // 1.00

static void* app_exe = 0;

attr_public int plugin_load(struct SceEntry* args)
{

    Notify("", "Notify 0x%lx\n", (uintptr_t)sceKernelSendNotificationRequest);
    return 0;
    Root_Domain = mono_get_root_domain();
    int app_exe_h = sceKernelLoadStartModule("/app0/psm/Application/app.exe.sprx", 0, 0, 0, 0, 0);
    printf("app_exe 0x%x\n", app_exe_h);
    struct OrbisKernelModuleInfo info_app = {0};
    info_app.size = sizeof(info_app);
    const int r = sceKernelGetModuleInfo(app_exe_h, &info_app);
    app_exe = mono_get_image("/app0/psm/Application/app.exe");
    const uintptr_t start = (uintptr_t)info_app.segmentInfo[0].address;
    // System.Boolean Sce.Vsh.ShellUI.Settings.PkgInstaller.PageExecute::IsEnableMultiInstall()
    // System.Void Sce.Vsh.ShellUI.Settings.PkgInstaller.SearchJob::SearchDir(System.String, System.String)
    uintptr_t IsEnableMultiInstall = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.PkgInstaller", "PageExecute", "IsEnableMultiInstall", 0);
    Notify("", "IsEnableMultiInstall 0x%lx 0x%lx\n", IsEnableMultiInstall, IsEnableMultiInstall - start);
    uintptr_t SearchDir = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.PkgInstaller", "SearchJob", "SearchDir", 2);
    Notify("", "SearchDir 0x%lx 0x%lx\n", SearchDir, SearchDir - start);
    uintptr_t OnPress = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.SettingsRoot", "SettingsRootHandler", "OnPress", 2);
    Notify("", "OnPress 0x%lx 0x%lx\n", OnPress, OnPress - start);
    uintptr_t OnPreCreate = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.SettingsRoot", "SettingsRootHandler", "OnPreCreate", 2 );
    Notify("", "OnPreCreate 0x%lx 0x%lx\n", OnPreCreate, OnPreCreate - start);
    int bgft = sceKernelLoadStartModule("libSceBgft.sprx", 0, 0, 0, 0, 0);
    uintptr_t sceBgftServiceIntDownloadRegisterTaskByStorageEx = 0;
    sceKernelDlsym(bgft, "sceBgftServiceIntDownloadRegisterTaskByStorageEx", (void**)&sceBgftServiceIntDownloadRegisterTaskByStorageEx);
    Notify("", "bgft 0x%lx sceBgftServiceIntDownloadRegisterTaskByStorageEx 0x%lx\n", bgft, sceBgftServiceIntDownloadRegisterTaskByStorageEx);

    {
        const char* sandbox_path = sceKernelGetFsSandboxRandomWord();
        char bgft_dll[260] = {};
        snprintf(bgft_dll, sizeof(bgft_dll), "/%s/common/lib/Sce.Vsh.Orbis.Bgft.dll", sandbox_path);
        void* bgft_img = mono_get_image(bgft_dll);
        if (bgft_img)
        {
            uintptr_t BgftWrapper = (uintptr_t)Mono_Get_Address_of_Method(bgft_img, "Sce.Vsh.Orbis.Bgft", "BgftWrapper", "DownloadRegisterTaskByStorageEx", 2);
            Notify("", "BgftWrapper 0x%lx 0x%lx\n", BgftWrapper, BgftWrapper);
        }
        // System.Int32 Sce.Vsh.Orbis.Bgft.BgftWrapper::DownloadRegisterTaskByStorageEx(Sce.Vsh.Orbis.Bgft.DownloadParamEx&,System.Int32&)
        //
    }

    // mount_data();
    return 0;
#if 0
    // CaveBlockInit(cavePad, sizeof(cavePad));
    // Notify("", "Unregister 0x%llx\n", 0);
    // Root_Domain = mono_get_root_domain();
    // test();
    // sceKernelGetUtokenStoreModeForRcmgr
    // sceKernelGetDebugMenuModeForRcmgr
    void* sceKernelGetUtokenStoreModeForRcmgr = 0;
    void* sceKernelGetDebugMenuModeForRcmgr = 0;
    sceKernelDlsym(0x2001, "sceKernelGetUtokenStoreModeForRcmgr", &sceKernelGetUtokenStoreModeForRcmgr);
    sceKernelDlsym(0x2001, "sceKernelGetDebugMenuModeForRcmgr", &sceKernelGetDebugMenuModeForRcmgr);
    printf("sceKernelGetUtokenStoreModeForRcmgr: 0x%p\n", sceKernelGetUtokenStoreModeForRcmgr);
    printf("sceKernelGetDebugMenuModeForRcmgr: 0x%p\n", sceKernelGetDebugMenuModeForRcmgr);
    return 0;
    int app_exe_h = sceKernelLoadStartModule("/app0/psm/Application/app.exe.sprx", 0, 0, 0, 0, 0);
    printf("app_exe 0x%x\n", app_exe_h);
    struct OrbisKernelModuleInfo info_app = {0};
    info_app.size = sizeof(info_app);
    const int r = sceKernelGetModuleInfo(app_exe_h, &info_app);
    app_exe = mono_get_image("/app0/psm/Application/app.exe");
    const uintptr_t start = (uintptr_t)info_app.segmentInfo[0].address;
    uintptr_t reg = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.AppSystem", "PluginManager", "Unregister", 1);
    Notify("", "Unregister 0x%llx\n", reg - start);
    // Register
    uintptr_t unreg = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.AppSystem", "PluginManager", "Register", 1);
    Notify("", "Register 0x%llx\n", unreg - start);
    // Sce.PlayStation.PUI.UI2.Panel Sce.Vsh.ShellUI.TopMenu.SystemAreaManager::GetIconPanel(System.String)
    // System.Void Sce.Vsh.ShellUI.TopMenu.AreaManager::appendDevKitLabel(System.String)
    {
    }
    return 0;
    #endif
}
