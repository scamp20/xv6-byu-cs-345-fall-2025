#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  if(argc > 1){
    printf("Usage: pingpong\n");
    exit(0);
  }

  int p2c[2];
  int c2p[2];
  pipe(p2c);
  pipe(c2p);

  if (fork() == 0) {
    // child
    char buf[4];

    read(p2c[0], buf, 4);
    close(p2c[0]);
    close(p2c[1]);

    printf("%d: received ping\n", getpid());

    write(c2p[1], "pong", 4);
    close(c2p[0]);
    close(c2p[1]);
  } else {
    // parent
    char buf[4];
    write(p2c[1], "ping", 4);
    close(p2c[0]);
    close(p2c[1]);

    read(c2p[0], buf, 4);
    close(c2p[0]);
    close(c2p[1]);
    
    printf("%d: received pong\n", getpid());
    wait(0);
  }

  exit(0);
}
