#include "types.h"
#include "stat.h"
#include "user.h"

//each time variables that a process could have
struct processTimeFeatures{
  int creationTime;
  int terminationTime;
  int sleepingTime; 
  int readyTime; 
  int runningTime;
  };
  
// ATT & AWT $ AverageCB Time of childrens
struct averageTimeVariables{
  int averageTurnAroundTime;
  int averageCBT;
  int averageWaitingTime;
  };

  int main(void)
  {
   struct averageTimeVariables atv;
   struct processTimeFeatures *ptf = malloc(sizeof(struct processTimeFeatures));
   int i,pid,j;

    atv.averageTurnAroundTime = 0;
    atv.averageCBT = 0;
    atv.averageWaitingTime = 0;

   // Change the algorithm of scheduler to quantum algorithm
   changePolicy(2);

    for(i=0;i<10;i++){
        pid = fork();
        if(pid == 0){
            for(j=0;j<500;j++)
            printf(1, "%d : %d \n", getpid(), i);
            exit();
        }
    }

    for(i=0;i<10;i++){
        
        pid = waitingForChild(ptf);
        atv.averageTurnAroundTime += ptf->terminationTime - ptf->creationTime;
        atv.averageWaitingTime += ptf->sleepingTime + ptf->readyTime;
        atv.averageCBT += ptf->runningTime;
    /*printf(1,"averageTurnAroundTime :%d\naverageWaitingTime :%d\naverageCBT :%d ID :%d\n"
    ,atv.averageTurnAroundTime,atv.averageWaitingTime,atv.averageCBT,pid);*/
    }
    
    printf(1,"averageTurnAroundTime :%d\naverageWaitingTime :%d\naverageCBT :%d\n%d\n"
    ,atv.averageTurnAroundTime / 10, atv.averageWaitingTime / 10, atv.averageCBT / 10, pid);

    exit();
   
  }
