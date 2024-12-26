#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
#include "object.h"
#include "scanner.h"

#define UINT8_COUNT (UINT8_MAX + 1)

typedef struct {
  Token previous;
  Token current;
  bool isOk;
  bool isInPanic;
} Parser;

typedef struct {
  Token name;
  int depth;
} Local;

ObjFunction* compile(const char *source);

#endif
