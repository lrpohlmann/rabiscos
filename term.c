#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "crash.h"
#include "term.h"

struct termios T;

void T_Reset() {
  int ok = tcsetattr(STDIN_FILENO, TCSAFLUSH, &T);
  if (ok == -1) {
    crash("tcsetattr");
  }
}

void T_Setup(Terminal *terminal) {
  int ok = tcgetattr(STDIN_FILENO, &T);
  if (ok == -1) {
    crash("tcsetattr");
  }

  atexit(T_Reset);
  struct termios T_config = T;
  T_config.c_lflag &= ~(ICANON | ECHO);
  T_config.c_oflag &= ~(OPOST);
  T_config.c_iflag &= ~(ICRNL);

  ok = tcsetattr(STDIN_FILENO, TCSAFLUSH, &T_config);
  if (ok == -1) {
    crash("tcsetattr");
  }

  struct winsize tamanho;
  ok = ioctl(STDOUT_FILENO, TIOCGWINSZ, &tamanho);
  if (ok == -1) {
    crash("ioctl");
  }

  terminal->colunas = tamanho.ws_col;
  terminal->linhas = tamanho.ws_row;
}
