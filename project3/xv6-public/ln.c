#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 4){ //TODO: 4로 변경
    printf(2, "Usage: ln [-h or -s] old new\n");
    exit();
  }
  if( !strcmp(argv[1],"-h") ){
    if (link(argv[2], argv[3]) < 0)
      printf(2, "link (hard) %s %s: failed\n", argv[1], argv[2]);
  }
  else if( !strcmp(argv[1],"-s") ){
    if (link_symbolic(argv[2], argv[3]) < 0)
      printf(2, "link (symbolic) %s %s: failed\n", argv[1], argv[2]);
  }
  else
    printf(2, "undefined option. Usage: ln [-h or -s] old new\n");
  exit();
}
