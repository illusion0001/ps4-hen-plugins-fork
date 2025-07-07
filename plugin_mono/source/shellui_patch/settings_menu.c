extern "C"
{
#include "settings_menu.h"
#include "../../../common/file.h"
#include "../../../common/function_ptr.h"
#include "../../../common/memory.h"
#include "../../../common/notify.h"
#include "../../../common/plugin_common.h"
#include "../../../common/stringid.h"
#include <stdio.h>

#include <dirent.h>
#include "../mono.h"

#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <signal.h>
#include <pthread.h>

#include "../../../common/syscall.h"

#include "../ini.h"
#include "../shellui_mono.h"
}

typedef enum
{
    REPLACE_MATCH,
    ADD_BEFORE,
    ADD_AFTER,
    REPLACE_LINE
} ReplaceMode;

static void str_replace(const char* input, const char* search, const char* replacement, char* output, size_t output_size, ReplaceMode mode)
{
    const char* pos = input;
    size_t search_len = strlen(search);
    size_t replacement_len = strlen(replacement);
    size_t out_len = 0;

    while (*pos && out_len < output_size - 1)
    {
        if (strncmp(pos, search, search_len) == 0)
        {
            switch (mode)
            {
                case REPLACE_MATCH:
                    if (out_len + replacement_len >= output_size - 1)
                    {
                        goto done;
                    }
                    memcpy(output + out_len, replacement, replacement_len);
                    out_len += replacement_len;
                    pos += search_len;
                    break;

                case ADD_BEFORE:
                    if (out_len + replacement_len + search_len >= output_size - 1)
                    {
                        goto done;
                    }
                    memcpy(output + out_len, replacement, replacement_len);
                    out_len += replacement_len;
                    memcpy(output + out_len, pos, search_len);
                    out_len += search_len;
                    pos += search_len;
                    break;

                case ADD_AFTER:
                    if (out_len + search_len + replacement_len >= output_size - 1)
                    {
                        goto done;
                    }
                    memcpy(output + out_len, pos, search_len);
                    out_len += search_len;
                    memcpy(output + out_len, replacement, replacement_len);
                    out_len += replacement_len;
                    pos += search_len;
                    break;

                case REPLACE_LINE:
                {
                    // Find start of line
                    const char* line_start = pos;
                    while (line_start > input && *(line_start - 1) != '\n')
                    {
                        line_start--;
                    }
                    // Find end of line
                    const char* line_end = pos;
                    while (*line_end && *line_end != '\n')
                    {
                        line_end++;
                    }
                    if (out_len + replacement_len + 1 >= output_size - 1)
                    {
                        goto done;
                    }
                    memcpy(output + out_len, replacement, replacement_len);
                    out_len += replacement_len;
                    if (*line_end == '\n')
                    {
                        output[out_len++] = '\n';
                        pos = line_end + 1;
                    }
                    else
                    {
                        pos = line_end;
                    }
                    break;
                }

                default:
                    // Unknown mode, just copy as normal
                    output[out_len++] = *pos++;
                    break;
            }
        }
        else
        {
            output[out_len++] = *pos++;
        }
    }

done:
    output[out_len] = '\0';
}

#define MAX_BUF 8 * 1024
static char buf_fixed[MAX_BUF] = {};
static size_t buflen = 0;

static void SetupSettingsRoot(const char* xml)
{
    char buf[MAX_BUF] = {};
    char buf2[MAX_BUF] = {};
#undef MAX_BUF
    memset(buf_fixed, 0, sizeof(buf_fixed));
    str_replace(xml,
                "\t\t<link id=\"sandbox\" title=\"Sandbox\" file=\"Sandbox/sandbox.xml\" />\r\n",
                "\t\t<link id=\"hen_settings\" title=\"★ HEN Settings\" file=\"hen_settings.xml\" />\r\n",
                buf,
                _countof(buf),
                ADD_BEFORE);
    strncat(buf2, buf, strlen(buf));
    str_replace(buf,
                "initial_focus_to=\"psn\">\r\n",
                "initial_focus_to=\"hen_settings\">\r\n",
                buf2,
                _countof(buf2),
                REPLACE_LINE);
    // printf("New str:\n%s\n", buf2);
    mono_free(xml);
    memset(buf, 0, sizeof(buf));
    static const uint8_t bom_header[] = {0xEF, 0xBB, 0xBF};
    memcpy(buf, bom_header, sizeof(bom_header));
    strncat(buf, buf2, strlen(buf2));
    strncpy(buf_fixed, buf, strlen(buf));
    buflen = strlen(buf_fixed);
}

static bool g_PluginsListRun = false;
static pthread_t g_PluginsRunThread = 0;
static uint64_t g_PageID = 0;
static char g_PluginsFoundMsg[512]{};

static void UI_RequestReset(void)
{
    UI_ResetItem(ID_WAIT);
    ffinal_printf("sent reset!\n");
}

static void RunPluginsBuild(void)
{
    struct dirent* entry;
    DIR* dp = opendir(USER_PLUGIN_PATH);
    uint64_t nPrx = 0;
    memset(g_PluginsFoundMsg, 0, sizeof(g_PluginsFoundMsg));
    if (!dp)
    {
        perror("opendir");
        strncpy0(g_PluginsFoundMsg, "Unable to open `" USER_PLUGIN_PATH "`.", _countof(g_PluginsFoundMsg));
        goto reset;
    }
    while ((entry = readdir(dp)))
    {
        const char* filename = entry->d_name;
        if (endsWith(filename, ".prx") || endsWith(filename, ".sprx"))
        {
            char prx_path[260] = {0};
            snprintf(prx_path, _countof(prx_path) - 1, USER_PLUGIN_PATH "/%s", filename);
            int mod = 0;
            // Don't EVER trust user modules that they won't put malicious code in `module_start`
            // sceKernelLoadStartModule will always run `module_start` and even CRT startup sequences.
            // Using syscall should be safest way to just get the info and close it.
            int ret = sys_dynlib_load_prx(prx_path, &mod);
            if (ret == 0 && mod > 0)
            {
                const char* pluginName = 0;
                const char* pluginDesc = 0;
                const char* pluginAuth = 0;
                const char* pluginVersion = 0;
                sys_dynlib_dlsym(mod, "g_pluginName", &pluginName);
                sys_dynlib_dlsym(mod, "g_pluginDesc", &pluginDesc);
                sys_dynlib_dlsym(mod, "g_pluginAuth", &pluginAuth);
                sys_dynlib_dlsym(mod, "g_pluginVersion", &pluginVersion);
                if (!pluginName || !pluginDesc || !pluginAuth || !pluginVersion)
                {
                    sys_dynlib_unload_prx(mod);
                    continue;
                }
                pluginName = *(const char**)pluginName;
                pluginDesc = *(const char**)pluginDesc;
                pluginAuth = *(const char**)pluginAuth;
                pluginVersion = *(const char**)pluginVersion;
                char desc[4096] = {0};
                snprintf(desc, _countof_1(desc), "Path: %s\nAuthor: %s\nDescription: %s", prx_path, pluginAuth, pluginDesc);
                void* item = UI_NewElementData(ToggleSwitch, prx_path, pluginName, desc, 0);
                if (item)
                {
                    UI_AddItem(item);
                    nPrx++;
                }
                sys_dynlib_unload_prx(mod);
            }
        }
    }
    if (nPrx == 0)
    {
        strncpy0(g_PluginsFoundMsg, "No plugins found.", _countof(g_PluginsFoundMsg));
    }
    closedir(dp);
reset:;
    UI_RequestReset();
}

static void* test_thread(void* a)
{
    mono_thread_attach(Root_Domain);
    g_PluginsListRun = true;
    (void)a;
    RunPluginsBuild();
    g_PluginsRunThread = g_PluginsListRun = false;
    pthread_exit(0);
    return 0;
}

static void BuildPluginsList2(void)
{
    RunPluginsBuild();
    return;
    // this isn't really reliable
    // so i'm forced to do it in OnPageActiviate thread
    g_PluginsRunThread = g_PluginsListRun = 0;
    pthread_create(&g_PluginsRunThread, 0, test_thread, 0);
}

uiTYPEDEF_FUNCTION_PTR(void*, ReadResourceStream_Original, void* inst, void* string);

static void* ReadResourceStream(void* inst, MonoString* filestring)
{
    printf("%s (%ls)\n", __FUNCTION__, filestring->str);
    {
        static void* mscorlib_ptr = 0;
        if (!mscorlib_ptr)
        {
            mscorlib_ptr = GetMsCorlib();
        }
        const uint64_t s = wSID(filestring->str);
        printf("SID: 0x%lx\n", s);
        switch (s)
        {
            case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.SettingsRoot.data.settings_root.xml"):
            {
                void* stream = ReadResourceStream_Original.ptr(inst, filestring);
                // must be utf8 with bom, parser doesn't like utf16
                const char* xml = Mono_Read_Stream(Root_Domain, mscorlib_ptr, stream);
                if (xml)
                {
                    if (buf_fixed[0] != '\xEF')
                    {
                        SetupSettingsRoot(xml);
                    }
                    return Mono_New_Stream(mscorlib_ptr, buf_fixed, buflen);
                }
                break;
            }
            case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.hen_settings.xml"):
            {
                return Mono_File_Stream(mscorlib_ptr, SHELLUI_HEN_SETTINGS);
            }
            case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.external_hdd.xml"):
            {
                return Mono_File_Stream(mscorlib_ptr, SHELLUI_DATA_PATH "/external_hdd.xml");
            }
            case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.PkgInstaller.data.pkginstaller.xml"):
            case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.PkgInstaller.data.pkginstaller_usb.xml"):
            case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.PkgInstaller.data.pkginstaller_hdd.xml"):
            {
                extern bool g_only_hdd;
                constexpr uint64_t tmp = SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.PkgInstaller.data.pkginstaller_hdd.xml");
                g_only_hdd = s == tmp;
                return ReadResourceStream_Original.ptr(inst, Mono_New_String("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.PkgInstaller.data.pkginstaller.xml"));
            }
            case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.HEN.data.PluginManager.xml"):
            {
                // clang-format off
                static const char xml_start[] =
                    "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
                    "<system_settings version=\"1.0\" plugin=\"settings_root_plugin\">\n"
                    "  <setting_list id=\"id_plugin_manager\" title=\"Plugins Manager\">\n"
                    "  <message id=\"" ID_WAIT "\" title=\"msg_wait\" busyindicator=\"true\"/>\n"
                    "  </setting_list>\n"
                    "</system_settings>\n";
                // clang-format on
                return Mono_New_Stream(mscorlib_ptr, xml_start, sizeof(xml_start) - 1);
            }
        }
    }
    return ReadResourceStream_Original.ptr(inst, filestring);
}

static void UploadResourceStreamBranch(void)
{
    {
        void* mscorlib_ptr = GetMsCorlib();
        if (mscorlib_ptr)
        {
            static const char ns[] = "Assembly";
            static const char mm[] = "GetManifestResourceStream";
            const uintptr_t ctor = (uintptr_t)Mono_Get_Address_of_Method(mscorlib_ptr, "System.Reflection", ns, mm, 1);
            if (ctor)
            {
                const uintptr_t pHook = CreatePrologueHook(ctor, 17);
                if (pHook)
                {
                    ReadResourceStream_Original.addr = pHook;
                    WriteJump64(ctor, (uintptr_t)ReadResourceStream);
                    final_printf("pHook 0x%lx\n", pHook);
                    final_printf("ctor 0x%lx\n", ctor);
                }
            }
            else
            {
                Notify("",
                       "Failed to resolve %s.%s!\n"
                       "Settings menu will not be patched.\n",
                       ns,
                       mm);
            }
        }
    }
}

uiTYPEDEF_FUNCTION_PTR(void, OnPress_Original, const void* p1, const void* p2, const void* p3);
uiTYPEDEF_FUNCTION_PTR(void, OnPreCreate_Original, const void* p1, const void* p2, const void* p3);
uiTYPEDEF_FUNCTION_PTR(void, OnPageActivating_Original, const void* p1, const void* p2, const void* p3);
uiTYPEDEF_FUNCTION_PTR(void, OnCheckVisible_Original, const void* p1, const void* p2, const void* p3);
uiTYPEDEF_FUNCTION_PTR(void, OnConfirm_Original, const void* p1, const void* p2);
uiTYPEDEF_FUNCTION_PTR(void, OnPressed_Original, const void* p1, const void* p2);

static bool IsValidElementType(MonoObject* Type)
{
    bool r = false;
    switch (Type->value.u32)
    {
        case List:
        case ToggleSwitch:
        {
            r = true;
        }
        default:
        {
            break;
        }
    }
    return r;
}

static void ReadWriteLocalSettings(MonoObject* Type, const char* ini_path, const char* ini_section, const char* Id, const char* Value, bool want_read, char* data_out, const size_t data_out_buf_size)
{
    ffinal_printf("enter\n");
    ffinal_printf("ini_path %s\n", ini_path);
    ffinal_printf("ini_section %s\n", ini_section);
    if (want_read)
    {
        INIFile* ini = ini_load(ini_path);
        if (!ini)
        {
            final_printf("Failed to load `%s`\n", ini_path);
            return;
        }
        char* v = ini_get(ini, ini_section, Id);
        const char* v2 = v ? v : "";
        const size_t sv2 = strlen(v2);
        memcpy(data_out, v2, sv2 > data_out_buf_size ? data_out_buf_size - 1 : sv2);
        if (!v)
        {
            ini_set(ini, ini_section, Id, "0");
            ini_save(ini);
        }
        ini_free(ini);
    }
    else
    {
        INIFile* ini = ini_load(ini_path);
        if (!ini)
        {
            final_printf("Failed to load `%s`\n", ini_path);
            return;
        }
        ini_set(ini, ini_section, Id, Value);
        ini_save(ini);
        ini_free(ini);
    }
    ffinal_printf("finish %s\n", want_read ? "read" : "write");
}

extern "C" int kill(int, int);

static void NewOnPress(const void* p1, const void* element, const void* p3)
{
    const void* settings = Mono_Get_Class(App_Exe, "Sce.Vsh.ShellUI.Settings.Core", "SettingElement");
    const char* Id = mono_string_to_utf8(Mono_Get_Property(settings, element, "Id"));
    const char* Value = mono_string_to_utf8(Mono_Get_Property(settings, element, "Value"));
    MonoObject* Type = (MonoObject*)Mono_Get_Property(settings, element, "Type");
    final_printf("Id %s Value %s Type %d\n", Id, Value, Type->value.u32);
    if (IsValidElementType(Type))
    {
        const char* p = 0;
        const char* i = 0;
        switch (g_PageID)
        {
            case wSID(L"payload_options"):
            {
                p = HDD_INI_PATH;
                i = HEN_SECTION;
                break;
            }
            case wSID(L"id_plugin_manager"):
            {
                p = PLUGINS_INI_PATH;
                i = PLUGINS_ALL_SECTION;
                break;
            }
        }
        if (p && i)
        {
            ReadWriteLocalSettings(Type, p, i, Id, Value, false, 0, 0);
        }
    }
    // handle buttons
    if (Type->value.u32 == Button)
    {
        switch (SID(Id))
        {
            case SID("id_restart_shellui"):
            {
                final_printf("kill(getpid(), SIGKILL) %d\n", kill(getpid(), SIGKILL));
                break;
            }
            default:
            {
                break;
            }
        }
    }
    OnPress_Original.ptr(p1, element, p3);
}

static void NewOnPreCreate(void* p1, const void* element, void* p3)
{
    char buf[128] = {};
    const void* settings = Mono_Get_Class(App_Exe, "Sce.Vsh.ShellUI.Settings.Core", "SettingElement");
    const char* Id = mono_string_to_utf8(Mono_Get_Property(settings, element, "Id"));
    const char* Value = mono_string_to_utf8(Mono_Get_Property(settings, element, "Value"));
    MonoObject* Type = (MonoObject*)Mono_Get_Property(settings, element, "Type");
    if (IsValidElementType(Type))
    {
        const char* p = 0;
        const char* i = 0;
        switch (g_PageID)
        {
            case wSID(L"payload_options"):
            {
                p = HDD_INI_PATH;
                i = HEN_SECTION;
                break;
            }
            case wSID(L"id_plugin_manager"):
            {
                p = PLUGINS_INI_PATH;
                i = PLUGINS_ALL_SECTION;
                break;
            }
        }
        if (p && i)
        {
            ReadWriteLocalSettings(Type, p, i, Id, Value, true, buf, sizeof(buf));
        }
        if (buf[0])
        {
            Mono_Set_Property(settings, element, "Value", Mono_New_String(buf));
        }
    }
    final_printf("Id %s Value %s (read %s) Type %d\n", Id, Value, buf, Type->value.u32);
    // Disable builtin user plugins from being altered
    switch (SID(Id))
    {
        case SID("/data/hen/plugins/plugin_bootloader.prx"):
        case SID("/data/hen/plugins/plugin_loader.prx"):
        case SID("/data/hen/plugins/plugin_mono.prx"):
        case SID("/data/hen/plugins/plugin_server.prx"):
        case SID("/data/hen/plugins/plugin_shellcore.prx"):
        {
            const int v = false;
            Mono_Set_Property(settings, element, "Enabled", &v);
            break;
        }
        default:
        {
            break;
        }
    }
    OnPreCreate_Original.ptr(p1, element, p3);
}

static void NewOnPageActivating(void* p1, const void* element, MonoObject* p3)
{
    enum TransPush
    {
        OnNone = 0,
        OnPush,
        OnPop,
    };
    const uint32_t uPush = p3->value.u32;
    if (uPush == OnPush || uPush == OnPop)
    {
        const void* settings = Mono_Get_Class(App_Exe, "Sce.Vsh.ShellUI.Settings.Core", "SettingPage");
        const MonoString* Id = (MonoString*)Mono_Get_Property(settings, element, "Id");
        final_printf("id %ls\n", Id->str);
        const uint64_t local_pageid = wSID(Id->str);
        g_PageID = local_pageid;
        switch (uPush)
        {
            case OnPush:
            {
                switch (local_pageid)
                {
                    case wSID(L"id_plugin_manager"):
                    {
                        BuildPluginsList2();
                        break;
                    }
                }
                break;
            }
            case OnPop:
            {
                break;
            }
            default:
            {
                break;
            }
        }
    }
    OnPageActivating_Original.ptr(p1, element, p3);
}

static void NewOnConfirm(void* _this, void* action_)
{
    const void* settings = Mono_Get_Class(App_Exe, "Sce.Vsh.ShellUI.Settings.Core", "SettingElement");
    const char* Id = mono_string_to_utf8(Mono_Get_Property(settings, _this, "Id"));
    const char* Value = mono_string_to_utf8(Mono_Get_Property(settings, _this, "Value"));
    final_printf("Id %s Value %s\n", Id, Value);
    bool skip_confirm = false;
    // Confirm prompt when disabling
    if (Value && Value[0] == '1')
    {
        switch (SID(Id))
        {
            case SID("enable_plugins"):
            case SID("block_updates"):
            {
                skip_confirm = true;
                break;
            }
            default:
            {
                break;
            }
        }
    }
    // the other way around
    else if (Value && Value[0] == '0')
    {
        switch (SID(Id))
        {
            case SID("skip_patches"):
            {
                skip_confirm = true;
                break;
            }
            default:
            {
                break;
            }
        }
    }
    if (skip_confirm && OnPressed_Original.ptr)
    {
        // Commit the value to disk
        OnPressed_Original.ptr(_this, Mono_Get_Property(settings, _this, "Ui"));
        return;
    }
    OnConfirm_Original.ptr(_this, action_);
}

static void NewOnCheckVisible(const void* param1, const void* e, const void* args)
{
    const void* settings = Mono_Get_Class(App_Exe, "Sce.Vsh.ShellUI.Settings.Core", "SettingElement");
    const void* MsgData = Mono_Get_Class(App_Exe, "Sce.Vsh.ShellUI.Settings.Core", "MessageElementData");
    const MonoString* Id = (MonoString*)Mono_Get_Property(settings, e, "Id");
    const MonoObject* Type = (MonoObject*)Mono_Get_Property(settings, e, "Type");
    const MonoObject* Data = (MonoObject*)Mono_Get_Property(settings, e, "Data");
    switch (wSID(Id->str))
    {
        case wSID(L"" ID_WAIT):
        {
            ffinal_printf("Match %ls\n", Id->str);
            ffinal_printf("g_PluginsFoundMsg %s\n", g_PluginsFoundMsg);
            if (Type->value.u32 == Message)
            {
                if (!g_PluginsFoundMsg[0])
                {
                    const int v = false;
                    Mono_Set_Property(settings, e, "Visible", &v);
                }
                else
                {
                    const int v = false;
                    Mono_Set_Property(MsgData, Data, "BusyIndicator", &v);
                    Mono_Set_Property(MsgData, Data, "Title", Mono_New_String(g_PluginsFoundMsg));
                }
            }
            break;
        }
        case wSID(L"id_restart_shellui"):
        {
            const int v = false;
            Mono_Set_Property(settings, e, "Visible", &v);
            break;
        }
        default:
        {
            break;
        }
    }
    OnCheckVisible_Original.ptr(param1, e, args);
}

static void NewOnPageDeactivating(const void* param1, const void* p, const void* args)
{
    const void* settings = Mono_Get_Class(App_Exe, "Sce.Vsh.ShellUI.Settings.Core", "SettingPage");
    const MonoString* Id = (MonoString*)Mono_Get_Property(settings, p, "Id");
    ffinal_printf("Id %ls\n", Id->str);
    switch (wSID(Id->str))
    {
        // kill it, kill it now!
        case wSID(L"id_plugin_manager"):
        {
            if (g_PluginsRunThread > 0)
            {
                pthread_cancel(g_PluginsRunThread);
                pthread_join(g_PluginsRunThread, 0);
                g_PluginsRunThread = 0;
            }
        }
    }
    g_PageID = 0;
}

void UploadOnBranch(void* app_exe)
{
    const uintptr_t OnPress = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.SettingsRoot", "SettingsRootHandler", "OnPress", 2);
    if (OnPress)
    {
        const uintptr_t pHook = CreatePrologueHook(OnPress, 15);
        if (pHook)
        {
            OnPress_Original.addr = pHook;
            WriteJump64(OnPress, (uintptr_t)NewOnPress);
        }
    }
    const uintptr_t OnPreCreate = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.SettingsRoot", "SettingsRootHandler", "OnPreCreate", 2);
    if (OnPreCreate)
    {
        const uintptr_t pHook = CreatePrologueHook(OnPreCreate, 15);
        if (pHook)
        {
            OnPreCreate_Original.addr = pHook;
            WriteJump64(OnPreCreate, (uintptr_t)NewOnPreCreate);
        }
    }
    const uintptr_t OnPageActivating = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.SettingsRoot", "SettingsRootHandler", "OnPageActivating", 2);
    if (OnPageActivating)
    {
        const uintptr_t pHook = CreatePrologueHook(OnPageActivating, 15);
        if (pHook)
        {
            OnPageActivating_Original.addr = pHook;
            WriteJump64(OnPageActivating, (uintptr_t)NewOnPageActivating);
        }
    }
    const int app_exe_h = sceKernelLoadStartModule("/app0/psm/Application/app.exe.sprx", 0, 0, 0, 0, 0);
    if (app_exe_h > 0)
    {
        struct OrbisKernelModuleInfo info = {0};
        info.size = sizeof(info);
        const int r = sceKernelGetModuleInfo(app_exe_h, &info);
        if (r == 0)
        {
            const void* mm_p = info.segmentInfo[0].address;
            const uint64_t mm_s = info.segmentInfo[0].size;
            const uintptr_t OnCheckVisible = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.SettingsRoot", "SettingsRootHandler", "OnCheckVisible", 2);
            if (OnCheckVisible)
            {
                const size_t srclen = 6;
                const uintptr_t pHook = CreatePrologueHook(OnCheckVisible, srclen);
                if (pHook)
                {
                    OnCheckVisible_Original.addr = pHook;
                    Make32to64Jmp((uintptr_t)mm_p, mm_s, OnCheckVisible, (uintptr_t)NewOnCheckVisible, srclen, false, 0);
                }
            }
        }
    }
    const uintptr_t OnConfirm = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.Core", "SettingElement", "Confirm", 1);
    if (OnConfirm)
    {
        const uintptr_t pHook = CreatePrologueHook(OnConfirm, 15);
        if (pHook)
        {
            OnConfirm_Original.addr = pHook;
            WriteJump64(OnConfirm, (uintptr_t)NewOnConfirm);
        }
    }
    OnPressed_Original.v = Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.Core", "SettingElement", "OnPressed", 1);
    const uintptr_t OnPageDeactivating = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.Core", "SettingsHandler", "OnPageDeactivating", 2);
    if (OnPageDeactivating)
    {
        WriteJump64(OnPageDeactivating, (uintptr_t)NewOnPageDeactivating);
    }
}

void UploadNewCorelibStreamReader(void)
{
    UploadResourceStreamBranch();
}

uiTYPEDEF_FUNCTION_PTR(void, FinishBootEffect_original, void* _this);
void PrintTimeTick(void);

static void FinishBootEffect(void* _this)
{
    static bool once = false;
    if (!once)
    {
        PrintTimeTick();
        once = true;
    }
    FinishBootEffect_original.ptr(_this);
}

void UploadFinishBootEffectCode(const struct OrbisKernelModuleInfo* info)
{
    const void* mm_p = info->segmentInfo[0].address;
    const uint32_t mm_s = info->segmentInfo[0].size;
    const uintptr_t FinishBoot = PatternScan(mm_p, mm_s,
                                             "55 "
                                             "48 8B EC "
                                             "48 83 EC ? "
                                             "4C 89 7D ? "
                                             "4C 8B FF "
                                             "33 C0 "
                                             "48 89 45 ? "
                                             "48 89 45 ? "
                                             "48 89 45 ? "
                                             "49 8B 47 38 "
                                             "48 8B F5 "
                                             "48 83 C6 ? "
                                             "48 8B F8 "
                                             "83 38 00",
                                             0);
    if (FinishBoot)
    {
        const uintptr_t BootCave = CreatePrologueHook(FinishBoot, 15);
        if (BootCave)
        {
            FinishBootEffect_original.addr = BootCave;
            WriteJump64(FinishBoot, (uintptr_t)FinishBootEffect);
        }
    }
}
