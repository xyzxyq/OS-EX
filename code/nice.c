#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
  int pid, priority;

  if(argc != 3){
    printf(2, "Usage: nice pid priority\n");
    exit();
  }

  pid = atoi(argv[1]);
  priority = atoi(argv[2]);

  if (priority < 1 || priority > 20) {
    printf(2, "nice: priority must be between 1 and 20\n");
    exit();
  }

  if(setpriority(pid, priority) < 0){
    printf(2, "nice: failed to set priority (pid %d not found?)\n", pid);
  } else {
    printf(1, "nice: priority of process %d set to %d\n", pid, priority);
  }

  exit();
}