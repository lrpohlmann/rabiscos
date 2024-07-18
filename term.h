#pragma once

typedef struct {
  int linhas;
  int colunas;
} Terminal;

void T_Reset();

void T_Setup(Terminal *terminal);
