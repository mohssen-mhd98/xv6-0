// Mutual exclusion spin locks.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "ticketlock.h"

int holding_ticketLock(struct ticketlock *lock);

void
ini_ticketLock(struct ticketlock *lk, char *name)
{
  lk->name = name;
  lk->ticketlock = 0;
  lk->ticket = 0;
  lk->proc = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void
acquire_ticketLock(struct ticketlock *lk)
{
  acquire(&lk->lk);
  pushcli(); // disable interrupts to avoid deadlock.
  uint ticketlock;
  if(holding_ticketLock(lk))
    panic("acquire");

  ticketlock = fetch_and_add(&lk->ticketlock, 1);
  //cprintf("***%d***%s\n",lk->ticket, lk->name );
  // The xchg is atomic.
  release(&lk->lk);
  while(lk->ticket != ticketlock)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen after the lock is acquired.
  __sync_synchronize();

  // Record info about lock acquisition for debugging.
  lk->proc = myproc();
  lk->cpu = mycpu();
  getcallerpcs(&lk, lk->pcs);
}

// Release the lock.
void
release_ticketLock(struct ticketlock *lk)
{
  acquire(&lk->lk);
  if(!holding_ticketLock(lk))
    panic("release");

  lk->pcs[0] = 0;
  lk->cpu = 0;
  lk->proc = myproc();
  fetch_and_add(&lk->ticket, 1);
  release(&lk->lk);
  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other cores before the lock is released.
  // Both the C compiler and the hardware may re-order loads and
  // stores; __sync_synchronize() tells them both not to.
  __sync_synchronize();

  wakeup(lk);

  popcli();
}



// Check whether this cpu is holding the lock.
int
holding_ticketLock(struct ticketlock *lock)
{
  int r;
  pushcli();
  r = (lock->ticketlock != lock->ticket) && (lock->proc == myproc());
  popcli();
  return r;
}
