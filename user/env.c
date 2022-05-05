
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
{
    int pids[16] ={0};
    for(int i=0; i<5; i++){
        int pid = fork();
        if( pid != 0) pids[pid] +=1;
    }
    for(int i=0; i<16; i++){
        if(pids[i]>1) printf("duplicate pid");
    }
    return 0;
}