#include "types.h"
#include "fcntl.h"
#include "stat.h"
#include "user.h"

int main(int argc, char** argv) {

    if (argc <= 1) {
        printf(2, "ERROR : time : Invalid input, input must be in form of [time <process>]\n");
        exit();
    }
    else
    {
        int pid;
        pid = fork();
        if (pid < 0) 
        {
            printf(2, "ERROR : time : Forking, Unable to run command\n");
            exit();
        } 
        else if (pid == 0) 
        {
          
            exec(argv[1], argv + 1) ;
                printf(2, "ERROR : time : Exec [%s] Unable to run command\n",*argv[1]);
                exit();
            
        } 
        else if (pid > 0) 
        {
            int wtime, rtime;
            int wid = waitx(&wtime, &rtime);
            printf(1, "Time report for [%s] with Process ID : [%d]\nWaiting time: [%d]\tRunning time: [%d]\n\n", argv[1], wid, wtime, rtime);
            exit();
        }
    }

}