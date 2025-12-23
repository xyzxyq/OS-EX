#include "sdh.h"

// 调度器名称数组定义
char *schedulerName[] = {"DEFAULT", "PRIORITY", "FCFS", "CFS", "SML"};

// 调度器数组长度
int schedulerCount = 5;

// 调度函数指针数组定义 - 初始为NULL，由initScheduler初始化
struct proc *(*schedulerFunction[5])(void);

// 当前调度器函数指针 - 初始为NULL，由initScheduler初始化
struct proc *(*ready_process)(void) = 0;

// 当前调度器ID
#if defined(DEFAULT)
int schedSelected = 0;
#elif defined(PRIORITY)
int schedSelected = 1;
#elif defined(FCFS)
int schedSelected = 2;
#elif defined(CFS)
int schedSelected = 3;
#elif defined(SML)
int schedSelected = 4;
#else
int schedSelected = 0;
#endif

// 初始化调度器函数指针 - 在 pinit() 中调用
void initScheduler(void) {
  // 填充调度函数指针数组
  schedulerFunction[0] = defaultScheduler;
  schedulerFunction[1] = priorityScheduler;
  schedulerFunction[2] = fcfsScheduler;
  schedulerFunction[3] = rrScheduler;
  schedulerFunction[4] = smlScheduler;

  // 根据 schedSelected 设置当前调度器
  ready_process = schedulerFunction[schedSelected];
}
