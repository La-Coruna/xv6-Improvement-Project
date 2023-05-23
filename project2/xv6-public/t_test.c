// Process Manager

#include "types.h"
#include "stat.h"
#include "user.h"

// Parsed pmanager command representation

void *worker(void *arg){
  printf(1,"\n&&&&&&&&&&&\nthread is executing. with arg: %d, &arg:%d\n&&&&&&&&&&&&\n", *(int *)arg, (int)arg);

  exit();
}

int main(int argc, char *argv[])
{
  thread_t thread;
  int i=9515;

  //printf(1, "<when call> &thread: %d, sr: %d, arg: %d, &arg: %d\n", (int) &thread,  worker, i, &i);
  thread_create(&thread,worker,&i);
  //printf(1, "<after call> thread: %d, sr: %d, arg: %d\n", thread, worker, &i);
  //printf(1,"res: %d\n",res);
  procdump();

  exit();
}