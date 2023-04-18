// #include "types.h"
// #include "stat.h"
// #include "user.h"

// #define NUM_LOOP 100000
// #define NUM_YIELD 20000
// #define NUM_SLEEP 1000

// #define NUM_THREAD 4
// #define MAX_LEVEL 5

// int parent;

// int fork_children()
// {
//   int i, p;
//   for (i = 0; i < NUM_THREAD; i++)
//     if ((p = fork()) == 0)
//     {
//       sleep(10);
//       return getpid();
//     }
//   return parent;
// }


// int fork_children2()
// {
//   int i, p;
//   for (i = 0; i < NUM_THREAD; i++)
//   {
//     if ((p = fork()) == 0)
//     {
//       sleep(300);
//       return getpid();
//     }
//     else
//     {
//       setPriority(p, i); // setPriority는 void
//       int r = 1;
//       if (r < 0)
//       {
//         printf(1, "setPriority returned %d\n", r);
//         exit();
//       }
//     }
//   }
//   return parent;
// }

// int max_level;

// int fork_children3()
// {
//   int i, p;
//   for (i = 0; i < NUM_THREAD; i++)
//   {
//     if ((p = fork()) == 0)
//     {
//       sleep(10);
//       max_level = i;
//       return getpid();
//     }
//   }
//   return parent;
// }
// void exit_children()
// {
//   if (getpid() != parent)
//     exit();
//   while (wait() != -1);
// }

// int main(int argc, char *argv[])
// {
//   int i, pid;
//   int count[MAX_LEVEL] = {0};
// //  int child;

//   parent = getpid();

//   printf(1, "MLFQ test start\n");

//   printf(1, "[Test 1] default\n");
//   pid = fork_children();

//   if (pid != parent)
//   {
//     for (i = 0; i < NUM_LOOP; i++)
//     {
//       int x = getLevel();
//       if (x < 0 || x > 4)
//       {
//         printf(1, "Wrong level: %d\n", x);
//         exit();
//       }
//       count[x]++;
//     }
//     printf(1, "Process %d\n", pid);
//     for (i = 0; i < MAX_LEVEL; i++)
//       printf(1, "L%d: %d\n", i, count[i]);
//   }
//   exit_children();
//   printf(1, "[Test 1] finished\n");
//   printf(1, "done\n");
//   exit();
// }


// #include "types.h"
// #include "stat.h"
// #include "user.h"

// int
// main(int argc, char *argv[])
// {
// 	// if(strcmp(argv[1], "\"user\"") !=0){
// 	// 	exit();
// 	// }	
//   //int password = 2019019043;

//   __asm__("int $129");
//   __asm__("int $130");


// 	char *buf= "Hello xv6!";
// 	int ret_val;
// 	ret_val = myfunction(buf);

//   schedulerLock(300);
//   schedulerLock(500);
//   setPriority(1234,5678);

// 	printf(1, "Return value : 0x%x\n", ret_val);
// 	exit();
// };

#include "types.h"
#include "user.h"
#include "stat.h"

typedef volatile int lock_t;

void lock_init(lock_t *lock) {
  *lock = 0;
}

void lock_acquire(lock_t *lock) {
  while (1) {
    if (*lock == 0) {
      *lock = 1;
      break;
    }
  }
}

void lock_release(lock_t *lock) {
  *lock = 0;
}

void workload() {
  int i;
  for (i = 0; i < 2000000; i++) {
    asm("nop");
  }
}

void workload2() {
  for (int i = 0; i < 300; i++) {
    workload();
  }
}

lock_t print_lock;

void print1(){
  lock_acquire(&print_lock);
  //printf(1, "PID %d, level %d\n", getpid(), getLevel());
  lock_release(&print_lock);
}

int main(int argc, char *argv[]) {
	int pid;
	int num_children = 6;
	lock_init(&print_lock);

	setPriority(3,0);

	for (int i = 0; i < num_children; i++) {
	pid = fork();
	if (pid < 0) {
		printf(1, "fork failed\n");
		exit();
	} else if (pid == 0) {
		
    //setPriority(getpid(),0);
		
    if(getpid()>=9){
			//schedulerLock(2019019043);
      __asm__("int $129");
		}


		for(int l=getLevel();l==0;l=getLevel())
			workload();


		for(int l=getLevel();l<2;l=getLevel())
			workload();

    if(getpid()>=9){
			//schedulerUnlock(2019019043);
      __asm__("int $130");
			//printf(1,"asdfasdfasdfasdfasdffsdafasdfasdfasdfsad");
		}
    if(getpid()>=9)
		  workload2();

		lock_acquire(&print_lock);
		printf(1, "[Completed] PID %d, level %d\n", getpid(), getLevel());
		lock_release(&print_lock);

		exit();
	}
	}

	setPriority(6,2);
	setPriority(7,2);
	setPriority(8,1);
	setPriority(9,1);
	setPriority(10,0);
	setPriority(11,0);

	for (int i = 0; i < num_children; i++) {
	wait();
	}

	printf(1, "All children completed\n");
	exit();
}