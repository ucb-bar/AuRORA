// See LICENSE for license details.
// This singlecore testbench acquires 1 GEMM accelerator, compute resadd, and check the result
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef BAREMETAL
#include <sys/mman.h>
#endif
#define FLOAT false
#include "include/gemmini_testutils.h"
#include "aurora-sw/rerocc.h"

#define CHECK_RESULT 1

#ifndef BAREMETAL

#define MAT_DIM_I 128
#define MAT_DIM_J 512

#else
#define MAT_DIM_I 238
#define MAT_DIM_J 37 
#endif

#define A_SCALE 2
#define B_SCALE MVIN_SCALE_IDENTITY
#define C_SCALE ACC_SCALE_IDENTITY
#define USE_RELU true

// number of accelerator in the system
#define NUM_ACCEL 4
// CFGID would like to assign to
#define CFGID 2

void full_printMatrix(elem_t m[MAT_DIM_I][MAT_DIM_J]) {
  for (size_t i = 0; i < MAT_DIM_I; ++i) {
    for (size_t j = 0; j < MAT_DIM_J; ++j)
      printf("%d ", m[i][j]);
    printf("\n");
  }
}

int full_is_equal(elem_t x[MAT_DIM_I][MAT_DIM_J], elem_t y[MAT_DIM_I][MAT_DIM_J]) {
  for (size_t i = 0; i < MAT_DIM_I; ++i)
    for (size_t j = 0; j < MAT_DIM_J; ++j)
      if (x[i][j] != y[i][j])
        return 0;
  return 1;
}

int main() {
#ifndef BAREMETAL
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
      perror("mlockall failed");
      exit(1);
    }
#endif

    printf("I: %d, J: %d\n", MAT_DIM_I, MAT_DIM_J);

    int cfgid = CFGID;

    for(int i = 0; i < NUM_ACCEL; i++){
        bool acquired = rr_acquire_single(cfgid, i);
        if(acquired){
            printf("gemmini %d acquired to cfgid %d\n", i, cfgid);
            break;
        }
    }
    rr_set_opc(XCUSTOM_ACC, cfgid);
    gemmini_flush(0);

    static elem_t A[MAT_DIM_I][MAT_DIM_J] row_align(MAX_BLOCK_LEN);
    static elem_t B[MAT_DIM_I][MAT_DIM_J] row_align(MAX_BLOCK_LEN);
    static elem_t C[MAT_DIM_I][MAT_DIM_J] row_align(MAX_BLOCK_LEN);
    static elem_t gold[MAT_DIM_I][MAT_DIM_J];

#if CHECK_RESULT == 1
    // printf("Init A and B\n");
    for (size_t i = 0; i < MAT_DIM_I; ++i) {
      for (size_t j = 0; j < MAT_DIM_J; ++j) {
        A[i][j] = (rand() % 64) - 32;
        B[i][j] = (rand() % 8) - 4;
      }
    }

#endif

    printf("Starting gemmini resadd\n");
    unsigned long start = read_cycles();
    tiled_resadd_auto(MAT_DIM_I, MAT_DIM_J, A_SCALE, B_SCALE, C_SCALE, (elem_t*)A, (elem_t*)B,
            (elem_t*)C, USE_RELU, WS);
    
    rr_fence(cfgid);
    unsigned long end = read_cycles();
    printf("Cycles taken: %u\n", end-start);
    
    rr_release(cfgid);

#if CHECK_RESULT == 1
    printf("Starting slow CPU resadd\n");
    unsigned long cpu_start = read_cycles();
    resadd_cpu(MAT_DIM_I, MAT_DIM_J, MAT_DIM_J, A_SCALE, B_SCALE, C_SCALE, (elem_t*)A, (elem_t*)B,
            (elem_t*)gold, USE_RELU);
    unsigned long cpu_end = read_cycles();
    printf("Cycles taken: %u\n", cpu_end-cpu_start);
    
    if (!full_is_equal(C, gold)) {
      printf("C:\n");
      full_printMatrix(C);
      printf("Gold:\n");
      full_printMatrix(gold);
      printf("A:\n");
      full_printMatrix(A);
      printf("B:\n");
      full_printMatrix(B);
      printf("\n");

      exit(1);
    }
#endif

  exit(0);
}

