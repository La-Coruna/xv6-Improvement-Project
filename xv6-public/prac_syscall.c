#include "types.h"
#include "defs.h"

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
	cprintf("usermode yield\n");
}

int
getLevel(void)
{
	cprintf("getLevel\n");
	return 1;
}

void
setPriority(int pid, int priority)
{
	cprintf("setPriority\n");
}

void
schedulerLock(int password)
{
	cprintf("schedulerLock\n");
}

void
schedulerUnlock(int password)
{
	cprintf("schedulerUnlock\n");
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
sys_setPriority(int pid, int priority)
{
	return setPriority(1,1);
}

void
sys_schedulerLock(int password)
{
	return schedulerLock(1);
}

void
sys_schedulerUnlock(int password)
{
	return schedulerUnlock(1);
}