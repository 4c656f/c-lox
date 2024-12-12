#include "compiler.h"
#include "scanner.h"
#include "vm.h"
#include <stdio.h>

InterpritationResult compile(const char *source) {
  initScanner(source);
  int line = -1;
  InterpritationResult result = INTERPRET_OK;
  for (;;) {
    Token token = scanToken();
    printToken(token);
    if (token.type == TOKEN_ERROR) {
      result = INTERPRET_COMPILE_ERROR;
    }
    if (token.type == TOKEN_EOF) {
      break;
    }
  }
  return result;
}
