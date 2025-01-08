#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(pointer);
    return NULL;
  };

  void *result = realloc(pointer, newSize);

  if (result == NULL) {
    exit(1);
  }

  return result;
}

static void freeObject(Obj *object) {

  switch (object->type) {
  case OBJ_STRING: {
    ObjString *stringObj = (ObjString *)object;
    FREE_ARRAY(char *, stringObj->chars, stringObj->length + 1);
    FREE(ObjString, stringObj);
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    freeChunk(&function->chunk);
    FREE(ObjFunction, object);
    break;
  }
  case OBJ_NATIVE: {
    FREE(ObjNative, object);
    break;
  }
  }
}

void freeObjectPool() {
  Obj *cur = vm.objectHeap;
  while (cur != NULL) {
    Obj *next = cur->next;
    freeObject(cur);
    cur = next;
  }
}
