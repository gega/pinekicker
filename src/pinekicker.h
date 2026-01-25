#ifndef PINEKICKER_H
#define PINEKICKER_H

#include <stdint.h>
#include <stdbool.h>

#define SLOT_MAGIC 0x504B4B52u  // "PKKR"
#define SLOT_SCAN_LIMIT 0x1000  // first 4k

#define SLOT_STATUS_NEW         0xFFFFFFF0u
#define SLOT_STATUS_TESTING     0xFFFFFF00u
#define SLOT_STATUS_CONFIRMED   0xFFFFF000u
#define SLOT_STATUS_FAILED      0x00000000u

#ifndef UNIT_TEST
#define SLOT_BASE_A ((uintptr_t)0x00008000)
#define SLOT_BASE_B ((uintptr_t)0x00042000)
#define PACKED __attribute__((packed))
#else
#define PACKED
#endif

struct PACKED slot_header {
    uint32_t magic;
    uint32_t version;
    uint32_t status;
    uint32_t length;
    uint8_t  signature[64];
};

#endif
