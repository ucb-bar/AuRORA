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

#define MAT_DIM 384//288
#define NUM_LAYER 4

#define NUM_CORE 2
#define NUM_ACCEL 4

elem_t A[NUM_CORE][MAT_DIM][MAT_DIM] row_align(1) = {0};
elem_t B[NUM_CORE][MAT_DIM][MAT_DIM] row_align(1) = {0};
elem_t C[NUM_CORE][MAT_DIM][MAT_DIM] row_align(1) = {0};

static bool thread_done = false;

void thread_entry (int cid, int nc){
    for(int i = 0; i < nc; i++){
      if (i == cid) printf("Thread %d/%d starting\n", cid, nc);
      barrier(nc);
    }

    // set target cycles differently
    uint64_t target_cycles = cid == 0 ? 550000 : 550000*2;//.2;

    for(int i = 0; i < nc; i++){
      if (i == cid) printf("Thread %d target cycles: %llu\n", cid, target_cycles);
      barrier(nc);
    }

    uint64_t total_cycles = {0};
    int layer_acquired_accel[NUM_LAYER] = {0};
    int layer_requested_accel[NUM_LAYER] = {0};
    int count_waiting[NUM_LAYER] = {0};

    for(int j = 0; j < nc; j++){
      if(j == cid){
        int start = read_cycles();
        for(int layer = 0; layer < NUM_LAYER; layer++){
          int acquired_accel = 0;
          while(acquired_accel == 0){
            int layer_target_cycles = (target_cycles - ((int)(read_cycles()) - start)) / (NUM_LAYER - layer);
            // number of accel to request
            int num_want_accel = thread_done ? NUM_ACCEL : calc_num_accel_needed_matmul(MAT_DIM, MAT_DIM, MAT_DIM, layer_target_cycles, NUM_ACCEL);
            layer_requested_accel[layer] = num_want_accel;
            // attempt to acquire accel
            // number of accel acquired
            acquired_accel = multi_acquire(0, num_want_accel, 0, NUM_ACCEL);
            count_waiting[layer] ++;
          }
          layer_acquired_accel[layer] = acquired_accel;

          tiled_rerocc_matmul_auto(MAT_DIM, MAT_DIM, MAT_DIM,
                  (elem_t*)A[cid], (elem_t*)B[cid], NULL, (elem_t*)C[cid],
                  MAT_DIM, MAT_DIM, MAT_DIM, MAT_DIM,
                  1, 1, 1,
                  NO_ACTIVATION, ACC_SCALE_IDENTITY, 0, false,
                  false, false,
                  false, false,
                  0,
                  acquired_accel);
          // release all 
          multi_release(0, acquired_accel);
        }
        int end = read_cycles();
        thread_done = true;
        total_cycles = end - start;
        
      }
    }
    barrier(nc);

    for(int i = 0; i < nc; i++){
      if(i == cid){
        printf("thread %d total cycles: %d\n", i, total_cycles);
        printf("thread %d layer requested accel: ", i);
        for (int layer = 0; layer < NUM_LAYER; layer++){
          printf("%d, ", layer_requested_accel[layer]);
        }
        printf("\n");
        printf("thread %d layer acquired accel: ", i);
        for (int layer = 0; layer < NUM_LAYER; layer++){
          printf("%d, ", layer_acquired_accel[layer]);
        }
        printf("\n"); 
        printf("thread %d waiting: ", i);
        for (int layer = 0; layer < NUM_LAYER; layer++){
          printf("%d, ", count_waiting[layer]);
        }
        printf("\n"); 
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
