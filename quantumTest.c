#include "types.h"
#include "stat.h"
#include "user.h"

struct timeVariables{
  int creationTime;
  int terminationTime;
  int sleepingTime; 
  int readyTime; 
  int runningTime;
  };
  

// Childrens ATT and AWT Time
struct averageTimeVariables{
  float averageTurnAroundTime;
  float averageCBT;
  float averageWaitingTime;
  };
  int main(void)
  {
   struct averageTimeVariables atv;
   struct timeVariables *tv = malloc(sizeof(struct timeVariables));
   int i,pid,j;

    atv.averageTurnAroundTime = 0;
    atv.averageCBT = 0;
    atv.averageWaitingTime = 0;

    for(i=0;i<15;i++){
        pid = fork();
        if(pid == 0){
            for(j=0;j<200;j++);
            exit();
        }
    }

    for(i=0;i<15;i++){
        
        int pid = waitForChild(tv);
        atv.averageTurnAroundTime += tv->terminationTime - tv->creationTime;
        atv.averageWaitingTime += tv->sleepingTime + tv->readyTime;
        atv.averageCBT += tv->runningTime;
    }
    
    printf(1,"averageTurnAroundTime :%.2f\naverageWaitingTime :%.2f\naverageCBT :%.2f"
    ,atv.averageTurnAroundTime,atv.averageWaitingTime,atv.averageCBT);
   
  }
