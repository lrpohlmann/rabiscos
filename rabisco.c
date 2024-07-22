#include <asm-generic/ioctls.h>
#include <assert.h>
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
  int inicio;
  int fim;
} AreaConteudo;

typedef struct {
  int numero_caracteres;
  char *caracteres;
} Linha;

typedef struct {
  int numero_linhas;
  Linha *linhas;
  int y_offset;
  int x_offset;
} Texto;

typedef struct {
  PosicaoCursor cursor;
  Texto txt;
  AreaConteudo area_conteudo;
} ContextoEditor;

typedef struct {
  int tamanho;
  char *buf;
} OutputBuffer;

void EscreverNoOutputBuffer(OutputBuffer *b, char *conteudo, int tamanho) {
  char *novo_b = (char *)realloc(b->buf, b->tamanho + tamanho);
  if (novo_b == NULL) {
    crash("realloc");
  }
  memcpy((void *)&novo_b[b->tamanho], conteudo, tamanho);
  b->buf = novo_b;
  b->tamanho += tamanho;
}

void InitContextoEditor(ContextoEditor *ctx, Terminal *terminal) {
  ctx->area_conteudo.inicio = 2;
  ctx->area_conteudo.fim = terminal->linhas - 1;
  ctx->cursor.x = 1;
  ctx->cursor.y = ctx->area_conteudo.inicio;
  ctx->txt.numero_linhas = 0;
  ctx->txt.y_offset = 0;
  ctx->txt.x_offset = 0;
  ctx->txt.linhas = (Linha *)malloc(sizeof(Linha));
}

void AtualizarTela(ContextoEditor *ctx, Terminal *terminal,
                   char *nome_arquivo) {
  int ok = 0;

  OutputBuffer b = {.buf = NULL, .tamanho = 0};

  EscreverNoOutputBuffer(&b, "\x1b[H", 3);

  int indice_linha_terminal = 1;
  int indice_y_texto = ctx->txt.y_offset;
  while (indice_linha_terminal <= terminal->linhas) {
    if (indice_linha_terminal == 1) {
      EscreverNoOutputBuffer(&b, "\x1b[47m\x1b[1;30m",
                             strlen("\x1b[47m\x1b[1;30m"));

      EscreverNoOutputBuffer(&b, nome_arquivo, strlen(nome_arquivo));

      EscreverNoOutputBuffer(&b, "\x1b[0m", strlen("\x1b[0m"));
    } else if (indice_linha_terminal == terminal->linhas) {
      char rodape[] = "\x1b[47m\x1b[1;30mq - Sair | Setas - Navegação\x1b[0m";
      EscreverNoOutputBuffer(&b, rodape, strlen(rodape));
    } else {
      if (indice_y_texto < ctx->txt.numero_linhas) {
        Linha *l = &ctx->txt.linhas[indice_y_texto];
        if (l->numero_caracteres - ctx->txt.x_offset > 0) {
          int bp = l->numero_caracteres - ctx->txt.x_offset;
          if (bp > terminal->colunas) {
            bp = terminal->colunas;
          }
          EscreverNoOutputBuffer(&b, &l->caracteres[ctx->txt.x_offset], bp);
        }

        indice_y_texto++;

      } else {
        EscreverNoOutputBuffer(&b, "~", 1);
      }
    }

    EscreverNoOutputBuffer(&b, "\x1b[K", 3);
    if (indice_linha_terminal <= ctx->area_conteudo.fim) {
      EscreverNoOutputBuffer(&b, "\r\n", 2);
    }
    indice_linha_terminal++;
  }

  char recolocar_cursor[32];
  ok = snprintf(recolocar_cursor, sizeof(recolocar_cursor), "\x1b[%d;%dH",
                ctx->cursor.y, ctx->cursor.x);
  if (ok == -1) {
    crash("snprintf");
  }

  EscreverNoOutputBuffer(&b, recolocar_cursor, strlen(recolocar_cursor));

  ok = write(STDOUT_FILENO, b.buf, b.tamanho);
  if (ok == -1) {
    crash("write");
  }

  free(b.buf);
}

void AbrirArquivo(char *caminho, Texto *txt) {
  FILE *f = fopen(caminho, "r");
  if (f == NULL) {
    crash("fopen");
  }

  txt->numero_linhas = 0;
  txt->y_offset = 0;

  while (1) {
    char *linha = NULL;
    size_t n = 0;
    ssize_t caracteres_lidos = getline(&linha, &n, f);
    if (caracteres_lidos != -1) {
      while (caracteres_lidos > 0 && (linha[caracteres_lidos - 1] == '\n' ||
                                      linha[caracteres_lidos - 1] == '\r')) {
        caracteres_lidos--;
      }
      int l = txt->numero_linhas;
      txt->linhas = (Linha *)realloc(txt->linhas, sizeof(Linha) * (l + 1));

      txt->linhas[l].caracteres = (char *)malloc(n + 1);
      if (txt->linhas[l].caracteres == NULL) {
        crash("malloc");
      }
      txt->linhas[l].caracteres = memcpy(txt->linhas[l].caracteres, linha, n);
      txt->linhas[l].caracteres[n] = '\0';
      txt->linhas[l].numero_caracteres = caracteres_lidos;

      txt->numero_linhas++;
    } else {
      free(linha);
      fclose(f);
      return;
    }
  }
}

void Editor(char *arquivo, Terminal *terminal) {
  ContextoEditor ta;
  InitContextoEditor(&ta, terminal);

  AbrirArquivo(arquivo, &ta.txt);
  while (1) {
    AtualizarTela(&ta, terminal, arquivo);
    char caractere_recebido = '\0';
    int bytes_lidos = read(STDIN_FILENO, &caractere_recebido, 1);
    if (bytes_lidos == -1) {
      crash("read");
    }

    if (caractere_recebido == 'q') {
      int ok = 0;
      ok = write(STDOUT_FILENO, "\x1b[2J", 4);
      if (ok == -1) {
        crash("write");
      }
      ok = write(STDOUT_FILENO, "\x1b[H", 3);
      if (ok == -1) {
        crash("write");
      }
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
        int indice_linha_cursor =
            ta.txt.y_offset + ta.cursor.y - ta.area_conteudo.inicio;

        int numero_de_caracteres_menos_offset_coluna =
            ta.txt.linhas[indice_linha_cursor].numero_caracteres -
            ta.txt.x_offset;

        assert(indice_linha_cursor >= 0 &&
               indice_linha_cursor <= ta.txt.numero_linhas);
        assert(ta.cursor.y > 0 && ta.cursor.y <= terminal->linhas);
        assert(ta.cursor.x > 0 && ta.cursor.x <= terminal->colunas);

        switch (proximos_caracteres[1]) {
        case 'A':
          if (ta.cursor.y > ta.area_conteudo.inicio) {
            ta.cursor.y--;
          } else if (ta.txt.y_offset > 0) {
            ta.txt.y_offset--;
          }
          break;

        case 'B':
          if (ta.cursor.y <= ta.area_conteudo.fim &&
              (indice_linha_cursor < ta.txt.numero_linhas)) {
            ta.cursor.y++;
            break;
          }

          if ((ta.txt.y_offset < ta.txt.numero_linhas) &&
              (terminal->linhas + ta.txt.y_offset < ta.txt.numero_linhas)) {
            ta.txt.y_offset++;
            break;
          }
          break;

        case 'C':
          if ((ta.cursor.x < numero_de_caracteres_menos_offset_coluna + 1) &&
              (ta.cursor.x < terminal->colunas)) {
            ta.cursor.x++;
            break;
          }

          if (ta.cursor.x >= numero_de_caracteres_menos_offset_coluna + 1 &&
              (indice_linha_cursor < ta.txt.numero_linhas)) {
            ta.cursor.x = 1;
            ta.txt.x_offset = 0;
            if (ta.cursor.y == ta.area_conteudo.fim) {
              ta.txt.y_offset++;
            } else {
              ta.cursor.y++;
            }
            break;
          }

          if ((numero_de_caracteres_menos_offset_coluna + 1 > ta.cursor.x) &&
              (ta.cursor.x == terminal->colunas)) {
            ta.txt.x_offset++;
            break;
          }
          break;

        case 'D':
          if (ta.cursor.x > 1) {
            ta.cursor.x--;
            break;
          }

          if (ta.cursor.x == 1 && ta.txt.x_offset > 0) {
            ta.txt.x_offset--;
            break;
          }

          if (ta.cursor.x == 1 && ta.txt.x_offset == 0 &&
              indice_linha_cursor > 0) {
            indice_linha_cursor--;
            if (ta.cursor.y == ta.area_conteudo.inicio) {
              ta.txt.y_offset--;
            } else {
              ta.cursor.y--;
            }

            int diferenca_numero_caracteres_vs_tela =
                ta.txt.linhas[indice_linha_cursor].numero_caracteres + 1 -
                terminal->colunas;
            if (diferenca_numero_caracteres_vs_tela > 0) {
              ta.txt.x_offset = diferenca_numero_caracteres_vs_tela;
              ta.cursor.x =
                  ta.txt.linhas[indice_linha_cursor].numero_caracteres -
                  diferenca_numero_caracteres_vs_tela + 1;
              break;
            };

            ta.cursor.x =
                ta.txt.linhas[indice_linha_cursor].numero_caracteres + 1;
            break;
          }
          break;
        }
      }
    }
  }
}

/*** main ***/

int main(int argc, char *argv[]) {
  Terminal terminal = {.linhas = 0, .colunas = 0};
  T_Setup(&terminal);

  char *buf_nome_arquivo;
  if (argc < 2) {
    Menu(&terminal, &buf_nome_arquivo);
    Editor(buf_nome_arquivo, &terminal);
  } else {
    Editor(argv[1], &terminal);
  }

  return 0;
}
