
#include "types.h"
#include "stat.h"
#include "user.h"


   
int
main(void)
{
int pid, pid1, pid2,pid3,pid4;
/*int shmid; 
key_t key = IPC_PRIVATE; 
int *shm_array;
size_t SHM_SIZE = sizeof(int)*length; */
pid = fork();
pid1 = fork();
pid2 = fork();
pid3 = fork();
pid4 = fork();

if(pid == 0)
   {
      while(1);
      exit();
	}
if(pid1==0){
    while(1);
    exit();
}  
if(pid2==0){
    while(1);
    exit();
}
if(pid3==0){
    while(1);
    exit();
}
if(pid4==0){
    while(1);
    exit();
} 	
if(pid>0&&pid1>0&&pid2>0&&pid3>0&&pid4>0){
    int id = getChildren();  
    printf(1,"%d****parent id :%d",id,getpid());

    exit();
}
 kill(getpid());       	
exit();
}
