
#include "types.h"
#include "user.h"

int set_priority(int pid, int priority);


int number_of_processes = 10;

int main(int argc, char *argv[])
{

  
  #ifdef PBS

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
      
          sleep(200); //io time
      
      
      printf(1, "Process: PID %d :%d Finished\n", getpid(),j);;
      exit();
    }
      #ifdef PBS
      else
      set_priority(pid,100-(20+j)); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
      #endif
    
  }
 
  for (j = 0; j < number_of_processes+5; j++)
  {
    wait();
  }
  #else
  printf(1, "Scheduler flag is not PBS\n");
  #endif
  exit();
}