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
  int numero_caracteres;
  char *caracteres;
} Linha;

typedef struct {
  int numero_linhas;
  Linha *linhas;
  int linha_no_topo_da_tela;
  int coluna_no_inicio_da_tela;
} Texto;

typedef struct {
  PosicaoCursor cursor;
  Texto txt;
} TextoAberto;

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

void AtualizarTela(Texto *txt, PosicaoCursor *cursor, Terminal *terminal) {
  int ok = 0;

  OutputBuffer b = {.buf = NULL, .tamanho = 0};

  EscreverNoOutputBuffer(&b, "\x1b[H", 3);

  for (int i = 0; i < terminal->linhas; i++) {
    int j = i + txt->linha_no_topo_da_tela;
    if (j < txt->numero_linhas) {
      Linha *l = &txt->linhas[j];
      if (l->numero_caracteres - txt->coluna_no_inicio_da_tela > 0) {
        int bp = l->numero_caracteres - txt->coluna_no_inicio_da_tela;
        if (bp > terminal->colunas) {
          bp = terminal->colunas;
        }
        EscreverNoOutputBuffer(
            &b, &l->caracteres[txt->coluna_no_inicio_da_tela], bp);
      }
    } else {
      EscreverNoOutputBuffer(&b, "~", 1);
    }

    EscreverNoOutputBuffer(&b, "\x1b[K", 3);
    if (i < terminal->linhas - 1) {
      EscreverNoOutputBuffer(&b, "\r\n", 2);
    }
  }

  char recolocar_cursor[32];
  ok = snprintf(recolocar_cursor, sizeof(recolocar_cursor), "\x1b[%d;%dH",
                cursor->y, cursor->x);
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
  txt->linha_no_topo_da_tela = 0;

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
  TextoAberto ta = {.cursor = {.x = 1, .y = 1},
                    .txt = {.numero_linhas = 0,
                            .linha_no_topo_da_tela = 0,
                            .coluna_no_inicio_da_tela = 0,
                            .linhas = (Linha *)malloc(sizeof(Linha))}};

  AbrirArquivo(arquivo, &ta.txt);
  while (1) {
    AtualizarTela(&ta.txt, &ta.cursor, terminal);
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
            ta.txt.linha_no_topo_da_tela + ta.cursor.y - 1;

        assert(indice_linha_cursor >= 0 &&
               indice_linha_cursor <= ta.txt.numero_linhas);

        Linha *linha_texto_onde_cursor_esta =
            &ta.txt.linhas[indice_linha_cursor];
        int numero_de_caracteres_menos_offset_coluna =
            linha_texto_onde_cursor_esta->numero_caracteres -
            ta.txt.coluna_no_inicio_da_tela;

        assert(ta.cursor.y > 0 && ta.cursor.y <= terminal->linhas);
        assert(ta.cursor.x > 0 && ta.cursor.x <= terminal->colunas);

        switch (proximos_caracteres[1]) {
        case 'A':
          if (ta.cursor.y > 1) {
            ta.cursor.y--;
          } else if (ta.txt.linha_no_topo_da_tela > 0) {
            ta.txt.linha_no_topo_da_tela--;
          }
          break;

        case 'B':
          if (ta.cursor.y < terminal->linhas &&
              (indice_linha_cursor < ta.txt.numero_linhas)) {
            ta.cursor.y++;
            break;
          }

          if ((ta.txt.linha_no_topo_da_tela < ta.txt.numero_linhas) &&
              (terminal->linhas + ta.txt.linha_no_topo_da_tela <
               ta.txt.numero_linhas)) {
            ta.txt.linha_no_topo_da_tela++;
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
            if (ta.cursor.y == terminal->linhas) {
              ta.txt.linha_no_topo_da_tela++;
            } else {
              ta.cursor.y++;
            }
            break;
          }

          if ((numero_de_caracteres_menos_offset_coluna + 1 > ta.cursor.x) &&
              (ta.cursor.x == terminal->colunas)) {
            ta.txt.coluna_no_inicio_da_tela++;
            break;
          }
          break;

        case 'D':
          if (ta.cursor.x > 1) {
            ta.cursor.x--;
            break;
          }

          if (ta.cursor.x == 1 && ta.txt.coluna_no_inicio_da_tela > 0) {
            ta.txt.coluna_no_inicio_da_tela--;
            break;
          }

          if (ta.cursor.x == 1 && ta.txt.coluna_no_inicio_da_tela == 0 &&
              indice_linha_cursor > 0) {
            indice_linha_cursor--;
            if (ta.cursor.y == 1) {
              ta.txt.linha_no_topo_da_tela--;
            } else {
              ta.cursor.y--;
            }

            int diferenca_numero_caracteres_vs_tela =
                ta.txt.linhas[indice_linha_cursor].numero_caracteres + 1 -
                terminal->colunas;
            if (diferenca_numero_caracteres_vs_tela > 0) {
              ta.txt.coluna_no_inicio_da_tela =
                  diferenca_numero_caracteres_vs_tela;
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
