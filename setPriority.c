#include "types.h"
#include "fcntl.h"
#include "stat.h"
#include "user.h"

int main(int argc, char** argv) {

    // #ifdef PBS
    if (argc != 3) {
        printf(2, "ERROR : time : Invalid input, input must be in form of [setPriority <new_priority> <PID>]\n");
        exit();
    }
    else
    {
        int pri=0,pid=0;
        int base=1;
        for(int i=strlen(argv[1]);i>0;i--)
        {
            pri+=((int)(argv[1][i-1])-'0')*base;
            base*=10;
        }
        base=1;
        for(int i=strlen(argv[2]);i>0;i--)
        {
            pid+=((int)(argv[2][i-1])-'0')*base;
            base*=10;
        }
;
        if(pri<=100 && pri>=0)
        {
            set_priority(pri,pid);
            exit();
            // encountering a page fault
        }
        else
        {
            printf(2, "ERROR : time : Invalid input, inputvalue for <new_priority> must be between 0 and 100\n");
            exit();
        }
        
    }
    // #endif

    // #ifndef PBS
    // printf(2, "ERROR : scheduling : This will work only for PBS scheduler\n");
    //         exit();
    // #endif

}