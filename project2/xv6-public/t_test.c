// Process Manager

#include "types.h"
#include "stat.h"
#include "user.h"

// Parsed pmanager command representation
void workload() {
  int i;
  for (i = 0; i < 2000000; i++) {
    __asm__("nop");
  }
}

void *worker(void *arg){
  int ret=2019;
  for(int a = 0; a < 100; a++)
    workload();
  printf(1,"\n&&&&&&&&&&&\nthread is executing. with arg: %d, &arg:%d\n&&&&&&&&&&&&\n", *(int *)arg, (int)arg);
  *(int *)arg -= 3695;

  //printf(1,"왜 이러니~~.\n");
  for(int a = 0; a < 1000; a++)
    workload();
  thread_exit(&ret);
  printf(1,"이게 출력되면 안됨.\n");
  while(1)
    workload();
  exit();
}

int main(int argc, char *argv[])
{
  //printf(1,"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@~~\n");
  thread_t thread;
  int i=81472;

  //printf(1, "<when call> &thread: %d, sr: %d, arg: %d, &arg: %d\n", (int) &thread,  worker, i, &i);
  thread_create(&thread,worker,&i);
  printf(1, "[thread create 직후]\n");
  procdump(); //@
  for(int a = 0; a < 200; a++)
    workload();
  printf(1,"thread에 의해 바뀐 i값: %d\n\n", i);
  printf(1, "[thread 종료 직후]\n");
  procdump(); //@
  printf(1, "\n");
  //printf(1, "<after call> thread: %d, sr: %d, arg: %d\n", thread, worker, &i);
  //printf(1,"res: %d\n",res);
  //@@
  int* retval;
  printf(1, "[thread join 시작]\n");
  thread_join(thread, (void *) &retval);
  printf(1,"thread에 의해 retval: %d\n\n", *retval);
  printf(1, "[thread join 완료 직후]\n");
  //@@
  procdump(); //@
  // for(int a = 0; a < 9999999; a++)
  //   workload();

  exit();
}