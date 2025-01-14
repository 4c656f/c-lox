#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "compiler.h"
#include "hash_table.h"
#include "object.h"
#include "value.h"
#include <stdint.h>

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
  ObjClosure *closure;
  // pointer to the current instruction inside function chunk object
  uint8_t *ip;
  // realtive pointer to the top of the stack of current CallFrame window
  Value *slots;
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frameCount;

  Value stack[STACK_MAX];
  Value *stackTop;

  HashTable stringsPool;
  HashTable globals;

  Obj *objectHeap;

  ObjUpvalue *openUpvalues;
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
