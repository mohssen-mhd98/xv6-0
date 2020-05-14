
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
  int priority;
  };
  
// Childrens Attributes of a process  
struct processAttributes{
  int pid;
  int turnAroundTime;
  int CBT;
  int waitingTime;
  };

// ATT & AWT & AverageCB Time of childrens
struct averageTimeVariables{
  int averageTurnAroundTime;
  int averageCBT;
  int averageWaitingTime;
  };

int main(void) {
   struct processAttributes pa[5][5] ;
   struct averageTimeVariables atv[5];
   //struct processAttributes p1[5], p2[5], p3[5], p4[5], p5[5];
   int countOfProc [5];
   int i,j;
   //priority = 0;
   i = j = 0;
   //int i,pc1, pc2, pc3, pc4, pc5;
   
   // Reinitialize the averageTimeVariables valuse
   for(i=0;i<5;i++){
       atv[i].averageTurnAroundTime = 0;
       atv[i].averageCBT = 0;
       atv[i].averageWaitingTime = 0;
   }
   
   // Change the algorithm of scheduler to priority algorithm
   changePolicy(2); 

   // Create childrens to print their pid 1000 times and assign their priority to them
   for(int f=0; f<25;f++){
    int pid = fork();

    if (pid == 0){    
    	setPriority(1 + getpid() % 5);
        for (int i = 0; i < 200; ++i)
		    printf(1, "%d : %d \n", getpid(), i);

        exit();
    } 
    }
    int pid = 0;
    //pc1 = pc2 = pc3 = pc4 = pc5 = 0;
    struct processTimeFeatures *ptf = malloc(sizeof(struct processTimeFeatures));
    for(int f=0;f<25;f++){

    	// Get pid of the finished child        	
	    pid = waitingForChild(ptf);

        //find priority of finished child
        i = pid % 5;
	    //priority = ptf->priority;
        
        /*switch (priority)
        {
        case 1:
            p1[pc1].pid = pid;
            p1[pc1].turnAroundTime = ptf->terminationTime - ptf->creationTime;
            p1[pc1].CBT = ptf->runningTime;
            p1[pc1].waitingTime = ptf->sleepingTime + ptf->readyTime;
            pc1++;
            break;

        case 2:
            p2[pc2].pid = pid;
            p2[pc2].turnAroundTime = ptf->terminationTime - ptf->creationTime;
            p2[pc2].CBT = ptf->runningTime;
            p2[pc2].waitingTime = ptf->sleepingTime + ptf->readyTime;
            pc2++;
            break;

        case 3:
            p1[pc3].pid = pid;
            p1[pc3].turnAroundTime = ptf->terminationTime - ptf->creationTime;
            p1[pc3].CBT = ptf->runningTime;
            p1[pc3].waitingTime = ptf->sleepingTime + ptf->readyTime;
            pc3++;
            break;

        case 4:
            p4[pc4].pid = pid;
            p4[pc4].turnAroundTime = ptf->terminationTime - ptf->creationTime;
            p4[pc4].CBT = ptf->runningTime;
            p4[pc4].waitingTime = ptf->sleepingTime + ptf->readyTime;
            pc4++;
            break;

        case 5:
            p5[pc5].pid = pid;
            p5[pc5].turnAroundTime = ptf->terminationTime - ptf->creationTime;
            p5[pc5].CBT = ptf->runningTime;
            p5[pc5].waitingTime = ptf->sleepingTime + ptf->readyTime;
            pc5++;
            break;
        
        default:
            break;
        }*/

        // Set process Attributes after a children finished
	    pa[i][countOfProc[i]].pid = pid; 
        pa[i][countOfProc[i]].turnAroundTime = ptf->terminationTime - ptf->creationTime;
        pa[i][countOfProc[i]].CBT = ptf->runningTime;
        pa[i][countOfProc[i]].waitingTime = ptf->sleepingTime + ptf->readyTime;
        
        // Update field of average time variablles
        atv[i].averageTurnAroundTime += pa[i][countOfProc[i]].turnAroundTime;
        atv[i].averageCBT += pa[i][countOfProc[i]].CBT;
        atv[i].averageWaitingTime += pa[i][countOfProc[i]].waitingTime;
        
        // Update the count of the processes that are in special priority category
        countOfProc[i]+=1;
        /*printf(1, "pid %d create %d term %d ready %d sleep %d cbt %d \n"
        , ptv[f].pid, tv->creationTime, tv->terminationTime,
         tv->readyTime, tv->sleepingTime, tv->runningTime);*/
        /*printf(1, "creationTime %d\n", tv->creationTime);
        printf(1, "terminationTime %d\n", tv->terminationTime);
        printf(1, "readyTime %d\n", tv->readyTime);
        printf(1, "sleepingTime %d\n", tv->sleepingTime);
        printf(1, "runningTime %d\n \n", tv->runningTime);*/
        }
        
    // Print Process Attributes of each children
    for(i=0; i<5;i++){
    	for(j=0;j<5;j++){
    		printf(1,"Process PID : %d , Process priority %d, Turn-around time of process %d, CBT %d, and Waiting time of process %d .\n"
    		, pa[i][j].pid, (i+1),pa[i][j].turnAroundTime, pa[i][j].CBT, pa[i][j].waitingTime);
    	}
    }
    
    // Print the average time variables of processes for each Priority category 
    for(int i=0;i<5;i++){
    	printf(1, "Priority category of %d , Has Average Turn-around Time %d, And Average CBT %d, And Average Waiting time %d .\n"
    	, (i+1), (atv[i].averageTurnAroundTime / 5), (atv[i].averageCBT / 5), (atv[i].averageWaitingTime / 5));
    	}
    exit();
}
