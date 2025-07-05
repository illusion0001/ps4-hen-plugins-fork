#include "settings_menu.h"
#include "../../../common/file.h"
#include "../../../common/function_ptr.h"
#include "../../../common/memory.h"
#include "../../../common/notify.h"
#include "../../../common/plugin_common.h"
#include "../../../common/stringid.h"
#include <stdio.h>
#include "../mono.h"

#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>

#include "../ini.h"

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
    //printf("New str:\n%s\n", buf2);
    mono_free(xml);
    memset(buf, 0, sizeof(buf));
    static const uint8_t bom_header[] = {0xEF, 0xBB, 0xBF};
    memcpy(buf, bom_header, sizeof(bom_header));
    strncat(buf, buf2, strlen(buf2));
    strncpy(buf_fixed, buf, strlen(buf));
    buflen = strlen(buf_fixed);
}

uiTYPEDEF_FUNCTION_PTR(void*, ReadResourceStream_Original, void* inst, void* string);

static void* ReadResourceStream(void* inst, MonoString* filestring)
{
    printf("%s (%ls)\n", __FUNCTION__, filestring->str);
    {
        static void* mscorlib_ptr = 0;
        if (!mscorlib_ptr)
        {
            char mscorlib_sprx[260] = {};
            const char* sandbox_path = sceKernelGetFsSandboxRandomWord();
            if (sandbox_path)
            {
                snprintf(mscorlib_sprx, sizeof(mscorlib_sprx), "/%s/common/lib/mscorlib.dll", sandbox_path);
                mscorlib_ptr = mono_get_image(mscorlib_sprx);
            }
        }
        const uint64_t s = wSID(filestring->str);
        printf("SID: 0x%lx\n", s);
        switch (s)
        {
            // c23 doesn't support function constexpr yet
            // this is the workaround for now
            // case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.SettingsRoot.data.settings_root.xml"):
            case 0x625E2DDDEC3244C2:
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
            // case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.hen_settings.xml"):
            case 0xE6168C18F98D1DF6:
            {
                return Mono_File_Stream(mscorlib_ptr, SHELLUI_HEN_SETTINGS);
            }
            // case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.external_hdd.xml"):
            case 0x959FE82777191437:
            {
                return Mono_File_Stream(mscorlib_ptr, SHELLUI_DATA_PATH "/external_hdd.xml");
            }
            // case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.PkgInstaller.data.pkginstaller.xml"):
            case 0x8a00500dee1b4143:
            // case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.PkgInstaller.data.pkginstaller_usb.xml"):
            case 0x6186e56a05de0492:
            // case SID("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.PkgInstaller.data.pkginstaller_hdd.xml"):
            case 0xf3591e9176d4dd3c:
            {
                extern bool g_only_hdd;
                g_only_hdd = s == 0xf3591e9176d4dd3c;
                return ReadResourceStream_Original.ptr(inst, Mono_New_String("Sce.Vsh.ShellUI.src.Sce.Vsh.ShellUI.Settings.Plugins.PkgInstaller.data.pkginstaller.xml"));
            }
        }
    }
    return ReadResourceStream_Original.ptr(inst, filestring);
}

static void UploadResourceStreamBranch(void)
{
    const char* sandbox_path = sceKernelGetFsSandboxRandomWord();
    if (sandbox_path)
    {
        char mscorlib_sprx[260] = {};
        snprintf(mscorlib_sprx, sizeof(mscorlib_sprx), "/%s/common/lib/mscorlib.dll", sandbox_path);
        void* mscorlib_ptr = mono_get_image(mscorlib_sprx);
        if (mscorlib_ptr)
        {
            static const char namespace[] = "Assembly";
            static const char method[] = "GetManifestResourceStream";
            const uintptr_t ctor = (uintptr_t)Mono_Get_Address_of_Method(mscorlib_ptr, "System.Reflection", namespace, method, 1);
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
                       namespace,
                       method);
            }
        }
    }
}

enum ShellUIElementType
{
    Invalid = 0,
    List = 5,
    ToggleSwitch = 8,
};

uiTYPEDEF_FUNCTION_PTR(void, OnPress_Original, const void* p1, const void* p2, const void* p3);
uiTYPEDEF_FUNCTION_PTR(void, OnPreCreate_Original, const void* p1, const void* p2, const void* p3);
uiTYPEDEF_FUNCTION_PTR(void, OnPageActivating_Original, const void* p1, const void* p2, const void* p3);

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

static void ReadWriteLocalSettings(MonoObject* Type, const char* Id, const char* Value, bool want_read, char* data_out, const size_t data_out_buf_size)
{
    final_printf("enter\n");
    if (want_read)
    {
        INIFile* ini = ini_load(HDD_INI_PATH);
        if (!ini)
        {
            final_printf("Failed to load `" HDD_INI_PATH "`\n");
            return;
        }
        char* v = ini_get(ini, HEN_SECTION, Id);
        const char* v2 = v ? v : "";
        const size_t sv2 = strlen(v2);
        memcpy(data_out, v2, sv2 > data_out_buf_size ? data_out_buf_size - 1 : sv2);
        if (!v)
        {
            ini_set(ini, HEN_SECTION, Id, "0");
            ini_save(ini);
        }
        ini_free(ini);
    }
    else
    {
        INIFile* ini = ini_load(HDD_INI_PATH);
        if (!ini)
        {
            final_printf("Failed to load `" HDD_INI_PATH "`\n");
            return;
        }
        ini_set(ini, HEN_SECTION, Id, Value);
        ini_save(ini);
        ini_free(ini);
    }
    final_printf("finish %s\n", want_read ? "read" : "write");
}

static void NewOnPress(const void* p1, const void* element, const void* p3)
{
    const void* settings = Mono_Get_Class(App_Exe, "Sce.Vsh.ShellUI.Settings.Core", "SettingElement");
    const char* Id = mono_string_to_utf8(Mono_Get_Property(settings, element, "Id"));
    const char* Value = mono_string_to_utf8(Mono_Get_Property(settings, element, "Value"));
    MonoObject* Type = (MonoObject*)Mono_Get_Property(settings, element, "Type");
    final_printf("Id %s Value %s Type %d\n", Id, Value, Type->value.u32);
    if (IsValidElementType(Type))
    {
        ReadWriteLocalSettings(Type, Id, Value, false, 0, 0);
    }
    if (SID(Id) == SID("id_show_sysiteminit"))
    {
        const uintptr_t p = (uintptr_t)Mono_Get_Address_of_Method(App_Exe, "Sce.Vsh.ShellUI.TopMenu", "SystemAreaPanel", "SysItemInit", 0);
        const int app_exe_h = sceKernelLoadStartModule("/app0/psm/Application/app.exe.sprx", 0, 0, 0, 0, 0);
        if (app_exe_h > 0)
        {
            struct OrbisKernelModuleInfo info = {0};
            info.size = sizeof(info);
            const int r = sceKernelGetModuleInfo(app_exe_h, &info);
            if (r == 0)
            {
                Notify("", "SysItemInit 0x%lx 0x%lx\n", p, p - (uintptr_t)info.segmentInfo[0].address);
            }
        }
    }
    // id_restart_shellui
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
        ReadWriteLocalSettings(Type, Id, Value, true, buf, sizeof(buf));
        if (buf[0])
        {
            Mono_Set_Property(settings, element, "Value", Mono_New_String(buf));
        }
    }
    final_printf("Id %s Value %s (read %s) Type %d\n", Id, Value, buf, Type->value.u32);
    OnPreCreate_Original.ptr(p1, element, p3);
}

void UploadOnBranch(void* app_exe)
{
    const uintptr_t OnPress = (uintptr_t)Mono_Get_Address_of_Method(app_exe, "Sce.Vsh.ShellUI.Settings.SettingsRoot", "SettingsRootHandler", "OnPress", 2);
    if (OnPress)
    {
        const uintptr_t pHook = CreatePrologueHook(OnPress,15);
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
