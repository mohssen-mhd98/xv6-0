#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "ticketlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;
struct ticketlock tl;
struct ticketlock mutex, write;
int sharedCounter = 0;
int nextpid = 1;
uint a =0;
int q = 0;
int schedType = 0;
int readerCount = 0;
int quantumQ_size, defaultq_size;
extern void forkret(void);
extern void trapret(void);
struct proc* highestPriority(void);
static void wakeup1(void *chan);
int leastPriorityNum(void);
void scheduler3(struct cpu *c);
static void wakeup1TicketLock(int pid);
void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}
int countDigit(int n)
{ 
    if (n == 0) 
    	{
        return 0;
        } 
    return 1 + countDigit(n / 10); 
}
// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
/*
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}*/
static struct proc*
allocproc(void)
{
  int min = leastPriorityNum();
  char *sp;
  struct proc *p;
  acquire(&ptable.lock);
       
  //p = highestPriority();
  //min = p-> calculatedPriority;
  if (min == Max){
  	min = 0;
  }
  //min = 1;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->priority = 5;//default for new processes
  p->changeablePriority = min;//default for new processes
  /*pushcli();
  p->creationTime = mycpu()->time_slot;
  popcli();*/
  p->creationTime = ticks;//marking creation time by the current value of ticks.
  p->runningTime = 0;
  p->getFirstCpu = 0;//hasn't acquired the CPU yet.
  p->sleepingTime = 0;
  p->readyTime = 0;
  p->terminationTime = 0;
  p->qLevel = 1;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}


//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

//****************Default Algorithm for Scheduler**************************
/*void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      p->getFirstCpu = 1;
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}*/

void
scheduler1(struct cpu *c)
{
  struct proc *p;

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      p->getFirstCpu = 1;
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  
}

//****************Default Algorithm for Scheduler**************************

//Correct Answer for (3.3)****##****
/*void
scheduler(void)
{
     struct proc *p;
    struct proc *minp;
    int min,f;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){

     min = Max;
     minp =myproc();
     f=0;
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      if(p->state == RUNNABLE && p->changeablePriority < min){
        min = p->changeablePriority;
        f=1;
        minp = p;
      }
    }

      if (f==1){

      minp->changeablePriority += minp->priority;
      minp->getFirstCpu = 1;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = minp;
      switchuvm(minp);
      minp->state = RUNNING;

      swtch(&(c->scheduler), minp->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;

      }

    release(&ptable.lock);

  }
}*/

/*void
scheduler(void)
{
  struct proc *p;
  struct proc *minp;
  int min;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){

     min = Max;
     minp =myproc(); //Minimum priority of processes
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      if(p->state == RUNNABLE && p->changeablePriority < min){
        min = p->changeablePriority;

        minp = p;
      }
    }

      if (minp != myproc()){

      minp->changeablePriority += minp->priority;
      minp->getFirstCpu = 1;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = minp;
      switchuvm(minp);
      minp->state = RUNNING;

      swtch(&(c->scheduler), minp->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;

      }

    release(&ptable.lock);

  }
}*/

void
scheduler2(struct cpu *c)
{
  struct proc *p;
  struct proc *minp;
  int min;
  //struct cpu *c = mycpu();
  //c->proc = 0;

     min = Max;
     minp =myproc(); //Minimum priority of processes


    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      if(p->state == RUNNABLE && p->changeablePriority < min){
        min = p->changeablePriority;

        minp = p;
      }
    }

      if (minp != myproc()){

      minp->changeablePriority += minp->priority;
      minp->getFirstCpu = 1;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = minp;
      switchuvm(minp);
      minp->state = RUNNING;

      swtch(&(c->scheduler), minp->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;

      }

    release(&ptable.lock);
}

//Correct Answer for (3.3)****##****
/*
void
scheduler(void)
{
  struct proc *p;
  struct proc *minp;
  struct cpu *c = mycpu();
  struct proc *Q1[NPROC];
  struct proc *Q2[NPROC];
  int min,i,j,empty1,empty2;
  c->proc = 0;
  j = 0 ;
  i = 0 ;
  empty1 = 1;
  empty2 = 1;
  for(;;){
  
   q = (q+1)%3;
    // Enable interrupts on this processor.
    sti();
  
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
  if(q == 1)
  {

     min = Max;
     minp =myproc();

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      if(p->state == RUNNABLE && p->changeablePriority < min){
        min = p->changeablePriority;
        minp = p;
        
      }
    }

      if (minp != myproc()){

      minp->changeablePriority += minp->priority;
      minp->getFirstCpu = 1;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = minp;
      switchuvm(minp);
      minp->state = RUNNING;

      swtch(&(c->scheduler), minp->context);
      switchkvm();

      if(minp->state != UNUSED){
        Q1[i] = minp;
        i++;
        empty1 = 0;
        p->qLevel = 2;
      }

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;

      }
  }

   if (q == 2 && empty1 == 0)
   {
      empty1 = 1;
        //cprintf("&&&&&ksjdkjkdjs&&&&&&&&");
      for(p = Q1[0]; p < Q1[i+1]; p++){
        //cprintf("\n&&&&&kkkkkkkkkkkkkkkkk&&&&&&&&");
      if(p->state != RUNNABLE)
        continue;

      empty1 = 0;
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

        if(p->state != UNUSED){
        Q2[j] = p;
        j++;
        empty2 = 0;
       }

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }

    if(empty1 ==0) {
      i = 0;
    }
  }

   if (q == 0 && empty2 == 0)
   {
      empty2 = 1;

      for(p = Q2[0]; p < Q2[j+1]; p++){
      if(p->state != RUNNABLE)
        continue;

      empty2 = 0;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
     }

    if(empty2 ==1){
     j = 0;
     }

   }

    release(&ptable.lock);   
  }
}*/
//************************Policy Scheduler*************************
void
scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;
  quantumQ_size = defaultq_size = 0;
  q  = 0;
  for(;;){
    //q = (q+1)%3;

    // Enable interrupts on this processor.
    sti();
    if (schedType == 0)
	  scheduler1(c);
    else if(schedType == 1){
      scheduler1(c);
      q = 3;
    }else if(schedType == 2)
      scheduler2(c);
    else if(schedType == 3)
      scheduler3(c);

    else{

    struct proc *p;

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      p->changeablePriority += p->priority;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
    }
  }
}
//************************Policy Scheduler*************************


// Queue Algorithms
// If you wanna use this methods you have to change condition of q in trap.c to 0 except 3
/*
void
scheduler3(struct cpu *c)
{
  struct proc *p;
  struct proc *minp;
  int min;

  
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
  q = (q+1)%3;
  if(q == 1)
  {

     min = Max;
     minp =myproc();
     //if(myproc()->qLevel==1) minp->qLevel;
     for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
       if(p->state != RUNNABLE && p->qLevel !=1)
         continue;
       if(p->state == RUNNABLE && p->changeablePriority < min){
         min = p->changeablePriority;
         minp = p; 
       }
      }

      if (minp != myproc()){

      minp->changeablePriority += minp->priority;
      minp->getFirstCpu = 1;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = minp;
      switchuvm(minp);
      minp->state = RUNNING;

      swtch(&(c->scheduler), minp->context);
      switchkvm();

      if((minp->state != UNUSED || minp->state != ZOMBIE || minp->killed == 0) && defaultq_size < 12){
        minp->qLevel = 2;
        defaultq_size++;
      }

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
    
  }

   if (q == 2 && defaultq_size < 12)
   {

      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){

      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      if(p->qLevel == 2){
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();


        if((p->state != UNUSED || p->state != ZOMBIE || p->killed == 0) && quantumQ_size < 15){

          p->qLevel = 0;
          quantumQ_size++;
        }else if(p->state ==ZOMBIE || p->killed!=0 || p->state == UNUSED){
          defaultq_size--;
          p->qLevel = p->parent->qLevel;
          wakeup1(initproc);

        }

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
    }
  }

   if (q == 0 && quantumQ_size < 15)
   {

      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE )
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      if(p->qLevel == 0){
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
     }

    if(p->state == ZOMBIE || p->killed!=0 || p->state != UNUSED){
      quantumQ_size--;
      p->qLevel = p->parent->qLevel;
      wakeup1(initproc);
     }

   }

    release(&ptable.lock);   
  
}*/
/*
void
scheduler(void)
{

  struct proc *p,*p1;
  struct proc *minp;
  struct cpu *c = mycpu();
  c->proc = 0;
  int min,f,f1;
  q = 0;
  defaultq_size = 0;
  quantumQ_size = 0;
  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    f = 0;
    f1 = 0;
    q = q%3;
    q += 1;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      if(q==1 && p->qLevel==1){

        min = Max;
        minp = p;
        //f = 0;
        for(p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++){
          if(p1->state != RUNNABLE && p1->qLevel !=1)
            continue;
          
          if(p1->state == RUNNABLE && p1->changeablePriority < min){
            min = p1->changeablePriority;
            minp = p1; 
            f = 1;
          }
          if (f == 1){
            minp->changeablePriority += minp->priority;
            //minp->getFirstCpu = 1;
            p = minp;
          }
        }
      }else if((q==1 && p->qLevel != 1) || (p->qLevel==1 && q!=1)) f1 = 1;
      if(defaultq_size !=0 )
        if((q == 2 && p->qLevel != 2) || (q != 2 && p->qLevel == 2) || (q == 2 && p->qLevel == 2 && defaultq_size != 10))
          f1 = 1;
      
      if(quantumQ_size !=0 && ((q == 3 && p->qLevel != 3) || (q != 3 && p->qLevel == 3)))
        if((q == 3 && p->qLevel != 3) || (q != 3 && p->qLevel == 3) || (q == 3 && p->qLevel == 3 && quantumQ_size != 5))
          f1 = 1;
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      if(f1==0){
        p->getFirstCpu = 1;
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        swtch(&(c->scheduler), p->context);
        switchkvm();

        if((p->state != ZOMBIE || p->killed == 0) && defaultq_size < 10 && p->qLevel == 1){
          p->qLevel = 2;
          defaultq_size++;
        }
        if((p->state == ZOMBIE || p->killed!=0) && p->qLevel == 2){
          defaultq_size--;
          //p->qLevel = 1;
          wakeup1(initproc);
        }
        if((p->state != ZOMBIE || p->killed == 0) && quantumQ_size < 5 && p->qLevel == 2){
          p->qLevel = 3;
          quantumQ_size++;
        }
        if((p->state == ZOMBIE || p->killed!=0) && p->qLevel == 3){
          quantumQ_size--;
          //p->qLevel = 1;
          wakeup1(initproc);
        }
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
    }
    release(&ptable.lock);
  }
}
*/


void
scheduler3(struct cpu *c)
{
  struct proc *p;
  struct proc *minp;
  int min;
  
  q = 0;
  for(;;){
  
    // Enable interrupts on this processor.
    sti();
  
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    q = q%3;
    q += 1;

  if(q == 1)
  {

     min = Max;
     minp =myproc();
     //if(myproc()->qLevel==1) minp->qLevel;
     for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
       if(p->state != RUNNABLE && p->qLevel !=1)
         continue;
       if(p->state == RUNNABLE && p->changeablePriority < min){
         min = p->changeablePriority;
         minp = p; 
       }
      }

      if (minp != myproc() && minp->qLevel==1){

      minp->changeablePriority += minp->priority;
      minp->getFirstCpu = 1;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = minp;
      switchuvm(minp);
      minp->state = RUNNING;

      swtch(&(c->scheduler), minp->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
    
  }

   if (q == 2)
   {

      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){

      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      if(p->qLevel == 2){
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
    }
  }

   if (q == 3)
   {

      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE )
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      if(p->qLevel == 3){
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
     }
   }
    release(&ptable.lock);   
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
  
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}


//PAGEBREAK!
// Wake up process with the specific  pid.
// The ptable lock must be held.
static void
wakeup1TicketLock(int pid)
{
    struct proc *p;

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->pid == pid) {
            acquire(&ptable.lock);
            for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){//iterating through the process's thread table
                if (p->state == SLEEPING)
                    p->state = RUNNABLE;
            }
            release(&ptable.lock);
        }
    }

}

// Wake up  process with specific pid.
void
wakeupTicketLock(int pid)
{
    acquire(&ptable.lock);
    wakeup1TicketLock(pid);
    release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
int
getyear()
{
  //cprintf("****%d***%d\n",a,*a);
  return fetch_and_add(&a, 1);
}
/*char
getChild(void)
{
  struct proc *currp = myproc();
  struct proc *p;
  char* child_id[20];
  int id[20];
  int i = 0;
  int j;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(currp->pid == p->parent->pid)
      id[i] = p->pid;
    i++;
  }
  for(j=0;j<i;j++)
    child_id[j] = id[j] + '0';

  return child_id;
}*/
int sys_getChildren(void){
    int id;
    struct proc *p,*currp; 
    currp = myproc();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    id = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent->pid == currp->pid){
        id = (id*100) + p->pid;
        cprintf("child id : %d****Parent id :%d\n",p->pid,currp->pid);
      }
    }

    release(&ptable.lock);
    return id;

}

int
sys_changePolicy(void)
{
int n;
argint(0, &n);
if (n >= 0 && n < 4){
	schedType = n;
	return 1;
	}
else {
	return -1;
	}
}

int
leastPriorityNum(void)
{
  struct proc *p;
  int min = Max;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state == RUNNABLE && p->changeablePriority < min){
        min = p->changeablePriority;
        }
} 
 return min;
}

//updating time variables based on the process state
void
waitForChild(void){
  //q = 0;
struct proc *p;
acquire(&ptable.lock);
for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
	if(p->state == RUNNING){
		p->runningTime+=1;
    if(p->runningTime % 100 == 0 && p->qLevel < 3)
    p->qLevel +=1;
	}
	else if(p->state == SLEEPING){
		p->sleepingTime+=1;
	}
	else if(p->state == RUNNABLE && p->getFirstCpu){
		p->readyTime +=1;
		}
	else if(p->state == EMBRYO){
		p->creationTime += 1;

	}
	}
release(&ptable.lock);

}

int
waitingForChild(void){
  
  struct processTimeFeatures *t;
  argptr(0, (void*)&t, sizeof(*t));
  struct proc *p;
  int havekids, pid=0;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;

      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        t->creationTime = p->creationTime;
      	t->terminationTime = p->creationTime + p->sleepingTime + p->readyTime + p->runningTime;
      	t->sleepingTime = p->sleepingTime;
      	t->readyTime = p->readyTime;
      	t->runningTime = p->runningTime;
        t->Queue_level = p->qLevel;
        /*if(p->qLevel == 2)
        cprintf("**********Default_size****** : %d\n",defaultq_size);
        if(p->qLevel == 0)
        cprintf("**********Default_size****** : %d\n",quantumQ_size);*/
        //cprintf("\ncreationTime :%d***terminationTime :%d***runningTime :%d***ID : %d\nreadyTime : %d***sleepingTime : %d**"
                //,t->creationTime,t->terminationTime,t->runningTime,pid,t->readyTime,t->sleepingTime);
        release(&ptable.lock);
        return pid;
      }
      //cprintf("running Time : %d-----ID : %d\n",time->runningTime,pid);
    }

    // No point waiting if we don't have any children.
    if(havekids == 0 || curproc->killed !=0){
      t->creationTime = 0;
      t->terminationTime = 0;
      t->sleepingTime = 0;
      t->readyTime = 0;
      t->runningTime = 0;
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//set new priority for a given process
int
sys_setPriority(void)
{
  int new_priority;
  argint(0, &new_priority);
  if(new_priority < 1 || new_priority > 5)
    return -1;
  myproc()->priority = new_priority;
  return 1;
}

int
sys_ticketlockInit(void)
{
    sharedCounter = 0;
    ini_ticketLock(&tl,"ticketLock");
    return 0;
}

int
sys_ticketlockTest(void)
{
    acquire_ticketLock(&tl);
    microdelay(700000);
    sharedCounter++;
    cprintf("***%d***\n", sharedCounter);
    release_ticketLock(&tl);
    return tl.ticket;
}

int
sys_rwinit(void)
{
    sharedCounter = 0;
    readerCount = 0;
    ini_ticketLock(&mutex, "mutex readerwriter");
    ini_ticketLock(&write, "write readerwriter");
    return 0;
}

int
sys_rwtest(void)
{
    int pattern;
    //int result = 0;
    argint(0, &pattern);
    // Writer
    if (pattern == 1) {
        acquire_ticketLock(&write);
        //microdelay(70);
        sharedCounter++;
        release_ticketLock(&write);
    }
        // Reader
    else if (pattern == 0){
        acquire_ticketLock(&mutex);
        readerCount++;
        if (readerCount == 1){
          acquire_ticketLock(&write);
        }
        release_ticketLock(&mutex);
        //microdelay(70);
        acquire_ticketLock(&mutex);
        readerCount--;
        if (readerCount == 0){
            release_ticketLock(&write);
        }
        release_ticketLock(&mutex);
    }
    return sharedCounter;
}

/*    if(countDigit(id)<2 && i==0){
        a[k] = id + '0';
        k = k+1;
        i = 1;
    }
    if(countDigit(id) >1){
    j = countDigit(id);
    num = id;
    while (num>0){
    tmp[j-1] = num % 10;
    c[j-1] = tmp[j-1] + '0';
    num = num / 10;
        //printf("\n%d////",num);
    j--;
   	}

    for(j=0;j<countDigit(id);j++){
      a[k] = c[j];
      k++;
    }
        //a[k] = '0';
        k = k + countDigit(id);
    }else{
        if(i!=0){
          a[k] = '0';
          a[k+1] = id + '0';
          k = k+2;
        }
       // printf("**%c**\n",*(*commandArray+i));
      }
  }*/
