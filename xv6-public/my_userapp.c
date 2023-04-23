#include "types.h"
#include "stat.h"
#include "user.h"

void workload() {
  int i;
  for (i = 0; i < 2000000; i++) {
    __asm__("nop");
  }
}

void test_rr();
void test_priority();

int main(int argc, char *argv[])
{
  if(argc <= 1){
    printf(1,"Error: Insufficient arguments.\nPlease input 1~2\n");
    exit();
  }

  if(strcmp(argv[1], "1") == 0){
	 	test_rr();
	}	
  if(strcmp(argv[1], "2") == 0){
	 	test_priority();
	}	

  return 0;
}

// priority test
// ! 예상 결과 값: pid가 높은 자식이 queue에는 늦게 들어갔지만 priority가 높게 설정되기 때문에 
// !              L2까지 실행되면 더 빨리 종료된다.
void test_priority() {
	int pid;
	int num_children = 4;
  
  // # schedulerLock을 하여 fork를 하여도 자식들이 실행되지 못하게 막음.
  schedulerLock(2019019043);

  // # fork를 통해 num_children만큼 process를 생성한다.
	for (int i = 0; i < num_children; i++) {
    pid = fork();
    
    // # fork에 실패한 경우
    if (pid < 0) {
      printf(1, "fork failed\n");
      exit();
    }

    // # 부모인 경우.
    else if(pid > 0){
      
      // # 자식 순서대로 priority 3~0 으로 설정
      setPriority(pid,3-i);
    }

    // # child일 경우
    else if (pid == 0) {
      // # priority에 따른 실행 순서를 보기 위해 L2가 될 때까지 실행.
      while(getLevel()!=2)
        workload();
      
      // # L2가 되고 나서의 실행
      for(int i = 0; i<20; i++){
        workload();
      }
      printf(1,"process[%d] exit\n", getpid());
      exit();
    }
    
	}
  

  // # 아래는 parent의 코드.

  // # MLFQ scheduler로 돌아와 자식들의 실행 시작.
  schedulerUnlock(2019019043);

  // # 자식들을 기다림
  for (int i = 0; i < num_children; i++) {
    wait();
  }

  printf(1, "All children completed\n");
  exit();

}


// RR test
// ! RR로 동작하는 것을 보기 위해
// ! proc.c의 execProc 시작 시 디버깅을 위한 출력문을 삽입함.
/* 
  // ! for debug
  if(p->level!=2)
    cprintf("process[%d](%s) exec. lv-ticks: %d-%d\n",p->pid, p->name, p->level, p->ticks);
  else
    cprintf("process[%d](%s) exec. lv-prio-ticks: %d-%d-%d\n",p->pid, p->name, p->level, p->priority,p->ticks);
*/
void test_rr() {
	int pid;
	int num_children = 3;
  
  // # schedulerLock을 하여 fork를 하여도 자식들이 실행되지 못하게 막음.
  schedulerLock(2019019043);

  // # fork를 통해 num_children만큼 process를 생성한다.
	for (int i = 0; i < num_children; i++) {
    pid = fork();
    
    // # fork에 실패한 경우
    if (pid < 0) {
      printf(1, "fork failed\n");
      exit();
    }

    // # 부모인 경우.
    else if(pid > 0){
      printf(1, "Create process[%d]\n", pid);
    }

    // # child일 경우
    else if (pid == 0) {
      while(getLevel()!=2)
        workload();

      exit();
    }
    
	}

  // # 아래는 parent의 코드.

  // # MLFQ scheduler로 돌아와 자식들의 실행 시작.
  schedulerUnlock(2019019043);

  // # 자식들을 기다림
  for (int i = 0; i < num_children; i++) {
    wait();
  }

  printf(1, "All children completed\n");
  exit();

}
