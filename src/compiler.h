#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
#include "scanner.h"

typedef struct {
  Token previous;
  Token current;
  bool isOk;
  bool isInPanic;
} Parser;

bool compile(const char *source, Chunk *chunk);

#endif
