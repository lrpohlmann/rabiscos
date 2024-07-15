#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

typedef struct {
  int x;
  int y;
} PosicaoCursor;

typedef struct {
  int linhas;
  int colunas;
} Terminal;

typedef struct {
  int y_offset;
} JanelaTexto;

typedef struct {
  char *nome;
  int tamanho_nome;
} NomeArquivo;

typedef struct {
  NomeArquivo *arquivos;
  int quantidade_arquivos;
  PosicaoCursor cursor;
  JanelaTexto jt;
} ContextoMenu;

#define INIT_CONTEXTOMENU                                                      \
  {                                                                            \
    .arquivos = NULL, .quantidade_arquivos = 0, .cursor = {.x = 1, .y = 1},    \
    .jt = {                                                                    \
      .y_offset = 0                                                            \
    }                                                                          \
  }

void crash(char *e) {
  int ok = 0;
  ok = write(STDOUT_FILENO, "\x1b[2J", 4);
  ok = write(STDOUT_FILENO, "\x1b[H", 3);
  perror(e);
  exit(1);
}

struct termios T;

void T_Reset() {
  int ok = tcsetattr(STDIN_FILENO, TCSAFLUSH, &T);
  if (ok == -1) {
    crash("tcsetattr");
  }
  ok = write(STDOUT_FILENO, "\x1b[2J", 4);
  ok = write(STDOUT_FILENO, "\x1b[H", 3);
}

typedef struct {
  int tamanho;
  char *buf;
} OutputBuffer;

void EscreverBufferTela(OutputBuffer *b, char *texto, int tamanho) {
  char *novo_buf = (char *)realloc(b->buf, b->tamanho + tamanho);
  memcpy(&novo_buf[b->tamanho], texto, tamanho);

  b->buf = novo_buf;
  b->tamanho += tamanho;
}

void AtualizarTela(ContextoMenu *m, Terminal *terminal) {
  write(STDOUT_FILENO, "\x1b[H", 3);

  OutputBuffer b = {.tamanho = 0, .buf = NULL};

  for (int i = 0; i < terminal->linhas; i++) {
    if (i < m->quantidade_arquivos) {
      EscreverBufferTela(&b, m->arquivos[i].nome, m->arquivos[i].tamanho_nome);
    } else {
      EscreverBufferTela(&b, "~", 1);
    }

    EscreverBufferTela(&b, "\x1b[K", 3);
    if (i < terminal->linhas - 1) {
      EscreverBufferTela(&b, "\r\n", 2);
    }
  }

  char posicionar_cursor[32];
  snprintf(posicionar_cursor, 32, "\x1b[%d;%dH", m->cursor.y, m->cursor.x);
  EscreverBufferTela(&b, posicionar_cursor, strlen(posicionar_cursor));

  write(STDOUT_FILENO, b.buf, b.tamanho);

  free(b.buf);
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

void Menu(Terminal *terminal, char **buf_nome_arquivo) {
  ContextoMenu m = INIT_CONTEXTOMENU;
  glob_t files;
  glob("*.*", 0, NULL, &files);
  if (files.gl_pathc == 0) {
    printf("Pasta vazia.\r\n");
    exit(0);
  }

  m.arquivos = (NomeArquivo *)malloc(sizeof(NomeArquivo) * files.gl_pathc);
  m.quantidade_arquivos = files.gl_pathc;
  for (int i = 0; i < files.gl_pathc; i++) {
    m.arquivos[i].tamanho_nome = strlen(files.gl_pathv[i]);
    m.arquivos[i].nome = malloc(m.arquivos[i].tamanho_nome + 1);
    strncpy(m.arquivos[i].nome, files.gl_pathv[i],
            m.arquivos[i].tamanho_nome + 1);
  }
  globfree(&files);

  while (1) {
    AtualizarTela(&m, terminal);
    char caractere_lido = '\0';
    int ok = read(STDIN_FILENO, &caractere_lido, 1);
    if (ok == -1) {
      crash("read");
    }

    if (caractere_lido == 'q') {
      exit(0);
    } else if (caractere_lido == '\x1b') {
      char proximos_caracteres[2];
      read(STDIN_FILENO, &proximos_caracteres[0], 1);
      read(STDIN_FILENO, &proximos_caracteres[1], 1);
      if (proximos_caracteres[0] == '[') {
        switch (proximos_caracteres[1]) {
        case 'A':
          if (m.cursor.y > 1) {
            m.cursor.y--;
          }
          break;

        case 'B':
          if (m.cursor.y < m.quantidade_arquivos) {
            m.cursor.y++;
          }
          break;
        }
      }
    } else if (caractere_lido == '\r') {
      int escolhido = m.cursor.y - 1;
      char *buf = (char *)malloc(m.arquivos[escolhido].tamanho_nome);
      *buf_nome_arquivo = buf;
      strncpy(*buf_nome_arquivo, m.arquivos[escolhido].nome,
              m.arquivos[escolhido].tamanho_nome + 1);

      for (int i = 0; i < m.quantidade_arquivos; i++) {
        free(m.arquivos[i].nome);
      }
      free(m.arquivos);
      break;
    }
  }
}

int main() {
  Terminal terminal = {.linhas = 0, .colunas = 0};
  T_Setup(&terminal);

  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  char *buf_nome_arquivo = NULL;
  Menu(&terminal, &buf_nome_arquivo);
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  write(STDOUT_FILENO, buf_nome_arquivo, strlen(buf_nome_arquivo));
  sleep(2);
  return 0;
}
