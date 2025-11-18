#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "ptable.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


///////////////////////////////////
// sysproc.c

int
sys_getptable(void)
{
  char *buf;
  int size;

  // 1. 解析用户传递的参数
  // 第一个参数 (buf) 是一个指针，指向的内存大小应该是 pinfo 数组那么大
  // 第二个参数 (size) 是一个整数
  if(argptr(0, &buf, sizeof(struct proc_info) * NPROC) || argint(1, &size))
    return -1;

  // 2. 调用 proc.c 中的内部函数来完成真正的工作
  return getptable(buf, size);
}

int
sys_getppid(void)
{
  return myproc()->parent->pid;
}

int
sys_setpriority(void)
{
  int pid, priority;

  if(argint(0, &pid) < 0)
    return -1;

  if(argint(1, &priority) < 0)
    return -1;

  return setpriority(pid, priority);
}

int 
sys_sem_init(void)
{
  int sem;
  int value;

  if (argint(0, &sem) < 0) 
    return -1;
  if (argint(1, &value) < 0)
    return -1;

  return sem_init(sem, value);
}

int
sys_sem_destroy(void)
{
  int sem;

  if (argint(0, &sem) < 0)
    return -1;

  return sem_destroy(sem);
}

int sys_sem_wait(void)
{
  int sem;
  int count;

  if (argint(0, &sem) < 0)
    return -1;
  if (argint(1, &count) < 0)
    return -1;

  return sem_wait(sem, count);
}

int sys_sem_signal(void)
{
  int sem;
  int count;

  if (argint(0, &sem) < 0)
    return -1;
  if (argint(1, &count) < 0)
    return -1;

  return sem_signal(sem, count);
}

int sys_clone(void)
{
  int func_add;
  int arg;
  int stack_add;

  if (argint(0, &func_add) < 0)
     return -1;
  if (argint(1, &arg) < 0)
     return -1;
  if (argint(2, &stack_add) < 0)
     return -1;
 
  return clone((void *)func_add, (void *)arg, (void *)stack_add);
  
}

int sys_join(void)
{
  int stack_add;

  if (argint(0, &stack_add) < 0)
     return -1;

  return join((void **)stack_add);
}

int
sys_getscheduler(void)
{
  return getscheduler();
}

int
sys_setscheduler(void)
{
  int sid;

  if(argint(0, &sid) < 0)
    return -1;

  return setscheduler(sid);
}


int sys_yield(void) {
  yield();
  return 0;
}
/*
  this is the actual function being called from syscall.c
  @returns - pidof the terminated child process ‐ if successful
­             -1, upon failure
*/
int sys_wait2(void) {
  int *retime, *rutime, *stime;
  if (argptr(0, (void*)&retime, sizeof(retime)) < 0)
    return -1;
  if (argptr(1, (void*)&rutime, sizeof(retime)) < 0)
    return -1;
  if (argptr(2, (void*)&stime, sizeof(stime)) < 0)
    return -1;
  return wait2(retime, rutime, stime);
}
