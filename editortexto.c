#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>


struct termios T;

void TerminalReset(){
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &T);
}

void TerminalSetup(){
	atexit(TerminalReset);
	tcgetattr(STDIN_FILENO, &T);

	struct termios term_config = T;

	term_config.c_lflag &= ~(ICANON | ECHO);

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_config);
}

typedef struct {
	int tamanho;
	char *buf;
} BufferEscritaNoTerminal;

void AbrirArquivo(const char *caminho, BufferEscritaNoTerminal *bt){
	FILE *f = fopen(caminho, "r");
	char *txt = NULL;
	size_t n = 0;
	ssize_t lido = getline(&txt, &n, f);
	
	if (lido == -1){
		return;	
	}
	
	bt->buf = (char *)realloc(bt->buf, sizeof(char)*(bt->tamanho + lido));
	memcpy(bt->buf, txt, lido);
	bt->tamanho += lido;
	free(txt);
	fclose(f);
}

void EscreverNoTerminal(BufferEscritaNoTerminal *bt){
	write(STDOUT_FILENO, bt->buf, bt->tamanho);
}

int main(int argc, char *argv[]){
	TerminalSetup();

	BufferEscritaNoTerminal bt;
	bt.buf = (char *)malloc(sizeof(char));
	bt.tamanho = 1;
	
	if (argc == 2) {
		AbrirArquivo("a.txt", &bt);
	}	
	
	EscreverNoTerminal(&bt);
	
	bool editor_deve_rodar = true;
	while (editor_deve_rodar) {
		char input_buf = '\0';
		if (read(STDIN_FILENO, &input_buf, 1) != -1){;
			switch(input_buf){
				case 'q':
					printf("Saíndo.\n");
					editor_deve_rodar = false;
					break;
			}	
		}
	}	
	return 0;
}
