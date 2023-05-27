// Process Manager

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

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

void *worker2(void *arg){
  int ret=2019;
  workload(100);
  printf(1,"\n&&&&&&&&&&&\nthread is executing. with arg: %d, &arg:%d\n&&&&&&&&&&&&\n", *(int *)arg, (int)arg);
  *(int *)arg += 1;
  thread_exit(&ret);
  printf(1,"이게 출력되면 안됨.\n");
  exit();
}

void *worker3(void *arg){
  int ret=2019;
  workload(100);
  printf(1,"\n&&&&&&&&&&&\nthread is executing. with arg: %d, &arg:%d\n&&&&&&&&&&&&\n", *(int *)arg, (int)arg);
  printf(1,"in arg: %d",*(int *)arg);
  *(int *)arg += 3;
  printf(1," -> %d\n",*(int *)arg);

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

void *worker_open(void *arg)
{
  int fd;
  char buffer[256];
  int a = 1;

  // Open the file
  fd = open("file.txt", O_RDONLY);
  if (fd < 0) {
    printf(1, "Error opening the file\n");
    thread_exit(&a);
  }

  // Read and print the file contents
  int bytesRead = read(fd, buffer, sizeof(buffer));
  if (bytesRead < 0) {
    printf(1, "Error reading the file\n");
    thread_exit(&a);
  }
  printf(1, "File contents: %s\n", buffer);

  // Close the file
  close(fd);

  thread_exit(&a);
  exit();
}

int t_test_1()
{
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
  thread_t thread;
  thread_t thread2;
  thread_t thread3;
  int i=81472;

  //printf(1, "<when call> &thread: %d, sr: %d, arg: %d, &arg: %d\n", (int) &thread,  worker, i, &i);
  thread_create(&thread,worker,&i);
  thread_create(&thread2,worker,&i);
  thread_create(&thread3,worker,&i);
  printf(1, "[thread create 직후]\n");
  procdump(); //@

  //workload(200);
  printf(1,"thread에 의해 바뀐 i값: %d\n\n", i);
  procdump(); //@
  printf(1, "[exit 호출]\n");
  exit();
}


int t_test_3()
{
  thread_t thread[5];
  int arg = 0;

  printf(1, "[thread 여러개 생성 test]\n");
  for(int i = 0; i < 5 ; i++){
    thread_create(&thread[i],worker3,&arg);
    proclist();
  }
  printf(1, "[thread create 직후]\n");
  procdump(); //@

  workload(200);
  printf(1,"thread에 의해 바뀐 i값: %d\n\n", arg);
  printf(1, "[thread 종료 직후]\n");
  procdump(); //@
  printf(1, "\n");
  //printf(1, "<after call> thread: %d, sr: %d, arg: %d\n", thread, worker, &i);
  //printf(1,"res: %d\n",res);
  //@@
  int* retval;
  printf(1, "[thread join 시작]\n");
  for(int i = 0; i < 5 ; i++)
    thread_join(thread[i], (void *) &retval);
  printf(1,"thread에 의해 retval: %d\n\n", *retval);
  printf(1, "[thread join 완료 직후]\n");
  //@@
  procdump(); //@
  printf(1,"tread test 3 종료\n");
  exit();
}

// fork test
int t_test_4()
{
  //printf(1,"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@~~\n");
  thread_t thread;
  int i=3;

  thread_create(&thread,worker_fork,&i);
  printf(1, "[thread create 직후]\n");

/*   int* retval;
  thread_join(thread, (void *) &retval);
 */

  exit();
}

// open test
int t_test_5()
{
  int i;
  thread_t tid[5];

  for (i = 0; i < 5; i++) {
    thread_create(&(tid[i]), worker_open, 0);
    sleep(50);
    if (tid[i] == -1) {
      printf(1, "Error creating thread\n");
      exit();
    }
  }

  for (i = 0; i < 5; i++) {
    thread_join(tid[i], 0);
  }

  exit();
}


// open test2
int t_test_6()
{
  int i;
  thread_t tid[5];
  int fd;
  char buffer[256];
  int a = 1;

  // # Open the file
  fd = open("file.txt", O_RDONLY);
  if (fd < 0) {
    printf(1, "Error opening the file\n");
    exit();
  }

  // # thread 생성
  for (i = 0; i < 5; i++) {
    thread_create(&(tid[i]), worker2, &a);
    sleep(50);
    if (tid[i] == -1) {
      printf(1, "Error creating thread\n");
      exit();
    }
  }

  // # thread join
  for (i = 0; i < 5; i++) {
    thread_join(tid[i], 0);
  }

  // # Read and print the file contents
  int bytesRead = read(fd, buffer, sizeof(buffer));
  if (bytesRead < 0) {
    printf(1, "Error reading the file\n");
    exit();
  }
  printf(1, "File contents: %s\n", buffer);

  exit();
}


int main(int argc, char *argv[])
{
  if(argc <= 1){
    printf(1,"Error: Insufficient arguments.\nPlease input 1~6\n");
    exit();
  }
  int test_num = atoi(argv[1]);
  switch(test_num){
    default:
      printf(1,"1~6까지의 옵션이 있다.\n");
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

    case 4:
      t_test_4();
      break;

    case 5:
      t_test_5();
      break;

    case 6:
      t_test_6();
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