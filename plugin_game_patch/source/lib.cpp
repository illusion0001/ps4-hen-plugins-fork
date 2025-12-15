#include <mxml.h>
#include <orbis/libkernel.h>
#include "patch.h"
#include "utils.h"
#include "../../common/path.h"
#include "../../common/plugin_common.h"
#include "../../common/notify.h"

#include "../../common/entry.h"
#include "../../plugin_shellcore/source/local_appinfo.h"

#define HEN_PATH BASE_PATH
// Legacy path testing
// #define HEN_PATH "/data/GoldHEN"
#define BASE_PATH_PATCH HEN_PATH "/patches"
#define BASE_PATH_PATCH_SETTINGS BASE_PATH_PATCH "/settings"
#define BASE_PATH_PATCH_XML BASE_PATH_PATCH "/xml"
#define PLUGIN_NAME "game_patch"
#define PLUGIN_DESC "Patches game at boot"
#define PLUGIN_AUTH "illusion"
#define PLUGIN_VER 0x110  // 1.10

#define NO_ASLR_ADDR 0x00400000

attr_public const char* g_pluginName = PLUGIN_NAME;
attr_public const char* g_pluginDesc = PLUGIN_DESC;
attr_public const char* g_pluginAuth = PLUGIN_AUTH;
attr_public uint32_t g_pluginVersion = PLUGIN_VER;

char g_titleid[16] = {0};
char g_game_elf[MAX_PATH_] = {0};
char g_game_prx[MAX_PATH_] = {0};
char g_game_ver[8] = {0};

uint64_t g_module_base = 0;
uint32_t g_module_size = 0;
// unused for now
bool g_PRX = false;
uint64_t g_PRX_module_base = 0;
uint32_t g_PRX_module_size = 0;

static const char* GetXMLAttr(mxml_node_t* node, const char* name)
{
    const char* AttrData = mxmlElementGetAttr(node, name);
    if (AttrData == NULL)
    {
        AttrData = "\0";
    }
    return AttrData;
}

static void get_key_init(void)
{
    uint32_t patch_lines = 0;
    uint32_t patch_items = 0;
    char* patch_buffer = nullptr;
    uint64_t patch_size = 0;
    char input_file[MAX_PATH_] = {0};
    snprintf(input_file, sizeof(input_file), BASE_PATH_PATCH_XML "/%s.xml", g_titleid);
    int32_t res = Read_File(input_file, &patch_buffer, &patch_size, 0);

    if (res < 0)
    {
        final_printf("failed to open %s(0x%08x), trying legacy path\n", input_file, res);
        // try old goldhen path
        memset(input_file, 0, sizeof(input_file));
        snprintf(input_file, sizeof(input_file), "/data/GoldHEN/patches/xml/%s.xml", g_titleid);
        res = Read_File(input_file, &patch_buffer, &patch_size, 0);
    }
    if (res < 0)
    {
        final_printf("file %s not found\n", input_file);
        final_printf("error: 0x%08x\n", res);
        return;
    }
    final_printf("open success %s\n", input_file);

    if (patch_buffer && patch_size)
    {
        mxml_node_t *node, *tree = NULL;
        tree = mxmlLoadString(NULL, patch_buffer, MXML_NO_CALLBACK);

        if (!tree)
        {
            final_printf("XML: could not parse XML:\n%s\n", patch_buffer);
            free(patch_buffer);
            return;
        }

        for (node = mxmlFindElement(tree, tree, "Metadata", NULL, NULL, MXML_DESCEND); node != NULL;
             node = mxmlFindElement(node, tree, "Metadata", NULL, NULL, MXML_DESCEND))
        {
            char* settings_buffer = nullptr;
            uint64_t settings_size = 0;
            bool PRX_patch = false;
            const char* TitleData = GetXMLAttr(node, "Title");
            const char* NameData = GetXMLAttr(node, "Name");
            const char* AppVerData = GetXMLAttr(node, "AppVer");
            const char* AppElfData = GetXMLAttr(node, "AppElf");

            debug_printf("Title: \"%s\"\n", TitleData);
            debug_printf("Name: \"%s\"\n", NameData);
            debug_printf("AppVer: \"%s\"\n", AppVerData);
            debug_printf("AppElf: \"%s\"\n", AppElfData);

            uint64_t hashout = patch_hash_calc(TitleData, NameData, AppVerData, input_file, AppElfData);
            char settings_path[MAX_PATH_] = {0};
            snprintf(settings_path, sizeof(settings_path), BASE_PATH_PATCH_SETTINGS "/0x%016lx.txt", hashout);
            sceKernelChmod(settings_path, 0777);
            int32_t res = Read_File(settings_path, &settings_buffer, &settings_size, 0);
            final_printf("settings_path: %s, 0x%08x\n", settings_path, res);
            if (res == ORBIS_KERNEL_ERROR_ENOENT)
            {
                debug_printf("file %s not found, initializing false. ret: 0x%08x\n", settings_path, res);
                static const uint8_t false_data[] = {'0', '\n'};
                Write_File(settings_path, false_data, sizeof(false_data));
                continue;
            }
            if (!settings_buffer || !settings_size)
            {
                final_printf("Settings 0x%016lx has no data!\n", hashout);
                final_printf("File size %li bytes\n", settings_size);
                continue;
            }
            if (settings_buffer[0] == '1' && !strcmp(g_game_elf, AppElfData))
            {
                int32_t ret_cmp = strcmp(g_game_ver, AppVerData);
                if (!ret_cmp)
                {
                    final_printf("App ver %s == %s\n", g_game_ver, AppVerData);
                }
                else if (startsWith(AppVerData, "mask") || startsWith(AppVerData, "all"))
                {
                    final_printf("App ver masked: %s\n", AppVerData);
                }
                else if (ret_cmp)
                {
                    final_printf("App ver %s != %s\n", g_game_ver, AppVerData);
                    final_printf("Skipping patch entry\n");
                    continue;
                }
                patch_items++;
                mxml_node_t* Patchlist_node = mxmlFindElement(node, node, "PatchList", NULL, NULL, MXML_DESCEND);
                for (mxml_node_t* Line_node = mxmlFindElement(node, node, "Line", NULL, NULL, MXML_DESCEND); Line_node != NULL;
                     Line_node = mxmlFindElement(Line_node, Patchlist_node, "Line", NULL, NULL, MXML_DESCEND))
                {
                    uint64_t addr_real = 0;
                    uint64_t jump_addr = 0;
                    uint32_t jump_size = 0;
                    bool use_mask = false;
                    const char* gameType = GetXMLAttr(Line_node, "Type");
                    const char* gameAddr = GetXMLAttr(Line_node, "Address");
                    const char* gameValue = GetXMLAttr(Line_node, "Value");
                    const char* gameOffset = nullptr;
                    // starts with `mask`
                    if (startsWith(gameType, "mask"))
                    {
                        use_mask = true;
                    }
                    if (use_mask)
                    {
                        if (startsWith(gameType, "mask_jump32"))
                        {
                            const char* gameJumpTarget = GetXMLAttr(Line_node, "Target");
                            const char* gameJumpSize = GetXMLAttr(Line_node, "Size");
                            jump_addr = addr_real = (uint64_t)PatternScan(g_module_base, g_module_size, gameJumpTarget);
                            jump_size = strtoul(gameJumpSize, NULL, 10);
                            debug_printf("Target: 0x%lx jump size %u\n", jump_addr, jump_size);
                        }
                        gameOffset = GetXMLAttr(Line_node, "Offset");
                        addr_real = (uint64_t)PatternScan(g_module_base, g_module_size, gameAddr);
                        if (!addr_real)
                        {
                            final_printf("Masked Address: %s not found\n", gameAddr);
                            continue;
                        }
                        final_printf("Masked Address: 0x%lx\n", addr_real);
                        debug_printf("Offset: %s\n", gameOffset);
                        uint32_t real_offset = 0;
                        if (gameOffset[0] != '0')
                        {
                            if (gameOffset[0] == '-')
                            {
                                debug_printf("Offset mode: subtract\n");
                                real_offset = strtoul(gameOffset + 1, NULL, 10);
                                debug_printf("before offset: 0x%lx\n", addr_real);
                                addr_real = addr_real - real_offset;
                                debug_printf("after offset: 0x%lx\n", addr_real);
                            }
                            else if (gameOffset[0] == '+')
                            {
                                debug_printf("Offset mode: addition\n");
                                real_offset = strtoul(gameOffset + 1, NULL, 10);
                                debug_printf("before offset: 0x%lx\n", addr_real);
                                addr_real = addr_real + real_offset;
                                debug_printf("after offset: 0x%lx\n", addr_real);
                            }
                        }
                        else
                        {
                            debug_printf("Mask does not reqiure offsetting.\n");
                        }
                    }
                    debug_printf("Type: \"%s\"\n", gameType);
                    if (gameAddr && !use_mask)
                    {
                        addr_real = strtoull(gameAddr, NULL, 16);
                        debug_printf("Address: 0x%lx\n", addr_real);
                    }
                    debug_printf("Value: \"%s\"\n", gameValue);
                    debug_printf("patch line: %u\n", patch_lines);
                    if (gameType && addr_real && *gameValue != '\0')  // type, address and value must be present
                    {
                        if (!PRX_patch && !use_mask)
                        {
                            // previous self, eboot patches were made with no aslr addresses
                            addr_real = g_module_base + (addr_real - NO_ASLR_ADDR);
                        }
                        else if (PRX_patch && !use_mask)
                        {
                            addr_real = g_module_base + addr_real;
                        }
                        patch_data1(gameType, addr_real, gameValue, jump_size, jump_addr);
                        patch_lines++;
                    }
                }
            }
            if (settings_buffer)
            {
                free(settings_buffer);
            }
        }

        mxmlDelete(node);
        mxmlDelete(tree);
        free(patch_buffer);

        if (patch_items > 0 && patch_lines > 0)
        {
            char msg[128] = {0};
            snprintf(msg, sizeof(msg),
                     "%u %s Applied\n"
                     "%u %s Applied",
                     patch_items,
                     (patch_items == 1) ? "Patch" : "Patches",
                     patch_lines,
                     (patch_lines == 1) ? "Patch Line" : "Patch Lines");
            Notify("%s", msg);
        }
    }
    else  // if (!patch_buffer && !patch_size)
    {
        char msg[128] = {0};
        snprintf(msg, sizeof(msg), "File %s\nis empty", input_file);
        Notify("%s", msg);
    }
}

static void mkdir_chmod(const char* path, OrbisKernelMode mode)
{
    sceKernelMkdir(path, mode);
    sceKernelChmod(path, mode);
}

static void make_folders(void)
{
    mkdir_chmod(HEN_PATH, 0777);
    mkdir_chmod(BASE_PATH_PATCH, 0777);
    mkdir_chmod(BASE_PATH_PATCH_XML, 0777);
    mkdir_chmod(BASE_PATH_PATCH_SETTINGS, 0777);
}

extern "C"
{
int32_t attr_public plugin_load(SceEntry* e, disk_appinfo* info)
{
    struct OrbisKernelModuleInfo info2 = {0};
    info2.size = sizeof(info2);
    const int r = sceKernelGetModuleInfo(0, &info2);
    printf("sceKernelGetModuleInfoEx 0x%08x\n", r);
    if (r == 0)
    {
        make_folders();
        strncpy(g_titleid, info->m_titleid, sizeof(info->m_titleid));
        strncpy(g_game_elf, info2.name, sizeof(info2.name));
        strncpy(g_game_ver, info->m_appver, sizeof(info->m_appver));
        printf("g_titleid: %s\n", g_titleid);
        printf("g_game_elf: %s\n", g_game_elf);
        printf("g_game_ver: %s\n", g_game_ver);
        g_module_base = (uint64_t)info2.segmentInfo[0].address;
        printf("g_module_base 0x%lx\n", g_module_base);
        get_key_init();
        return 0;
    }

    return 1;
}

int32_t attr_public plugin_unload(SceEntry* e)
{
    final_printf("<%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    return 0;
}
}
