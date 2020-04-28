#include "types.h"
#include "stat.h"
#include "user.h"


int main()
{
    int pid,pid1,i;
    i=0;
    pid = fork();
    pid1 = fork();
    if(pid==0){
        while (i!=5)
        {
            i++;
        }
        exit();
    }
    if(pid1==0){
        while (i!=5)
        {
            i++;
        }
        exit();
    }
     wait(); 
        printf(1,"the year is %d---%d\n",getyear(),getpid());
        kill(getpid());
    exit();
}
