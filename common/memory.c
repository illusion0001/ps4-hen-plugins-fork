#include "memory.h"

#include <orbis/libkernel.h>

#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include "plugin_common.h"

#include <stdarg.h>

#include "HDE/HDE64.h"
#include "HDE/HDE64.c"

#include <stdlib.h>
#include <sys/mman.h>

static uint32_t pattern_to_byte(const char* pattern, uint8_t* bytes)
{
    uint32_t count = 0;
    const char* start = pattern;
    const char* end = pattern + strlen(pattern);

    for (const char* current = start; current < end; ++current)
    {
        if (*current == '?')
        {
            ++current;
            if (*current == '?')
            {
                ++current;
            }
            bytes[count++] = -1;
        }
        else
        {
            bytes[count++] = strtoull(current, (char**)&current, 16);
        }
    }
    return count;
}

/*
 * @brief Scan for a given byte pattern on a module
 *
 * @param module_base Base of the module to search
 * @param module_size Size of the module to search
 * @param signature   IDA-style byte array pattern
 * @credit
 * https://github.com/OneshotGH/CSGOSimple-master/blob/59c1f2ec655b2fcd20a45881f66bbbc9cd0e562e/CSGOSimple/helpers/utils.cpp#L182
 * @returns           Address of the first occurrence
 */
uintptr_t PatternScan(const void* module_base, const uint64_t module_size, const char* signature, const uint64_t offset)
{
    if (!module_base || !module_size)
    {
        return 0;
    }
// constexpr uint32_t MAX_PATTERN_LENGTH = 256;
#define MAX_PATTERN_LENGTH 512
    uint8_t patternBytes[MAX_PATTERN_LENGTH] = {0};
    int32_t patternLength = pattern_to_byte(signature, patternBytes);
    if (!patternLength || patternLength >= MAX_PATTERN_LENGTH)
    {
        return 0;
    }
    uint8_t* scanBytes = (uint8_t*)module_base;

    for (uint64_t i = 0; i < module_size; ++i)
    {
        bool found = true;
        for (int32_t j = 0; j < patternLength; ++j)
        {
            if (scanBytes[i + j] != patternBytes[j] &&
                patternBytes[j] != 0xff)
            {
                found = false;
                break;
            }
        }
        if (found)
        {
            return (((uintptr_t)&scanBytes[i] + offset));
        }
    }
    return 0;
}

static int jmpcallbytes = 5;

void WriteJump32(const uintptr_t src, const uintptr_t dst, const uint64_t len, const bool call)
{
    if (!src || !dst || len < jmpcallbytes)
    {
        return;
    }
    const int pid = getpid();
    if (len != jmpcallbytes)
    {
        sys_proc_memset(pid, src, 0x90, len);
    }
    const int32_t relativeAddress = ((uintptr_t)dst - (uintptr_t)src) - jmpcallbytes;
    const uint8_t op = call ? 0xe8 : 0xe9;
    sys_proc_rw(pid, src, &op, sizeof(op), 1);
    sys_proc_rw(pid, src + 1, &relativeAddress, sizeof(relativeAddress), 1);
}

const static uint8_t JMPstub[] = {
    0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,  // jmp qword ptr [$+6]
};

void WriteJump64(const uintptr_t src, const uintptr_t dst)
{
    const int pid = getpid();
    sys_proc_rw(pid, src, JMPstub, sizeof(JMPstub), 1);
    sys_proc_rw(pid, src + sizeof(JMPstub), &dst, sizeof(dst), 1);
}

uintptr_t ReadLEA32(uintptr_t Address, size_t offset, size_t lea_size, size_t lea_opcode_size)
{
    uintptr_t Address_Result = Address;
    uintptr_t Patch_Address = 0;
    int32_t lea_offset = 0;
    uintptr_t New_Offset = 0;
    if (Address_Result)
    {
        if (offset)
        {
            Patch_Address = offset + Address_Result;
            lea_offset = *(int32_t*)(lea_size + Address_Result);
            New_Offset = Patch_Address + lea_offset + lea_opcode_size;
        }
        else
        {
            Patch_Address = Address_Result;
            lea_offset = *(int32_t*)(lea_size + Address_Result);
            New_Offset = Patch_Address + lea_offset + lea_opcode_size;
        }
        // LOG(L"%s: 0x%016llx -> 0x%016llx\n", Pattern_Name, Address_Result, New_Offset);
        return New_Offset;
    }
    return 0;
}

void Make32to64Jmp(const uintptr_t textbase, const uintptr_t textsz, const uintptr_t src, const uintptr_t dst, const uint64_t srclen, const bool call, uintptr_t* src_out)
{
    uintptr_t jmparea = PatternScan((void*)textbase, textsz, "0f 0b 90 90 90 90 90 90 90 90 90 90 90 90 90 90", 2);
    if (!jmparea)
    {
        jmparea = PatternScan((void*)textbase, textsz, "c3 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00", 1);
    }
    if (jmparea)
    {
        if (src_out)
        {
            const uintptr_t res = ReadLEA32(src, 0, 1, 5);
            final_printf("returning to 0x%lx\n", res);
            *src_out = res;
        }
        WriteJump32(src, jmparea, srclen, call);
        WriteJump64(jmparea, dst);
    }
}

void hex_dump(const uintptr_t data, const uint64_t size, const uintptr_t real)
{
    if (real)
    {
        printf("offset: %lx\n", real);
    }
    const unsigned char* p = (const unsigned char*)data;
    uintptr_t i = 0, j = 0;

    for (i = 0; i < size; i += 16)
    {
        printf("%016lx: ", i);  // Print address offset

        for (j = 0; j < 16; j++)
        {
            if (i + j < size)
            {
                printf("%02x ", p[i + j]);  // Print hex value
            }
            else
            {
                printf("   ");  // Pad with spaces if less than 16 bytes
            }
        }

        printf("  |");

        for (j = 0; j < 16; j++)
        {
            if (i + j < size)
            {
                extern int isprint(int);
                if (isprint(p[i + j]))
                {
                    printf("%c", p[i + j]);  // Print printable ASCII characters
                }
                else
                {
                    printf(".");  // Replace non-printable characters with '.'
                }
            }
        }

        printf("|\n");
    }
}

// clang compatible implement of syscall
// https://github.com/john-tornblom/tiny-ps4-shell/blob/5ca90b7d2825e3e2c7f9fb1e81cf8949b37af563/kern_orbis.c#L39

extern int errno;

static long __syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
    unsigned long ret;
    char iserror;

    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    register long r9 __asm__("r9") = a6;

    __asm__ __volatile__(
        "syscall" : "=a"(ret), "=@ccc"(iserror), "+r"(r10), "+r"(r8), "+r"(r9) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");

    return iserror ? -ret : ret;
}

long orbis_syscall(long sysno, ...)
{
    int err;
    va_list args;
    long arg0, arg1, arg2, arg3, arg4, arg5;

    va_start(args, sysno);
    arg0 = va_arg(args, long);
    arg1 = va_arg(args, long);
    arg2 = va_arg(args, long);
    arg3 = va_arg(args, long);
    arg4 = va_arg(args, long);
    arg5 = va_arg(args, long);
    va_end(args);

    err = __syscall6(sysno, arg0, arg1, arg2, arg3, arg4, arg5);
    if (err < 0)
    {
        errno = -err;
        return -1;
    }

    return err;
}

int sys_proc_rw(const int pid, const uintptr_t addr, const void* data, const uint64_t datasz, const uint64_t write_)
{
    return orbis_syscall(108, pid, addr, data, datasz, write_);
}

int sys_proc_memset(const int pid, const uintptr_t src, const uint32_t c, const uint64_t len)
{
#define max_len 8ul * 1024
    if (len > max_len)
    {
        final_printf("Attempting to memset pid %d with length %lu is larger than maximum length %lu\n", pid, len, max_len);
        return -1;
    }
    uint8_t temp[max_len];
    memset(temp, c, len);
    return sys_proc_rw(pid, src, temp, len, 1);
#undef max_len
    const uint8_t op = (uint8_t)c;
    int r = 0;
    for (uint64_t l = 0; l < len; l++)
    {
        r |= sys_proc_rw(pid, src + l, &op, sizeof(op), 1);
    }
    return r;
}

uint64_t* u64_Scan(const void* module, const uint64_t sizeOfImage, const uint64_t value)
{
    uint8_t* scanBytes = (uint8_t*)module;
    for (size_t i = 0; i < sizeOfImage - sizeof(uintptr_t); ++i)
    {
        uintptr_t* p = (uintptr_t*)&scanBytes[i];
        uint64_t currentValue = *p;
        if (currentValue == value)
        {
            return (uint64_t*)&scanBytes[i];
        }
    }
    return 0;
}

void* Mem_Scan(const void* module, const uint64_t sizeOfImage, const void* value, const size_t value_sz)
{
    uint8_t* scanBytes = (uint8_t*)module;
    for (size_t i = 0; i < sizeOfImage - value_sz; ++i)
    {
        uint8_t* v = &scanBytes[i];
        if (memcmp(v, value, value_sz) == 0)
        {
            return v;
        }
    }
    return 0;
}

extern int sysctl(const int* name, unsigned int namelen, void* oldp, size_t* oldlenp, const void* newp, size_t newlen);

// https://github.com/idc/ps4-experiments-405/blob/361738a2ee8a0fd32090c80bd2b49dae94ff08a5/hostapp_launch_patcher/source/patch.c#L57
// TODO: make addrstart match with
int get_code_info(const int pid, const void* addrstart, uint64_t* paddress, uint64_t* psize, const uint32_t page_idx)
{
    int mib[4] = {1, 14, 32, pid};
    size_t size = 0, count = 0;
    char* data;
    char* entry;

    if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0)
    {
        return -1;
    }

    if (size == 0)
    {
        return -2;
    }

    data = (char*)malloc(size);
    if (data == NULL)
    {
        return -3;
    }

    if (sysctl(mib, 4, data, &size, NULL, 0) < 0)
    {
        free(data);
        return -4;
    }

    int struct_size = *(int*)data;
    count = size / struct_size;
    entry = data;

    int found = 0;
    int valid = 0;
    uint32_t idx = 0;
    uint64_t first_addr = 0;
    while (count != 0)
    {
        int type = *(int*)(&entry[0x4]);
        uint64_t start_addr = *(uint64_t*)(&entry[0x8]);
        uint64_t end_addr = *(uint64_t*)(&entry[0x10]);
        uint64_t code_size = end_addr - start_addr;
        uint32_t prot = *(uint32_t*)(&entry[0x38]);

        if (addrstart && start_addr == (uint64_t)addrstart)
        {
            valid = 1;
            idx = 0;
            first_addr = start_addr;
        }
        else if (!first_addr && addrstart)
        {
            goto next;
        }

        printf("idx %d page_idx %d\n", idx, page_idx);
        printf("%d %lx %lx (%lu) %x\n", type, start_addr, end_addr, code_size, prot);

        if ((valid && idx == page_idx) || (page_idx == 0 && type == 9 && (prot == 4 || prot == 5)))
        {
            *paddress = start_addr;
            *psize = code_size;
            found = 1;
            break;
        }

next:
        entry += struct_size;
        count--;
        idx++;
    }

    free(data);
    return !found ? -5 : 0;
}

// ps4-payload-sdk
int findProcess(const char* procName)
{
    int procPID = 0;
    while (!procPID)
    {
        const int CTL_KERN = 1, KERN_PROC = 14, KERN_PROC_ALL = 0;
        struct kinfo_proc
        {
            int structSize;
            int layout;
            void* args;
            void* paddr;
            void* addr;
            void* tracep;
            void* textvp;
            void* fd;
            void* vmspace;
            void* wchan;
            int pid;
            char useless[0x173];
            char name[];
        };
        int mib[3];
        size_t len = 0;
        mib[0] = CTL_KERN;
        mib[1] = KERN_PROC;
        mib[2] = KERN_PROC_ALL;

        if (sysctl(mib, 3, NULL, &len, NULL, 0) != -1)
        {
            if (len > 0)
            {
                void* dump = malloc(len);
                if (dump == NULL)
                {
                    return -1;
                }
                if (sysctl(mib, 3, dump, &len, NULL, 0) != -1)
                {
                    int structSize = *(int*)dump;
                    for (size_t i = 0; i < (len / structSize); i++)
                    {
                        struct kinfo_proc* procInfo = (struct kinfo_proc*)(dump + (i * structSize));
                        if (!strcmp(procInfo->name, procName))
                        {
                            procPID = procInfo->pid;
                            break;
                        }
                    }
                }
                free(dump);
            }
        }
        sceKernelUsleep(1);
    }
    return procPID;
}

uintptr_t pid_chunk_scan(const int pid, const uintptr_t mem_start, const uintptr_t mem_sz, const char* pattern, const size_t pattern_offset)
{
#define chunk_size (8ul * 1024)
    uintptr_t found = 0;
    uint8_t mem[chunk_size];
    for (size_t i = 0; i < (mem_sz - chunk_size); i += chunk_size)
    {
        if (i % (chunk_size * 8))
        {
            debug_printf("scanning pid %d (%lu/%lu) mem 0x%p\n", pid, i, mem_sz, mem);
        }
        const uintptr_t chunk_start = mem_start + i;
        sys_proc_rw(pid, chunk_start, (const void*)mem, chunk_size, 0);
        const uintptr_t mem_start = (uintptr_t)mem;
        const uintptr_t found_pattern = PatternScan(mem, chunk_size, pattern, pattern_offset);
        if (found_pattern)
        {
            const uintptr_t chunk_offset = found_pattern - mem_start;
            const uintptr_t chunk_found = chunk_start + chunk_offset;
            found = chunk_found;
            printf("found data at 0x%lx 0x%lx chunk loc 0x%lx offset 0x%lx\n", found_pattern, chunk_offset, chunk_start, chunk_found);
            break;
        }
        sceKernelUsleep(1);
    }
    puts("free mem");
    if (!found)
    {
        printf(
            "couldn't find pattern\n"
            "%s\n"
            "pid %d\n",
            pattern,
            pid);
    }
    return found;
#undef chunk_size
}

uintptr_t* findSymbolPtrInEboot(const char* module, const char* symbol_name)
{
    uintptr_t symbol = 0;
    if (!symbol_name)
    {
        return 0;
    }
    {
        const int handle = sceKernelLoadStartModule(module, 0, 0, 0, 0, 0);
        printf("%s load 0x%08x\n", module, handle);
        if (handle > 0)
        {
            sceKernelDlsym(handle, symbol_name, (void**)&symbol);
            printf("symbol %s resolved to %lx\n", symbol_name, symbol);
        }
    }
    if (!symbol)
    {
        return 0;
    }
    struct OrbisKernelModuleInfo info = {0};
    info.size = sizeof(info);
    const int r = sceKernelGetModuleInfo(0, &info);
    printf("sceKernelGetModuleInfoEx 0x%08x\n", r);
    uintptr_t* p = 0;
    if (r == 0)
    {
        const uint32_t mm = 0;
        const void* mm_p = info.segmentInfo[mm].address;
        uintptr_t pPage = 0;
        uint64_t pPageSz = 0;
        const int codeerr = get_code_info(getpid(), mm_p, &pPage, &pPageSz, 1);
        printf("codeerr: %d\n", codeerr);
        printf("pPage 0x%lx size %ld bytes\n", pPage, pPageSz);
        if (codeerr != 0)
        {
            return 0;
        }
        p = u64_Scan((const void*)pPage, pPageSz, symbol);
        if (p)
        {
            printf("found %s at 0x%p -> 0x%lx\n", symbol_name, p, symbol);
        }
    }
    return p;
}

const char* char_Scan(const void* module, const size_t sizeOfImage, const char* value)
{
    char* scanBytes = (char*)module;
    const size_t value_len = strlen(value);
    for (size_t i = 0; i < sizeOfImage - value_len; ++i)
    {
        const char* currentValue = (const char*)&scanBytes[i];
        if (strncmp(currentValue, value, value_len) == 0)
        {
            return &scanBytes[i];
        }
    }
    return 0;
}

void PatchInternalCallList(const uintptr_t textbase, const uint64_t textsz, const char* target_symbol, uintptr_t* target_call, uintptr_t dest_call)
{
    final_printf("input %s\n", target_symbol);
    const char* mono_sym = char_Scan((void*)textbase, textsz, target_symbol);
    if (!mono_sym)
    {
        return;
    }
    final_printf("input found 0x%p\n", mono_sym);
    uintptr_t pPage = 0;
    uintptr_t pSize = 0;
    const int pid = getpid();
    if (get_code_info(pid, (void*)textbase, &pPage, &pSize, 1) == 0)
    {
        const uintptr_t u64_mono_sym = (uintptr_t)mono_sym;
        const uintptr_t* p = u64_Scan((void*)pPage, pSize, u64_mono_sym);
        if (p)
        {
            printf("found symbol at 0x%p (%s) func 0x%lx\n", p, (const char*)p[0], p[1]);
            *target_call = p[1];
            const uintptr_t func = (uintptr_t)dest_call;
            sys_proc_rw(pid, (uintptr_t)&p[1], &func, sizeof(func), 1);
        }
    }
}

void PatchCall_Internal(const struct OrbisKernelModuleInfo* info, const char* p, const size_t offset, const uintptr_t target, uintptr_t* original, const char* verbose)
{
    const uintptr_t start_addr = (uintptr_t)info->segmentInfo[0].address;
    const uint64_t start_size = info->segmentInfo[0].size;
    const uintptr_t call = PatternScan((void*)start_addr, start_size, p, offset);
    final_printf("Pattern scan for\n%s\nreturned 0x%lx\n", verbose, call);
    if (call)
    {
        Make32to64Jmp(start_addr, start_size, call, target, 5, true, original);
    }
}

static size_t GetInstructionSize(uint64_t Address, size_t MinSize)
{
    size_t InstructionSize = 0;

    if (!Address)
    {
        return 0;
    }

    while (InstructionSize < MinSize)
    {
        // hde64_disasm always clears it
        hde64s hs = {};
        uint32_t temp = hde64_disasm((void*)(Address + InstructionSize), &hs);

        if (hs.flags & F_ERROR)
        {
            return 0;
        }

        InstructionSize += temp;
    }

    return InstructionSize;
}

// Allocation-less version of a Prologue hook
// I use pre-allocated `returnPad`, copy instructions to it and write instructions to it
static size_t caveInstSize = 0;
static void CaveBlockInit(void)
{
    static bool once = false;
    if (!once)
    {
        const int pid = getpid();
        //sceKernelMprotect(cavePad, cavePadSize, 7);
        static const uint8_t m[] = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3};
        sys_proc_rw(pid, (uintptr_t)cavePadFunc, m, sizeof(m), 1);
        int (*test)(void);
        test = (void*)cavePadFunc;
        //memcpy(cavePad, m, sizeof(m));
        final_printf("checking executable code, it returned %d\n", test());
        sys_proc_memset(pid, (uintptr_t)cavePadFunc, 0xcc, MAX_CAVE_SIZE);
        //memset(cavePad, 0xcc, cavePadSize);
        // DWORD temp = caveInstSize = 0;
        // VirtualProtect(cavePad, cavePadSize, PAGE_EXECUTE_WRITECOPY, &temp);
        once = true;
        printf("cavePad setup at 0x%p! Size %ld\n", (void*)cavePadFunc, MAX_CAVE_SIZE);
    }
}
static bool validnateBlockSize(const size_t cavePadSize, const size_t newSize)
{
    if (newSize > cavePadSize)
    {
        printf("Block size %ld is smaller than requested size %ld!\n", cavePadSize, newSize);
        return 0;
    }
    return 1;
}
uintptr_t CreatePrologueHook(const uintptr_t address, const int min_instruction_size)
{
    CaveBlockInit();
    if (!address || min_instruction_size < 5)
    {
        return 0;
    }
    const size_t int_size = GetInstructionSize(address, min_instruction_size);
    if (!int_size || !validnateBlockSize(MAX_CAVE_SIZE, caveInstSize + int_size + sizeof(JMPstub)))
    {
        return 0;
    }
    const size_t addroffset = sizeof(JMPstub);
    const uintptr_t ucavePad = (uintptr_t)cavePadFunc;
    const uintptr_t ucavePadNew = ucavePad + caveInstSize;
    const uintptr_t retaddr = address + int_size;
    const int pid = getpid();
    sys_proc_rw(pid, ucavePadNew, (void*)address, int_size, 1);
    sys_proc_rw(pid, ucavePadNew + int_size, &JMPstub, addroffset, 1);
    sys_proc_rw(pid, ucavePadNew + int_size + addroffset, &retaddr, sizeof(retaddr), 1);
    const uintptr_t caveAddr = ucavePad + caveInstSize;
    const uint64_t new_cave_size = int_size + (sizeof(JMPstub) + sizeof(uintptr_t));
    caveInstSize += new_cave_size;
    printf("New caveInstSize: %ld\n", caveInstSize);
    hex_dump(caveAddr, new_cave_size, caveAddr);
    return caveAddr;
}

