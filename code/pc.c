#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "param.h"      // for NPROC
#include "proc.h"       // for enum procstate
#include "ptable.h"     // for struct proc_info

// xv6 的 procstate 枚举没有字符串，我们自己定义一个
static char *states[] = {
[UNUSED]    "UNUSED  ",
[EMBRYO]    "EMBRYO  ",
[SLEEPING]  "SLEEPING",
[RUNNABLE]  "RUNNABLE",
[RUNNING]   "RUNNING ",
[ZOMBIE]    "ZOMBIE  "
};

int
main(int argc, char *argv[])
{
  struct proc_info pinfo[NPROC];
  int i;

  // 调用新的系统调用
  if(getptable(pinfo, sizeof(pinfo)) < 0){
    printf(2, "ps: getptable 失败\n");
    exit();
  }

  // 打印表头，完全按照讲义的格式
  printf(1, "%s\t%s\t%s\t%s\t%s\t%s\n", "PID", "PPID", "PRI", "MEM", "STATE", "CMD");

  // 循环打印进程信息
  for(i=0; i < NPROC; i++){
    if (pinfo[i].pid == 0) // 假设 pid 0 是无效条目
      continue;

    // 处理 N/A
    if(pinfo[i].ppid == -1) {
      printf(1, "%d\t%s\t%d\t%d\t%s\t%s\n",
        pinfo[i].pid,
        "N/A",
        pinfo[i].priority,
        pinfo[i].mem_size,
        states[pinfo[i].state],
        pinfo[i].name
      );
    } else {
      printf(1, "%d\t%d\t%d\t%d\t%s\t%s\n",
        pinfo[i].pid,
        pinfo[i].ppid,
        pinfo[i].priority,
        pinfo[i].mem_size,
        states[pinfo[i].state],
        pinfo[i].name
      );
    }
  }

  exit();
}