#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "hash_table.h"
#include "value.h"
#include <stdint.h>

#define STACK_MAX 256

typedef struct {
  Chunk *chunk;
  uint8_t *ip;
  Value stack[STACK_MAX];
  Value *stackTop;
  HashTable strings;
  Obj *objectHeap;
} VM;



typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpritationResult;

extern VM vm;

void initVm();
void freeVm();

InterpritationResult interpret(char *source);

void push(Value value);
Value pop();

#endif
