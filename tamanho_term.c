#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

void crash(char *e) {
  perror(e);
  exit(1);
}

typedef struct {
  int altura;
  int largura;
} T_Contexto;

#define INIT_T_CONTEXTO                                                        \
  { 0, 0 }

void T_Tamanho(int *altura, int *largura) {
  struct winsize ws;

  int ok = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  if (ok == -1) {
    crash("ioctl");
  }

  *altura = ws.ws_row;
  *largura = ws.ws_col;
}

struct termios T;

void T_Reset() {
  int ok = tcsetattr(STDIN_FILENO, TCSAFLUSH, &T);
  if (ok == -1) {
    exit(1);
  }
}

void T_Setup() {
  int ok = tcgetattr(STDIN_FILENO, &T);
  atexit(T_Reset);
  if (ok == -1) {
    exit(1);
  }
  struct termios T_config = T;
  T_config.c_lflag &= ~(ICANON);

  ok = tcsetattr(STDIN_FILENO, TCSAFLUSH, &T_config);
}

char TEXTOFEIKI[5][5];
void SetTextoFeiki() {
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      TEXTOFEIKI[i][j] = 'a';
    }
  }
}

void ImprimirTextoFeiki(T_Contexto *T_Ctx) {
  int altura_impressao = 5;
  int largura_impressao = 5;
  if (altura_impressao > T_Ctx->altura) {
    altura_impressao = T_Ctx->altura;
  }

  if (largura_impressao > T_Ctx->largura) {
    largura_impressao = T_Ctx->largura;
  }

  for (int a = 0; a < altura_impressao; a++) {
    for (int l = 0; l < largura_impressao; l++) {
      printf("%c", TEXTOFEIKI[a][l]);
    }
    printf("\n");
  }
}

int main() {
  SetTextoFeiki();

  T_Setup();

  T_Contexto T_Ctx = INIT_T_CONTEXTO;
  // T_Tamanho(&T_Ctx.altura, &T_Ctx.largura);
  T_Ctx.altura = 3;
  T_Ctx.largura = 4;

  printf("A: %d, L: %d\n", T_Ctx.altura, T_Ctx.largura);
  ImprimirTextoFeiki(&T_Ctx);
  while (1) {
    char caractere_recebido = '\0';
    int bytes_lidos = read(STDIN_FILENO, &caractere_recebido, 1);
    if (bytes_lidos == -1) {
      crash("read");
    }

    if (caractere_recebido == 'q') {
      exit(0);
    }
  }
  return 0;
}
