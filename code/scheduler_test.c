// scheduler_test.c
// 调度算法测试程序
// 三个典型场景：车队效应、交互型vs背景、优先级饥饿
// 对比不同调度算法的性能指标

#include "types.h"
#include "user.h"
#include "fcntl.h"

#define NCHILD 6          // 子进程数量
#define CPU_WORK 5000000  // CPU 基准工作量（增大10倍确保跨越多个时钟周期）
#define IO_WORK 20        // IO 基准次数

// 调度算法编号（与内核保持一致）
#define SCHED_DEFAULT  0
#define SCHED_PRIORITY 1
#define SCHED_FCFS     2
#define SCHED_RR       3
#define SCHED_SML      4

// 任务类型
#define ROLE_LONG   0  // 长任务
#define ROLE_SHORT  1  // 短任务
#define ROLE_MEDIUM 2  // 中等任务
#define ROLE_INTER  3  // 交互型
#define ROLE_IO_BG  4  // 后台IO
#define ROLE_CPU_BG 5  // 后台CPU
#define ROLE_HIGH   6  // 高优先级
#define ROLE_MID    7  // 中优先级
#define ROLE_LOW    8  // 低优先级

// 全局文件描述符，用于输出到文件
int outfd = -1;

struct child_info {
  int pid;
  int role;
};

// 同时输出到屏幕和文件
void output(char *s) {
  printf(1, "%s", s);
  if(outfd >= 0) {
    write(outfd, s, strlen(s));
  }
}

// 输出数字
void output_int(int n) {
  char buf[16];
  int i = 0, neg = 0;
  if(n < 0) { neg = 1; n = -n; }
  do { buf[i++] = '0' + (n % 10); n /= 10; } while(n > 0);
  if(neg) buf[i++] = '-';
  char out[16];
  int j;
  for(j = 0; j < i; j++) out[j] = buf[i-1-j];
  out[j] = 0;
  output(out);
}

// CPU 密集型工作
void cpu_work(int work) {
  int i, j;
  volatile int sum = 0;
  for(i = 0; i < work; i++) {
    for(j = 0; j < 100; j++) {
      int x = i * j + (i ^ j);
      x = (x << 1) ^ (x >> 1);
      sum += x;
    }
  }
}

// IO 密集型工作（包含真实文件IO操作）
void io_work(int rounds) {
  int i, fd;
  char buf[] = "io test\n";
  for(i = 0; i < rounds; i++) {
    fd = open("io_test", O_CREATE | O_WRONLY);
    if(fd >= 0) {
      write(fd, buf, sizeof(buf) - 1);
      close(fd);
    }
    sleep(15);  // 模拟IO等待
  }
}

// 交互型工作：短CPU + 频繁sleep（模拟用户输入等待）
void interactive_work(int rounds) {
  int i;
  for(i = 0; i < rounds; i++) {
    cpu_work(CPU_WORK / 50);  // 很短的CPU计算
    sleep(10);                 // 等待用户/IO
  }
}

// ============================================================
// 场景1：纯CPU长短作业 - 展示FCFS车队效应 & RR公平性
// ============================================================
void test_scenario1(int schedId, char *schedName) {
  int pid, i, k;
  int retime, rutime, stime;
  struct child_info info[NCHILD];
  int nchildren = 0;
  
  // 统计变量
  int long_retime = 0, long_rutime = 0, long_count = 0;
  int short_retime = 0, short_rutime = 0, short_count = 0;
  int med_retime = 0, med_rutime = 0, med_count = 0;
  
  int cpu_long = CPU_WORK;
  int cpu_short = CPU_WORK / 10;
  int cpu_medium = CPU_WORK / 3;
  
  output("\n========== ");
  output(schedName);
  output(" - 场景1: 纯CPU长短作业(车队效应) ==========\n");
  
  setscheduler(schedId);
  
  // P1: 长作业，到达时间0
  pid = fork();
  if(pid == 0) {
    setpriority(getpid(), 10);
    cpu_work(cpu_long);
    exit();
  }
  if(pid > 0) { info[nchildren].pid = pid; info[nchildren].role = ROLE_LONG; nchildren++; }
  
  // P2: 长作业，到达时间0
  pid = fork();
  if(pid == 0) {
    setpriority(getpid(), 10);
    cpu_work(cpu_long);
    exit();
  }
  if(pid > 0) { info[nchildren].pid = pid; info[nchildren].role = ROLE_LONG; nchildren++; }
  
  sleep(10);  // 等一会儿再创建短作业
  
  // P3: 短作业，稍晚到达
  pid = fork();
  if(pid == 0) {
    setpriority(getpid(), 10);
    cpu_work(cpu_short);
    exit();
  }
  if(pid > 0) { info[nchildren].pid = pid; info[nchildren].role = ROLE_SHORT; nchildren++; }
  
  // P4: 短作业
  pid = fork();
  if(pid == 0) {
    setpriority(getpid(), 10);
    cpu_work(cpu_short);
    exit();
  }
  if(pid > 0) { info[nchildren].pid = pid; info[nchildren].role = ROLE_SHORT; nchildren++; }
  
  sleep(10);
  
  // P5: 中等作业
  pid = fork();
  if(pid == 0) {
    setpriority(getpid(), 10);
    cpu_work(cpu_medium);
    exit();
  }
  if(pid > 0) { info[nchildren].pid = pid; info[nchildren].role = ROLE_MEDIUM; nchildren++; }
  
  // P6: 中等作业
  pid = fork();
  if(pid == 0) {
    setpriority(getpid(), 10);
    cpu_work(cpu_medium);
    exit();
  }
  if(pid > 0) { info[nchildren].pid = pid; info[nchildren].role = ROLE_MEDIUM; nchildren++; }
  
  // 收集统计
  output("PID\t类型\t\t就绪时间\t运行时间\t周转时间\n");
  output("----\t----\t\t--------\t--------\t--------\n");
  
  for(i = 0; i < nchildren; i++) {
    pid = wait2(&retime, &rutime, &stime);
    if(pid > 0) {
      int turnaround = retime + rutime + stime;
      int role = ROLE_MEDIUM;
      for(k = 0; k < nchildren; k++) {
        if(info[k].pid == pid) { role = info[k].role; break; }
      }
      
      char *type = (role == ROLE_LONG) ? "长作业" : 
                   (role == ROLE_SHORT) ? "短作业" : "中作业";
      
      output_int(pid); output("\t"); output(type); output("\t\t");
      output_int(retime); output("\t\t");
      output_int(rutime); output("\t\t");
      output_int(turnaround); output("\n");
      
      if(role == ROLE_LONG) {
        long_retime += retime; long_rutime += rutime; long_count++;
      } else if(role == ROLE_SHORT) {
        short_retime += retime; short_rutime += rutime; short_count++;
      } else {
        med_retime += retime; med_rutime += rutime; med_count++;
      }
    }
  }
  
  output("----\t----\t\t--------\t--------\t--------\n");
  if(long_count > 0) {
    output("长作业平均\t\t"); output_int(long_retime/long_count);
    output("\t\t"); output_int(long_rutime/long_count);
    output("\t\t"); output_int((long_retime+long_rutime)/long_count); output("\n");
  }
  if(short_count > 0) {
    output("短作业平均\t\t"); output_int(short_retime/short_count);
    output("\t\t"); output_int(short_rutime/short_count);
    output("\t\t"); output_int((short_retime+short_rutime)/short_count); output("\n");
  }
  if(med_count > 0) {
    output("中作业平均\t\t"); output_int(med_retime/med_count);
    output("\t\t"); output_int(med_rutime/med_count);
    output("\t\t"); output_int((med_retime+med_rutime)/med_count); output("\n");
  }
}

// ============================================================
// 场景2：交互型 vs 背景任务 - 展示RR/SML对交互友好
// ============================================================
void test_scenario2(int schedId, char *schedName) {
  int pid, i, k;
  int retime, rutime, stime;
  struct child_info info[NCHILD];
  int nchildren = 0;
  
  int inter_retime = 0, inter_rutime = 0, inter_stime = 0, inter_count = 0;
  int cpubg_retime = 0, cpubg_rutime = 0, cpubg_count = 0;
  int iobg_retime = 0, iobg_rutime = 0, iobg_stime = 0, iobg_count = 0;
  
  output("\n========== ");
  output(schedName);
  output(" - 场景2: 交互型vs背景任务 ==========\n");
  
  setscheduler(schedId);
  
  // 2个重CPU背景任务（低优先级）
  for(i = 0; i < 2; i++) {
    pid = fork();
    if(pid == 0) {
      setpriority(getpid(), 18);  // 低优先级
      cpu_work(CPU_WORK);
      exit();
    }
    if(pid > 0) { info[nchildren].pid = pid; info[nchildren].role = ROLE_CPU_BG; nchildren++; }
  }
  
  // 2个交互型任务（高优先级，短CPU+频繁IO）
  for(i = 0; i < 2; i++) {
    pid = fork();
    if(pid == 0) {
      setpriority(getpid(), 3);  // 高优先级
      interactive_work(IO_WORK);
      exit();
    }
    if(pid > 0) { info[nchildren].pid = pid; info[nchildren].role = ROLE_INTER; nchildren++; }
  }
  
  // 2个后台IO任务（中等优先级）
  for(i = 0; i < 2; i++) {
    pid = fork();
    if(pid == 0) {
      setpriority(getpid(), 12);
      io_work(IO_WORK);
      exit();
    }
    if(pid > 0) { info[nchildren].pid = pid; info[nchildren].role = ROLE_IO_BG; nchildren++; }
  }
  
  output("PID\t类型\t\t就绪时间\t运行时间\t休眠时间\t周转时间\n");
  output("----\t----\t\t--------\t--------\t--------\t--------\n");
  
  for(i = 0; i < nchildren; i++) {
    pid = wait2(&retime, &rutime, &stime);
    if(pid > 0) {
      int turnaround = retime + rutime + stime;
      int role = ROLE_CPU_BG;
      for(k = 0; k < nchildren; k++) {
        if(info[k].pid == pid) { role = info[k].role; break; }
      }
      
      char *type = (role == ROLE_INTER) ? "交互型" :
                   (role == ROLE_CPU_BG) ? "CPU背景" : "IO背景";
      
      output_int(pid); output("\t"); output(type); output("\t\t");
      output_int(retime); output("\t\t");
      output_int(rutime); output("\t\t");
      output_int(stime); output("\t\t");
      output_int(turnaround); output("\n");
      
      if(role == ROLE_INTER) {
        inter_retime += retime; inter_rutime += rutime; inter_stime += stime; inter_count++;
      } else if(role == ROLE_CPU_BG) {
        cpubg_retime += retime; cpubg_rutime += rutime; cpubg_count++;
      } else {
        iobg_retime += retime; iobg_rutime += rutime; iobg_stime += stime; iobg_count++;
      }
    }
  }
  
  output("----\t----\t\t--------\t--------\t--------\t--------\n");
  if(inter_count > 0) {
    output("交互型平均\t\t"); output_int(inter_retime/inter_count);
    output("\t\t"); output_int(inter_rutime/inter_count);
    output("\t\t"); output_int(inter_stime/inter_count);
    output("\t\t"); output_int((inter_retime+inter_rutime+inter_stime)/inter_count); output("\n");
  }
  if(cpubg_count > 0) {
    output("CPU背景平均\t\t"); output_int(cpubg_retime/cpubg_count);
    output("\t\t"); output_int(cpubg_rutime/cpubg_count);
    output("\t\t0\t\t");
    output_int((cpubg_retime+cpubg_rutime)/cpubg_count); output("\n");
  }
  if(iobg_count > 0) {
    output("IO背景平均\t\t"); output_int(iobg_retime/iobg_count);
    output("\t\t"); output_int(iobg_rutime/iobg_count);
    output("\t\t"); output_int(iobg_stime/iobg_count);
    output("\t\t"); output_int((iobg_retime+iobg_rutime+iobg_stime)/iobg_count); output("\n");
  }
}

// ============================================================
// 场景3：优先级饥饿测试 - 展示Priority问题 & SML/RR缓解
// ============================================================
void test_scenario3(int schedId, char *schedName) {
  int pid, i, k;
  int retime, rutime, stime;
  struct child_info info[NCHILD];
  int nchildren = 0;
  
  int high_retime = 0, high_rutime = 0, high_count = 0;
  int mid_retime = 0, mid_rutime = 0, mid_count = 0;
  int low_retime = 0, low_rutime = 0, low_count = 0;
  
  int cpu_verylong = CPU_WORK * 2;
  int cpu_medium = CPU_WORK / 2;
  
  output("\n========== ");
  output(schedName);
  output(" - 场景3: 优先级饥饿测试 ==========\n");
  
  setscheduler(schedId);
  
  // P1: 超高优先级CPU大作业（会霸占CPU）
  pid = fork();
  if(pid == 0) {
    setpriority(getpid(), 1);  // 最高优先级
    cpu_work(cpu_verylong);
    exit();
  }
  if(pid > 0) { info[nchildren].pid = pid; info[nchildren].role = ROLE_HIGH; nchildren++; }
  
  // P2-P4: 中等优先级小任务
  for(i = 0; i < 3; i++) {
    pid = fork();
    if(pid == 0) {
      setpriority(getpid(), 10);  // 中优先级
      cpu_work(cpu_medium);
      exit();
    }
    if(pid > 0) { info[nchildren].pid = pid; info[nchildren].role = ROLE_MID; nchildren++; }
  }
  
  // P5-P6: 最低优先级小任务（可能饥饿）
  for(i = 0; i < 2; i++) {
    pid = fork();
    if(pid == 0) {
      setpriority(getpid(), 19);  // 最低优先级
      cpu_work(cpu_medium);
      exit();
    }
    if(pid > 0) { info[nchildren].pid = pid; info[nchildren].role = ROLE_LOW; nchildren++; }
  }
  
  output("PID\t优先级组\t就绪时间\t运行时间\t周转时间\n");
  output("----\t--------\t--------\t--------\t--------\n");
  
  for(i = 0; i < nchildren; i++) {
    pid = wait2(&retime, &rutime, &stime);
    if(pid > 0) {
      int turnaround = retime + rutime + stime;
      int role = ROLE_MID;
      for(k = 0; k < nchildren; k++) {
        if(info[k].pid == pid) { role = info[k].role; break; }
      }
      
      char *type = (role == ROLE_HIGH) ? "高优先级" :
                   (role == ROLE_MID) ? "中优先级" : "低优先级";
      
      output_int(pid); output("\t"); output(type); output("\t");
      output_int(retime); output("\t\t");
      output_int(rutime); output("\t\t");
      output_int(turnaround); output("\n");
      
      if(role == ROLE_HIGH) {
        high_retime += retime; high_rutime += rutime; high_count++;
      } else if(role == ROLE_MID) {
        mid_retime += retime; mid_rutime += rutime; mid_count++;
      } else {
        low_retime += retime; low_rutime += rutime; low_count++;
      }
    }
  }
  
  output("----\t--------\t--------\t--------\t--------\n");
  if(high_count > 0) {
    output("高优平均\t\t"); output_int(high_retime/high_count);
    output("\t\t"); output_int(high_rutime/high_count);
    output("\t\t"); output_int((high_retime+high_rutime)/high_count); output("\n");
  }
  if(mid_count > 0) {
    output("中优平均\t\t"); output_int(mid_retime/mid_count);
    output("\t\t"); output_int(mid_rutime/mid_count);
    output("\t\t"); output_int((mid_retime+mid_rutime)/mid_count); output("\n");
  }
  if(low_count > 0) {
    output("低优平均\t\t"); output_int(low_retime/low_count);
    output("\t\t"); output_int(low_rutime/low_count);
    output("\t\t"); output_int((low_retime+low_rutime)/low_count); output("\n");
  }
}

// ============================================================
// 主函数
// ============================================================
int main(int argc, char *argv[]) {
  int scenario = 0;  // 0 = 全部场景, 1/2/3 = 指定场景
  
  // 解析命令行参数
  if(argc > 1) {
    scenario = atoi(argv[1]);
    if(scenario < 0 || scenario > 3) {
      printf(1, "用法: scheduler_test [场景编号]\n");
      printf(1, "  无参数 - 运行全部 3 个场景\n");
      printf(1, "  1      - 仅运行场景1 (车队效应)\n");
      printf(1, "  2      - 仅运行场景2 (交互响应)\n");
      printf(1, "  3      - 仅运行场景3 (优先级饥饿)\n");
      exit();
    }
  }
  
  // 打开输出文件
  outfd = open("sched_result.txt", O_CREATE | O_WRONLY);
  
  output("==========================================\n");
  output("      调度算法性能比较测试              \n");
  output("==========================================\n");
  if(scenario == 0) {
    output("运行全部场景:\n");
  } else {
    output("运行场景 "); output_int(scenario); output(":\n");
  }
  if(scenario == 0 || scenario == 1)
    output("场景1: 纯CPU长短作业 - 测试车队效应\n");
  if(scenario == 0 || scenario == 2)
    output("场景2: 交互型vs背景 - 测试响应性\n");
  if(scenario == 0 || scenario == 3)
    output("场景3: 优先级饥饿 - 测试公平性\n");
  output("==========================================\n");
  
  // 调度算法列表
  int sched_ids[] = { SCHED_DEFAULT, SCHED_RR, SCHED_PRIORITY, SCHED_FCFS, SCHED_SML };
  char *sched_names[] = {
    "DEFAULT",
    "RR(轮转)",
    "PRIORITY",
    "FCFS",
    "SML(多级队列)"
  };
  int nsched = 5;
  
  int si;
  for(si = 0; si < nsched; si++) {
    // 根据参数选择运行哪些场景
    if(scenario == 0 || scenario == 1)
      test_scenario1(sched_ids[si], sched_names[si]);
    if(scenario == 0 || scenario == 2)
      test_scenario2(sched_ids[si], sched_names[si]);
    if(scenario == 0 || scenario == 3)
      test_scenario3(sched_ids[si], sched_names[si]);
  }
  
  output("\n========== 测试完成 ==========\n");
  output("结果已保存到 sched_result.txt\n");
  
  if(outfd >= 0) close(outfd);
  
  // 恢复默认调度器
  setscheduler(SCHED_DEFAULT);
  
  exit();
}
