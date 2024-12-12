#include "compiler.h"
#include "scanner.h"
#include <stdio.h>

void compile(const char *source) {
  initScanner(source);
  int line = -1;
  for (;;) {
    Token token = scanToken();
    printToken(token);
    if (token.type == TOKEN_EOF) {
      break;
    }
  }
}
