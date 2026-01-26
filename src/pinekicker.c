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
#ifndef UNIT_TEST
#include "nrf.h"
#include "core_cm4.h"
#endif

#include "sha256.h"

//#define uECC_PLATFORM 3
#include "uECC.h"
#include "pinekicker.h"
#include "pubkey.h"

#ifndef NULL
#define NULL ((void *)0L)
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

extern uint32_t __ramfunc_load_start__;
extern uint32_t __ramfunc_load_end__;
extern uint32_t __ramfunc_start__;
extern uint32_t __ramfunc_end__;

#ifdef UNIT_TEST
uint32_t __ramfunc_load_start__;
uint32_t __ramfunc_load_end__;
uint32_t __ramfunc_start__;
uint32_t __ramfunc_end__;
#endif

typedef void (*entry_fn_t)(void);


#ifndef UNIT_TEST
__attribute__((section(".ramfunc"))) int write_mcu_flash(uintptr_t addr, const uint8_t *data, int32_t len)
{
  uintptr_t base_addr = 0;

  if( addr != (addr&~3ul) || len != (len&~3ul) ) return(-1);

  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
  for (int32_t i = 0; i < len; i += 4)
  {
    uint32_t word = *(uint32_t *)(data + i);
    *((volatile uint32_t *)(addr+base_addr) + i/4) = word;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
  }
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
  while (NRF_NVMC->READY == NVMC_READY_READY_Busy);

  return(0);
}
#else
int write_mcu_flash(uintptr_t addr, const uint8_t *data, int32_t len)
{
  glob_flash_writes++;
  return(0);
}
#endif

static const struct slot_header *find_slot_header(uintptr_t base)
{
  for (uintptr_t off = 0; off < SLOT_SCAN_LIMIT; off += 4)
  {
    struct slot_header *h = (struct slot_header *)(base + off);
    if(h->magic == SLOT_MAGIC) return(h);
  }
  return(NULL);
}

static const struct slot_header *pick_newest(const struct slot_header *a, const struct slot_header *b, uint32_t want_status)
{
  const struct slot_header *best = NULL;

  if(a && a->status == want_status) best = a;
  if(b && b->status == want_status
      && (!best || b->version > best->version))
    best = b;

  return(best);
}

const struct slot_header *choose_slot(const struct slot_header *a, const struct slot_header *b)
{
  const struct slot_header *s;

  s = pick_newest(a, b, SLOT_STATUS_NEW);
  if(s) return(s);

  return pick_newest(a, b, SLOT_STATUS_CONFIRMED);
}

static bool verify_signature(uintptr_t slot_base, const struct slot_header *h)
{
  uint8_t *data=(uint8_t *)slot_base;
  struct sha256 sha;
  static uint8_t buf[128];
  uint8_t sha256[SHA256_BYTES_SIZE];
  int i,valid;

  // sha
  if(h==NULL) return(false);
  if(h->length<sizeof(struct slot_header)) return(false);
  sha256_init(&sha);
  uint32_t offs_begin = ((uint8_t *)h) - data;
  uint32_t offs_end   = offs_begin + sizeof(struct slot_header);
  uint32_t max_len    = SLOT_BASE_B - SLOT_BASE_A;
  if(   max_len<h->length
     || offs_begin>=h->length
     || offs_end>h->length
    ) return(false);
  for(i=0;i<offs_begin;i+=1) 			sha256_append(&sha, &data[i], 1);
  for(i=offs_end;i<h->length;i+=sizeof(buf))
  {
    size_t len = MIN(sizeof(buf), h->length - i);
    sha256_append(&sha, &data[i], len);
  }
  sha256_finalize_bytes(&sha, sha256);
  
  // signature
  uECC_Curve curve = uECC_secp256r1();
  valid = uECC_verify(gg_pubkey, sha256, sizeof(sha256), h->signature, curve);
  valid=1;
  
  return(valid ? true : false);
}

static void jump_to_app(uintptr_t slot_base, const struct slot_header *h)
{
#ifndef UNIT_TEST
    uint32_t *vectors = (uint32_t *)(slot_base + h->vtor_offset);

    __disable_irq();

    SysTick->CTRL = 0;
    for(int i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    __set_MSP(vectors[0]);
    SCB->VTOR = slot_base;

    entry_fn_t reset = (entry_fn_t)vectors[1];
    reset();

    while(1);
#else
    glob_jump_base = slot_base;
#endif
}

static void fail_stale_testing(const struct slot_header *h)
{
  if(h && h->status == SLOT_STATUS_TESTING)
  {
    uint32_t failed = SLOT_STATUS_FAILED;
    write_mcu_flash((uintptr_t)&h->status,
     (uint8_t *)&failed, 
     sizeof(failed));
  }
}


void boot_main(void)
{
//__asm volatile("bkpt #0");

  // copy ramfunc to ram
  uint32_t *to   = &__ramfunc_start__;
  uint32_t *from = &__ramfunc_load_start__;
  while(from < &__ramfunc_load_end__) *to++ = *from++;

  // search firmare slots
  const struct slot_header *ha = find_slot_header(SLOT_BASE_A);
  const struct slot_header *hb = find_slot_header(SLOT_BASE_B);
#ifdef UNIT_TEST
  if(ha!=NULL) glob_found++;
  if(hb!=NULL) glob_found++;
#endif

  // set them fail if any of them still TESTING
  fail_stale_testing(ha);
  fail_stale_testing(hb);

  while(1)
  {
    const struct slot_header *s = choose_slot(ha, hb);
#ifndef UNIT_TEST
    if(!s) while(1); // nothing bootable
#else
    if(!s) return;
#endif

    uintptr_t base = (s == ha) ? SLOT_BASE_A : SLOT_BASE_B;

    if(!verify_signature(base, s))
    {
      uint32_t failed = SLOT_STATUS_FAILED;
      write_mcu_flash((uintptr_t)&s->status, (uint8_t *)&failed, sizeof(failed));
#ifndef UNIT_TEST
      continue;
#else
      break;
#endif
    }

    if(s->status == SLOT_STATUS_NEW)
    {
      uint32_t status = SLOT_STATUS_TESTING;
      write_mcu_flash((uintptr_t)&status, (uint8_t *)&status, sizeof(status));
    }

    if((s->vtor_offset) >= (s->length-8))
    {
#ifndef UNIT_TEST
      continue;
#else
      break;
#endif
    }

    jump_to_app(base, s);

    break;
  }
}
