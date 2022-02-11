#ifndef CRC32_CALC_HEADER
#define CRC32_CALC_HEADER

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// param[in] dir - path to directory
// param[in] file - file name in directory
// return crc32 sum for file (0 - file not found or system error)
uint32_t calc_file_crc32(const char *dir, const char *file);

#ifdef __cplusplus
}
#endif

#endif // CRC32_CALC_HEADER
