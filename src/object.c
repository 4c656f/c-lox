#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>

#define ALLOCATE_OBJ(type, objectType)                                         \
  (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;
  object->next = vm.objectPool;
  vm.objectPool = object;
  return object;
}

static ObjString *allocateString(char *chars, int length) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  return string;
}

ObjString *copyString(const char *string, int length) {
  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, string, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length);
}

ObjString *takeString(char *string, int length) {
  return allocateString(string, length);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING: {
    printf("%s", AS_CSTRING(value));
    break;
  }
  }
}
