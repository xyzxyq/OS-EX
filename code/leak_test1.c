#include "types.h"
#include "stat.h"
#include "user.h"

int main() {
    int sem_id = 5; // 随便选一个 ID
    sem_init(sem_id, 1);

    int pid = fork();
    if (pid == 0) {
        printf(1, "Child: acquiring semaphore...\n");
        sem_wait(sem_id, 1);
        printf(1, "Child: acquired! Now crashing without signal...\n");
        exit(); // 直接退出，没有 signal！
    }

    wait(); // 等待子进程退出（此时内核应该触发回收）
    
    printf(1, "Parent: trying to acquire semaphore...\n");
    // 如果内核没有回收，这里会永远卡住
    sem_wait(sem_id, 1); 
    printf(1, "Parent: Success! Semaphore was recovered.\n");
    
    sem_signal(sem_id, 1);
    sem_destroy(sem_id);
    exit();
}