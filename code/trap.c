#include "types.h"     // First: defines uint, uchar, ushort, pde_t
#include "defs.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "spinlock.h"
#include "traps.h"
#include "x86.h"

// 调度器类型定义（与 scheduler_test.c 保持一致）
#define SCHED_FCFS 2

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[]; // in vectors.S: array of 256 entry pointers
extern int schedSelected; // 当前调度器ID，来自 sdh.c
struct spinlock tickslock;
uint ticks;

void tvinit(void) {
  int i;

  for (i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE << 3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE << 3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void idtinit(void) { lidt(idt, sizeof(idt)); }

// PAGEBREAK: 41
void trap(struct trapframe *tf) {
  if (tf->trapno == T_SYSCALL) {
    if (myproc()->killed)//发现被标记为死亡，立即杀死
      exit();
    myproc()->tf = tf;
    syscall();
    if (myproc()->killed)
      exit();
    return;
  }

  switch (tf->trapno) {
  case T_IRQ0 + IRQ_TIMER:
    if (cpuid() == 0) {
      acquire(&tickslock);
      ticks++;
      update_statistics(); // will update proc statistic every clock tick
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE + 1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n", cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  // PAGEBREAK: 13
  default:
    if (myproc() == 0 || (tf->cs & 3) == 0) {
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n", tf->trapno,
              cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno, tf->err, cpuid(),
            tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();

  // 抢占式调度：在时钟中断时让出 CPU
  // 注意：FCFS（先来先服务）是非抢占式的，进程运行直到完成或主动阻塞
  // 其他调度算法（DEFAULT、RR、PRIORITY、SML）是抢占式的
  if (myproc() && myproc()->state == RUNNING &&
      tf->trapno == T_IRQ0 + IRQ_TIMER) {
    if (schedSelected != SCHED_FCFS) {
      yield();  // 非FCFS调度器：时钟中断时让出CPU，由调度器重新选择进程
    }
    // FCFS调度器：不抢占，进程继续运行直到完成或主动阻塞
  }

  // Check if the process has been killed since we yielded
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();
}
