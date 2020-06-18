#include "types.h"
#include "stat.h"
#include "user.h"


int main()
{
    /*int pid,pid1,i;
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
        
    }
     wait(); 
     wait();
        printf(1,"the year is %d---%d\n",getyear(),getpid());
        kill(getpid());*/
    int i =0;
    int a =0;
    for(i = 0; i<3; i++){
        a = getyear();
        printf(1,"the year is %d\n",a);
        }
    exit();
}
