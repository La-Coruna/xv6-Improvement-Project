// Process Manager

#include "types.h"
#include "stat.h"
#include "user.h"

// Parsed pmanager command representation
#define LIST  1
#define KILL 2
#define EXEC  3
#define MEMLIM  4
#define EXIT  5

#define MAXARGLENGTH 10

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
int getPmgrCmd(char *buf, int nbuf);
struct pmgrcmd* parsecmd(char *s);
void runPmgrCmd(struct pmgrcmd *cmd);

// global variable
char whitespace[] = " \t\r\n\v";

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

// skip the whitespace, so move to the front of the next word
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
alloc_cmdarg0(void)
{
  struct cmdarg0 *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  return (struct pmgrcmd*)cmd;
}

struct pmgrcmd*
alloc_cmdarg1(const char *arg)
{
  struct cmdarg1 *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->arg = arg;
  return (struct pmgrcmd*)cmd;
}

struct pmgrcmd*
alloc_cmdarg2(const char *arg1, const char *arg2)
{
  struct cmdarg2 *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->arg1 = arg1;
  cmd->arg2 = arg2;
  return (struct pmgrcmd*)cmd;
}

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
    pmgrcmd = alloc_cmdarg0();
    pmgrcmd->type=LIST;
  }

  // kill
  else if (strcmp(commandName,"kill")==0){
    // parsing into argument
    char *arg = getWord(&s,es);
    pmgrcmd = alloc_cmdarg1(arg);
    pmgrcmd->type=KILL;
  }

  // execute
  else if (strcmp(commandName,"execute")==0){
      // parsing into argument
      char *arg1 = getWord(&s,es);
      char *arg2 = getWord(&s,es);
      pmgrcmd = alloc_cmdarg2(arg1,arg2);
      pmgrcmd->type=EXEC;
    ;
  }

  // memlim
  else if (strcmp(commandName,"memlim")==0){
      // parsing into argument
      char *arg1 = getWord(&s,es);
      char *arg2 = getWord(&s,es);
      pmgrcmd = alloc_cmdarg2(arg1,arg2);
      pmgrcmd->type=MEMLIM;
    ;
  }

  // exit
  else if (strcmp(commandName,"exit")==0){
    pmgrcmd = alloc_cmdarg0();
    pmgrcmd->type=EXIT;
  }
  return pmgrcmd;
}

// Execute pmanger cmd.
void
runPmgrCmd(struct pmgrcmd *pmgrcmd)
{
  struct cmdarg0 *cmd0;
  struct cmdarg1 *cmd1;
  struct cmdarg2 *cmd2;
  // struct memlimcmd *mlcmd;

  // 명세 외의 command를 입력했을 때 예외처리
  if(pmgrcmd == 0){
    printf(1,"wrong command.\n");
    return;
  }

  switch(pmgrcmd->type){
  default:
    break;

  case LIST:
    printf(1,"list\n");
    cmd0 = (struct cmdarg0 *) pmgrcmd;
    printf(1,"list type: %d\n",cmd0->type);
    break;

  case KILL:
    cmd1 = (struct cmdarg1 *) pmgrcmd;
    printf(1,"kill type: %d\n",cmd1->type);
    printf(1,"kill pid: %d\n",cmd1->arg);
    printf(1,"kill pid: %s\n",cmd1->arg);
    break;

  case EXEC:
    cmd2 = (struct cmdarg2 *) pmgrcmd;
    printf(1,"exec\n");
    printf(1,"exec type: %d\n",cmd2->type);
    printf(1,"exec pid: %s\n",cmd2->arg1);
    printf(1,"exec pid: %s\n",cmd2->arg2);
    // ecmd = (struct execcmd*)cmd;
    // if(ecmd->argv[0] == 0)
    //   exit();
    // exec(ecmd->argv[0], ecmd->argv);
    // printf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case MEMLIM:
    printf(1,"memlim\n");
    break;

  case EXIT:
    printf(1,"exit\n");
      exit();
    break;

  // case BACK:
    // bcmd = (struct backcmd*)cmd;
    // if(fork1() == 0)
    //   runcmd(bcmd->cmd);
    // break;
  }
  return;
}