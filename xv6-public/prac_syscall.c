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

void
yield_UM(void)
{
	yield();
	cprintf("usermode yield\n");
}

int
getLevel(void)
{
	return myproc()->level;
}

void
setPriority_UM(int pid, int priority)
{
	cprintf("setPriority pid: %d, priority: %d\n",pid,priority);
	setPriority(pid,priority);
}

void
schedulerLock(int password)
{
	cprintf("schedulerLock %d\n",password);
}

void
schedulerUnlock(int password)
{
	cprintf("schedulerUnlock %d\n",password);
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
	return yield_UM();
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
	if (argint(0,&pid)<0 || argint(1,&priority))
		return;
	return setPriority_UM(pid,priority);
}

void
sys_schedulerLock(void)
{
	int password;
	if (argint(0,&password)<0)
		return;
	return schedulerLock(password);
}

void
sys_schedulerUnlock(void)
{
	int password;
	if (argint(0,&password)<0)
		return;
	return schedulerUnlock(password);
}