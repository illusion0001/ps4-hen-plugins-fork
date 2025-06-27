#include <stdint.h>
#include "../../../common/plugin_common.h"
#include "../../../common/stringid.h"
#include "../../../common/function_ptr.h"
#include "../memory.h"
#include <orbis/libkernel.h>
#include "../mono.h"

#include <pthread.h>

void UploadDebugSettingsPatch(void)
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

uiTYPEDEF_FUNCTION_PTR(void, PkgInstallerSearchDir_Original, void* _this, MonoString* str, MonoString* str2);

bool g_only_hdd = false;

static void PkgInstallerSearchDir(void* _this, MonoString* str, MonoString* str2)
{
    const uint64_t disc_sid = 0xE992CF1CBC039673; // wSID(L"/disc");
    if (wSID(str->str) == disc_sid && g_only_hdd)
    {
        // search local hdd path
        PkgInstallerSearchDir_Original.ptr(_this, Mono_New_String("/../data/pkg"), str2);
        return;
    }
    if (g_only_hdd)
    {
        return;
    }
    // search original disc, usb paths
    PkgInstallerSearchDir_Original.ptr(_this, str, str2);
}

uiTYPEDEF_FUNCTION_PTR(int, _DownloadRegisterTaskByStorageEx_Original, void* bgft_params, int32_t* refout);

static int _DownloadRegisterTaskByStorageEx(void* bgft_params, int32_t* refout)
{
    struct bgft_mono
    {
        void* pad[2]; // user id and id str
        MonoString* content_url;
    };
    struct bgft_mono* bgft_params_u64 = (struct bgft_mono*)bgft_params;
    final_printf("path %ls\n", bgft_params_u64->content_url->str);
    {
        const char* curl = mono_string_to_utf8(bgft_params_u64->content_url);
        static const char mnt_sus[] = "/mnt/..";
        if (strstr(curl, mnt_sus))
        {
            final_printf("before fixup %ls\n", bgft_params_u64->content_url->str);
            bgft_params_u64->content_url = Mono_New_String(curl + (sizeof(mnt_sus) - 1));
            final_printf("after fixup %ls\n", bgft_params_u64->content_url->str);
        }
    }
    return _DownloadRegisterTaskByStorageEx_Original.ptr(bgft_params, refout);
}

static void* BgftFixupThread(void* arg)
{
    final_printf("[%s] enter\n", __FUNCTION__);
    int shellcore = findProcess("SceShellCore");
    uintptr_t shellcore_start = 0;
    uint64_t shellcore_sz = 0;
    if (get_code_info(shellcore, 0, &shellcore_start, &shellcore_sz, 0) == 0)
    {
        const uintptr_t addr = pid_chunk_scan(shellcore,
                                              shellcore_start,
                                              shellcore_sz,
                                              "48 8D 7D ? "
                                              "E8 ? ? ? ? "
                                              "84 C0 "
                                              "75 ? "
                                              "48 8D 7D ? "
                                              "E8 ? ? ? ? "
                                              "84 C0 "
                                              "75 ? "
                                              "48 8D 7D ? "
                                              "E8 ? ? ? ? "
                                              "84 C0",
                                              4);
        if (addr)
        {
            static const uint8_t m[] = {0xB8, 0x01, 0x00, 0x00, 0x00};
            sys_proc_rw(shellcore, addr, m, sizeof(m), 1);
            touch_temp((const char*)arg);
        }
    }
    final_printf("[%s] finish\n", __FUNCTION__);
    pthread_exit(0);
    return 0;
}

void UploadBgftFixup(const struct OrbisKernelModuleInfo* info)
{
    if (file_exists_temp(__FUNCTION__) == 0)
    {
        final_printf("[%s] already patched\n", __FUNCTION__);
        return;
    }
    pthread_t pt;
    pthread_create(&pt, 0, BgftFixupThread, (void*)__FUNCTION__);
    const void* mm_p = info->segmentInfo[0].address;
    const uint32_t mm_s = info->segmentInfo[0].size;
    PatchInternalCallList((uintptr_t)mm_p, mm_s, "Sce.Vsh.Orbis.Bgft.BgftWrapper::_DownloadRegisterTaskByStorageEx", &_DownloadRegisterTaskByStorageEx_Original.addr, (uintptr_t)_DownloadRegisterTaskByStorageEx);
}

void UploadNewPkgInstallerPath(void* app_exe)
{
    const uintptr_t SearchDir = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.PkgInstaller", "SearchJob", "SearchDir", 2);
    if (SearchDir)
    {
        final_printf("SeachDir 0x%lx\n", SearchDir);
        const uintptr_t pHook = CreatePrologueHook(cavePad, sizeof(cavePad), SearchDir, 15);
        if (pHook)
        {
            PkgInstallerSearchDir_Original.addr = pHook;
            final_printf("PkgInstallerSearchDir_Original.addr 0x%lx\n", PkgInstallerSearchDir_Original.addr);
            WriteJump64(SearchDir, (uintptr_t)PkgInstallerSearchDir);
        }
    }
}

static void* ShellUIDebugFixupThread(void* arg)
{
    final_printf("[%s] enter\n", __FUNCTION__);
    int shellcore = findProcess("SceShellCore");
    uintptr_t shellcore_start = 0;
    uint64_t shellcore_sz = 0;
    if (get_code_info(shellcore, 0, &shellcore_start, &shellcore_sz, 0) == 0)
    {
        const uintptr_t addr = pid_chunk_scan(shellcore, shellcore_start, shellcore_sz, "e8 ? ? ? ? 83 f8 01 0f 84 ? ? ? ? 4d 8b 27 4c 89 e7 e8 ? ? ? ?", 0);
        if (addr)
        {
            static const uint8_t m[] = {0xB8, 0x01, 0x00, 0x00, 0x00};
            sys_proc_rw(shellcore, addr, m, sizeof(m), 1);
            touch_temp((const char*)arg);
        }
    }
    final_printf("[%s] finish\n", __FUNCTION__);
    pthread_exit(0);
    return 0;
}

// Allows debugging of ShellUI without ShellCore killing it after n seconds
void UploadShellUICheck(void)
{
    return;
    if (file_exists_temp(__FUNCTION__) == 0)
    {
        final_printf("[%s] already patched\n", __FUNCTION__);
        return;
    }
    pthread_t pt;
    pthread_create(&pt, 0, ShellUIDebugFixupThread, (void*)__FUNCTION__);
}
