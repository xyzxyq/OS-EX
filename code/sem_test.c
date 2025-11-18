#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define NUM_CHILDREN 10
#define TARGET_COUNT_PER_CHILD 50
#define COUNTER_FILE "counter"
#define SEM_ID 0  // 使用 0 号信号量

void
test_counter()
{
  int i, j;
  int fd;
  int val;

  // 1. 初始化文件
  // 创建文件并写入初始值 0
  fd = open(COUNTER_FILE, O_CREATE | O_RDWR);
  if(fd < 0){
    printf(1, "Error: create counter file failed\n");
    exit();
  }
  val = 0;
  write(fd, &val, sizeof(int)); // 写入 4 字节整数
  close(fd);

  // 2. 初始化信号量
  // 初始值为 1，表示互斥锁（Mutex）
  if(sem_init(SEM_ID, 1) < 0){
    printf(1, "Error: sem_init failed\n");
    exit();
  }

  printf(1, "Starting %d children, each incrementing %d times...\n", NUM_CHILDREN, TARGET_COUNT_PER_CHILD);

  // 3. 创建子进程
  for(i = 0; i < NUM_CHILDREN; i++){
    int pid = fork();
    if(pid < 0){
      printf(1, "fork failed\n");
      exit();
    }

    if(pid == 0){
      // --- 子进程代码 ---
      for(j = 0; j < TARGET_COUNT_PER_CHILD; j++){
        
        // P 操作：进入临界区
        if(sem_wait(SEM_ID, 1) < 0){
             printf(1, "Child: sem_wait failed\n");
             exit();
        }

        // --- 临界区开始 ---
        // 读取当前值
        fd = open(COUNTER_FILE, O_RDONLY);
        if(fd < 0){
             printf(1, "Child: open read failed\n");
             exit();
        }
        if(read(fd, &val, sizeof(int)) != sizeof(int)){
             printf(1, "Child: read failed\n");
             exit();
        }
        close(fd);

        // 更新值
        val++;

        // 写回新值
        // 注意：xv6 open 会将 offset 置 0，覆盖原有数据
        fd = open(COUNTER_FILE, O_WRONLY); 
        if(fd < 0){
             printf(1, "Child: open write failed\n");
             exit();
        }
        if(write(fd, &val, sizeof(int)) != sizeof(int)){
             printf(1, "Child: write failed\n");
             exit();
        }
        close(fd);
        // --- 临界区结束 ---

        // V 操作：离开临界区
        if(sem_signal(SEM_ID, 1) < 0){
             printf(1, "Child: sem_signal failed\n");
             exit();
        }
      }
      
      exit(); // 子进程结束
    }
  }

  // 4. 父进程等待所有子进程退出
  for(i = 0; i < NUM_CHILDREN; i++){
    wait();
  }

  // 5. 验证结果
  fd = open(COUNTER_FILE, O_RDONLY);
  read(fd, &val, sizeof(int));
  close(fd);

  printf(1, "Final counter value: %d\n", val);

  if(val == NUM_CHILDREN * TARGET_COUNT_PER_CHILD){
    printf(1, "TEST PASSED!\n");
  } else {
    printf(1, "TEST FAILED! Expected %d\n", NUM_CHILDREN * TARGET_COUNT_PER_CHILD);
  }

  // 6. 清理信号量
  sem_destroy(SEM_ID);
  
  // 清理测试文件
  unlink(COUNTER_FILE);
  
  exit();
}

int
main(int argc, char *argv[])
{
  test_counter();
  return 0;
}