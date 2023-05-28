// Process Manager

#include "types.h"
#include "stat.h"
#include "user.h"

// Parsed pmanager command representation
#define WRONGARG  -1
#define WRONGCMD  0
#define LIST  1
#define KILL 2
#define EXEC  3
#define MEMLIM  4
#define EXIT  5
#define EXEC1  6 // ! for debug
#define SBRK  7 // ! for debug

#define MAXARGLENGTH 51

#define WHITESPACE " \t\r\n\v"

struct pmgrcmd {
  int type;
};

struct cmdarg0 {
  int type;
};

struct cmdarg1 {
  int type;
  const char *arg;
};

struct cmdarg2 {
  int type;
  const char *arg1;
  const char *arg2;
};

// function declaration
void skipWhitespace(char ** ss, char * es);
char* getWord(char ** ss, char * es);
struct pmgrcmd* alloc_cmdarg0(int type);
struct pmgrcmd* alloc_cmdarg1(int type, const char *arg);
struct pmgrcmd* alloc_cmdarg2(int type, const char *arg1, const char *arg2);
int getPmgrCmd(char *buf, int nbuf);
struct pmgrcmd* parsecmd(char *s);
void runPmgrCmd(struct pmgrcmd *cmd);

int main(int argc, char *argv[])
{
  static char buf[100];

  // Read and run input commands.
  while(getPmgrCmd(buf, sizeof(buf)) >= 0){
    runPmgrCmd(parsecmd(buf));
  }
  exit();
}

// function definition

// skip the whitespace, so move to the front of the next word.
void
skipWhitespace(char ** s, char * es)
{
  while(*s < es && strchr(WHITESPACE, **s)){
    (*s)++;
  }
  return;
}

// Get the front word from string. if there is some whitespace, ignore it.
char *
getWord(char ** s, char * es)
{
  // skip whitespace
  skipWhitespace(s,es);

  // get a word (till reach whitespace)
  char *arg = malloc(sizeof(char)*MAXARGLENGTH);
  memset(arg, 0, sizeof(char)*MAXARGLENGTH);
  char * ss = *s;
  while(*s < es && !strchr(WHITESPACE, **s)){
      (*s)++;
  }
  **s = 0; // 문자열 끝에 널문자 추가
  (*s)++;
  strcpy(arg, ss);
  return arg;
}

struct pmgrcmd*
alloc_cmdarg0(int type)
{
  struct cmdarg0 *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = type;
  return (struct pmgrcmd*)cmd;
}

struct pmgrcmd*
alloc_cmdarg1(int type, const char *arg)
{
  struct cmdarg1 *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));

  // if argument is wrong
  if(strcmp(arg,"")==0){
    cmd->type = WRONGARG;
    cmd->arg = 0;
  }
  // if argument is correct
  else {
    cmd->type = type;
    cmd->arg = arg;
  }

  return (struct pmgrcmd*)cmd;
}

struct pmgrcmd*
alloc_cmdarg2(int type, const char *arg1, const char *arg2)
{
  struct cmdarg2 *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));

  // if argument is wrong
  if(strcmp(arg1,"")==0 || strcmp(arg2,"")==0){
    cmd->type = WRONGARG;
    cmd->arg1 = 0;
    cmd->arg2 = 0;
  }
  // if argument is correct
  else {
    cmd->type = type;
    cmd->arg1 = arg1;
    cmd->arg2 = arg2;
  }

  return (struct pmgrcmd*)cmd;
}

// get one pmanager command
int
getPmgrCmd(char *buf, int nbuf)
{
  printf(2, ">>> ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

// parse command and return structure pmgrcmd pointer.
struct pmgrcmd*
parsecmd(char *s)
{
  char *es; // end of string
  char *commandName;
  struct pmgrcmd *pmgrcmd = 0;

  es = s + strlen(s);

  // 어떤 명령어인지 확인
  commandName = getWord(&s,es);
  // list
  if (strcmp(commandName,"list")==0){
    pmgrcmd = alloc_cmdarg0(LIST);
  }

  // kill
  else if (strcmp(commandName,"kill")==0){
    // parsing into argument
    char *arg = getWord(&s,es);
    pmgrcmd = alloc_cmdarg1(KILL,arg);
  }

  // execute
  else if (strcmp(commandName,"execute")==0){
    // parsing into argument
    char *arg1 = getWord(&s,es);
    char *arg2 = getWord(&s,es);
    pmgrcmd = alloc_cmdarg2(EXEC,arg1,arg2);
  }

  // memlim
  else if (strcmp(commandName,"memlim")==0){
    // parsing into argument
    char *arg1 = getWord(&s,es);
    char *arg2 = getWord(&s,es);
    pmgrcmd = alloc_cmdarg2(MEMLIM,arg1,arg2);
  }

  // exit
  else if (strcmp(commandName,"exit")==0){
    pmgrcmd = alloc_cmdarg0(EXIT);
  }

  // ! for debug
  // execute1
  else if (strcmp(commandName,"exec1")==0){
    // parsing into argument
    char *arg = getWord(&s,es);
    pmgrcmd = alloc_cmdarg1(EXEC1,arg);
  }

  // ! for debug
  // debug command
  else if (strcmp(commandName,"sbrk")==0){
    pmgrcmd = alloc_cmdarg0(SBRK);
  }

  // wrong command
  else {
    pmgrcmd = alloc_cmdarg0(WRONGCMD);
  }

  return pmgrcmd;
}

// Execute pmanger cmd.
void
runPmgrCmd(struct pmgrcmd *pmgrcmd)
{
  
  struct cmdarg1 *cmd1;
  struct cmdarg2 *cmd2;
  
  int pid;
  int limit;
  char * path;
  char ** argv;
  int stacksize;
  
  switch(pmgrcmd->type){
  default:
    break;

  // 명세 외의 command를 입력했을 때 예외처리
  case WRONGCMD:
    printf(1,"wrong command.\n");
    free(pmgrcmd);
    break;

  // 인자를 잘못 입력했을 때 예외처리
  case WRONGARG:
    printf(1,"wrong argument.\n");
    free(pmgrcmd);
    break;

  case LIST:
    proclist();
    free(pmgrcmd);
    break;

  case KILL:
    cmd1 = (struct cmdarg1 *) pmgrcmd;
    pid = atoi(cmd1->arg);
    if(pid == 0){
      printf(1,"You can't kill the process with pid:0.\n");
    }
    else if (kill(pid) == -1){
      printf(1,"killing process whose pid is %d failed.\n",pid);
    }
    else{
      printf(1,"killing process whose pid is %d successed.\n",pid);
    }
    free(cmd1);
    break;

  case EXEC:
    cmd2 = (struct cmdarg2 *) pmgrcmd;

    path = (char *) cmd2->arg1;
    argv = &path;
    stacksize = atoi(cmd2->arg2);

    if(fork() == 0){
      exec2(path,argv,stacksize);
      // If exec2 failed, then the forked process exit.
      exit();
    }

    free(cmd2);
    break;

  case MEMLIM:
    cmd2 = (struct cmdarg2 *) pmgrcmd;
    pid = atoi(cmd2->arg1);
    limit = atoi(cmd2->arg2);
    if(pid == 0){
      printf(1,"You can't set the memory limit of the process with pid:0.\n");
    }
    else if(setmemorylimit(pid,limit) == 0)
      printf(1,"set memory limit succeed.\n");

    free(cmd2);
    break;

  case EXIT:
    printf(1,"pmanager exit\n");
    free(pmgrcmd);
    exit();
    break;

  // ! for debug
  case EXEC1:
    cmd1 = (struct cmdarg1 *) pmgrcmd;

    printf(1,"[debug command] exec1: %s\n",cmd1->arg);

    path = (char *) cmd1->arg;
    argv = &path;

    if(fork() == 0){
      exec(path,argv);
      // If exec1 failed, then the forked process exit.
      exit();
    }
    free(cmd1);
    break;

  // ! for debug
  case SBRK:
    printf(1,"[debug command] sbrk(500) with current process.\n");

    sbrk(500);
    free(pmgrcmd);
    break;

  }
  return;
}