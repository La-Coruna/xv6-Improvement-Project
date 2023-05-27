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
  struct spinlock t_lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
int nexttid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  initlock(&ptable.t_lock, "ptable_thread");
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
// This process is also the main thread.
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
  thread_init(p);
  p->thread_info.main_thread = p;

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


// allocproc과 유사하게 thread로 사용할 proc할당.
static struct proc*
allocthread(struct proc* calling_thread)
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
  p->pid = calling_thread->pid;
  thread_init(p);

  release(&ptable.lock);

  // thread_id 배정. main_thread 연결
  acquire(&ptable.t_lock);
  p->thread_info.thread_id=nexttid++;
  p->thread_info.main_thread= (calling_thread->thread_info.thread_id==0) ? calling_thread : calling_thread->thread_info.main_thread;
  release(&ptable.t_lock);

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
  p->sz_limit = 0;
  p->stacknum = 1;
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

  acquire(&ptable.t_lock);
// cprintf("Try to grow process. [pid: %d, tid:%d, current size: %d, demanded size: %d, limit size: %d]\n", curproc->pid, curproc->thread_info.thread_id, curproc->sz, curproc->sz + n, curproc->sz_limit);
  sz = curproc->sz;
  if(n > 0){
    if(checkmemorylimit(curproc, n)){ // check if the new size of memory exceed the memory limit of the process
      if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0){
        release(&ptable.t_lock);
        return -1;
      }
    }
    else {
      // cprintf("the memory size exceed the limit. [pid: %d, demanded size: %d, limit size: %d]\n", curproc->pid, curproc->sz + n, curproc->sz_limit);
      release(&ptable.t_lock);
      return -1;
    }
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0){
      release(&ptable.t_lock);
      return -1;
    }
  }
  curproc->sz = sz;
  thread_update_proc_info(curproc);
  // cprintf("now the process is [pid: %d, current size: %d, limit size: %d]\n", curproc->pid, curproc->sz, curproc->sz_limit);
  release(&ptable.t_lock);
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
  np->sz_limit = curproc->sz_limit;
  np->stacknum = curproc->stacknum;
  np->parent = curproc; //TODO main thread인 process를 가리키도록 하자.
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

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  //! user program에서는 main thread만 exit를 호출함.
  //TODO 만약 worker thread에서 exit를 호출했다면, kill에 의해 호출된 것.
  // 하나의 thread가 kill 당하면, 모든 쓰레드를 종료해야함.
  // 즉, 어떤 thread가 exit당해도, 결국 그 thread가 속해있는 process 자체를 종료해야함.
  struct proc *curproc = myproc()->thread_info.main_thread;
  // struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  // exit를 호출한 thread가 main thread가 아니더라도, 종료하는 것은 main thread(process 자체)이다.
  // 따라서 종료하는 process를 main thread로 바꿔준다.
  if(curproc->thread_info.thread_id != 0)
    curproc = myproc()->thread_info.main_thread;

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
      //thread_update_proc_info(p); //@ p의 모든 thread가 parent기 바뀌도록 업데이트.
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
  release(&ptable.lock);

  // # 자식 thread들을 정리해줘야함.
  // ! exit를 호출하는 건 오로지 process(main thread)뿐.
  // ! main thread 외에 다른 thread들은 exit가 아닌 thread_exit로 종료됨.
  //all_thread_exit(curproc);
  //! new design
  all_child_thread_zombie(curproc);

  // Jump into the scheduler, never to return.
  acquire(&ptable.lock);
  curproc->state = ZOMBIE;
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
  // cprintf("wiat이 호출 되었대!![pid: %d, tid: %d]\n",curproc->pid, curproc->thread_info.thread_id);
  //procdump();
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE && p->thread_info.thread_id == 0){
        // Found one.
        pid = p->pid;
        int total_thread_num = (p->thread_info.thread_num) + 1; // main_thread도 포함하기 때문.
        cprintf("thread_num: %d\n", total_thread_num);
        int cnt = 0;
        for(struct proc* t = ptable.proc; t < &ptable.proc[NPROC]; t++){
          if(t->parent == curproc && t->state == ZOMBIE){
            kfree(t->kstack);
            t->kstack = 0;
            t->pid = 0;
            t->parent = 0;
            t->name[0] = 0;
            t->killed = 0;
            t->state = UNUSED;
            thread_init(t);

            if(total_thread_num == ++cnt)
              break;

          }
        }
        freevm(p->pgdir);
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
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
//      cprintf("@start@\n");//!
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      //procdump();
      //cprintf("@end@\n"); //!
      c->proc = 0;
    }
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

  /* for debug */
  // if(holding(&ptable.t_lock))
  //   panic("ptable.t_lock should not be locked now.");
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
    //acquire13(&ptable.lock,"sleep");
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
    cprintf("%d %s %s [tid: %d]", p->pid, state, p->name, p->thread_info.thread_id);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

void
proclist(void)
{
  acquire(&ptable.lock);
  cprintf("----------------- process list -----------------\n");

  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if((p->state == RUNNABLE || p->state == RUNNING || p->state == SLEEPING) && (p->thread_info.thread_id==0)){
      cprintf("%s %d %d %d %d\n", p->name, p->pid, p->stacknum, p->sz, p->sz_limit); //프로세스의 이름, pid, 스택용 페이지의 개수, 할당받은 메모리의 크기, 메모리의 최대 제한
    }
  }  


  cprintf("------------------------------------------------\n");
  release(&ptable.lock);
}

// If ok, then return 1.
// If not ok, then return 0.
int
checkmemorylimit(struct proc* p, uint newsz)
{
  if(p->sz_limit == 0 || p->sz_limit > p->sz + newsz)
    return 1;
  return 0;
}

// limit은 byte단위. 0 이상의 정수이며, 0인 경우 제한이 없음.
int
setmemorylimit(int pid, int limit)
{
  acquire(&ptable.lock);
  uint u_limit = (uint) limit;
  struct proc *p;
  struct proc *target_p = 0;

  // finding target process
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      target_p = p;
      break;
    }
  }  

  // If there isn't target or its memory size is already bigger than limit,
  // then setting memory limit failed.
  if(target_p == 0 || target_p->sz > u_limit){
    release(&ptable.lock);
    return -1;
  }
  target_p->sz_limit = u_limit;
  cprintf("%s %d %d\n", target_p->name, target_p->pid, target_p->sz_limit); //프로세스의 이름, pid, 스택용 페이지의 개수, 할당받은 메모리의 크기, 메모리의 최대 제한
  
  //cprintf("set memory limit. pid: %d, limit: %d\n",pid,u_limit);

  release(&ptable.lock);
  return 0;
}

//TODO

void
thread_init(struct proc *p){
  //cprintf("thread_init 시작: %d, %d\n",p->pid,p->thread_info.thread_id);
  p->thread_info.thread_id = 0;
  p->thread_info.thread_num = 0;
  p->thread_info.retval = 0;
  p->thread_info.main_thread = 0;
  //cprintf("thread_init 끝: %d, %d\n",p->pid,p->thread_info.thread_id);
  return;
}

//proc 내부 멤버 변수 값이 바뀌었을 때 같은 pid를 가진 thread들에게도 업데이트 해주는 함수
void
thread_update_proc_info(struct proc * curproc)
{
  int pid = curproc->pid;
  thread_t tid = curproc->thread_info.thread_id;
  struct proc * t;
  // cprintf("[pid: %d 인 thread의 정보를 update 하겠습니다.]\n",pid);
  for(t = ptable.proc; t < &ptable.proc[NPROC]; t++){
    if(t->pid == pid && t->thread_info.thread_id != tid){
      // cprintf("tid:%d 인 thread의 정보를 update 했습니다.\n",t->thread_info.thread_id);
      t->pgdir = curproc->pgdir;
      t->sz = curproc->sz;
      t->sz_limit = curproc->sz_limit;
      t->parent = curproc->parent;
    }
  }
  // cprintf("[pid: %d 인 thread의 정보를 update 완료했습니다.]\n",pid);
  return;
}

// Create a new thread copying current process as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned thread to RUNNABLE.
int
thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg)
{
  int i;
  struct proc *nt;
  struct proc *main_thread = myproc()->thread_info.main_thread;

  // Allocate process.
  if((nt = allocthread(main_thread)) == 0){ //TODO 인자를 mainthread로 바꿔주기.
    return -1;
  }
  
  acquire(&ptable.t_lock);
  nt->pgdir = main_thread->pgdir;
  nt->sz = main_thread->sz;
  nt->sz_limit = main_thread->sz_limit;
  nt->stacknum = main_thread->stacknum;
  nt->parent = main_thread->parent;

  *nt->tf = *main_thread->tf;
  nt->tf->eax = 0;

  nt->tf->eip = (uint)start_routine; //# 이동이 된다!

  for(i = 0; i < NOFILE; i++)
    if(main_thread->ofile[i])
      nt->ofile[i] = filedup(main_thread->ofile[i]);
  nt->cwd = idup(main_thread->cwd);

  safestrcpy(nt->name, main_thread->name, sizeof(main_thread->name));

  //TODO nt의 sz 말고도 process자체의 sz도 변경해줘야 할 듯.
  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  // cprintf("thread[pid:%d,tid:%d]의 stack 할당. 시키는 thread는 [pid:%d,tid:%d]\n",nt->pid, nt->thread_info.thread_id,main_thread->pid, main_thread->thread_info.thread_id);
  uint sz, sp, ustack[3];
  sz = PGROUNDUP(nt->sz);
  if((sz = allocuvm(nt->pgdir, sz, sz + 2*PGSIZE)) == 0)  //@ allocuvm point
    cprintf("error: stack 할당\n");
  clearpteu(nt->pgdir, (char*)(sz - 2*PGSIZE)); //# 가드페이지 생성
  sp = sz;
  nt->sz = sz;
  main_thread->sz = sz;
  thread_update_proc_info(nt); // TODO mainthread 기준으로 하게 만들었는데, 굳이 이거 해줘야할까? 안하면 sbrk test 못통과하네.

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = (uint) arg;

  sp -= (2) * 4;
  if(copyout(nt->pgdir, sp, ustack, 8) < 0)
    cprintf("error: stack 값 복사\n");

  nt->tf->esp = sp;

  release(&ptable.t_lock);

  acquire(&ptable.lock);
  nt->state = RUNNABLE;
  release(&ptable.lock);

  *thread = nt->thread_info.thread_id;
  main_thread->thread_info.thread_num++;

  return 0;
}

void
thread_exit(void *retval)
{
  struct proc *curproc = myproc();
  int fd;

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  // cwd(현재 작업 디렉토리) 닫기
  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // # join하고 있는 쓰레드가 있다면 깨워줌.
  wakeup1(curproc);

  // # join에서 받을 반환값 설정
  curproc->thread_info.retval = retval;

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  curproc->thread_info.main_thread->thread_info.thread_num--;
  // cprintf(" ---------- exit 완료! ---------- \n");
  sched();
  panic("zombie exit");
}

// process에서 exec를 호출하는 경우,
// exec를 호출하는 thread를 제외하고 전부 종료하기 위한 함수
// exec 내부에서 호출됨.
void
all_thread_exit_except_exec_thread(struct proc * thread)
{
  int curproc_pid = thread->pid;
  int curproc_tid = thread->thread_info.thread_id;
  struct proc * main_thread = thread->thread_info.main_thread;
  int child_thread_num = main_thread->thread_info.thread_num;
  struct proc *p;
  int fd;

  if(child_thread_num == 0){
    return;
  }
  // ! for debug
  else if(child_thread_num < 0){
    panic("thread exit more than create");
  }

  acquire(&ptable.lock);

  // finding worker thread
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == curproc_pid && p->thread_info.thread_id != curproc_tid){
      
      // Close all open files.
      release(&ptable.lock); // # iput과 fileclose에서 ptable.lock을 씀.
      for(fd = 0; fd < NOFILE; fd++){
        if(p->ofile[fd]){
          fileclose(p->ofile[fd]);
          p->ofile[fd] = 0;
        }
      }
      begin_op();
      iput(p->cwd);
      end_op();
      p->cwd = 0;

      acquire(&ptable.lock);
      //p->state = ZOMBIE;

      acquire(&ptable.t_lock);
      //cprintf("p[%d의 %d]->kstack: %d\n",p->pid,p->thread_info.thread_id,p->kstack);
      kfree(p->kstack); //@
      //cprintf("kfree완료\n",p->pid,p->thread_info.thread_id,p->kstack);
      p->kstack = 0;
      // freevm(p->pgdir); //@ pgdir은 exec에서 free해줄거임
      p->pid = 0;
      p->parent = 0;
      p->name[0] = 0;
      p->killed = 0;
      thread_init(p); // @@ 새로 추가해줌.
      p->state = UNUSED;
      //@@@
      main_thread->thread_info.thread_num--;
      release(&ptable.t_lock);

      if(main_thread->thread_info.thread_num == 0)
        break;
    }
  }
  release(&ptable.lock);
  return;  
}

// process에서 exit호출 시, 자식 thread를 전부 zombie로 바꾸는 함수.
// exit 내부에서 호출 됨.
void
all_child_thread_zombie(struct proc * thread)
{
  int curproc_pid = thread->pid;
  int curproc_tid = thread->thread_info.thread_id;
  struct proc * main_thread = thread->thread_info.main_thread;
  int child_thread_num = main_thread->thread_info.thread_num;
  struct proc *p;
  int fd;

  if(child_thread_num == 0){
    return;
  }
  // ! for debug
  else if(child_thread_num < 0){
    panic("thread exit more than create");
  }

  acquire(&ptable.lock);

  // finding worker thread
  int cnt = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == curproc_pid && p->thread_info.thread_id != curproc_tid){
      
      // Close all open files.
      release(&ptable.lock); // # iput과 fileclose에서 ptable.lock을 씀.
      for(fd = 0; fd < NOFILE; fd++){
        if(p->ofile[fd]){
          fileclose(p->ofile[fd]);
          p->ofile[fd] = 0;
        }
      }
      begin_op();
      iput(p->cwd);
      end_op();
      p->cwd = 0;

      acquire(&ptable.lock);
      p->state = ZOMBIE;

      if(child_thread_num == ++cnt)
        break;
    }
  }
  release(&ptable.lock);
  return;  
}


// Wait for a child thread to exit and return 0.
// Return -1 if this process has no thread.
int
thread_join(thread_t thread, void **retval)
{
  
  struct proc *p;
  int havethreads;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havethreads = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->thread_info.thread_id != thread)
        continue; 
      if(p->pid != curproc->pid)
        panic("error: 다른 process의 thread를 기다리고 있다.");
      havethreads = 1;
      if(p->state == ZOMBIE){
        // Found one.
        kfree(p->kstack);
        p->kstack = 0;
        // freevm(p->pgdir); //@ pgdir은 프로세스꺼 가져와서 쓰고 있어서 반환하면 안됨!
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        // # retval 전달
        *retval = (void *) p->thread_info.retval;
        release(&ptable.lock);
        return 0;
      } else {
        break;
      }
    }

    // No point waiting if we don't have the thread.
    if(!havethreads || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(p, &ptable.lock);  //DOC: wait-sleep
  }
}

//TODO 2023-05-27
/* 
  0. 주석 정리
*/
