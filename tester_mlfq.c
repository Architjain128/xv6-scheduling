
#include "types.h"
#include "user.h"


int number_of_processes = 5;

int main(int argc, char *argv[])
{
  #ifndef MLFQ
    printf(1, "Scheduler flag is not MLFQ\n");
    exit();
    return 0;
  #else
  int j;
  for (j = 0; j < number_of_processes; j++)
  {
    int pid = fork();
    if (pid < 0)
    {
      printf(1, "Fork failed\n");
      continue;
    }
    if (pid == 0)
    {
      sleep(j*200); //io time
      printf(1, "Process: PID %d :%d Finished\n", getpid(),j);
      exit();
    }
  }
  for (j = 0; j < number_of_processes+5; j++)
  {
    wait();
  }
  
  getps();
  #endif
  exit();
}