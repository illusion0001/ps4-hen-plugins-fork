#include <stdint.h>
#include <stdbool.h>

unsigned char* hexstrtochar2(const char* hexstr, size_t* size);
void sys_proc_rw(uint64_t address, void* data, uint64_t length);
bool hex_prefix(const char* str);

// http://www.cse.yorku.ca/~oz/hash.html
constexpr inline uint64_t djb2_hash(const char* str);

uint64_t patch_hash_calc(const char* title, const char* name, const char* app_ver, const char* title_id, const char* elf);
void patch_data1(const char* patch_type_str, uint64_t addr, const char* value, uint32_t source_size, uint64_t jump_target);
