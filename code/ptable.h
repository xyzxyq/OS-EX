// ptable.h
#ifndef _PTABLE_H_
#define _PTABLE_H_

#include "param.h" // for NPROC

struct proc_info {
  int pid;
  int ppid;
  int priority;
  int mem_size;
  enum procstate state;
  char name[16];
};

#endif // _PTABLE_H_