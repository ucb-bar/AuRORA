// See LICENSE for license details.

#ifndef SRC_MAIN_C_GEMMINI_AURORA_H
#define SRC_MAIN_C_GEMMINI_AURORA_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <stdbool.h>

#include "../include/gemmini_params.h"
#include "../include/gemmini.h"

#include "rocc-software/src/xcustom.h"



static void tiled_rerocc_matmul_outer(size_t dim_I, size_t dim_J, size_t dim_K,
        const elem_t* A, const elem_t* B,
        const void * D, void * C,
        size_t stride_A, size_t stride_B, size_t stride_D, size_t stride_C,
        scale_t A_scale_factor, scale_t B_scale_factor, scale_acc_t D_scale_factor,
        int act, acc_scale_t scale, acc_scale_t bert_scale, bool repeating_bias,
        size_t tile_I, size_t tile_J, size_t tile_K,
        bool a_transpose, bool b_transpose,
        bool full_C, bool low_D,
        uint8_t weightA,
        int num_accel) {

  const size_t dim_J_divided = ceil_divide_int(dim_J, num_accel);

  const size_t dim_I_padded = (dim_I / DIM + (dim_I % DIM != 0)) * DIM;
  const size_t dim_J_padded = (dim_J_divided / DIM + (dim_J_divided % DIM != 0)) * DIM;
  const size_t dim_K_padded = (dim_K / DIM + (dim_K % DIM != 0)) * DIM;

  const size_t I0 = dim_I_padded / (tile_I*DIM) + (dim_I_padded % (tile_I*DIM) != 0);
  const size_t J0 = dim_J_padded / (tile_J*DIM) + (dim_J_padded % (tile_J*DIM) != 0);
  const size_t K0 = dim_K_padded / (tile_K*DIM) + (dim_K_padded % (tile_K*DIM) != 0);

  // These lines here are supposed to help us deal with when the dimensions of
  // the systolic array aren't divisible by the tiling factors
  const size_t last_I = dim_I_padded % (tile_I*DIM) == 0 ? tile_I : (dim_I_padded/DIM) % tile_I;
  const size_t last_J = dim_J_padded % (tile_J*DIM) == 0 ? tile_J : (dim_J_padded/DIM) % tile_J;
  const size_t last_K = dim_K_padded % (tile_K*DIM) == 0 ? tile_K : (dim_K_padded/DIM) % tile_K;

  // These lines are supposed to figure out how much padding the hardware is
  // supposed to add for the final tile
  const size_t padding_I = dim_I_padded - dim_I;
  const size_t padding_J = dim_J_padded - dim_J;
  const size_t padding_K = dim_K_padded - dim_K;

  const bool no_bias = D == NULL;

  if (no_bias) {
    D = (void*) 1; // Dummy address which isn't NULL
  }

  const size_t sizeof_D = low_D ? sizeof(elem_t) : sizeof(acc_t) ;
  const size_t sizeof_C = full_C ? sizeof(acc_t) : sizeof(elem_t);
  
  for(int i = 0; i < num_accel; i++){
    rr_set_opc(XCUSTOM_ACC, i);
    gemmini_extended_config_ex(WS, act & 3, 0, 1, a_transpose, b_transpose);
    gemmini_extended_config_st(stride_C * sizeof_C, act & 3, scale);
    gemmini_extended3_config_ld(stride_A * sizeof(elem_t), A_scale_factor, false, 0);
    gemmini_extended3_config_ld(stride_B * sizeof(elem_t), B_scale_factor, false, 1)
    gemmini_extended3_config_ld(repeating_bias ? 0 : (stride_D * sizeof_D), D_scale_factor, low_D, 2);
  }
  // reuse operand if it fits scratchpad
  int a_spad_id = 0;
  int b_spad_id = 0;
  bool b_reuse = (J0 * K0 <= 2);
  bool a_reuse = (I0 * K0 <= 2);

  for (size_t i0 = 0; i0 < I0; i0++)
    for (size_t j0 = 0; j0 < J0; j0++)
      for (size_t k0 = 0; k0 < K0; k0++) {
        for(size_t n = 0; n < num_accel; n++){
            if(a_reuse)
                a_spad_id = ((i0+k0) == 0) ? 1 : 2;
            if(b_reuse)
                b_spad_id = ((j0+k0) == 0) ? 1 : 2;

            // divided into J dimension
            int J_offset = n * dim_J_divided;
            rr_set_opc(XCUSTOM_ACC, n);

            const void * pre;
            if (k0 != 0) {
            pre = NULL;
            } else {
            size_t bias_row = repeating_bias ? 0 : i0*tile_I*DIM;
            // pre = &(((acc_t*)D)[bias_row * stride_D + j0 * tile_J * DIM]);
            pre = (int8_t*)D + (bias_row * stride_D + (j0 * tile_J * DIM + J_offset))*sizeof_D;
            }

            void * out = k0 == K0-1 ? (int8_t*)C + (i0*tile_I*DIM*stride_C + (j0*tile_J*DIM + J_offset))*sizeof_C : NULL;

            const size_t I = i0 < I0-1 ? tile_I : last_I;
            const size_t J = j0 < J0-1 ? tile_J : last_J;
            const size_t K = k0 < K0-1 ? tile_K : last_K;

            const size_t pad_I = i0 == I0-1 ? padding_I : 0;
            const size_t pad_J = j0 == J0-1 ? padding_J : 0;
            const size_t pad_K = k0 == K0-1 ? padding_K : 0;

            const elem_t * a = a_transpose ? (A + k0*tile_K*DIM*stride_A + i0*tile_I*DIM)
            : (A + i0*tile_I*DIM*stride_A + k0*tile_K*DIM);

            const elem_t * b = b_transpose ? (B + (j0*tile_J*DIM + J_offset)*stride_B + k0*tile_K*DIM)
            : (B + k0*tile_K*DIM*stride_B + (j0*tile_J*DIM + J_offset));

            if(a_reuse && j0 >= 1) a = NULL;
            if(b_reuse && i0 >= 1) b = NULL;
            //printf("a_reuse: %d, b_reuse: %d, a_spad_id: %d, b_spad_id: %d, a: %llu, b: %llu \n", a_reuse, b_reuse, a_spad_id, b_spad_id, a, b);
            sp_tiled_matmul_ws(a, b, pre, out,
                A_scale_factor, B_scale_factor, D_scale_factor,
                I, J, K,
                pad_I, pad_J, pad_K,
                stride_A, stride_B, stride_D, stride_C,
                a_transpose, b_transpose,
                full_C, low_D,
                no_bias, repeating_bias,
                act, a_spad_id, b_spad_id);
        }
      }

}



// This function runs a tiled matrix multiplication, with automatically
// calculated tiling factors
static void tiled_rerocc_matmul_auto(size_t dim_I, size_t dim_J, size_t dim_K,
        const elem_t* A, const elem_t* B,
        const void * D, void * C,
        size_t stride_A, size_t stride_B, size_t stride_D, size_t stride_C,
        scale_t A_scale_factor, scale_t B_scale_factor, scale_acc_t D_scale_factor,
        int act, acc_scale_t scale, acc_scale_t bert_scale,
        bool repeating_bias,
        bool transpose_A, bool transpose_B,
        bool full_C, bool low_D,
        uint8_t weightA,
        int num_accel) {

#define partition_rows (BANK_NUM * BANK_ROWS / 2)
#define mats_in_partition (partition_rows / DIM)
#define mats_in_acc (ACC_ROWS / DIM)
#define max_tile_i_j ((size_t)sqrt(mats_in_acc))
#define max_tile_k (mats_in_partition / max_tile_i_j)

    // "db_" means "double-buffered"
#define db_partition_rows ((BANK_NUM * BANK_ROWS / 2) / 2)
#define db_mats_in_partition (db_partition_rows / DIM)
#define db_mats_in_acc ((ACC_ROWS / 2) / DIM)
#define db_max_tile_i_j ((size_t)sqrt(db_mats_in_acc))
#define db_max_tile_k (db_mats_in_partition / db_max_tile_i_j)

    // divide by J dimension
    const size_t dim_J_divided = ceil_divide_int(dim_J, num_accel);

    const size_t dim_I_padded = (dim_I / DIM + (dim_I % DIM != 0)) * DIM;
    const size_t dim_J_padded = (dim_J_divided / DIM + (dim_J_divided % DIM != 0)) * DIM;
    const size_t dim_K_padded = (dim_K / DIM + (dim_K % DIM != 0)) * DIM;

    const size_t max_spad_rows = BANK_NUM * BANK_ROWS / 2;
    const size_t max_acc_rows =  ACC_ROWS / 2;

    size_t tile_I, tile_J, tile_K;

    tile_I = dim_I_padded/DIM < db_max_tile_i_j ? dim_I_padded/DIM : db_max_tile_i_j;
    tile_J = dim_J_padded/DIM < db_max_tile_i_j ? dim_J_padded/DIM : db_max_tile_i_j;
    tile_K = dim_K_padded/DIM < db_max_tile_k ? dim_K_padded/DIM : db_max_tile_k;
    

    // Fill scratchpad as much as possible
    while (true) {
      bool increased = false;

      if (tiled_matmul_total_spad_rows(tile_I, tile_J+1, tile_K) <= max_spad_rows &&
          tiled_matmul_total_acc_rows(tile_I, tile_J+1) <= max_acc_rows &&
          (tile_J+1) * DIM <= dim_J_padded) {
        tile_J++;
        increased = true;
      }

      if (tiled_matmul_total_spad_rows(tile_I+1, tile_J, tile_K) <= max_spad_rows &&
          tiled_matmul_total_acc_rows(tile_I+1, tile_J) <= max_acc_rows &&
          (tile_I+1) * DIM <= dim_I_padded) {
        tile_I++;
        increased = true;
      }

      if (tiled_matmul_total_spad_rows(tile_I, tile_J, tile_K+1) <= max_spad_rows &&
          (tile_K+1) * DIM <= dim_K_padded) {
        tile_K++;
        increased = true;
      }

      if (!increased)
        break;
    }

#ifdef PRINT_TILE
#if PRINT_TILE
    const int spad_rows = tiled_matmul_total_spad_rows(tile_I, tile_J, tile_K);
    const int acc_rows = tiled_matmul_total_acc_rows(tile_I, tile_J);

    printf("tile_I: %d\n", tile_I);
    printf("tile_J: %d\n", tile_J);
    printf("tile_K: %d\n\n", tile_K);

    printf("spad_rows: %d\n", spad_rows);
    printf("acc_rows: %d\n\n", acc_rows);

    printf("spad_row utilization: %d%%\n", (spad_rows * 100) / max_spad_rows);
    printf("acc_row utilization: %d%%\n\n", (acc_rows * 100) / max_acc_rows);

    exit(EXIT_SUCCESS);
#endif
#endif

    tiled_rerocc_matmul_outer(dim_I, dim_J, dim_K,
        A, B, D, C,
        stride_A, stride_B, stride_D, stride_C,
        A_scale_factor, B_scale_factor, D_scale_factor,
        act, scale, bert_scale, repeating_bias,
        tile_I, tile_J, tile_K,
        transpose_A, transpose_B,
        full_C, low_D,
        weightA,
        num_accel);

#undef partition_rows
#undef mats_in_partition
#undef mats_in_acc
#undef max_tile_i_j
#undef max_tile_k
}

static int calc_num_accel_needed_matmul(size_t dim_I, size_t dim_J, size_t dim_K, 
        int64_t target_cycles, size_t total_num_accel){
      
    uint64_t comp_ideal_cycle = dim_I * dim_J * dim_K / (DIM * DIM);
    uint64_t mem_ideal_cycle = (dim_I * dim_K) / DIM + (dim_I * dim_J) / DIM + (dim_J * dim_K) / DIM;
    //uint64_t unit_ideal_cycle = comp_ideal_cycle > mem_ideal_cycle ? comp_ideal_cycle : mem_ideal_cycle;
    uint64_t unit_ideal_cycle = (comp_ideal_cycle + mem_ideal_cycle);

    float float_num_accel = unit_ideal_cycle / target_cycles;
    int num_accel = ceil_divide_int(unit_ideal_cycle*1.1, target_cycles);
    if(target_cycles < 0) num_accel = total_num_accel;
    else if(float_num_accel < 0.5) num_accel = 0;
    else if(num_accel >= total_num_accel - 1) num_accel = total_num_accel;

    return num_accel;

}

#endif
