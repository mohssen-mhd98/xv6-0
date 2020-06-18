// Mutual exclusion lock.
struct ticketlock {
  uint ticketlock;   //ticket    // Is the lock held?
  uint ticket; //turn

  struct proc *proc;
  struct spinlock lk;
  uint total;
  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.
};

