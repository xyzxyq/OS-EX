// scheduler_test.c
// 调度算法比较测试程序
// 创建6个进程（3个CPU密集型 + 3个IO密集型）
// 对比不同调度算法的性能

#include "types.h"
#include "user.h"
#include "sdh.h"
#include "fcntl.h"

// 调度算法名称
char *sched_names[] = {
  "DEFAULT",
  "PRIORITY",
  "FCFS",
  "RR",
  "SML"
};

// CPU密集型任务：大量计算
void cpu_intensive(int id) {
  volatile int sum = 0;
  int i, j;
  // 使用嵌套循环增加计算量
  for(i = 0; i < 100; i++) {
    for(j = 0; j < 100000; j++) {
      sum += j;
      sum = sum % 1000000;  // 防止溢出
    }
  }
  // 输出完成信息
  printf(1, "CPU Process %d completed (sum=%d)\n", id, sum);
}

// IO密集型任务：频繁sleep模拟IO等待
void io_intensive(int id) {
  int i, j;
  volatile int sum = 0;
  // 多次短暂计算后进入睡眠
  for(i = 0; i < 50; i++) {
    // 少量计算
    for(j = 0; j < 1000; j++) {
      sum += j;
    }
    // 模拟IO等待
    sleep(5);
  }
  printf(1, "IO Process %d completed\n", id);
}

// 打印统计信息
void print_statistics(int sched_id, int retime[], int rutime[], int stime[], int pids[]) {
  int total_retime = 0, total_rutime = 0, total_stime = 0;
  int total_turnaround = 0;
  int i;
  
  printf(1, "\n---------- %s Scheduler Results ----------\n", sched_names[sched_id]);
  printf(1, "PID\tReady\tRun\tSleep\tTurnaround\n");
  printf(1, "-------------------------------------------\n");
  
  for(i = 0; i < 6; i++) {
    int turnaround = retime[i] + rutime[i] + stime[i];
    printf(1, "%d\t%d\t%d\t%d\t%d\n", pids[i], retime[i], rutime[i], stime[i], turnaround);
    total_retime += retime[i];
    total_rutime += rutime[i];
    total_stime += stime[i];
    total_turnaround += turnaround;
  }
  
  printf(1, "-------------------------------------------\n");
  printf(1, "Avg:\t%d\t%d\t%d\t%d\n", 
         total_retime / 6, total_rutime / 6, total_stime / 6, total_turnaround / 6);
  printf(1, "\n");
}

// 运行单个调度算法的测试
void run_scheduler_test(int sched_id) {
  int pids[6];
  int retime[6], rutime[6], stime[6];
  int i, pid;
  int status;
  
  printf(1, "\n========== Testing %s Scheduler ==========\n", sched_names[sched_id]);
  
  // 设置调度算法
  status = setscheduler(sched_id);
  if(status < 0) {
    printf(1, "Error: Failed to set scheduler %d\n", sched_id);
    return;
  }
  
  // 创建6个子进程
  for(i = 0; i < 6; i++) {
    pid = fork();
    if(pid < 0) {
      printf(1, "Error: fork failed\n");
      exit();
    }
    
    if(pid == 0) {
      // 子进程
      // 设置不同优先级：CPU密集型使用高/中优先级，IO密集型使用中/低优先级
      if(i < 3) {
        // CPU密集型进程：优先级 2, 5, 8
        setpriority(getpid(), (i + 1) * 3 - 1);
        cpu_intensive(i);
      } else {
        // IO密集型进程：优先级 10, 14, 18
        setpriority(getpid(), (i - 3) * 4 + 10);
        io_intensive(i);
      }
      exit();
    } else {
      // 父进程记录子进程PID
      pids[i] = pid;
    }
  }
  
  // 等待所有子进程完成并收集统计信息
  for(i = 0; i < 6; i++) {
    wait2(&retime[i], &rutime[i], &stime[i]);
  }
  
  // 打印统计结果
  print_statistics(sched_id, retime, rutime, stime, pids);
}

// 打印综合对比表格
void print_summary_header() {
  printf(1, "\n");
  printf(1, "============================================\n");
  printf(1, "         SCHEDULER COMPARISON TEST         \n");
  printf(1, "============================================\n");
  printf(1, "Test Configuration:\n");
  printf(1, "  - 3 CPU-intensive processes (priority: 2, 5, 8)\n");
  printf(1, "  - 3 IO-intensive processes (priority: 10, 14, 18)\n");
  printf(1, "============================================\n");
}

int main(int argc, char *argv[]) {
  int test_schedulers[] = {1, 2, 3, 4};  // PRIORITY, FCFS, RR, SML
  int num_schedulers = 4;
  int i;
  
  // 如果有命令行参数，只测试指定的调度器
  if(argc > 1) {
    int sched_id = atoi(argv[1]);
    if(sched_id >= 0 && sched_id <= 4) {
      print_summary_header();
      run_scheduler_test(sched_id);
      exit();
    }
    printf(1, "Usage: scheduler_test [scheduler_id]\n");
    printf(1, "  0 - DEFAULT\n");
    printf(1, "  1 - PRIORITY\n");
    printf(1, "  2 - FCFS\n");
    printf(1, "  3 - RR (Round Robin)\n");
    printf(1, "  4 - SML (Static Multi-Level)\n");
    exit();
  }
  
  // 打印测试说明
  print_summary_header();
  
  // 依次测试各种调度算法
  for(i = 0; i < num_schedulers; i++) {
    run_scheduler_test(test_schedulers[i]);
  }
  
  // 最后恢复默认调度器
  setscheduler(0);
  
  printf(1, "\n============================================\n");
  printf(1, "         ALL TESTS COMPLETED               \n");
  printf(1, "============================================\n");
  
  exit();
}
