#include <assert.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "crash.h"
#include "menu.h"
#include "term.h"

typedef struct {
  int x;
  int y;
} PosicaoCursor;

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

typedef struct {
  int tamanho;
  char *buf;
} OutputBuffer;

void Menu_EscreverBufferTela(OutputBuffer *b, char *texto, int tamanho) {
  char *novo_buf = (char *)realloc(b->buf, b->tamanho + tamanho);
  memcpy(&novo_buf[b->tamanho], texto, tamanho);

  b->buf = novo_buf;
  b->tamanho += tamanho;
}

void Menu_AtualizarTela(ContextoMenu *m, Terminal *terminal) {
  OutputBuffer b = {.tamanho = 0, .buf = NULL};

  Menu_EscreverBufferTela(&b, "\x1b[H", 3);
  for (int i = 0; i < terminal->linhas; i++) {
    int j = i + m->jt.y_offset;
    if (j < m->quantidade_arquivos) {
      Menu_EscreverBufferTela(&b, m->arquivos[j].nome,
                              m->arquivos[j].tamanho_nome);
    } else {
      Menu_EscreverBufferTela(&b, "-", 1);
    }

    Menu_EscreverBufferTela(&b, "\x1b[K", 3);
    if (i < terminal->linhas - 1) {
      Menu_EscreverBufferTela(&b, "\r\n", 2);
    }
  }

  char posicionar_cursor[32];
  snprintf(posicionar_cursor, 32, "\x1b[%d;%dH", m->cursor.y, m->cursor.x);
  Menu_EscreverBufferTela(&b, posicionar_cursor, strlen(posicionar_cursor));

  write(STDOUT_FILENO, b.buf, b.tamanho);

  free(b.buf);
}

void Menu_ValidarNavegacao(ContextoMenu *m, Terminal *terminal) {
  assert(m->jt.y_offset >= 0);
  assert(m->cursor.y > 0);
  assert(m->cursor.y <= terminal->linhas);
  assert(m->jt.y_offset + m->cursor.y < m->quantidade_arquivos + 1);
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
    Menu_AtualizarTela(&m, terminal);
    char caractere_lido = '\0';
    int ok = read(STDIN_FILENO, &caractere_lido, 1);
    if (ok == -1) {
      crash("read");
    }

    if (caractere_lido == 'q') {
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
    } else if (caractere_lido == '\x1b') {
      char proximos_caracteres[2];
      read(STDIN_FILENO, &proximos_caracteres[0], 1);
      read(STDIN_FILENO, &proximos_caracteres[1], 1);
      if (proximos_caracteres[0] == '[') {
        Menu_ValidarNavegacao(&m, terminal);
        switch (proximos_caracteres[1]) {
        case 'A':
          if (m.cursor.y > 1) {
            m.cursor.y--;
          } else if ((m.cursor.y == 1) && (m.jt.y_offset > 0)) {
            m.jt.y_offset--;
          }
          break;

        case 'B':
          if (m.cursor.y + m.jt.y_offset < m.quantidade_arquivos) {
            if (m.cursor.y < terminal->linhas) {
              m.cursor.y++;
            } else {
              m.jt.y_offset++;
            }
          }
          break;
        }
      }
    } else if (caractere_lido == '\r') {
      int escolhido = m.jt.y_offset + m.cursor.y - 1;
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
