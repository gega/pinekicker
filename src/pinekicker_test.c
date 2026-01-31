// gcc -o pinekicker_test -DUNIT_TEST pinekicker_test.c
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
#ifdef UNIT_TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 *                           \A| sl wr
 *                          B \| NEW   |  TESTING  | CONFIRMED  | FAILED
 *      -----------------------+-----------------------------------------
 *       SLOT_STATUS_NEW       | ver 1 |     B  2  |     B   1  |   B  1
 *       SLOT_STATUS_TESTING   |  A  2 |     -  2  |     A   1  |   -  1
 *       SLOT_STATUS_CONFIRMED |  A  1 |     B  1  |    ver  0  |   B  0
 *       SLOT_STATUS_FAILED    |  A  1 |     -  1  |     A   0  |   -  0
 *
 *  sl: selected slot
 *  wr: number of status updates (transition from NEW->TESTING or TESTING->FAILED
 *
 */


#include "pinekicker.h"
#include "sha256.c"
#include "uECC.c"

uintptr_t SLOT_BASE_A;
uintptr_t SLOT_BASE_B;
uintptr_t glob_jump_base;
int glob_flash_writes;
int glob_found;

#include "pinekicker.c"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) ((sizeof x) / (sizeof *x))
#endif

#define ARR_MIN_SIZE (SLOT_SCAN_LIMIT/sizeof(struct slot_header))

struct slot_header slot_001_a[ARR_MIN_SIZE+2]={0};
struct slot_header slot_001_b[ARR_MIN_SIZE+2]={0};

#define SLOT_A (0)
#define SLOT_B (1)
#define SLOT_NONE (2)
#define SLOT_x SLOT_NONE

struct ut_pk
{
  uint32_t status_a;
  uint32_t status_b;
  uint32_t ver_a;
  uint32_t ver_b;
  int result_slot;
  int flash_writes;
  int found;
};

const struct ut_pk utpk[]=
{
//           status A         		 status B                 ver A  ver B   res    wr  fnd
 /*  0 */ { SLOT_STATUS_NEW, 		SLOT_STATUS_NEW,           2,     1,    SLOT_A, 1,   2  },
 /*  1 */ { SLOT_STATUS_NEW, 		SLOT_STATUS_NEW, 	   1,     2,    SLOT_B, 1,   2  },
 /*  2 */ { SLOT_STATUS_NEW, 		SLOT_STATUS_TESTING, 	   0,     0,    SLOT_A, 2,   2  },
 /*  3 */ { SLOT_STATUS_NEW, 		SLOT_STATUS_TESTING, 	   0,     1,    SLOT_A, 2,   2  },
 /*  4 */ { SLOT_STATUS_NEW, 		SLOT_STATUS_TESTING, 	   1,     0,    SLOT_A, 2,   2  },
 /*  5 */ { SLOT_STATUS_NEW, 		SLOT_STATUS_CONFIRMED, 	   0,     0,    SLOT_A, 1,   2  },
 /*  6 */ { SLOT_STATUS_NEW, 		SLOT_STATUS_CONFIRMED, 	   0,     1,    SLOT_A, 1,   2  },
 /*  7 */ { SLOT_STATUS_NEW, 		SLOT_STATUS_CONFIRMED, 	   1,     0,    SLOT_A, 1,   2  },
 /*  8 */ { SLOT_STATUS_NEW, 		SLOT_STATUS_FAILED, 	   0,     0,    SLOT_A, 1,   2  },
 /*  9 */ { SLOT_STATUS_NEW, 		SLOT_STATUS_FAILED, 	   0,     1,    SLOT_A, 1,   2  },
 /* 10 */ { SLOT_STATUS_NEW, 		SLOT_STATUS_FAILED, 	   1,     0,    SLOT_A, 1,   2  },
 /* 11 */ { SLOT_STATUS_TESTING, 	SLOT_STATUS_NEW, 	   2,     1,    SLOT_B, 2,   2  },
 /* 12 */ { SLOT_STATUS_TESTING, 	SLOT_STATUS_NEW,  	   1,     2,    SLOT_B, 2,   2  },
 /* 13 */ { SLOT_STATUS_TESTING, 	SLOT_STATUS_TESTING, 	   0,     0,    SLOT_x, 2,   2  },
 /* 14 */ { SLOT_STATUS_TESTING, 	SLOT_STATUS_TESTING, 	   0,     1,    SLOT_x, 2,   2  },
 /* 15 */ { SLOT_STATUS_TESTING, 	SLOT_STATUS_TESTING, 	   1,     0,    SLOT_x, 2,   2  },
 /* 16 */ { SLOT_STATUS_TESTING, 	SLOT_STATUS_CONFIRMED, 	   0,     0,    SLOT_B, 1,   2  },
 /* 17 */ { SLOT_STATUS_TESTING, 	SLOT_STATUS_CONFIRMED, 	   0,     1,    SLOT_B, 1,   2  },
 /* 18 */ { SLOT_STATUS_TESTING, 	SLOT_STATUS_CONFIRMED, 	   1,     0,    SLOT_B, 1,   2  },
 /* 19 */ { SLOT_STATUS_TESTING, 	SLOT_STATUS_FAILED, 	   0,     0,    SLOT_x, 1,   2  },
 /* 20 */ { SLOT_STATUS_TESTING, 	SLOT_STATUS_FAILED, 	   0,     1,    SLOT_x, 1,   2  },
 /* 21 */ { SLOT_STATUS_TESTING, 	SLOT_STATUS_FAILED, 	   1,     0,    SLOT_x, 1,   2  },
 /* 22 */ { SLOT_STATUS_CONFIRMED, 	SLOT_STATUS_NEW, 	   2,     1,    SLOT_B, 1,   2  },
 /* 23 */ { SLOT_STATUS_CONFIRMED, 	SLOT_STATUS_NEW,  	   1,     2,    SLOT_B, 1,   2  },
 /* 24 */ { SLOT_STATUS_CONFIRMED, 	SLOT_STATUS_NEW,  	   1,     1,    SLOT_B, 1,   2  },
 /* 25 */ { SLOT_STATUS_CONFIRMED, 	SLOT_STATUS_TESTING, 	   0,     0,    SLOT_A, 1,   2  },
 /* 26 */ { SLOT_STATUS_CONFIRMED, 	SLOT_STATUS_TESTING, 	   0,     1,    SLOT_A, 1,   2  },
 /* 27 */ { SLOT_STATUS_CONFIRMED, 	SLOT_STATUS_TESTING, 	   1,     0,    SLOT_A, 1,   2  },
 /* 28 */ { SLOT_STATUS_CONFIRMED, 	SLOT_STATUS_CONFIRMED, 	   0,     0,    SLOT_A, 0,   2  },
 /* 29 */ { SLOT_STATUS_CONFIRMED, 	SLOT_STATUS_CONFIRMED, 	   0,     1,    SLOT_B, 0,   2  },
 /* 30 */ { SLOT_STATUS_CONFIRMED, 	SLOT_STATUS_CONFIRMED, 	   1,     0,    SLOT_A, 0,   2  },
 /* 31 */ { SLOT_STATUS_CONFIRMED, 	SLOT_STATUS_FAILED, 	   0,     0,    SLOT_A, 0,   2  },
 /* 32 */ { SLOT_STATUS_CONFIRMED, 	SLOT_STATUS_FAILED, 	   0,     1,    SLOT_A, 0,   2  },
 /* 33 */ { SLOT_STATUS_CONFIRMED, 	SLOT_STATUS_FAILED, 	   1,     0,    SLOT_A, 0,   2  },
 /* 34 */ { SLOT_STATUS_FAILED, 	SLOT_STATUS_NEW, 	   2,     1,    SLOT_B, 1,   2  },
 /* 35 */ { SLOT_STATUS_FAILED, 	SLOT_STATUS_NEW,  	   1,     2,    SLOT_B, 1,   2  },
 /* 36 */ { SLOT_STATUS_FAILED, 	SLOT_STATUS_NEW,  	   1,     1,    SLOT_B, 1,   2  },
 /* 37 */ { SLOT_STATUS_FAILED, 	SLOT_STATUS_TESTING, 	   0,     0,    SLOT_x, 1,   2  },
 /* 38 */ { SLOT_STATUS_FAILED, 	SLOT_STATUS_TESTING, 	   0,     1,    SLOT_x, 1,   2  },
 /* 39 */ { SLOT_STATUS_FAILED, 	SLOT_STATUS_TESTING, 	   1,     0,    SLOT_x, 1,   2  },
 /* 40 */ { SLOT_STATUS_FAILED, 	SLOT_STATUS_CONFIRMED, 	   0,     0,    SLOT_B, 0,   2  },
 /* 41 */ { SLOT_STATUS_FAILED, 	SLOT_STATUS_CONFIRMED, 	   0,     1,    SLOT_B, 0,   2  },
 /* 42 */ { SLOT_STATUS_FAILED, 	SLOT_STATUS_CONFIRMED, 	   1,     0,    SLOT_B, 0,   2  },
 /* 43 */ { SLOT_STATUS_FAILED, 	SLOT_STATUS_FAILED, 	   0,     0,    SLOT_x, 0,   2  },
 /* 44 */ { SLOT_STATUS_FAILED, 	SLOT_STATUS_FAILED, 	   0,     1,    SLOT_x, 0,   2  },
 /* 45 */ { SLOT_STATUS_FAILED, 	SLOT_STATUS_FAILED, 	   1,     0,    SLOT_x, 0,   2  },

  {0,0,0,0,-1,0,0}
};


int main(int argc, char **argv)
{
  const int index[]={0, 5, ARR_MIN_SIZE-1, ARR_MIN_SIZE+1, -1 };
  int result,idx,ok;
  int i,j;
 
  for(i=0;utpk[i].result_slot>=0;i++)
  {
    ok=0;
    printf("TEST %3d ",i);
    for(j=0;index[j]>=0;j++)
    {
      glob_flash_writes=0;
      glob_jump_base=0;
      glob_found=0;
      SLOT_BASE_A=(uintptr_t)slot_001_a;
      SLOT_BASE_B=(uintptr_t)slot_001_b;
      idx=index[j];
      memset(slot_001_a,0xff,sizeof(slot_001_a));
      memset(slot_001_b,0xff,sizeof(slot_001_b));
      slot_001_a[idx].magic=SLOT_MAGIC;
      slot_001_b[idx].magic=SLOT_MAGIC;
      slot_001_a[idx].status=utpk[i].status_a;
      slot_001_b[idx].status=utpk[i].status_b;
      slot_001_a[idx].version=utpk[i].ver_a;
      slot_001_b[idx].version=utpk[i].ver_b;
      slot_001_a[idx].length=sizeof(slot_001_a);
      slot_001_b[idx].length=sizeof(slot_001_b);
      slot_001_a[idx].vtor_offset=0;
      slot_001_b[idx].vtor_offset=0;

      boot_main();

      if(glob_jump_base==SLOT_BASE_A) result=SLOT_A;
      else if(glob_jump_base==SLOT_BASE_B) result=SLOT_B;
      else result=SLOT_NONE;
      if(idx<ARR_MIN_SIZE)
      {
        if(    result               == utpk[i].result_slot
            && utpk[i].flash_writes == glob_flash_writes
            && utpk[i].found        == glob_found
          ) ok++;
        else
        {
          printf("FAILED\n");
          printf("--> j=%d wr=%d fnd=%d result=%d\n",j,glob_flash_writes,glob_found,result);
          break;
        }
      }
      else
      {
        if(result == SLOT_NONE && glob_found==0 && glob_flash_writes==0 ) ok++;
        else printf("FAILED\n");
      }
    }
    if(ok==j) printf("OK\n");
  }

  for(argv++;argc>1;argv++,argc--)
  {
    int len;
    bool result;
    uint8_t *slot;
    const struct slot_header *hdr;
    bool expected;
    
    expected=(argv[0][0]=='0'?false:true);
    FILE *f=fopen(&argv[0][1],"rb");
    if(f!=NULL)
    {
      fseek(f,0,SEEK_END);
      len=ftell(f);
      fseek(f,0,SEEK_SET);
      slot=malloc(len);
      SLOT_BASE_B=(uintptr_t)slot+len;
      fread(slot,1,len,f);
      hdr=find_slot_header((uintptr_t)slot);
      result=verify_signature((uintptr_t)slot, hdr);
      if(expected==result) printf("%s\tOK\n",&argv[0][1]);
      else printf("%s\tFAIL\tresult=%d expected=%d\n",&argv[0][1],result?1:0,expected?1:0);\
      free(slot);
      fclose(f);
    }
    else printf("Cannot open slot image '%s'\n",&argv[0][1]);
  }

  return(0);
}
#endif
