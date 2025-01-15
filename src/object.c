#include "object.h"
#include "chunk.h"
#include "compiler.h"
#include "hash_table.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALLOCATE_OBJ(type, objectType)                                         \
  (type *)allocateObject(sizeof(type), objectType)

static uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

static Obj *allocateObject(size_t size, ObjType type) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);

  object->type = type;
  object->next = vm.objectHeap;
  object->isMarked = false;
  vm.objectHeap = object;

#ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void *)object, size, type);
#endif
  return object;
}

static ObjString *allocateString(char *chars, int length, uint32_t hash) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  push(OBJ_VAL(string));
  setTableValue(&vm.stringsPool, string, NIL_VAL);
  pop();
  return string;
}

ObjFunction *newFunction() {
  ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjClosure *newClosure(ObjFunction *function) {
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }
  ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

ObjUpvalue *newUpvalue(Value *slot) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->location = slot;
  upvalue->next = NULL;
  upvalue->closed = NIL_VAL;
  return upvalue;
}

ObjNative *newNative(NativeFn function) {
  ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  return native;
}

ObjString *copyString(const char *string, int length) {
  uint32_t hash = hashString(string, length);
  ObjString *stringFromPool =
      findTableString(&vm.stringsPool, string, length, hash);

  if (stringFromPool != NULL) {
    return stringFromPool;
  }

  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, string, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

ObjString *takeString(char *string, int length) {
  uint32_t hash = hashString(string, length);
  ObjString *stringFromPool =
      findTableString(&vm.stringsPool, string, length, hash);
  if (stringFromPool != NULL) {
    FREE_ARRAY(char, string, length + 1);
    return stringFromPool;
  }
  return allocateString(string, length, hash);
}

static void printFunction(ObjFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING: {
    printf("%s", AS_CSTRING(value));
    break;
  }
  case OBJ_NATIVE: {
    printf("<native fn>");
    break;
  }
  case OBJ_FUNCTION: {
    printFunction(AS_FUNCTION(value));
    break;
  }
  case OBJ_CLOSURE: {
    printFunction(AS_CLOSURE(value)->function);
    break;
  }
  case OBJ_UPVALUE: {
    printf("upvalue");
    break;
  }
  }
}
