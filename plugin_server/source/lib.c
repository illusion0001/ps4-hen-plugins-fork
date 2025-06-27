#include "../../common/entry.h"

#include <stdint.h>
#include <stdio.h>
#include "../../common/plugin_common.h"
#include "../../common/notify.h"

#include "../../extern/libjbc/libjbc.h"

#include <pthread.h>
#include <unistd.h>

attr_public const char* g_pluginName = "plugin_example";
attr_public const char* g_pluginDesc = "Demonstrate usage of CXX in module. Based from OpenOrbis `library_example`";
attr_public const char* g_pluginAuth = "illusiony";
attr_public uint32_t g_pluginVersion = 0x00000100;  // 1.00

void* pthread_kserver(void* args);

int ftp_main(void);

// Verify jailbreak
static int is_jailbroken(void)
{
    FILE* s_FilePointer = fopen("/user/.jailbreak", "w");

    if (!s_FilePointer)
    {
        return 0;
    }

    fclose(s_FilePointer);
    remove("/user/.jailbreak");
    return 1;
}

void* pthread_ftp(void* args)
{
    // Variables for (un)jailbreaking
    // https://github.com/bucanero/apollo-ps4/blob/461ee5d58653a82ab4b901041a3cc0b7026bfffb/source/orbis_jbc.c#L243
    static jbc_cred g_Cred;
    static jbc_cred g_RootCreds;
    jbc_get_cred(&g_Cred);
    g_RootCreds = g_Cred;
    jbc_jailbreak_cred(&g_RootCreds);
    jbc_set_cred(&g_RootCreds);

    if (is_jailbroken())
    {
#if LOG_PRINTF_TO_TTY
        int open(const char*, int);
        int close(int);
        const int O_WRONLY = 2;
        const int stdout_ = 1;
        const int stderr_ = 2;
        const int fd = open("/dev/console", O_WRONLY);
        if (fd > 0)
        {
            dup2(fd, stdout_);
            dup2(stdout_, stderr_);
            close(fd);
        }
        Notify("", "console fd %d", fd);
#endif
        ftp_main();
    }
    else
    {
        Notify(TEX_ICON_SYSTEM,
               "Failed to jailbreak process!\n"
               "FTP will not start.\n");
    }
    return 0;
}

int plugin_load(struct SceEntry* args)
{
    pthread_t pthrd = 0;
    pthread_t pthrd_ftp = 0;
    pthread_create(&pthrd, 0, pthread_kserver, 0);
    pthread_create(&pthrd_ftp, 0, pthread_ftp, 0);
    // block main thread from exiting _init_env
    while (1)
    {
        sleep(60);
    }
    return 0;
}
