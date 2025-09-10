#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  if(argc != 2){
    printf("Usage: sleep <ticks>\n");
    exit(0);
  }

  int count = atoi(argv[1]);
  sleep(count);
  exit(0);
}
