#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include <stddef.h>


struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;
struct spinlock unused;
struct list ls_unused ;
struct node first_unused ;
struct spinlock zombie;
struct list ls_zombie;
struct node first_zombie;
struct spinlock sleeping;
struct list ls_sleeping;
struct node first_sleeping;
struct spinlock ready_cpu[NCPU];
struct list ls_ready_cpu[NCPU];
struct node first_ready_cpu[NCPU];
struct node nodes[NPROC+1];

uint64 num_of_procs[NCPU]; // each cpu has a counter for the current running processes
int num_of_cpus = 0;
int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S
extern uint64 cas( volatile void *addr , int expected , int newval);

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl) {
    struct proc *p;

    for(p = proc; p < &proc[NPROC]; p++) {
        char *pa = kalloc();
        if(pa == 0)
            panic("kalloc");
        uint64 va = KSTACK((int) (p - proc));
        kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
    }
}

// initialize the proc table at boot time.
void
procinit(void)
{
    struct proc *p;
    initlock(&unused, "unused");
    ls_unused = init_linked_list(&unused, &first_unused);
    initlock(&zombie, "zombie");
    ls_zombie = init_linked_list(&zombie, &first_zombie);
    initlock(&sleeping, "sleeping");
    ls_sleeping = init_linked_list(&sleeping, &first_sleeping);
    for(int i=0; i<NCPU; i++) {
        initlock(&ready_cpu[i], "ready_cpu[i]"+i);
        ls_ready_cpu[i]= init_linked_list(&ready_cpu[i], &first_ready_cpu[i]);
        num_of_procs[i] = 0;
    }
    initlock(&pid_lock, "nextpid");
    initlock(&wait_lock, "wait_lock");
    int i=0;
    for(p = proc; p < &proc[NPROC]; p++) {
        initlock(&p->lock, "proc");
        initlock(&p->lock_linked_list, "lock_linked_list");
        p->kstack = KSTACK((int) (p - proc));
        p->curr_proc_node = &nodes[i];
        p->curr_proc_node->proc_index= i;
        p->curr_proc_node->next = NULL;
        add(ls_unused,  p->curr_proc_node);
        i++;
    }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
    int id = r_tp();
    return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void) {
    int id = cpuid();
    if(num_of_cpus< 1+id ) num_of_cpus=1+id;
    int tmp_cpu;
    do{
        if(num_of_cpus< 1+id ) {
            tmp_cpu=num_of_cpus;
        }
        else break;
    } while(cas(&num_of_cpus, tmp_cpu, 1+id));
    struct cpu *c = &cpus[id];
    c->cpuid = id;
    return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void) {
    push_off();
    struct cpu *c = mycpu();
    struct proc *p = c->proc;
    pop_off();
    return p;
}

int
allocpid() {
    int pid;
    do{
        pid = nextpid;
    } while(cas(&nextpid, pid, pid+1));

    return pid;
}

int
add_num_of_procs(int cpuid, int addition){
    int curr;
    do{
        curr = num_of_procs[cpuid];
    } while(cas(&num_of_procs[cpuid],curr, curr + addition));
    return 0;
}

int
change_affiliation_cas(struct proc *p, int my_id){
    int curr;
//    do{
//        curr = p->cpus_affiliated;
//    }while(cas(&(p->cpus_affiliated), curr, my_id));
    curr = p->cpus_affiliated;
    if (!cas(&(p->cpus_affiliated), curr, my_id))
        return 1;
    else
        panic("Change Affiliation FAILED");
    return 0;
}


int steal_proc(int my_id){
    for (int cpuid = 0; cpuid < NCPU; cpuid++){
        if (cpuid != my_id && num_of_procs[cpuid] > 1){ // the running process is also counted
            struct proc *p;
            // remove from current cpu:
            int proc_ind = pop(ls_ready_cpu[cpuid]);
            if (proc_ind <NPROC){ // if remove was successful
                add_num_of_procs(cpuid,-1);
                p = &proc[proc_ind];
                // add to my cpu:
                change_affiliation_cas(p, my_id);
                add(ls_ready_cpu[my_id],p->curr_proc_node);
                add_num_of_procs(my_id,1);
                return proc_ind;
            }
        }
    }
    return NPROC + 1;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
    struct proc *p;

    uint index_proc;
    do {
        index_proc = pop(ls_unused);
    }while(index_proc > NPROC);
    p = &proc[index_proc];
    acquire(&p->lock);
    if(p->state == UNUSED) {
        goto found;
    } else {
        release(&p->lock);
    }
    return 0;

    found:
    p->pid = allocpid();
    p->state = USED;

    // Allocate a trapframe page.
    if((p->trapframe = (struct trapframe *)kalloc()) == 0){
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // An empty user page table.
    p->pagetable = proc_pagetable(p);
    if(p->pagetable == 0){
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // Set up new context to start executing at forkret,
    // which returns to user space.
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64)forkret;
    p->context.sp = p->kstack + PGSIZE;

    return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
    if(p->trapframe)
        kfree((void*)p->trapframe);
    p->trapframe = 0;
    if(p->pagetable)
        proc_freepagetable(p->pagetable, p->sz);
    p->pagetable = 0;
    p->sz = 0;
    p->pid = 0;
    p->parent = 0;
    p->name[0] = 0;
    p->chan = 0;
    p->killed = 0;
    p->xstate = 0;
    p->state = UNUSED;
    remove(ls_zombie, p->curr_proc_node);
    p->curr_proc_node->next=NULL;
    add(ls_unused,p->curr_proc_node);
}

// Create a user page table for a given process,
// with no user memory, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
    pagetable_t pagetable;

    // An empty page table.
    pagetable = uvmcreate();
    if(pagetable == 0)
        return 0;

    // map the trampoline code (for system call return)
    // at the highest user virtual address.
    // only the supervisor uses it, on the way
    // to/from user space, so not PTE_U.
    if(mappages(pagetable, TRAMPOLINE, PGSIZE,
                (uint64)trampoline, PTE_R | PTE_X) < 0){
        uvmfree(pagetable, 0);
        return 0;
    }

    // map the trapframe just below TRAMPOLINE, for trampoline.S.
    if(mappages(pagetable, TRAPFRAME, PGSIZE,
                (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
        uvmunmap(pagetable, TRAMPOLINE, 1, 0);
        uvmfree(pagetable, 0);
        return 0;
    }

    return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, TRAPFRAME, 1, 0);
    uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
        0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
        0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
        0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
        0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
        0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
        0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
    struct proc *p;

    p = allocproc();
    p->cpus_affiliated =0;
    p->curr_proc_node->next =NULL;
    add(ls_ready_cpu[p->cpus_affiliated], p->curr_proc_node);
    add_num_of_procs(p->cpus_affiliated, 1);
    initproc = p;

    // allocate one user page and copy init's instructions
    // and data into it.
    uvminit(p->pagetable, initcode, sizeof(initcode));
    p->sz = PGSIZE;

    // prepare for the very first "return" from kernel to user.
    p->trapframe->epc = 0;      // user program counter
    p->trapframe->sp = PGSIZE;  // user stack pointer

    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->cwd = namei("/");

    p->state = RUNNABLE;

    release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
    uint sz;
    struct proc *p = myproc();

    sz = p->sz;
    if(n > 0){
        if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
            return -1;
        }
    } else if(n < 0){
        sz = uvmdealloc(p->pagetable, sz, sz + n);
    }
    p->sz = sz;
    return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
    int i, pid;
    struct proc *np;
    struct proc *p = myproc();

    // Allocate process.
    if((np = allocproc()) == 0){
        return -1;
    }

    // Copy user memory from parent to child.
    if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
        freeproc(np);
        release(&np->lock);
        return -1;
    }
    np->sz = p->sz;

    // copy saved user registers.
    *(np->trapframe) = *(p->trapframe);

    // Cause fork to return 0 in the child.
    np->trapframe->a0 = 0;

    // increment reference counts on open file descriptors.
    for(i = 0; i < NOFILE; i++)
        if(p->ofile[i])
            np->ofile[i] = filedup(p->ofile[i]);
    np->cwd = idup(p->cwd);

    safestrcpy(np->name, p->name, sizeof(p->name));

    pid = np->pid;

    release(&np->lock);

    acquire(&wait_lock);
    np->parent = p;
    release(&wait_lock);

  acquire(&np->lock);
    np->state = RUNNABLE;
    int destination_cpu = -1; // I want it to crash and burn if the macros don't work!
    #if BLNCFLG==ON
        destination_cpu = min_num_of_procs();
    #elif BLNCFLG==OFF
        destination_cpu = p->cpus_affiliated;
    #else
        panic("Something is wrong with the macros!!");
    #endif
    np->curr_proc_node->next=NULL;
    np->cpus_affiliated = destination_cpu;
    add(ls_ready_cpu[destination_cpu], np->curr_proc_node);
    release(&np->lock);
    add_num_of_procs(destination_cpu, 1);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
    struct proc *pp;

    for(pp = proc; pp < &proc[NPROC]; pp++){
        if(pp->parent == p){
            pp->parent = initproc;
            wakeup(initproc);
        }
    }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
    struct proc *p = myproc();

    if(p == initproc)
        panic("init exiting");

    // Close all open files.
    for(int fd = 0; fd < NOFILE; fd++){
        if(p->ofile[fd]){
            struct file *f = p->ofile[fd];
            fileclose(f);
            p->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(p->cwd);
    end_op();
    p->cwd = 0;

    acquire(&wait_lock);

    // Give any children to init.
    reparent(p);

    // Parent might be sleeping in wait().
    wakeup(p->parent);

  acquire(&p->lock);
  p->curr_proc_node->next=NULL;
  add(ls_zombie, p->curr_proc_node);
  add_num_of_procs(p->cpus_affiliated, -1); //todo they didnt say to update here but it makes sense
  p->xstate = status;
  p->state = ZOMBIE;

    release(&wait_lock);

    // Jump into the scheduler, never to return.
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
    struct proc *np;
    int havekids, pid;
    struct proc *p = myproc();

    acquire(&wait_lock);

    for(;;){
        // Scan through table looking for exited children.
        havekids = 0;
        for(np = proc; np < &proc[NPROC]; np++){
            if(np->parent == p){
                // make sure the child isn't still in exit() or swtch().
                acquire(&np->lock);

                havekids = 1;
                if(np->state == ZOMBIE){
                    // Found one.
                    pid = np->pid;
                    if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                            sizeof(np->xstate)) < 0) {
                        release(&np->lock);
                        release(&wait_lock);
                        return -1;
                    }
                    freeproc(np);
                    release(&np->lock);
                    release(&wait_lock);
                    return pid;
                }
                release(&np->lock);
            }
        }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
    struct proc *p;
    struct cpu *c = mycpu();
    uint proc_index;
    c->proc = 0;
    for(;;){
        // Avoid deadlock by ensuring that devices can interrupt.
        intr_on();
        do{
            proc_index = pop(ls_ready_cpu[c->cpuid]);
            if (proc_index > NPROC)//if im empty, attempt to steal
                    proc_index = steal_proc(c->cpuid);
        }while(proc_index > NPROC); // if I was empty, and couldn't steal, try again

        p = &proc[proc_index];

        acquire(&p->lock);
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        release(&p->lock);
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
    int intena;
    struct proc *p = myproc();

    if(!holding(&p->lock))
        panic("sched p->lock");
    if(mycpu()->noff != 1)
        panic("sched locks");
    if(p->state == RUNNING)
        panic("sched running");
    if(intr_get())
        panic("sched interruptible");

    intena = mycpu()->intena;
    swtch(&p->context, &mycpu()->context);
    mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
    struct proc *p = myproc();
    acquire(&p->lock);
    p->state = RUNNABLE;
    p->curr_proc_node->next=NULL;
    add(ls_ready_cpu[p->cpus_affiliated], p->curr_proc_node);
    sched();
    release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
    static int first = 1;

    // Still holding p->lock from scheduler.
    release(&myproc()->lock);

    if (first) {
        // File system initialization must be run in the context of a
        // regular process (e.g., because it calls sleep), and thus cannot
        // be run from main().
        first = 0;
        fsinit(ROOTDEV);
    }

    usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
    struct proc *p = myproc();

    // Must acquire p->lock in order to
    // change p->state and then call sched.
    // Once we hold p->lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup locks p->lock),
    // so it's okay to release lk.

    acquire(&p->lock);  //DOC: sleeplock1
    release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  p->curr_proc_node->next =NULL;
  add(ls_sleeping, p->curr_proc_node);
  add_num_of_procs(p->cpus_affiliated, -1); //todo they didnt say to update here but it makes sense

    sched();

    // Tidy up.
    p->chan = 0;

    // Reacquire original lock.
    release(&p->lock);
    acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
    struct proc *p;
    struct node *curr =ls_sleeping.head->next;
    while(curr != NULL) {
        p = &proc[curr->proc_index];
        curr = curr->next;
        if(p != myproc()){
            acquire(&p->lock);
            if(p->state == SLEEPING && p->chan == chan) {
                int destination_cpu = -1;
                #if BLNCFLG==ON
                    destination_cpu = min_num_of_procs();
                #elif BLNCFLG==OFF
                    destination_cpu = p->cpus_affiliated;
                #else
                    panic("Something is wrong with the macros!!");
                #endif
                remove(ls_sleeping,p->curr_proc_node);
                p->curr_proc_node->next= NULL;
                p->cpus_affiliated = destination_cpu;
                add(ls_ready_cpu[p->cpus_affiliated], p->curr_proc_node);
                add_num_of_procs(destination_cpu, 1);
                p->state = RUNNABLE;
            }
            release(&p->lock);
        }
    }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
    struct proc *p;

    for(p = proc; p < &proc[NPROC]; p++){
        acquire(&p->lock);
        if(p->pid == pid){
            p->killed = 1;
            if(p->state == SLEEPING){
                // Wake process from sleep().
                p->state = RUNNABLE;
            }
            release(&p->lock);
            return 0;
        }
        release(&p->lock);
    }
    return -1;
}

int cpu_process_count(int cpu_num){
    if ( 0 <= cpu_num && cpu_num<NCPU) {
        return num_of_procs[cpu_num];
    }else{
        return -1;
    }
}


// move a process to a different cpu
// returns the cpu number if successful or negative number if fails
int
set_cpu(int cpu_num){
    if ( 0 < cpu_num && cpu_num<NCPU) {
        struct proc *p = myproc();
        acquire(&p->lock);
        p->cpus_affiliated = cpu_num;
        release(&p->lock);
        yield();
        return cpu_num;
    }else{
        return -1;
    }
}

int
get_cpu(){
    struct proc *p = myproc();
    acquire(&p->lock);
    int cpu_num = p->cpus_affiliated;
    release(&p->lock);
    return cpu_num;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
    struct proc *p = myproc();
    if(user_dst){
        return copyout(p->pagetable, dst, src, len);
    } else {
        memmove((char *)dst, src, len);
        return 0;
    }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
    struct proc *p = myproc();
    if(user_src){
        return copyin(p->pagetable, dst, src, len);
    } else {
        memmove(dst, (char*)src, len);
        return 0;
    }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
    static char *states[] = {
            [UNUSED]    "unused",
            [SLEEPING]  "sleep ",
            [RUNNABLE]  "runble",
            [RUNNING]   "run   ",
            [ZOMBIE]    "zombie"
    };
    struct proc *p;
    char *state;

    printf("\n");
    for(p = proc; p < &proc[NPROC]; p++){
        if(p->state == UNUSED)
            continue;
        if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        printf("%d %s %s", p->pid, state, p->name);
        printf("\n");
    }
}


//////////// TODO check if it is ok impl linked list here
struct node init_node(uint proc_index){
    struct node node;
    node.proc_index =proc_index;
    node.next = NULL;
    return node;
}


struct list init_linked_list(struct spinlock * lock, struct node * first){
    struct list ls;
    first->proc_index= NPROC+1;
    first->next =NULL;
    ls.head = first;
    ls.first_lock =*lock;
    return ls;
}

void add(struct list ls, struct node * node){
    struct node * curr = ls.head;
    while(curr->next != NULL){
        curr = curr->next;
    }
    acquire(&proc[curr->proc_index].lock_linked_list);
    curr->next = node;
    release(&proc[curr->proc_index].lock_linked_list);
}

uint pop(struct list ls){
    struct node * pred = ls.head;
    acquire(&ls.first_lock);
    if(pred->next != NULL){
        struct node * curr = pred->next;
        acquire(&proc[curr->proc_index].lock_linked_list);
        pred->next = curr->next;
        release(&ls.first_lock);
        release(&proc[curr->proc_index].lock_linked_list);
        return curr->proc_index;
    }
    else {
        release(&ls.first_lock);
        return NPROC+1;
    }
}
void remove(struct list ls, struct node * node){
    struct node * pred = ls.head;
    int flag = 0;
    acquire(&ls.first_lock);
    struct node * curr = pred->next;
    acquire(&proc[curr->proc_index].lock_linked_list);
    while(curr->proc_index != node->proc_index){
        if(flag == 0){
            release(&ls.first_lock);
            flag++;
        }
        else
            release(&proc[pred->proc_index].lock_linked_list);
        pred = curr;
        curr = curr->next;
        acquire(&proc[curr->proc_index].lock_linked_list);
    }
    pred->next = curr->next;
    if(flag == 0){
        release(&ls.first_lock);
        flag++;
    }
    else
        release(&proc[pred->proc_index].lock_linked_list);
    release(&proc[curr->proc_index].lock_linked_list);
}

int min_num_of_procs(){
    int min = 0;
    for (int i=0; i < num_of_cpus;i++){
        if (num_of_procs[i] < num_of_procs[min]) min = i;
    }
    return min;
}

