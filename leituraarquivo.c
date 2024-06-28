#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  char *b;
  int tamanho;
} BufferTela;

void EscreverBufferTela(BufferTela *bt, char *dados, int tamanho_dados) {
  char *memoria_reallocada =
      (char *)realloc(bt->b, bt->tamanho + tamanho_dados);

  memcpy(&memoria_reallocada[bt->tamanho], dados, tamanho_dados);
  bt->b = memoria_reallocada;
  bt->tamanho += tamanho_dados;
}

void FreeBufferTela(BufferTela *bt) { free(bt->b); }

void AtualizarTela(BufferTela *bt) {
  int bytes_escritos = write(STDOUT_FILENO, bt->b, bt->tamanho);
  if (bytes_escritos == -1) {
    printf("Erro - AtualizarTela\n");
    exit(1);
  }
}

typedef struct {
  int num_letras;
  char *letras;
} Linha;

typedef struct {
  int num_linhas;
  Linha *linhas;
} Texto;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    exit(0);
  }

  FILE *f;
  f = fopen(argv[1], "r");

  Texto l;
  l.num_linhas = 0;
  l.linhas = (Linha *)malloc(sizeof(Linha));

  while (1) {
    char *line = NULL;
    size_t n = 0;
    int num_c = 0;
    if ((num_c = getline(&line, &n, f)) == -1) {
      free(line);
      fclose(f);
      break;
    }

    l.num_linhas++;
    l.linhas = (Linha *)realloc(l.linhas, sizeof(Linha) * l.num_linhas);
    l.linhas[l.num_linhas - 1].letras = (char *)malloc(n);
    memcpy(l.linhas[l.num_linhas - 1].letras, line, n);
    l.linhas[l.num_linhas - 1].num_letras = num_c;
  }

  BufferTela bt;
  bt.b = NULL;
  bt.tamanho = 0;

  for (int i = 0; i < l.num_linhas; i++) {
    EscreverBufferTela(&bt, l.linhas[i].letras, l.linhas[i].num_letras);
  }

  AtualizarTela(&bt);

  FreeBufferTela(&bt);

  for (int i = 0; i < l.num_linhas; i++) {
    free(l.linhas[i].letras);
  }
  free(l.linhas);
  return 0;
}
