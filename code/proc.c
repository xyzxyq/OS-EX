#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#include "ptable.h" //自定义头文件

#include "sdh.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct semaphore {
  int value;
  int active;
  struct spinlock lock;
};

struct semaphore sema[32];

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

struct spinlock schedulerlock;

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  initlock(&schedulerlock, "schedulerlock");
  
  // 初始化信号量锁
  for(int i = 0; i < 32; i++)
    initlock(&sema[i].lock, "semaphore"); // 使用 sema
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
  p->priority = 10;
  p->pid = nextpid++;

  // 清空信号量持有记录
  for(int i = 0; i < 32; i++){
    p->sem_held[i] = 0;
  }
  //end

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
  int i; // 用于循环

  if(curproc == initproc)
    panic("init exiting");

  // --- 新增代码：信号量自动回收机制 ---
  // 检查所有信号量，看当前进程是否持有资源
  for(i = 0; i < 32; i++) {
    if(curproc->sem_held[i] > 0) {
      // 发现未释放的资源！
      int count = curproc->sem_held[i];
      
      // 打印内核警告（可选，但在调试阶段非常有用）
      cprintf("WARNING: pid %d exited with %d resources of sem %d held. Auto-releasing.\n", 
              curproc->pid, count, i);
      
      // 强制执行 signal 操作
      // 注意：这里我们不能直接调用 sem_signal，因为 sem_signal 会操作 myproc()->sem_held，
      // 虽然逻辑上兼容，但为了避免不必要的锁竞争或副作用，直接操作底层 sema 结构更稳妥。
      // 不过，直接调用 sem_signal 是最复用代码且安全的方式。
      
      sem_signal(i, count);
      
      // 确保账本清零（sem_signal 会做，但这里再次确保）
      curproc->sem_held[i] = 0;
    }
  }
  // --------------------------------

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

int wait2(int *retime, int *rutime, int *stime) {
  struct proc *p;
  int havekids, pid;
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != myproc())
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        *retime = p->retime;
        *rutime = p->rutime;
        *stime = p->stime;
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->ctime = 0;
        p->retime = 0;
        p->rutime = 0;
        p->stime = 0;
        p->priority = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || myproc()->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(myproc(), &ptable.lock);  //DOC: wait-sleep
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
/*
void
scheduler(void)
{
  struct proc *p = 0, *p1 = 0, *highP = 0;

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

      // Choose the process with highest priority (among RUNNABLEs)
      highP = p;
      for(p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++){
        if((p1->state == RUNNABLE) && (highP->priority > p1->priority))   
          highP = p1;
      }
      p = highP;

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
    release(&ptable.lock);
  }
}
*/
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for(;;) {
      // Enable interrupts on this processor.
      sti();

      // Loop over process table looking for process to run.
      acquire(&ptable.lock);
      //p = ready_process();

      acquire(&schedulerlock);
      p = (*ready_process)();
      release(&schedulerlock);
     
      if (p != 0) {
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

// Change Process Priority
//pid is the id of process
//priority is the priority value
//the return value is pid
// proc.c
int
setpriority(int pid, int priority)
{
  struct proc *p;
  int found = 0;

  // 检查优先级范围 [1, 20] (讲义要求)
  // 注意：讲义说 [1,20]，数值越小优先级越高
  if(priority < 1 || priority > 20)
    return -1;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->priority = priority;
      found = 1;
      break;
    }
  }
  release(&ptable.lock);

  if(found)
    return 0; // 成功
  else
    return -1; // 未找到 PID
}

//sem is the index of sema
// proc.c

int
sem_init(int sem, int value)
{
  if(sem < 0 || sem >= 32)
    return -1;

  acquire(&sema[sem].lock);
  
  // 如果已经在使用中，则返回错误
  if(sema[sem].active == 1) {
    release(&sema[sem].lock);
    return -1;
  }

  sema[sem].value = value;
  sema[sem].active = 1; // 关键！必须置为 1，sem_wait 才能工作
  
  release(&sema[sem].lock);
  return 0;
}

int
sem_destroy(int sem)
{
  if(sem < 0 || sem >= 32)
    return -1;

  acquire(&sema[sem].lock);
  
  if(sema[sem].active == 0) {
    release(&sema[sem].lock);
    return -1;
  }

  sema[sem].active = 0; // 标记为未使用
  sema[sem].value = 0;
  
  release(&sema[sem].lock);
  return 0;
}

int clone(void (*func)(void *), void *arg, void *stack)
{

   int i, pid;
   struct proc *np;
   int *myarg;
   int *myret;
   struct proc *curproc = myproc();

   if((np = allocproc()) == 0)
     return -1;

   np->pgdir = curproc->pgdir; 
   np->sz = curproc->sz;
   np->parent = curproc;
   *np->tf = *curproc->tf;
   np->stack = stack;

   np->tf->eax = 0; 

   /*
   *myarg = (int)arg;

   *myret = np->tf->eip;
   */
   
   np->tf->eip = (int)func;

   myret = stack + 4096 - 2 * sizeof(int *);
   *myret = 0xFFFFFFFF;
   
   myarg = stack + 4096 - sizeof(int *);
   *myarg = (int)arg;

   np->tf->esp = (int)stack +  PGSIZE - 2 * sizeof(int *);
   np->tf->ebp = np->tf->esp;

   np->isthread = 1;
  
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

int join(void **stack)
{

  struct proc *p;
  int haveKids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;) {
    haveKids = 0;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->parent != curproc || p->isthread != 1 )
        continue;
      haveKids = 1;

      if (p->state == ZOMBIE) {
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        *stack = p->stack;
        release(&ptable.lock);
        return pid;
      }
    }
    
    if (!haveKids || curproc->killed) {
      release(&ptable.lock);
      return -1;
    }

    sleep(curproc, &ptable.lock);

  }
  return 0;
}

// Run every clock tick and update the statistic fields of each process
void 
update_statistics() {
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    switch(p->state) {
      case SLEEPING:
        p->stime++;
        break;
      case RUNNABLE:
        p->retime++;
        break;
      case RUNNING:
        p->rutime++;
        break;
      default:
        ;
    }
  }
  release(&ptable.lock);
}

// Generate a random number, between 0 and M
// This is a modified version of the LFSR alogrithm
// found here: http://goo.gl/At4AIC */
int
random(int max) {

  if(max <= 0) {
    return 1;
  }

  static int z1 = 12345; // 12345 for rest of zx
  static int z2 = 12345; // 12345 for rest of zx
  static int z3 = 12345; // 12345 for rest of zx
  static int z4 = 12345; // 12345 for rest of zx

  int b;
  b = (((z1 << 6) ^ z1) >> 13);
  z1 = (((z1 & 4294967294) << 18) ^ b);
  b = (((z2 << 2) ^ z2) >> 27);
  z2 = (((z2 & 4294967288) << 2) ^ b);
  b = (((z3 << 13) ^ z3) >> 21);
  z3 = (((z3 & 4294967280) << 7) ^ b);
  b = (((z4 << 3) ^ z4) >> 12);
  z4 = (((z4 & 4294967168) << 13) ^ b);

  // if we have an argument, then we can use it
  int rand = ((z1 ^ z2 ^ z3 ^ z4)) % max;

  if(rand < 0) {
    rand = rand * -1;
  }

  return rand;
}

// system call - get scheduler policy id
int
getscheduler()
{
  int sid; 
  acquire(&schedulerlock);
  sid = schedSelected;
  release(&schedulerlock);
  return sid;
}

// system call - set scheduler policy
int
setscheduler(int sid) 
{
  int max = sizeof(schedulerName) / sizeof(char *);
  if(sid < 0 || sid > max) {
    return -1;
  }

  acquire(&schedulerlock);

  ///////////////////////////////////////////////
  // Init / remove scheduler policy in runtime //
  ///////////////////////////////////////////////
  // if (sid == n) {
  //    -> init
  // } else {
  //    -> remove
  // }
  ///////////////////////////////////////////////

  ready_process = schedulerFunction[sid];
  schedSelected = sid;
  release(&schedulerlock);

  return sid;
}


// The following funtions are used by scheduler() for selecting 
// the next process to be executed.
// All of them select a process (using a different policy) among runnables.

/////////////////////////////////////////////
// Scheduler policies - Pre-installed code //
/////////////////////////////////////////////

// Default scheduler -------------------
struct proc *defaultScheduler(void) {
  struct proc *p;
  int i, rnd;
  
  rnd = random(NPROC);
  for (i = 0; i < NPROC; i++) {
    p = ptable.proc + ((rnd + i) % NPROC); 
    if (p->state == RUNNABLE) return p;
  }
  return 0;
}

// Priority Scheduler -------------------
struct proc *priorityScheduler() {

}

// FCFS Scheduler -----------------------
struct proc *fcfsScheduler() {

}

// CFS Scheduler -------------------------
struct proc *rrScheduler() {

}

// SML Scheduler ---------------------------
struct proc *smlScheduler() {

}

// proc.c

// (确保你在文件的最末尾添加这个函数)

// proc.c
// (请确保你删除了旧的、参数错误的 getptable 函数)
// (将这个函数粘贴到 proc.c 的末尾)

// 在 proc.c 文件的末尾

int 
getptable(void *ubuf, int size)
{
  struct proc *p;
  struct proc_info pi; // 只在栈上分配一个结构体，非常安全
  char *uptr = (char*)ubuf;
  int i = 0;

  // 检查用户提供的缓冲区是否足够大
  // 如果用户传来的 size 小于总表大小，可能会导致溢出，这里做个防御性检查
  if(size < NPROC * sizeof(struct proc_info))
    return -1;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    // 每次循环前清零 pi，防止数据残留
    memset(&pi, 0, sizeof(pi));

    if(p->state != UNUSED){
      pi.pid = p->pid;
      // 处理父进程 PID，如果是 init 或无父进程，设为 -1 (N/A)
      pi.ppid = (p->parent) ? p->parent->pid : -1; 
      pi.priority = p->priority;
      pi.mem_size = p->sz;
      pi.state = p->state;
      safestrcpy(pi.name, p->name, sizeof(pi.name));
    } 
    // 如果是 UNUSED，pi 已经被 memset 为 0，pid 也是 0，ps 命令会跳过它

    // 计算当前结构体在用户缓冲区的目标地址
    // 直接从内核将这就一个结构体 copy 到用户空间的正确偏移位置
    if(copyout(myproc()->pgdir, (uint)(uptr + i * sizeof(struct proc_info)), (char*)&pi, sizeof(pi)) < 0){
      release(&ptable.lock);
      return -1;
    }
    i++;
  }

  release(&ptable.lock);
  return 0;
}

// proc.c

// P 操作：等待并消耗资源
int 
sem_wait(int sem, int count)
{
  // 1. 安全性检查
  if(sem < 0 || sem >= 32)
    return -1;

  acquire(&sema[sem].lock);

  // 2. 检查该信号量是否处于活跃状态
  if(sema[sem].active == 0){
    release(&sema[sem].lock);
    return -1;
  }

  // 3. 循环检查资源是否足够 (关键逻辑)
  // 只要当前值小于请求值，就进入睡眠
  while(sema[sem].value < count){
    // sleep 函数的参数：
    // 参数1: 睡眠的“频道” (channel)，我们用信号量数组元素的地址作为唯一标识
    // 参数2: 当前持有的锁 (sleep 会自动释放这把锁，醒来后自动重新获取)
    sleep(&sema[sem], &sema[sem].lock);
    
    // 进程醒来后会从这里继续执行，并已经重新持有了锁。
    // 此时回到 while 开头再次检查 value < count
    
    // 防御性检查：如果在睡眠期间信号量被销毁了
    if(sema[sem].active == 0){
       release(&sema[sem].lock);
       return -1;
    }
  }

  // 4. 消耗资源
  sema[sem].value -= count;

  // 增加资源
  sema[sem].value += count;
  
  // --- 新增代码：销账 ---
  // 如果该进程确实持有这个信号量的资源，则扣除
  if(myproc()->sem_held[sem] >= count) {
      myproc()->sem_held[sem] -= count;
  } else {
      // 这种情况可能是“生产者”在释放它从未申请过的资源，
      // 或者出现了逻辑错误。为了安全，我们只清零，不减成负数。
      myproc()->sem_held[sem] = 0;
  }
  // -------------------

  wakeup(&sema[sem]);

  release(&sema[sem].lock);
  return 0;
}

// proc.c

// V 操作：释放资源并唤醒等待者
int 
sem_signal(int sem, int count)
{
  // 1. 安全性检查
  if(sem < 0 || sem >= 32)
    return -1;

  acquire(&sema[sem].lock);

  // 2. 检查活跃状态
  if(sema[sem].active == 0){
    release(&sema[sem].lock);
    return -1;
  }

  // 3. 增加资源
  sema[sem].value += count;

  // 4. 唤醒等待该信号量的所有进程
  // wakeup 的参数必须与 sleep 的第一个参数完全一致 (即信号量的地址)
  wakeup(&sema[sem]);

  release(&sema[sem].lock);
  return 0;
}