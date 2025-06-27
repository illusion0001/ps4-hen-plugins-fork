#include "remoteplay.h"
#include "../memory.h"
#include <unistd.h>

void UploadRemotePlayPatch(const struct OrbisKernelModuleInfo* info)
{
    // credits to Aida
    const uintptr_t errcode = PatternScan(
        info->segmentInfo[0].address,
        info->segmentInfo[0].size,
        "bf 00 01 86 02 48 be ff ff ff ff ff ff ff ff e8 ? ? ? ? 83 f8 01 0f 84 ? ? ? ?",
        0);
    if (errcode)
    {
        const int pid = getpid();
        // to keep behavior of original patch
        // whoever wrote this mustn't notice the branch
        // right below this error code
        const uint64_t offset = 23;
        sys_proc_memset(pid, errcode, 0x90, offset);
        static const uint8_t p[] = {0x48, 0xe9};
        sys_proc_rw(pid, errcode + offset, p, sizeof(p), 1);
    }
}
