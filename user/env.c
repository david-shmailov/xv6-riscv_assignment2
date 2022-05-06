
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"



int
main(int argc, char *argv[])
{   if(fork()){
        for(int i = 0; i < NCPU; i++){
            printf("CPU: %d has %d proccesses\n", i, cpu_process_count(i));
        }
    }

    exit(0);
    return 0;
}