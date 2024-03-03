// See LICENSE for license details.
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef BAREMETAL
#include <sys/mman.h>
#include <pthread.h>
#endif
#define NUM_CORE 4

#include "include/gemmini_testutils.h"
#include "aurora-sw/aurora.h"

#define MAT_DIM_I 512
#define MAT_DIM_K 512
#define MAT_DIM_J 512

#define NO_BIAS false
#define FULL_BIAS_WIDTH 1

#if FULL_BIAS_WIDTH
typedef acc_t ACC_T;
#else
typedef elem_t ACC_T;
#endif
#define NUM_ACCEL 4

pthread_barrier_t barrier_global;

struct thread_args{
    int cid, cycles;
};
static elem_t full_A[MAT_DIM_I][MAT_DIM_K] row_align(1);
static elem_t full_B[MAT_DIM_K][MAT_DIM_J] row_align(1);
static elem_t full_C[NUM_CORE][MAT_DIM_I][MAT_DIM_J] row_align(1);
static ACC_T full_D[MAT_DIM_I][MAT_DIM_J] row_align_acc(1);

//static full_t gold_full[MAT_DIM_I][MAT_DIM_J];
static elem_t gold[MAT_DIM_I][MAT_DIM_J];


void full_matmul(elem_t A[MAT_DIM_I][MAT_DIM_K], elem_t B[MAT_DIM_K][MAT_DIM_J], ACC_T D[MAT_DIM_I][MAT_DIM_J], elem_t C_full[MAT_DIM_I][MAT_DIM_J]) {
  for (size_t r = 0; r < MAT_DIM_I; r++)
    for (size_t c = 0; c < MAT_DIM_J; c++) {
      C_full[r][c] = (elem_t) D[r][c];
      for (size_t k = 0; k < MAT_DIM_K; k++)
        C_full[r][c] += A[r][k]*B[k][c];
    }
}

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

void *thread_test(void *arg){
    //pthread_barrier_wait(&barrier_global);
    struct thread_args * nn_args = (struct thread_args *) arg;
    int cid = nn_args->cid;
    int cfgid = 0;
    pthread_barrier_wait(&barrier_global);

    uint64_t start = read_cycles();
    bool granted = false;
    while(!granted){
        single_acquire(cfgid, 0, NUM_ACCEL);
    }

    tiled_matmul_auto(MAT_DIM_I, MAT_DIM_J, MAT_DIM_K,
            (elem_t*)full_A, (elem_t*)full_B, NO_BIAS ? NULL : &full_D[0][0], (elem_t*)full_C[cid],
            MAT_DIM_K, MAT_DIM_J, MAT_DIM_J, MAT_DIM_J,
            MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
            NO_ACTIVATION, ACC_SCALE_IDENTITY, 0, false,
            false, false,
            false, !FULL_BIAS_WIDTH,
            0,
            WS);

    rr_fence(cfgid);
    uint64_t end = read_cycles();

    nn_args->cycles = end - start;

    rr_release(cfgid);
}


void *print_message(void *ptr){
    int cpu_id = sched_getcpu();
   // char *msg;
   // msg = (char *) ptr;
    printf("print msg - cpu_id: %d \n", cpu_id);
   // printf("%s \n", msg);
}

int main() {
#ifndef BAREMETAL
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
      perror("mlockall failed");
      exit(1);
    }
#endif
    int cpu_id;
    cpu_id = sched_getcpu();
    printf("main thread cpuid: %d \n", cpu_id);

    cpu_set_t cpuset[NUM_CORE];
    pthread_t thread[NUM_CORE];
    pthread_attr_t attr[NUM_CORE];
    for(int i = 0; i < NUM_CORE; i++)
	pthread_attr_init(&attr[i]);
    struct thread_args nn_args[NUM_CORE];

    printf("create threading \n");
    for(int i = 0; i < NUM_CORE; i++){
	  CPU_ZERO(&cpuset[i]);
	  CPU_SET(i, &cpuset[i]);
	  pthread_attr_setaffinity_np(&attr[i], sizeof(cpu_set_t), &cpuset[i]);
	  pthread_create(&thread[i], &attr[i], print_message, NULL);
    }

    for(int i = 0; i < NUM_CORE; i++){
      pthread_join(thread[i], NULL);
    }
    printf("thread joined after message printing\n");
    
    // printf("Init A\n");
    for (size_t i = 0; i < MAT_DIM_I; ++i) {
      for (size_t j = 0; j < MAT_DIM_K; ++j) {
        full_A[i][j] = rand() % 3 - 1;
      }
    }

    // printf("Init B\n");
    for (size_t i = 0; i < MAT_DIM_K; ++i) {
      for (size_t j = 0; j < MAT_DIM_J; ++j) {
        full_B[i][j] =  rand() % 3 - 1;
      }
    }

    // printf("Init D\n");
    for (size_t i = 0; i < MAT_DIM_I; ++i) {
      for (size_t j = 0; j < MAT_DIM_J; ++j) {
        full_D[i][j] = NO_BIAS ? 0 : rand() % 2;
      }
    }

    printf("Starting slow CPU matmul\n");
    unsigned long cpu_start = read_cycles();
    full_matmul(full_A, full_B, full_D, gold);
    unsigned long cpu_end = read_cycles();
    printf("CPU cycles taken: %u\n", cpu_end-cpu_start);
 
    pthread_barrier_init(&barrier_global, NULL, NUM_CORE);

	for(int i = 0; i < NUM_CORE; i++){
	  nn_args[i].cid = i;
	  pthread_create(&thread[i], &attr[i], thread_test, &nn_args[i]);
	}

	for(int i = 0; i < NUM_CORE; i++)
	  pthread_join(thread[i], NULL);

    pthread_barrier_destroy(&barrier_global);
 
    for(int i = 0; i < NUM_CORE; i++){
      if (!full_is_equal(full_C[i], gold)) {
        printf("C %d:\n", i);
        full_printMatrix(full_C[i]);
        printf("Gold:\n");
        full_printMatrix(gold);
        printf("\n");

        exit(1);
      }
    }
    printf("end of test\n");
}
