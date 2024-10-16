#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"


//Simple system call
int
myfunction(char *str)
{
	cprintf("%s\n", str);
	return 0xABCDABCD;
}

int
getLevel(void)
{
	return myproc()->level;
}


//Wrapper for my_syscall
int
sys_myfunction(void)
{
	char* str;
	//Decode argument using argstr
	if (argstr(0,&str)<0)
		return -1;
	return myfunction(str);
}

void
sys_yield(void)
{
	return yield();
}

int
sys_getLevel(void)
{
	return getLevel();
}

void
sys_setPriority(void)
{
	int pid, priority;
	if (argint(0,&pid)<0 || argint(1,&priority)<0)
		return;
	//cprintf("setPriority pid: %d, priority: %d\n",pid,priority);
	setPriority(pid,priority);
	return;
}

void
sys_schedulerLock(void)
{
	int password;
	if (argint(0,&password)<0)
		return;

  else if(password != 2019019043){
    struct proc *p = myproc();
    cprintf("Wrong password. SchedulerLock is failed.\n");
    cprintf("pid: %d, ticks: %d, level: %d\n",p->pid, (2*(p->level)+4) - (p->ticks), p->level);
    exit();
  }

	schedulerLock();

	/* for debug */
	// if(schedulerLock())
	// 	cprintf("SchedulerLock is successed.\n");
	// else
	// 	cprintf("Already SchedulerLock is on.\n");
	// return;
}

void
sys_schedulerUnlock(void)
{
	int password;
	if (argint(0,&password)<0)
		return;

	else if(password != 2019019043){
		struct proc *p = myproc();
		cprintf("Wrong password. SchedulerUnlock is failed.\n");
    cprintf("pid: %d, ticks: %d, level: %d\n",p->pid, (2*(p->level)+4) - (p->ticks), p->level);
		exit();
	}

	schedulerUnlock();

	/* for debug */
	// if(schedulerUnlock())
	// 	cprintf("SchedulerUnlock is successed.\n");
	// else
	// 	cprintf("Already current scheduler is for MLFQ.\n");
	// return;
}