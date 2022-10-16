#ifndef PROCESSESINFO
#define PROCESSESINFO
#include "param.h"

struct processes_info {
    int num_processes;
    int pids [NPROC];
    int times_scheduled[NPROC];
    int tickets[NPROC];
};

#endif
