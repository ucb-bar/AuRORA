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

#define TURN 15

#define NUM_ARRAY 4
void thread_entry (int cid, int nc){
    for(int i = 0; i < nc; i++){
      if (i == cid) printf("Thread %d/%d starting\n", cid, nc);
      barrier(nc);
    }
    for(int n = 0; n < 3; n++){
      int num_requested_array[TURN] = {0};
      int num_acquired_array[TURN] = {0};
      for(int j = 0; j < nc; j++){
        if(j == cid){
          for(int t = 0; t < TURN; t++){ 
            int requested_array = (cid + rand()) % NUM_ARRAY; 
            if (requested_array == 0){
              requested_array = NUM_ARRAY - 1;
            }
            // number of requested accels
            num_requested_array[t] = requested_array;
            // attempt to acquire
            int acquired_array = multi_acquire(0, requested_array, 0, NUM_ARRAY);
            // number of acquired accels
            num_acquired_array[t] = acquired_array;
            for (int i = 0; i < acquired_array; i++) {
              rr_set_opc(XCUSTOM_ACC, i);
              gemmini_flush(0);
            }
            // release
            multi_release(0, acquired_array);
          }
        }
      }
      barrier(nc);

      for(int i = 0; i < nc; i++){
        if(i == cid){
          printf("round %d thread %d array req: ", n, i);
          for (int t = 0; t < TURN; t++){
            printf("%d, ", num_requested_array[t]);
          }
          printf("\n");
          printf("round %d thread %d array use: ", n, i);
          for (int t = 0; t < TURN; t++){
            printf("%d, ", num_acquired_array[t]);
          }
          printf("\n");
        }
        barrier(nc);
      }
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
