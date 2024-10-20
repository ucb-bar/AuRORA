// This test enables multiple threads to request accelerator simulatenously, which induces contention
// record how many accelerators it ended up acquiring for each turn
// perform small resadd after succeeding acquire
// need to enable multithread in risv-test
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef BAREMETAL
#include <sys/mman.h>
#endif
#include "util.h"
#include "include/gemmini_testutils.h"
#include "aurora-sw/aurora.h"
#include "aurora-sw/gemmini_aurora.h"

#define MAT_DIM 288
#define TURN 1
#define NUM_LAYER 4

#define NUM_CORE 2
#define NUM_ACCEL 4

elem_t A[NUM_CORE][MAT_DIM][MAT_DIM] row_align(1) = {0};
elem_t B[NUM_CORE][MAT_DIM][MAT_DIM] row_align(1) = {0};
elem_t C[NUM_CORE][MAT_DIM][MAT_DIM] row_align(1) = {0};

void thread_entry (int cid, int nc){
    for(int i = 0; i < nc; i++){
      if (i == cid) printf("Thread %d/%d starting\n", cid, nc);
      barrier(nc);
    }
    uint64_t target_cycles = cid == 0 ? 220000 : 220000*2.3;

    for(int i = 0; i < nc; i++){
      if (i == cid) printf("Thread %d target cycles: %llu\n", cid, target_cycles);
      barrier(nc);
    }

    uint64_t total_cycles[TURN] = {0};
    int layer_acquired_accel[TURN][NUM_LAYER] = {0};
    int layer_requested_accel[TURN][NUM_LAYER] = {0};
    int waiting_spin[TURN][NUM_LAYER] = {0};

    for(int j = 0; j < nc; j++){
      if(j == cid){
        for(int t = 0; t < TURN; t++){
          int turn_start = read_cycles();
          for(int layer = 0; layer < NUM_LAYER; layer++){
            int acquired_accel = 0;
            while(acquired_accel == 0){
              int layer_target_cycles = (target_cycles - ((int)(read_cycles()) - turn_start)) / (NUM_LAYER - layer);
              int num_want_accel = calc_num_accel_needed_matmul(MAT_DIM, MAT_DIM, MAT_DIM, layer_target_cycles, NUM_ACCEL);
              layer_requested_accel[t][layer] = num_want_accel;
              acquired_accel = multi_acquire(0, num_want_accel, 0, NUM_ACCEL);
              waiting_spin[t][layer] ++;
              //if (num_want_accel != 0) printf("thread %d target %d layer %d requested %d, acquired %d\n", cid, layer_target_cycles, layer, num_want_accel, acquired_accel);
            }
            layer_acquired_accel[t][layer] = acquired_accel;

            tiled_rerocc_matmul_auto(MAT_DIM, MAT_DIM, MAT_DIM,
                    (elem_t*)A[cid], (elem_t*)B[cid], NULL, (elem_t*)C[cid],
                    MAT_DIM, MAT_DIM, MAT_DIM, MAT_DIM,
                    1, 1, 1,
                    NO_ACTIVATION, ACC_SCALE_IDENTITY, 0, false,
                    false, false,
                    false, false,
                    0,
                    acquired_accel);
             multi_release(0, acquired_accel);
          }
          int turn_end = read_cycles();
          total_cycles[t] = turn_end - turn_start;
        }
      }
    }
    barrier(nc);

    for(int i = 0; i < nc; i++){
      if(i == cid){
        printf("thread %d total cycles: ", i);
        for (int t = 0; t < TURN; t++){
          printf("%llu, ", total_cycles[t]);
        }
        printf("\n");
        printf("thread %d layer requested accel: ", i);
        for (int t = 0; t < TURN; t++){
          for (int layer = 0; layer < NUM_LAYER; layer++){
            printf("%d, ", layer_requested_accel[t][layer]);
          }
          printf("\n");
        }
        printf("thread %d layer acquired accel: ", i);
        for (int t = 0; t < TURN; t++){
          for (int layer = 0; layer < NUM_LAYER; layer++){
            printf("%d, ", layer_acquired_accel[t][layer]);
          }
          printf("\n");
        }
        printf("thread %d waiting: ", i);
        for (int t = 0; t < TURN; t++){
          for (int layer = 0; layer < NUM_LAYER; layer++){
            printf("%d, ", waiting_spin[t][layer]);
          }
          printf("\n");
        }
      }
      barrier(nc);
    }

    exit(0);
}

int main() {
#ifndef BAREMETAL
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
      perror("mlockall failed");
      exit(1);
    }
#endif
  exit(0);
}
