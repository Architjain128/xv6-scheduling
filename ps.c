#include "types.h"
#include "fcntl.h"
#include "stat.h"
#include "user.h"

int main(int argc, char** argv) {

    if(argc==1)
    {
        if(getps()<0)
        {
            // printf(1,"ERROR : ps : Unable to run \n");
            exit();
        }
        exit();
    }
    else
    {
        if(argc>1)
        printf(1,"ERROR : ps : too many arguments\n");
        else
        printf(1,"ERROR : ps : too less arguments\n");
        exit();
    }
    
}