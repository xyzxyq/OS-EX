// ptable.h
#ifndef _PTABLE_H_
#define _PTABLE_H_

#include "param.h" // for NPROC

// Lightweight snapshot exported to user programs such as ps(1).
struct proc_info {
  int pid;                   // Process ID
  int ppid;                  // Parent PID (-1 if none)
  int priority;              // Scheduling priority at the time of sampling
  int mem_size;              // Bytes mapped in the process
  enum procstate state;      // Kernel enum mirrored into user space
  char name[16];             // Command name (truncated to 16 bytes)
};

#endif // _PTABLE_H_
