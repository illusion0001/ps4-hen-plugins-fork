#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <orbis/libkernel.h>

#if !defined(MAX_CAVE_SIZE)
#define MAX_CAVE_SIZE 4096
#endif

static __attribute__((section(".text"))) uint8_t cavePad[MAX_CAVE_SIZE] = {};

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
uintptr_t PatternScan(const void* module_base, const uint64_t module_size, const char* signature, const uint64_t offset);

void WriteJump32(const uintptr_t src, const uintptr_t dst, const uint64_t len, const bool call);
void WriteJump64(const uintptr_t src, const uintptr_t dst);
uintptr_t ReadLEA32(uintptr_t Address, size_t offset, size_t lea_size, size_t lea_opcode_size);
void Make32to64Jmp(const uintptr_t textbase, const uintptr_t textsz, const uintptr_t src, const uintptr_t dst, const uint64_t srclen, const bool call, uintptr_t* src_out);
void hex_dump(const uintptr_t data, const uint64_t size, const uintptr_t real);

long orbis_syscall(long sysno, ...);
int sys_proc_rw(const int pid, const uintptr_t addr, const void* data, const uint64_t datasz, const uint64_t write_);
int sys_proc_memset(const int pid, const uintptr_t src, const uint32_t c, const uint64_t len);
uint64_t* u64_Scan(const void* module, const uint64_t sizeOfImage, const uint64_t value);

// https://github.com/idc/ps4-experiments-405/blob/361738a2ee8a0fd32090c80bd2b49dae94ff08a5/hostapp_launch_patcher/source/patch.c#L57
int get_code_info(const int pid, const void* addrstart, uint64_t* paddress, uint64_t* psize, const uint32_t page_idx);
int findProcess(const char* procName);
uintptr_t pid_chunk_scan(const int pid, const uintptr_t mem_start, const uintptr_t mem_sz, const char* pattern, const size_t pattern_offset);
uintptr_t* findSymbolPtrInEboot(const char* module, const char* symbol_name);
const char* char_Scan(const void* module, const size_t sizeOfImage, const char* value);
void PatchInternalCallList(const uintptr_t textbase, const uint64_t textsz, const char* target_symbol, uintptr_t* target_call, uintptr_t dest_call);
void PatchCall_Internal(const struct OrbisKernelModuleInfo* info, const char* p, const size_t offset, const uintptr_t target, uintptr_t* original, const char* verbose);
uintptr_t CreatePrologueHook(uint8_t* cavePad, const size_t cavePadSize, const uintptr_t address, const int min_instruction_size);

#define expression_to_str(...) __VA_ARGS__, #__VA_ARGS__
#define PatchCall(info, p, offset, target, original) PatchCall_Internal(expression_to_str(info, p, offset, target, original))
