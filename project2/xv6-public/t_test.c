// Process Manager

#include "types.h"
#include "stat.h"
#include "user.h"

// Parsed pmanager command representation
void workload(int num) {
  for(int j = 0; j < num; j++){
    for (int i = 0; i < 2000000; i++) {
      __asm__("nop");
    }
  }
}

void *worker(void *arg){
  int ret=2019;
  workload(100);
  printf(1,"\n&&&&&&&&&&&\nthread is executing. with arg: %d, &arg:%d\n&&&&&&&&&&&&\n", *(int *)arg, (int)arg);
  *(int *)arg -= 3695;

  workload(1000);
  thread_exit(&ret);
  printf(1,"이게 출력되면 안됨.\n");
  while(1)
    ;
  exit();
}

void *worker_fork(void *arg){
  int pid;
  int ret = 0;
  workload(100);
  printf(1,"\n&&&&&&&&&&&\nthread is executing. with arg: %d, &arg:%d\n&&&&&&&&&&&&\n", *(int *)arg, (int)arg);
  for(int i = 0; i < *(int *)arg ; i++){
    pid = fork();
    if(pid == 0){
      printf(1,"process[pid: %d]가 생성되었습니다!\n",getpid());
      //@
      workload(200);
      exit();
    }
    ret += pid;
    workload(200);
  }

  workload(1000);
  thread_exit(&ret);
  printf(1,"이게 출력되면 안됨.\n");
  while(1)
    ;
  exit();
}

int t_test_1()
{
  //printf(1,"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@~~\n");
  thread_t thread;
  int i=81472;

  //printf(1, "<when call> &thread: %d, sr: %d, arg: %d, &arg: %d\n", (int) &thread,  worker, i, &i);
  thread_create(&thread,worker,&i);
  printf(1, "[thread create 직후]\n");
  procdump(); //@

  workload(200);
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

int t_test_2()
{
  //printf(1,"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@~~\n");
  thread_t thread;
  int i=81472;

  //printf(1, "<when call> &thread: %d, sr: %d, arg: %d, &arg: %d\n", (int) &thread,  worker, i, &i);
  thread_create(&thread,worker,&i);
  printf(1, "[thread create 직후]\n");
  procdump(); //@

  workload(200);
  printf(1,"thread에 의해 바뀐 i값: %d\n\n", i);
  printf(1, "[thread 종료 직후]\n");
  procdump(); //@
  printf(1, "\n");
  //printf(1, "<after call> thread: %d, sr: %d, arg: %d\n", thread, worker, &i);
  //printf(1,"res: %d\n",res);
  //@@
  // int* retval;
  // printf(1, "[thread join 시작]\n");
  // thread_join(thread, (void *) &retval);
  // printf(1,"thread에 의해 retval: %d\n\n", *retval);
  // printf(1, "[thread join 완료 직후]\n");
  //@@
  procdump(); //@
  // for(int a = 0; a < 9999999; a++)
  //   workload();

  exit();
}

// fork test
int t_test_3()
{
  //printf(1,"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@~~\n");
  thread_t thread;
  int i=3;

  //printf(1, "<when call> &thread: %d, sr: %d, arg: %d, &arg: %d\n", (int) &thread,  worker, i, &i);
  thread_create(&thread,worker_fork,&i);
  printf(1, "[thread create 직후]\n");
  procdump(); //@

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

  exit();
}


int main(int argc, char *argv[])
{
  if(argc <= 1){
    printf(1,"Error: Insufficient arguments.\nPlease input 1~3\n");
    exit();
  }
  int test_num = atoi(argv[1]);
  switch(test_num){
    default:
      break;

    case 1:
      t_test_1();
      break;

    case 2:
      t_test_2();
      break;

    case 3:
      t_test_3();
      break;
  }

  printf(1,"test 종료\n");

  // if(strcmp(argv[1], "1") == 0){
	//  	t_test_1();
	// }	
  // if(strcmp(argv[1], "2") == 0){
	//  	t_test_2();
	// }	

  return 0;
}