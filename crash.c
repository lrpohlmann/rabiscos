#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void crash(char *e) {
  int ok = 0;
  ok = write(STDOUT_FILENO, "\x1b[2J", 4);
  ok = write(STDOUT_FILENO, "\x1b[H", 3);
  perror(e);
  exit(1);
}
