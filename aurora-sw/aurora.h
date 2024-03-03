#ifndef AURORA_H
#define AURORA_H

#include "aurora-sw/rerocc.h"

#ifndef PRINT_LOG 
#define PRINT_LOG 0
#endif

bool single_acquire(int cfgid, int start_id, int num_search_accel){
    for(int i = start_id; i < start_id + num_search_accel; i++){
        bool acquired = rr_acquire(cfgid, i);
        if(acquired){
#if PRINT_LOG == 1
            printf("accel %d acquired to cfgid %d\n", i, cfgid);
#endif
            return true;
        }
    }
    return false;
}

int multi_acquire(int start_cfgid, int num_accel, int start_id, int num_search_accel){
    int num_acquired = 0;
    for(int i = start_id; i < start_id + num_search_accel; i++){
        int cfgid = num_acquired + start_cfgid;
        bool acquired = rr_acquire(cfgid, i);
        if(acquired){
#if PRINT_LOG == 1
            printf("accel %d acquired to cfgid %d\n", i, cfgid);
#endif
            num_acquired ++;
            if(num_acquired == num_accel)
                return num_acquired;
        }
    }
    // can't acquire num_accel
    // acquired start_cfgid ~ start_cfgid + num_acquired - 1
    return num_acquired;
}

void multi_release(int start_cfgid, int num_accel){
    for(int i = start_cfgid; i < start_cfgid + num_accel; i++){
#if PRINT_LOG == 1
        printf("accel cfgid %d released\n", i);
#endif
        rr_release(i);
    }
}
#endif
