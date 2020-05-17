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
  int Queue_level;
  };
  
// ATT & AWT $ AverageCB Time of childrens
struct averageTimeVariables{
  int averageTurnAroundTime;
  int averageCBT;
  int averageWaitingTime;
  };

  int main(void)
  {
   struct averageTimeVariables atv[3][20];
   struct processTimeFeatures *ptf = malloc(sizeof(struct processTimeFeatures));
   int i,pid,j;
   int numOfPriority, numOfQuantum, numOfDefault;
   numOfPriority = numOfQuantum = numOfDefault = 0;

    for(i=0;i<3;i++){
        for(j=0;j<20;j++){
            atv[i][j].averageTurnAroundTime = 0;
            atv[i][j].averageCBT = 0;
            atv[i][j].averageWaitingTime = 0;
            }
    }
   // Change the algorithm of scheduler to multiLevelq algorithm
   changePolicy(3);

    for(i=0;i<10;i++){
        pid = fork();
        if(pid == 0){
            for(j=0;j<100;j++)
            printf(1, "%d : %d \n", getpid(), i);
            exit();
        }
    }
    for(i=0;i<10;i++){
        pid = fork();
        if(pid == 0){
            for(j=0;j<200;j++)
            printf(1, "%d : %d \n", getpid(), i);
            exit();
        }
    }
    for(i=0;i<5;i++){
        pid = fork();
        if(pid == 0){
            for(j=0;j<500;j++)
            printf(1, "%d : %d \n", getpid(), i);
            exit();
        }
    }
    for(i=0;i<25;i++){
        
        pid = waitingForChild(ptf);
        if(ptf->Queue_level == 1){
        atv[0][numOfPriority].averageTurnAroundTime += ptf->terminationTime - ptf->creationTime;
        atv[0][numOfPriority].averageWaitingTime += ptf->sleepingTime + ptf->readyTime;
        atv[0][numOfPriority].averageCBT += ptf->runningTime;
        numOfPriority++;
        }

        
        if(ptf->Queue_level == 2){
        atv[1][numOfDefault].averageTurnAroundTime += ptf->terminationTime - ptf->creationTime;
        atv[1][numOfDefault].averageWaitingTime += ptf->sleepingTime + ptf->readyTime;
        atv[1][numOfDefault].averageCBT += ptf->runningTime;
        numOfDefault++;
        }

        
        if(ptf->Queue_level == 3){
        atv[2][numOfQuantum].averageTurnAroundTime += ptf->terminationTime - ptf->creationTime;
        atv[2][numOfQuantum].averageWaitingTime += ptf->sleepingTime + ptf->readyTime;
        atv[2][numOfQuantum].averageCBT += ptf->runningTime;
        numOfQuantum++;
        }
    /*printf(1,"averageTurnAroundTime :%d\naverageWaitingTime :%d\naverageCBT :%d ID :%d\n"
    ,atv.averageTurnAroundTime,atv.averageWaitingTime,atv.averageCBT,pid);*/
    }
    printf(1,"Number of process in Priority Levelq : %d\n", numOfPriority);
    for(i=0;i<numOfPriority;i++)
        printf(1,"Turn Around Time :%d\n Waiting Time :%d\n CBT :%d\n"
            ,atv[0][i].averageTurnAroundTime, atv[0][i].averageWaitingTime, atv[0][i].averageCBT);

    printf(1,"Number of process in Default Levelq : %d\n", numOfDefault);
    for(i=0;i<numOfDefault;i++)
        printf(1,"Turn Around Time :%d\n Waiting Time :%d\n CBT :%d\n"
            ,atv[1][i].averageTurnAroundTime , atv[1][i].averageWaitingTime, atv[1][i].averageCBT);

    printf(1,"Number of process in Quantum Levelq : %d\n", numOfQuantum);
    for(i=0;i<numOfQuantum;i++)
        printf(1,"Turn Around Time :%d\n Waiting Time :%d\n CBT :%d\n"
            ,atv[2][i].averageTurnAroundTime, atv[2][i].averageWaitingTime, atv[2][i].averageCBT);
    exit();
   
  }
