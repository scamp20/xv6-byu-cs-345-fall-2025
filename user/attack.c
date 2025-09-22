#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  char *end = sbrk(PGSIZE*32);
  char *str = "very very secret pw is:   ";
  for (int i = 0; i < 32; i++) {
    if (memcmp(end + (i * PGSIZE) + 8, str, 24) == 0) {
      write(2, end + (i * PGSIZE) + 8 + 24, 8);
      exit(0);
    }
  }

  exit(1);
}
