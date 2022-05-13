
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

void cas_test(){
    printf("%d\n", getpid());
    for(int i=0; i <4;i++){
        int pid = fork();
        if (pid != 0)
            printf("%d\n", pid);
    }
}

int
main(int argc, char *argv[])
{
    //cas_test();
    int pid1 = fork();
    int pid2 =fork();
    int pid3 =fork();
    int pid4 =fork();
    int pid5 =fork();
    int pid6 =fork();
    if(pid1 == 0&&pid2 == 0&&pid3 == 00&&pid4 == 00&&pid5 == 00&&pid6 == 0){
        for(int i = 0; i < CPUS; i++){
            printf("CPU: %d has %d proccesses\n", i, cpu_process_count(i));
        }
    }
    for(int i=0; i<100000; i++){
        printf("");
    }
    printf("pid %d finished\n", getpid());

    exit(0);
    return 0;
}