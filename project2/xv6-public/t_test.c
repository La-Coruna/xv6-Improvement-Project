#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

void *make77777(void *arg){
  int ret=2019019043;
  *(int *)arg -= 3695;
  thread_exit(&ret);
  printf(1,"이게 출력되면 안됨.\n");
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

  thread_create(&thread,make77777,&i);

  int* retval;

  thread_join(thread, (void *) &retval);

  if(*retval == 2019019043 && i == 77777)
    printf(1,"argument and retval passing test passed!\n");
  exit();
}

// open test
int t_test_2()
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

  printf(1,"WAVE를 5번 타면 test 통과한 것입니다.\n");
  exit();
}

int main(int argc, char *argv[])
{
  if(argc <= 1){
    printf(1,"Error: Insufficient arguments.\nPlease input 1~2\n");
    exit();
  }
  int test_num = atoi(argv[1]);
  switch(test_num){
    default:
      printf(1,"1~2까지의 옵션이 있습니다.\n");
      break;

    case 1:
      t_test_1();
      break;

    case 2:
      t_test_2();
      break;
  }
  exit();
}