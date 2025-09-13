#include "kernel/types.h"
#include "user/user.h"

// xargs for xv6
int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(2, "Usage: xargs <command> [args...]\n");
    exit(1);
  }

  char buf[512];
  int n;

  // read stdin into buf
  while ((n = read(0, buf, sizeof(buf)-1)) > 0) {
    buf[n] = '\0';
    char *p = buf;
    char *line_start = p;

    // process line by line
    while (*p) {
      if (*p == '\n') {
        *p = '\0';

        // build args for exec
        char *args[32];
        int arg_index = 0;

        args[arg_index++] = argv[1];  // the command

        // copy any fixed args from command line (e.g., "hello")
        for (int i = 2; i < argc; i++) {
          args[arg_index++] = argv[i];
        }

        // append this stdin line
        args[arg_index++] = line_start;
        args[arg_index] = 0; // null terminate

        if (fork() == 0) {
          exec(argv[1], args);
          fprintf(2, "xargs: exec %s failed\n", argv[1]);
          exit(1);
        } else {
          wait(0);
        }

        line_start = p + 1; // move past newline
      }
      p++;
    }
  }

  if (n < 0) {
    fprintf(2, "xargs: read error\n");
    exit(1);
  }

  exit(0);
}
