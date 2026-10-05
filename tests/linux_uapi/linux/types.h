/* Host simulation only: fixed-width types used by Linux v6.6 UAPI fixtures. */
#ifndef TEST_LINUX_TYPES_H
#define TEST_LINUX_TYPES_H
#include <stdint.h>
typedef uint64_t __u64;
typedef uint32_t __u32;
typedef uint16_t __u16;
typedef uint8_t __u8;
typedef int32_t __s32;
#define __aligned_u64 __u64 __attribute__((aligned(8)))
#endif
