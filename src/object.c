#include "object.h"
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
  vm.objectHeap = object;
  return object;
}

static ObjString *allocateString(char *chars, int length, uint32_t hash) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  setTableValue(&vm.stringsPool, string, NIL_VAL);
  return string;
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

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING: {
    printf("%s", AS_CSTRING(value));
    break;
  }
  }
}
