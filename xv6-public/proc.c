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
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  ptable.globalTicks = 0;
  ptable.preferentialProc = 0;
  ptable.firstLv0Proc = 0;
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
// !ptable lock을 얻고 호출되어야함
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
  struct proc *p;
  procdump();//! for debug
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    p->level = 0;
    p->priority = 3;
    p->ticks=4;
  }
  ptable.globalTicks = 0;
  procdump();//! for debug
  return;
}

// when process ran, global ticks increase by 1
// !ptablelock을 얻은 후에만 호출되어야 함
void
updateGlobalTicks(void)
{
  // # when the globalTicks become 100
  cprintf("gt: %d\n",ptable.globalTicks); //! for debug
  if(ptable.globalTicks == 99){
    // # If schedulerLock is on,
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
// !ptablelock을 얻은 후에만 호출되어야 함
void
spendTicks(struct proc *p)
{
  if (p->ticks < 1)
    panic("tick error");

  p->ticks--;
  if(p->ticks == 0){
    if(p->level < 2){
      p->level++;
      p->ticks = 2*(p->level)+4;
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
// !ptablelock을 얻은 후에만 호출되어야 함
void
execProc(struct proc *p)
{
  //! for debug
  if(p->level!=2)
    cprintf("%d proc exec. lv-ticks: %d-%d\n",p->pid,p->level, p->ticks);
  else
    cprintf("%d proc exec. lv-p-ticks: %d-%d-%d\n",p->pid,p->level, p->priority,p->ticks);

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

  // Process is done running for now.
  // It should have changed its p->state before coming back.
  c->proc = 0;
}

// round-robin scheduling with ptable for process at 'procLv'
// !ptablelock을 얻은 후에만 호출되어야 함
void
rrScheduler(struct proc *startp, int procLv, int isFullRotation)
{
  struct proc *p;
  for(p = startp; !(ptable.preferentialProc) && p < &ptable.proc[NPROC]; p++){
    if(p->state != RUNNABLE)
      continue;
    if(p->level == procLv)
      execProc(p);
  }
  if(isFullRotation){
    for(p = ptable.proc; !(ptable.preferentialProc) && p < startp; p++){
      if(p->state != RUNNABLE)
        continue;
      if(p->level == procLv)
        execProc(p);
    }
  }
}

// priority scheduling with ptable for process at L2
// !ptablelock을 얻은 후에만 호출되어야 함
void
priorityScheduler()
{
  int isPrio0Exist = 0;
  struct proc *firstPrio1Proc = 0;
  struct proc *firstPrio2Proc = 0;
  struct proc *firstPrio3Proc = 0;
  struct proc *p;
  int prio = 0;

  // # Exploring which priority processes are runnable
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state != RUNNABLE)
      continue;
    if(firstPrio3Proc == 0 && p->priority == 3){
      firstPrio3Proc = p;
      continue;
    }
    if(firstPrio2Proc == 0 && p->priority == 2){
      firstPrio2Proc = p;
      continue;
    }
    if(firstPrio1Proc == 0 && p->priority == 1){
      firstPrio1Proc = p;
      continue;
    }
    if(p->priority == 0){
      isPrio0Exist = 1;
      break;
    }
  }
  // # scheduling 시작부분 정하기
  if (!isPrio0Exist){
    if(firstPrio1Proc){
      p = firstPrio1Proc;
      prio = 1;
    }
    else if(firstPrio2Proc){
      p = firstPrio2Proc;
      prio = 2;
    }
    else if(firstPrio3Proc){
      p = firstPrio3Proc;
      prio = 3;
    }
  }
  for(; !(ptable.preferentialProc) && p < &ptable.proc[NPROC]; p++){
    if(p->state != RUNNABLE)
      continue;
    if(p->priority == prio)
      execProc(p);
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

    struct proc *firstLv1Proc = 0;
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

    // #1 If we aldeady know whether the first process at level 0 is existed
    if(ptable.firstLv0Proc != 0 && ptable.firstLv0Proc->level==0 && ptable.firstLv0Proc->state == RUNNABLE){
      rrScheduler(ptable.firstLv0Proc,0,1);
      release(&ptable.lock);
      continue;
    }

    // #2 If we don't know whether the first process at level 0 is existed
    if(ptable.firstLv0Proc == 0)
      p = ptable.proc;
    else // firstLv0Proc->state 가 RUNNABLE이 아닌 경우
      p = ptable.firstLv0Proc;

    for(; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      if(firstLv1Proc == 0 && p->level == 1 ){
        firstLv1Proc = p;
        continue;
      }
      if(p->level == 0){
        ptable.firstLv0Proc = p;
        break;
      }
    }

    // #2-a If you started to search from middle of ptable.proc, you should keep searching until back to start point
    if(ptable.firstLv0Proc != 0 && (ptable.firstLv0Proc->state != RUNNABLE || ptable.firstLv0Proc->level!=0)){
      struct proc *loopEnd = ptable.firstLv0Proc;
      ptable.firstLv0Proc = 0;
      for(p=ptable.proc; p < loopEnd; p++){
        if(p->state != RUNNABLE)
          continue;
        if(firstLv1Proc == 0 && p->level == 1 ){
          firstLv1Proc = p;
          continue;
        }
        if(p->level == 0){
          ptable.firstLv0Proc = p;
          break;
        }
      }
    }

    //lv0이 하나라도 존재하는 경우, ptable을 돌며 lv0을 우선적으로 실행하
    if(ptable.firstLv0Proc){
      rrScheduler(p,0,1);
    }
    else if(firstLv1Proc){ // ptable에 lv0이 없고, lv1이 하나라도 있는 경우. lv1만 실행한다.
      rrScheduler(firstLv1Proc,1,0);
    }
    else{ // ptable에 lv0과 lv1이 모두 없는 경우. lv2를 priority scheduling 으로 실행.
      priorityScheduler();
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
