// #include "types.h"
// #include "stat.h"
// #include "user.h"

// int
// main(int argc, char *argv[])
// {
// 	if(argc <=1){
// 		exit();
// 	}

// 	if(strcmp(argv[1], "\"user\"") !=0){
// 		exit();
// 	}	

// 	char *buf= "Hello xv6!";
// 	int ret_val;
// 	ret_val = myfunction(buf);

// 	printf(1, "Return value : 0x%x\n", ret_val);
// 	exit();
// };

#include "types.h"
#include "user.h"
#include "stat.h"

void workload() {
  int i;
  for (i = 0; i < 2000000; i++) {
    asm("nop");
  }
}

int main(int argc, char *argv[]) {
  int pid;
  int num_children = 6;

  for (int i = 0; i < num_children; i++) {
    pid = fork();
    if (pid < 0) {
      printf(1, "fork failed\n");
      exit();
    } else if (pid == 0) {
      // printf(1, "Child %d created with PID %d\n", i, getpid());
      printf(1, "%d", i);
      //setPriority(getpid(), 3 - (i % 4)); // Vary priorities from 3 to 0
      workload();
      printf(1, "%d", i);
      workload();
      printf(1, "%d", i);
      workload();
      printf(1, "%d", i);
      workload();
      printf(1, "%d", i);
      // printf(1, "Child %d with PID %d completed\n", i, getpid());
      exit();
    }
  }

  for (int i = 0; i < num_children; i++) {
    wait();
  }

  printf(1, "All children completed\n");

  exit();
}
