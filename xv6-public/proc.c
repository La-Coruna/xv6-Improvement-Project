#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  int globalTicks;
  struct proc *preferentialProc;
  struct proc *firstLv0Proc;
  struct proc *headLv[3];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// !ptablelock을 얻은 후에만 호출되어야 함!
// void
// insertHead(struct proc *newHead, int lvOfHead)
// {
//   // # 1. head가 기존에 없는 경우
//   if(ptable.headLv[lvOfHead] == 0){
//     newHead->prev = newHead;
//     newHead->next = newHead;
//     ptable.headLv[lvOfHead] = newHead;
//     return;
//   }

//   // # 2. head가 있는 경우, new process가 새로운 head가 된다.
//   struct proc* oldHead = ptable.headLv[lvOfHead];
//   struct proc* tail = oldHead->prev;
//   tail->next = newHead;
//   newHead->prev = oldHead->prev;
//   newHead->next = oldHead;
//   oldHead->prev = newHead;
//   return;
// }

// !ptablelock을 얻은 후에만 호출되어야 함!
void
insertTail(struct proc *newTail, int lvOfHead)
{
  if(newTail->level != lvOfHead){
    panic("진짜 말도 안되는 경우.");
  }
  //cprintf("@@@@insert pid:%d in Lv%d\n",newTail->pid, lvOfHead); // ! for debug
  // # 1. head가 기존에 없는 경우, newTail이 head가 된다.
  if(ptable.headLv[lvOfHead] == 0){
    //cprintf("나 들어가려는데 헤드가 없다! 내가 헤드해야지~\n");
    newTail->prev = newTail;
    newTail->next = newTail;
    ptable.headLv[lvOfHead] = newTail;

    //checkQueue(lvOfHead); // ! for debug
    return;
  }

  // # 2. head가 있는 경우, new process가 새로운 tail가 된다.
  struct proc* head = ptable.headLv[lvOfHead];
  struct proc* oldTail = head->prev;
  oldTail->next = newTail;
  newTail->prev = oldTail;
  newTail->next = head;
  head->prev = newTail;
  return;
}

// void
// insertTail_allOfQueue(struct proc *srcQ, struct proc *destQ)
// {
//   // ! for debug
//   if(srcQ == destQ)
//     panic("말도 안됨.");
//   //cprintf("@@@@insert pid:%d in Lv%d\n",newTail->pid, lvOfHead); // ! for debug
//   // # 1. head가 기존에 없는 경우, newTail이 head가 된다.
//   if(ptable.headLv[lvOfHead] == 0){
//     //cprintf("나 들어가려는데 헤드가 없다! 내가 헤드해야지~\n");
//     newTail->prev = newTail;
//     newTail->next = newTail;
//     ptable.headLv[lvOfHead] = newTail;

//     //checkQueue(lvOfHead); // ! for debug
//     return;
//   }

//   // # 2. head가 있는 경우, new process가 새로운 tail가 된다.
//   struct proc* head = ptable.headLv[lvOfHead];
//   struct proc* oldTail = head->prev;
//   oldTail->next = newTail;
//   newTail->prev = oldTail;
//   newTail->next = head;
//   head->prev = newTail;
//   return;
// }

// 무조건 큐 안에 있는 노드에만 사용해야함.
// !ptablelock을 얻은 후에만 호출되어야 함!
void
detachNode(struct proc* p)
{
  //cprintf("나(%d) 이제 큐에서 없어져!\n",p->pid); // ! for debug
  if(p->prev == 0 || p->next == 0){
    //cprintf("나(%d) 이미 큐에서 연결이 끊겼어 ㅜㅜ*\n",p->pid);
    printPrevNext(p);
    return;
  }
  // # 1. p가 큐에 혼자일 경우
  if(p->next == p){
    // //cprintf("나는 이 큐에 혼자 있었네?\n");
    if(p != ptable.headLv[p->level]) // ! for debug
      panic("process link error");
    ptable.headLv[p->level] = 0;
    //cprintf("나 혼자 큐에 있어서 헤드를 바꿨어! HeadLv%d : %d\n",p->level,ptable.headLv[p->level]); // ! for debug
  } 
  // # 2. 혼자가 아닐 경우
  else {
    //cprintf("나 사라지기전에 이 큐 체크좀 해볼게!");
    checkQueue(p->level);
    p->prev->next = p->next;
    p->next->prev = p->prev;
    // # 2-1. head일 경우
    if(p == ptable.headLv[p->level])
      ptable.headLv[p->level] = p->next;
  }
  p->prev = 0;
  p->next = 0;
  return;
}

int
shiftHead(int level)
{
  if(ptable.headLv[level]==0){
    cprintf("head%d가 없어서 쉬프트가 일어나지 않앗습니다.\n",level);
    return 0;
  }
  ptable.headLv[level] = ptable.headLv[level]->next;
  return 1;
}

void
mergeQueueToLv0(){
  // # lv0의 tail에 lv1 삽입 후 lv2를 삽입하는 방식으로 병합. (최대한 기존의 순서를 보장하기 위해.)
  for(int i = 1; i < 3; i++){
    // # 삽입하려는 큐가 비어있을 때, 다음 큐 삽입
    if(ptable.headLv[i] == 0)
      continue;

    // # 삽입하려는 큐의 proc들의 lv과 tick을 초기화해줌.
    struct proc *p;
    for(p=ptable.headLv[i];;p=p->next){
      p->level = 0;
      p->priority = 3;
      p->ticks = 4;
      if(p->next == ptable.headLv[i])
        break;
    }

    // # when Lv0 quqeue is empty
    if(ptable.headLv[0] == 0){
      ptable.headLv[0] = ptable.headLv[i];
      continue;
    }
    // # when Lv0 queue is non-empty
    else{
      (ptable.headLv[0]->prev)->next = (ptable.headLv[i]);
      (ptable.headLv[i]->prev)->next = (ptable.headLv[0]);
      struct proc *tmp = (ptable.headLv[0]->prev);
      (ptable.headLv[0])->prev = (ptable.headLv[i]->prev);
      (ptable.headLv[i])->prev = tmp;
    }
    ptable.headLv[i]=0;
  }  
}

void printPrevNext(struct proc* p){
  cprintf("(%d<-)%d(->%d)\n",p->prev != 0 ? p->prev->pid : 0 , p->pid ,p->next != 0 ? p->next->pid : 0 );
}

int
checkQueue1(int level)
{
  cprintf("check queue Lv%d \n",level);
  struct proc *head = ptable.headLv[level];
  struct proc *p = head;
  if (head == 0){
    cprintf("Lv%d is empty queue.\n",level);
    return 0;
  }
  // # next test
  cprintf("next test : ");
  while(1){
    // ! 다른 레벨이 큐에 들어온 에러케이스
    if(p->level != level){
      cprintf("pid:(%d<-)%d(->%d) (lv:%d) is in Lv%d queue.*\n",p->prev->pid,p->pid,p->next->pid,p->level,level);
      printAllNode();
      procdump();
      while(1)
        ;
      //return -1;
    }
    cprintf("%d",p->pid);

    // ! next가 0인 에러케이스
    if(p->next == 0){
      cprintf("pid:%d's next is 0\n",p->pid);
      return -1;
    }


    cprintf("(->%d) ", p->next->pid);
    if(p->next == p && p != head){
      cprintf("pid:(%d<-)%d(->%d)'s next is self. infinite loop error.*\n", p->prev->pid,p->pid,p->next->pid);
      printAllNode();
      while(1)
        ;
     // return -1;
    }
    if(p->next == head)
      break;
    else
      p = p->next;
  }



  p = head;
  cprintf("\nprev test : ");
  while(1){
    cprintf("%d",p->pid);
    if(p->prev == 0){
      cprintf("pid:%d's prev is 0\n",p->pid);
      printAllNode();
      while(1)
        ;
      //return -1;
    }
    cprintf("(->%d) ", p->prev->pid);
    if(p->prev == p && p != head){
      cprintf("pid:(%d<-)%d(->%d)'s prev is self. infinite loop error.*\n", p->prev->pid,p->pid,p->next->pid);
      printAllNode();
      while(1)
        ;
      //return -1;
    }
    if(p->prev == head)
      break;
    else
      p = p->prev;
  }

  cprintf("\ncheck is successed\n");
  return 1;
}

int
checkQueue(int level)
{
  //cprintf("check queue Lv%d \n",level);
  struct proc *head = ptable.headLv[level];
  struct proc *p = head;
  if (head == 0){
    //cprintf("Lv%d is empty queue.\n",level);
    return 0;
  }
  // # next test
  //cprintf("next test : ");
  while(1){
    // ! 다른 레벨이 큐에 들어온 에러케이스
    if(p->level != level){
      cprintf("pid:(%d<-)%d(->%d) (lv:%d) is in Lv%d queue.*\n",p->prev->pid,p->pid,p->next->pid,p->level,level);
      printAllNode();
      procdump();
      while(1)
        ;
      //return -1;
    }
    //cprintf("%d",p->pid);

    // ! next가 0인 에러케이스
    if(p->next == 0){
      cprintf("pid:%d's next is 0\n",p->pid);
      return -1;
    }


    //cprintf("(->%d) ", p->next->pid);
    if(p->next == p && p != head){
      cprintf("pid:(%d<-)%d(->%d)'s next is self. infinite loop error.*\n", p->prev->pid,p->pid,p->next->pid);
      printAllNode();
      while(1)
        ;
     // return -1;
    }
    if(p->next == head)
      break;
    else
      p = p->next;
  }



  p = head;
  //cprintf("\nprev test : ");
  while(1){
    //cprintf("%d",p->pid);
    if(p->prev == 0){
      cprintf("pid:%d's prev is 0\n",p->pid);
      printAllNode();
      while(1)
        ;
      //return -1;
    }
    //cprintf("(->%d) ", p->prev->pid);
    if(p->prev == p && p != head){
      cprintf("pid:(%d<-)%d(->%d)'s prev is self. infinite loop error.*\n", p->prev->pid,p->pid,p->next->pid);
      printAllNode();
      while(1)
        ;
      //return -1;
    }
    if(p->prev == head)
      break;
    else
      p = p->prev;
  }

  //cprintf("\ncheck is successed\n");
  return 1;
}

void checkAllQueue(){
  checkQueue(0);
  checkQueue(1);
  checkQueue(2);
  return;
}

void
printAllNode()
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  cprintf("-------------------------------------\n");
  for(int i = 0; i < 3; i++){
    cprintf("Lv%d queue : ",i);
    if(ptable.headLv[i]==0){
      cprintf("empty\n");
      continue;
    }
    // error
    else if(ptable.headLv[i]->next == 0){
      cprintf("error"); // ! for debug
      panic("procces link error");
    }
    for(p=ptable.headLv[i] ; ; p=p->next){
      char* state;
      if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
        state = states[p->state];
      else
        state = "???";
      if(i<2)
        cprintf("%d(%s) ",p->pid, state);
      else
        cprintf("%d(%s, %d) ",p->pid, state,p->priority);
      if(p->next==ptable.headLv[i])
        break;        
    }
    cprintf("\n");
  }
  cprintf("-------------------------------------\n");
  return;
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  ptable.globalTicks = 0;
  ptable.preferentialProc = 0;
  ptable.firstLv0Proc = 0;
  ptable.headLv[0] = 0;
  ptable.headLv[1] = 0;
  ptable.headLv[2] = 0;
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->level = 0; // process가 처음 생성될 때는 lv0으로 시작.
  p->priority = 3;
  p->ticks=4;
  //cprintf("나(%d) 생성돼서 큐에 들어간다~\n",p->pid);
  checkQueue(0); // ! for debug
  checkQueue(1); // ! for debug
  checkQueue(2);  // ! for debug
  insertTail(p,0);

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);
  cprintf("fork: 나(%d)가 (%d)를 만들었어.\n",curproc->pid,np->pid); // ! for debug
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  //cprintf("나(%d) 좀비가 됐오~~\n",curproc->pid);
  printPrevNext(curproc);
  detachNode(curproc);
  //cprintf("나 좀비인데 큐에서도 나왔어!!~~\n");
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

void setPriority(int pid, int priority){
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->priority = priority;
      return;
    }
  }
}
// ! ptable.lock을 얻은 후에만 호출되어야 함 !
// int
// relocateToFirstInLv(struct proc *curproc)
// {
  // int curlevel = curproc->level;
  // struct proc *firstp = 0;
  // struct proc *emptyp = 0;
  // struct proc *p;
  // //acquire(&ptable.lock);
  // for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
  //   if(emptyp == 0 && p->state == UNUSED){
  //     emptyp = p;
  //   }
  //   else if(p->level==curlevel && p->state == RUNNABLE){
  //     firstp = p;
  //     break;
  //   }
  // }
  // // # when curproc is already first
  // if(firstp == curproc){
  //   return 1;
  // }
  // // # when empty space is not existed before firstp
  // if(emptyp == 0){
  //   for(;p<&ptable.proc[NPROC];p++){
  //     if(p->state == UNUSED){
  //       emptyp = p;
  //       break;
  //     }
  //   }
  // }
  // // # when empty space is not existed after firstp too
  // if(emptyp == 0){
  //   // relocation is failed.
  //   return 0;
  // }
  // for(;emptyp>firstp; emptyp--){
  //   *emptyp = *(--emptyp);
  // }
  // *emptyp = *curproc;
  // curproc->state = UNUSED;
  // //release(&ptable.lock);
//   return 1;
// }

int
schedulerLock(void)
{
  acquire(&ptable.lock);
  cprintf("<<<scheduler Lock execute, pid:%d\n",myproc()->pid); // ! for debug

  // # 1. reset global ticks
  ptable.globalTicks = 0;

  // # if there is already a preferentialProc, schedulerLock will be failed
  if(ptable.preferentialProc && ptable.preferentialProc->state == RUNNABLE){
    release(&ptable.lock);
    cprintf("<<<scheduler Lock execute but failed, preferentialProc:%d\n",ptable.preferentialProc); // ! for debug
    return 0;
  }

  // # 2. myproc() become a preferentialProc
  ptable.preferentialProc = myproc();

  cprintf("<<<scheduler Lock execute success>>, preferentialProc:%d\n",ptable.preferentialProc->pid); // ! for debug
  release(&ptable.lock);
  return 1;
}

int
schedulerUnlock(void)
{
  struct proc *curproc = myproc();
  acquire(&ptable.lock);
  cprintf("scheduler 'UN'lock execute\n"); // ! for debug
  if (curproc != ptable.preferentialProc){
    release(&ptable.lock);
    cprintf("scheduler 'UN'lock execute but failed\n"); // ! for debug
    return 0;
  }
  //TODO: L0 이동 수정.
  ptable.firstLv0Proc = curproc;
  curproc->level = 0;
  curproc->priority = 3;
  curproc->ticks = 4;

  // # 먼저 스캐줄링 해야하는 프로세스를 없앴다.
  ptable.preferentialProc = 0;
  cprintf("<<<scheduler 'UN'lock execute success, preferentialProc:%d\n",ptable.preferentialProc); // ! for debug


  release(&ptable.lock);
  return 1;
}

// when global ticks become 100, priority boosting occurs.
void
priorityBoosting(void)
{
  //struct proc *p;
  //procdump();//! for debug
  //checkAllQueue();
  //mergeQueueToLv0();
  //procdump();//! for debug
  //checkAllQueue();
  ptable.globalTicks = 0;
  return;
}

// when process ran, global ticks increase by 1
// ! ptable.lock을 얻은 후에만 호출되어야 함 !
void
updateGlobalTicks(void)
{
  // # when the globalTicks become 100
  //cprintf("gt: %d\n",ptable.globalTicks); //! for debug
  if(ptable.globalTicks == 99){
    // # If scheduler"Lock" is on,
    if(ptable.preferentialProc){
      ptable.firstLv0Proc = ptable.preferentialProc;
      ptable.preferentialProc = 0;
    }
    return priorityBoosting();
  }

  ptable.globalTicks++;
  return;
}

// when process ran, the processs's ticks decrease by 1
// ! ptable.lock을 얻은 후에만 호출되어야 함 !
void
spendTicks(struct proc *p)
{
  if (p->ticks < 1)
    panic("tick error");

  p->ticks--;
  if(p->ticks == 0){

    //# process가 종료된 상황이면, 굳이 level을 변경하거나 할 필요 없음.
    if(p->state==ZOMBIE){
      return;
    }

    if(p->level < 2){
      //cprintf("나(%d) level 높아져서 %d큐에서 나올거야!\n",p->pid,p->level);
      detachNode(p);
      (p->level)++;
      //cprintf("나(%d) level 높아져서 %d큐에 들어간다~\n",p->pid,p->level);
      insertTail(p,p->level);
      p->ticks = 2*(p->level)+4;
      //cprintf("나(%d) headLv[%d]in spend time: %d\n",p->pid,p->level,ptable.headLv[(p->level)] != 0 ? ptable.headLv[(p->level)]->pid : 0 );
      return;
    }
    // If it is at lv2
    if(p->priority > 0)
      p->priority--;
    p->ticks=8;
  }
  return;
}

// execute Process.
// ! ptable.lock을 얻은 후에만 호출되어야 함 !
void
execProc(struct proc *p)
{
  //! for debug
  //printAllNode();
  // if(p->level!=2)
  //   cprintf("%d proc exec. lv-ticks: %d-%d\n",p->pid,p->level, p->ticks);
  // else
  //   cprintf("%d proc exec. lv-p-ticks: %d-%d-%d\n",p->pid,p->level, p->priority,p->ticks);

  struct cpu *c = mycpu();
  // Switch to chosen process.  It is the process's job
  // to release ptable.lock and then reacquire it
  // before jumping back to us.
  c->proc = p;
  switchuvm(p);
  p->state = RUNNING;

  swtch(&(c->scheduler), p->context);
  switchkvm();

  //# tick 줄이기
  spendTicks(p);
  updateGlobalTicks();
  //cprintf("나(%d) headLv[%d]in exec time: %d\n",p->pid,p->level,ptable.headLv[(p->level)] != 0 ? ptable.headLv[(p->level)]->pid : 0 );

  // ! for debug
  // Process is done running for now.
  // It should have changed its p->state before coming back.
  c->proc = 0;
}

// find runnable procces in queue and exec.
// if found and executed process, return 1
// else if didn't find runnable process and didn't excute, retrun 0
// ! ptable.lock을 얻은 후에만 호출되어야 함 !
int
findAndExec_RR(int level){
  struct proc *p; // 실행시킬 프로세스 변수
  struct proc* searchingStartPoint = ptable.headLv[level];

  // # Queue가 비어 있지 않다면 탐색 시작.
  if(ptable.headLv[level]!=0){
    //checkQueue(level); // ! for debug
    while(1){
      // # runnable proc 발견
      if(ptable.headLv[level]->state == RUNNABLE){
        p = ptable.headLv[level]; // 실행할 프로세스 저장.
        // # head(다음 스케줄링 시의searchingStartPoint)를 옮긴다. RR의 핵심.
        shiftHead(level);
        execProc(p);  // 실행된 head프로세스가 혼자여서 shift해도 자기가 head였는데, exec하면서 레벨이 전환되면서, head가 사라질 수도 있음.
        return 1;
      }
      // # runnable proc가 아니라면 헤드를 한칸 옮겨준다. ( 원형 큐이기 때문에, 이 행위는 살펴본 프로세스를 tail로 넘겨주는 것과 같은 행위. )
      shiftHead(level);
      // # 시작했던 큐의 처음 위치까지 돌아오면 탐색 종료
      if(ptable.headLv[level] == searchingStartPoint)
        break;
    }
  }
  // # 실행한 process가 없다고 0을 반환하여 알려줌.
  return 0;
}
int
findAndExec_PRIO(int level){
  struct proc *p; // 실행시킬 프로세스 변수
  struct proc *searchingStartPoint = ptable.headLv[level];
  struct proc *firstPrio1Proc = 0;
  struct proc *firstPrio2Proc = 0;
  struct proc *firstPrio3Proc = 0;

  // # Queue가 비어 있지 않다면 탐색 시작.
  if(ptable.headLv[level]!=0){
    //checkQueue(level);
    while(1){
      // # runnable proc 발견
      if(ptable.headLv[level]->state == RUNNABLE){

        // # priority가 0이라면 주저말고 실행
        if(ptable.headLv[level]->priority == 0){
          p = ptable.headLv[level]; // 실행할 프로세스 저장.
          // # prio queue 에서는 rr 처럼 shiftHead(다음 스케줄링 시의searchingStartPoint를 옮김)을 해주지 않는다. FCFS를 보장하기 위해서.
          // # 대신 반대로 head를 큐의 처음 위치로 옮겨놓음. (FSFC 보장)
          ptable.headLv[level] = searchingStartPoint;
          execProc(p);  // 실행된 head프로세스가 혼자여서 shift해도 자기가 head였는데, exec하면서 레벨이 전환되면서, head가 사라질 수도 있음.
          return 1;
        }

        // # priority가 0이 아니라면 위치만 기록
        else if(firstPrio1Proc == 0 && ptable.headLv[level]->priority==1)
          firstPrio1Proc = ptable.headLv[level];
        else if(firstPrio2Proc == 0 && ptable.headLv[level]->priority==2)
          firstPrio2Proc = ptable.headLv[level];
        else if(firstPrio3Proc == 0 && ptable.headLv[level]->priority==3)
          firstPrio3Proc = ptable.headLv[level];
      }

      // # runnable proc가 아니라면 헤드를 한칸 옮겨준다. ( 원형 큐이기 때문에, 이 행위는 살펴본 프로세스를 tail로 넘겨주는 것과 같은 행위. )
      shiftHead(level);
      //cprintf("%d, %d, %d\n",ptable.headLv[level] , searchingStartPoint,ptable.headLv[level] == searchingStartPoint); // ! for debug
      // # 시작했던 큐의 처음 위치까지 돌아오면 탐색 종료
      if(ptable.headLv[level] == searchingStartPoint)
        break;
      //cprintf("A "); // ! for debug
    }

    // # priority가 0인 process를 못 찾았기 때문에 그 아래 우선순위가 있다면 실행 후 리턴.
    if(firstPrio1Proc != 0){
      execProc(firstPrio1Proc);
      return 1;
    }
    else if(firstPrio2Proc != 0){
      execProc(firstPrio2Proc);
      return 1;
    }
    else if(firstPrio3Proc != 0){
      execProc(firstPrio3Proc);
      return 1;
    }
  }
  // # 실행한 process가 없다면 0을 반환
  return 0;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  //struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for(;;){
    //cprintf("A"); // ! for debug
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    //struct proc *firstLv1Proc = 0;

    if(ptable.preferentialProc)
      cprintf("preferentialProc: %d\n",ptable.preferentialProc->pid); // ! for debug
    // # EXECUTE ONE PROCESS
    // # If there is a preferential process ( when the schedulerLock is called )
    if(ptable.preferentialProc){
      if(ptable.preferentialProc->state == RUNNABLE)
        execProc(ptable.preferentialProc);
      else
        ptable.preferentialProc = 0;

      release(&ptable.lock);
      continue;
    }

    // # MLFQ SCHEDULING

    // # Lv0 scheduling
    //cprintf("headLv[0] before schedule: %d\n",ptable.headLv[0] != 0 ? ptable.headLv[0]->pid : 0 );
    if(findAndExec_RR(0)){
      release(&ptable.lock);
      continue;
    }

    // # Lv1 scheduling
    //cprintf("headLv[1] before schedule: %d\n",ptable.headLv[1] != 0 ? ptable.headLv[1]->pid : 0 );
    if(findAndExec_RR(1)){
      release(&ptable.lock);
      continue;
    }

    // # Lv2 scheduling
    if(findAndExec_PRIO(2)){
      release(&ptable.lock);
      continue;
    }
    // cprintf("z "); // ! for debug
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s [Lv:%d] [prio:%d] [ticks:%d]", p->pid, state, p->name, p->level, p->priority, p->ticks);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
