struct proc_stat
{
  int pid; 
  int runtime; 
  int num_run; 
  int current_queue; 
  int ticks[5]; 
};