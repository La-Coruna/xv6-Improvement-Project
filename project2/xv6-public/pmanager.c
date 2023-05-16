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
int strncmp(const char *p, const char *q, uint n);
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

//function definition

int
strncmp(const char *p, const char *q, uint n)
{
  while(n > 0 && *p && *p == *q)
    n--, p++, q++;
  if(n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
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
  //printf(1,"in arg: %s\n",cmd->arg); // ! for debug
  //printf(1,"in arg d: %d\n",cmd->arg); // ! for debug
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
  // char *es;
  // struct cmd *cmd;

  // es = s + strlen(s);
  // cmd = parseline(&s, es);
  // peek(&s, es, "");
  // if(s != es){
  //   printf(2, "leftovers: %s\n", s);
  //   panic("syntax");
  // }
  // nulterminate(cmd);
  // return cmd;

  char *ss; // end of string
  char *es; // end of string
  int cmdlen = 0; // length of command
  struct pmgrcmd *pmgrcmd = 0;

  //ss = s;
  es = s + strlen(s);
  printf(1, "input is %s\n",s);
  printf(1, "zinput is %d\n",strncmp(s,"list",cmdlen));

  // //TODO 공백을 기준으로 parsing 해야함
  // if (strcmp(s,"list")==0){
  //   pmgrcmd = alloc_cmdarg0();
  //   pmgrcmd->type=LIST;
  // }
  // else if (strcmp(s,"kill")==0){
  //   pmgrcmd = alloc_cmdarg0();
  //   pmgrcmd->type=KILL;
  // }
  // else if (strcmp(s,"exit")==0){
  //   pmgrcmd = alloc_cmdarg0();
  //   pmgrcmd->type=EXIT;
  // }

//TODO 이 while문 이용하면 될듯
  // printf(1,"s: %s\n", s);
  // printf(1,"s: %d, es: %d\n",s,es);
  // printf(1,"s: %d, es: %d\n",s,es);
  
  // 명령어 앞에 공백 지우기
  while(s < es && strchr(whitespace, *s)){
    s++;
  }

  // 다음 공백까지 끊기 (명령어가 무엇인지 확인하기 위해)
  ss = s;
  printf(1,"s: %s",s); // ! for debug
  //TODO 이 while문 모듈화가 가능할 것 같은데... 빈칸 없애는 함수1 그리고 다음 빈칸까지 strcpy해서 새로운 문자열로 반환하는 거 1
  while(s < es && !strchr(whitespace, *s)){
    s++;
    cmdlen++;
  }
  printf(1,"s: %s",s); // ! for debug
  s++;
  printf(1,"s: %s",s); // ! for debug
  // 어떤 명령어인지 확인
  if(cmdlen == 4){
    // list
    if (strncmp(ss,"list",cmdlen)==0){
      pmgrcmd = alloc_cmdarg0();
      pmgrcmd->type=LIST;
    }

    // kill
    else if (strncmp(ss,"kill",cmdlen)==0){
      // parsing into argument
      char *arg = malloc(sizeof(char)*MAXARGLENGTH);
      memset(arg, 0, sizeof(char)*MAXARGLENGTH);
      //printf(1,"s: %s\n",s );// ! for debug
      ss = s;
      //printf(1,"ss: %s\n",ss );// ! for debug
      //printf(1,"ss pointer: %d\n",ss );// ! for debug
      while(s < es && !strchr(whitespace, *s)){
        //printf(1,"+ss pointer: %d\n",ss );// ! for debug
        s++;
      }
      //printf(1,"rss pointer: %d\n",ss );// ! for debug
      *s = 0;
      //printf(1,"ss: %s\n",ss );// ! for debug
      strcpy(arg, ss);
      //printf(1,"arg: %s\n",arg );// ! for debug
      pmgrcmd = alloc_cmdarg1(arg);
      pmgrcmd->type=KILL;
    }

    // exit
    else if (strncmp(ss,"exit",cmdlen)==0){
      pmgrcmd = alloc_cmdarg0();
      pmgrcmd->type=EXIT;
    }
  }
  else if (cmdlen==7 && strncmp(ss,"execute",cmdlen)==0){
      // parsing into argument
      char *arg1 = malloc(sizeof(char)*MAXARGLENGTH);
      char *arg2 = malloc(sizeof(char)*MAXARGLENGTH);
      memset(arg1, 0, sizeof(char)*MAXARGLENGTH);
      memset(arg2, 0, sizeof(char)*MAXARGLENGTH);
      //printf(1,"s: %s\n",s );// ! for debug
      ss = s;
      //printf(1,"ss: %s\n",ss );// ! for debug
      //printf(1,"ss pointer: %d\n",ss );// ! for debug
      while(s < es && !strchr(whitespace, *s)){
        //printf(1,"+ss pointer: %d\n",ss );// ! for debug
        s++;
      }
      //printf(1,"rss pointer: %d\n",ss );// ! for debug
      *s = 0;
      strcpy(arg1, ss);
      ss = ++s;
      while(s < es && !strchr(whitespace, *s)){
        //printf(1,"+ss pointer: %d\n",ss );// ! for debug
        s++;
      }
      *s = 0;
      //printf(1,"ss: %s\n",ss );// ! for debug
      strcpy(arg2, ss);
      //printf(1,"arg: %s\n",arg );// ! for debug
      pmgrcmd = alloc_cmdarg2(arg1,arg2);
      pmgrcmd->type=EXEC;
    ;
  }
  printf(1,"pmgrcmd=%d\n",pmgrcmd);
  return pmgrcmd;
}

// Execute pmanger cmd.
void
runPmgrCmd(struct pmgrcmd *pmgrcmd)
{
  // struct listcmd *lcmd;
  struct cmdarg1 *kcmd;
  struct cmdarg2 *ecmd;
  // struct memlimcmd *mlcmd;

  if(pmgrcmd == 0)
    exit();
  //printf(1, "pmgrcmd->type: %d\n",pmgrcmd->type); // ! for debug
  switch(pmgrcmd->type){
  default:
    //panic("runcmd");
    break;

  case LIST:
    printf(1,"list\n");
    // lcmd = (struct listcmd*)cmd;
    // if(fork1() == 0)
    //   runcmd(lcmd->left);
    // wait();
    // runcmd(lcmd->right);
    break;

  case KILL:
    kcmd = (struct cmdarg1 *) pmgrcmd;
    printf(1,"kill type: %d\n",kcmd->type);
    printf(1,"kill pid: %d\n",kcmd->arg);
    printf(1,"kill pid: %s\n",kcmd->arg);
    break;

  case EXEC:
    printf(1,"exec\n");
    ecmd = (struct cmdarg2 *) pmgrcmd;
    printf(1,"exec type: %d\n",ecmd->type);
    printf(1,"exec pid: %s\n",ecmd->arg1);
    printf(1,"exec pid: %s\n",ecmd->arg2);
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