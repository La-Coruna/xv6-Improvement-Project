#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
  if(argc <= 1){
    printf(1,"Error: Insufficient arguments.\nPlease input 1~2\n");
    exit();
  }

  if(strcmp(argv[1], "1") == 0){
	 	printf(1,"1\n");
	}	
  if(strcmp(argv[1], "2") == 0){
	 	printf(1,"2\n");
	}	

  return 0;
}