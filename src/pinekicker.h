/*
    BSD 2-Clause License

    Copyright (c) 2026, Gergely Gati

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef PINEKICKER_H
#define PINEKICKER_H

#include <stdint.h>
#include <stdbool.h>

#define SLOT_MAGIC 0x504B4B52u  // "PKKR"
#define SLOT_SCAN_LIMIT 0x1000  // first 4k

#define SLOT_STATUS_NEW         0xFFFFFFFFu
#define SLOT_STATUS_TESTING     0xFFFFFF00u
#define SLOT_STATUS_CONFIRMED   0xFFFF0000u
#define SLOT_STATUS_FAILED      0xFF000000u

#ifndef UNIT_TEST
#define SLOT_BASE_A ((uintptr_t)0x00008000)
#define SLOT_BASE_B ((uintptr_t)0x00042000)
#define PACKED __attribute__((packed))
#else
#define PACKED
#endif

#define HEADER_INIT_LENGTH (~0UL)
#define HEADER_INIT_LOAD_ADDR (~0UL)
#define HEADER_INIT_SIGNATURE {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}

struct PACKED slot_header
{
    uint32_t magic;		// init to SLOT_MAGIC		0
    uint32_t version;		//				4
    uint32_t vtor_offset;	//				8
    uint32_t status;		// init to SLOT_STATUS_NEW	12
    uint32_t length;		// init to 0xFF			16
    uint32_t load_addr;		// init to 0xFF			20
    uint8_t  signature[64];	// init to 0xFF			24
};

#endif
