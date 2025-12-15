#include <stdint.h>
#include <orbis/libkernel.h>

int32_t Read_File(const char* input_file, char** file_data, uint64_t* filesize, uint32_t extra);
int32_t Write_File(const char* input_file, const void* file_data, uint64_t filesize);

int32_t get_module_info(OrbisKernelModuleInfo moduleInfo, const char* name, uint64_t* base, uint32_t* size);
uint32_t pattern_to_byte(const char* pattern, uint8_t* bytes);

/*
 * @brief Scan for a given byte pattern on a module
 *
 * @param module_base Base of the module to search
 * @param module_size Size of the module to search
 * @param signature   IDA-style byte array pattern
 * @credit            https://github.com/OneshotGH/CSGOSimple-master/blob/59c1f2ec655b2fcd20a45881f66bbbc9cd0e562e/CSGOSimple/helpers/utils.cpp#L182
 * @returns           Address of the first occurrence
 */
uint8_t* PatternScan(uint64_t module_base, uint32_t module_size, const char* signature);
