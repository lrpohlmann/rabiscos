#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

void crash(char *e) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(e);
  exit(1);
}

typedef struct {
  int x;
  int y;
} PosicaoCursor;

typedef struct {
  int numero_caracteres;
  char *caracteres;
} Linha;

typedef struct {
  int numero_linhas;
  Linha *linhas;
} Texto;

typedef struct {
  int tamanho;
  char *buf;
} OutputBuffer;

void AtualizarTela(Texto *txt, PosicaoCursor *cursor) {
  int ok = write(STDOUT_FILENO, "\x1b[2J", 4);
  if (ok == -1) {
    crash("write");
  }

  ok = write(STDOUT_FILENO, "\x1b[H", 3);
  if (ok == -1) {
    crash("write");
  }

  OutputBuffer b;
  b.tamanho = 0;
  b.buf = NULL;

  for (int i = 0; i < txt->numero_linhas; i++) {
    Linha *l = &txt->linhas[i];
    b.buf = (char *)realloc(b.buf, b.tamanho + l->numero_caracteres);
    memcpy(&b.buf[b.tamanho], l->caracteres, l->numero_caracteres);
    b.tamanho += l->numero_caracteres;
  }

  ok = write(STDOUT_FILENO, b.buf, b.tamanho);
  if (ok == -1) {
    crash("write");
  }

  char recolocar_cursor[7];
  snprintf(recolocar_cursor, sizeof(recolocar_cursor), "\x1b[%d;%dH", cursor->y,
           cursor->x);
  write(STDOUT_FILENO, recolocar_cursor, sizeof(recolocar_cursor));

  free(b.buf);
}

void AbrirArquivo(char *caminho, Texto *txt) {
  FILE *f = fopen(caminho, "r");
  if (f == NULL) {
    crash("fopen");
  }

  txt->numero_linhas = 0;

  while (1) {
    char *linha = NULL;
    size_t n = 0;
    ssize_t caracteres_lidos = getline(&linha, &n, f);
    if (caracteres_lidos != -1) {
      int l = txt->numero_linhas;
      txt->linhas = (Linha *)realloc(txt->linhas, sizeof(Linha) * (l + 1));

      txt->linhas[l].caracteres = (char *)malloc(n);
      txt->linhas[l].caracteres = memcpy(txt->linhas[l].caracteres, linha, n);
      txt->linhas[l].numero_caracteres = caracteres_lidos + 1;
      txt->linhas[l].caracteres[txt->linhas[l].numero_caracteres] = '\0';

      txt->numero_linhas++;
    } else {
      free(linha);
      fclose(f);
      return;
    }
  }
}

/*** Terminal ***/

struct termios T;

void T_Reset() {
  int ok = tcsetattr(STDIN_FILENO, TCSAFLUSH, &T);
  if (ok == -1) {
    crash("tcsetattr");
  }
}

void T_Setup() {
  int ok = tcgetattr(STDIN_FILENO, &T);
  if (ok == -1) {
    crash("tcsetattr");
  }

  atexit(T_Reset);
  struct termios T_config = T;
  T_config.c_lflag &= ~(ICANON | ECHO);

  ok = tcsetattr(STDIN_FILENO, TCSAFLUSH, &T_config);
  if (ok == -1) {
    crash("tcsetattr");
  }
}

/*** main ***/

int main(int argc, char *argv[]) {
  if (argc < 2) {
    exit(0);
  }
  T_Setup();

  PosicaoCursor cursor = {.x = 2, .y = 3};

  Texto txt = {.numero_linhas = 0, .linhas = (Linha *)malloc(sizeof(Linha))};

  AbrirArquivo(argv[1], &txt);
  AtualizarTela(&txt, &cursor);
  while (1) {
    char caractere_recebido = '\0';
    int bytes_lidos = read(STDIN_FILENO, &caractere_recebido, 1);
    if (bytes_lidos == -1) {
      crash("read");
    }

    if (caractere_recebido == 'q') {
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
    } else if (caractere_recebido == '\x1b') {
      char proximos_caracteres[3];
      int bytes_lidos = read(STDIN_FILENO, &proximos_caracteres[0], 1);
      if (bytes_lidos == -1) {
        crash("read");
      }

      bytes_lidos = read(STDIN_FILENO, &proximos_caracteres[1], 1);
      if (bytes_lidos == -1) {
        crash("read");
      }

      if (proximos_caracteres[0] == '[') {
        switch (proximos_caracteres[1]) {
        case 'A':
          cursor.y--;
          break;

        case 'B':
          cursor.y++;
          break;

        case 'C':
          cursor.x++;
          break;

        case 'D':
          cursor.x--;
          break;
        }
      }
    } else {
    }

    AtualizarTela(&txt, &cursor);
  }
  return 0;
}
