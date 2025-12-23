// 前向声明：struct proc 在 proc.h 中定义
struct proc;

/////////////////////////////////////////
/// Scheduler policies - Name mapping ///
/// 调度器策略 - 名称映射表            ///
/////////////////////////////////////////
// 调度器名称数组，用于打印和调试（在 sdh.c 中定义）
extern char *schedulerName[];

//////////////////////////////////////////
/// Scheduler policies - Main function ///
/// 调度器策略 - 函数声明               ///
//////////////////////////////////////////
// 各调度算法的函数声明，返回下一个要执行的进程
struct proc *defaultScheduler(void);  // 默认调度器（随机/FCFS队列）
struct proc *priorityScheduler(void); // 优先级调度器（优先级数值小 = 优先级高）
struct proc *fcfsScheduler(void);     // 先来先服务调度器
struct proc *rrScheduler(void);       // 轮转调度器（Round Robin）
struct proc *smlScheduler(void);      // 静态多级队列调度器

/////////////////////////////////////////////
/// Scheduler policies - Function mapping ///
/// 调度器策略 - 函数指针数组（在 sdh.c 中定义）
/////////////////////////////////////////////
// 调度函数指针数组，通过索引（调度器ID）获取对应的调度函数
// 用于运行时动态切换调度器：ready_process = schedulerFunction[sid]
extern struct proc *(*schedulerFunction[])(void);

//////////////////////////////////////////////
/// Default scheduler policy from compiler ///
/// 编译时默认调度器策略选择               ///
//////////////////////////////////////////////
// 通过预处理器宏在编译时选择默认调度器
// 编译命令示例：make SCHEDULER=PRIORITY 会定义 PRIORITY 宏
//
// ready_process: 函数指针，指向当前使用的调度算法（在 sdh.c 中定义）
extern struct proc *(*ready_process)(void);
// schedSelected: 当前调度器的ID编号，用于运行时判断（在 sdh.c 中定义）
extern int schedSelected;
// schedulerCount: 调度器名称/函数数组的长度（在 sdh.c 中定义）
extern int schedulerCount;

// 初始化调度器函数指针（在 pinit 中调用）
void initScheduler(void);
