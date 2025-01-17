#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "hash_table.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include "compiler.h"
#include "debug.h"
#include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR 2
void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  vm.bytesAllocated += newSize - oldSize;
  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    runGc();
#endif
    if (vm.bytesAllocated > vm.nextGC) {
      runGc();
    }
  }
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
#ifdef DEBUG_LOG_GC
  printf("%p free type %d ", (void *)object, object->type);
  printObject(object);
  printf("\n");
#endif
  switch (object->type) {
  case OBJ_STRING: {
    ObjString *stringObj = (ObjString *)object;
    FREE_ARRAY(char, stringObj->chars, stringObj->length + 1);
    FREE(ObjString, stringObj);
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    freeChunk(&function->chunk);
    FREE(ObjFunction, object);
    break;
  }
  case OBJ_CLOSURE: {
    ObjClosure *closure = (ObjClosure *)object;
    FREE_ARRAY(ObjUpvalue *, closure->upvalues, closure->upvalueCount);
    FREE(ObjClosure, object);
    break;
  }
  case OBJ_UPVALUE: {
    FREE(ObjUpvalue, object);
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
  free(vm.grayStack);
  vm.objectHeap = NULL;
}

static void sweep() {
  Obj *prev = NULL;
  Obj *cur = vm.objectHeap;

  while (cur != NULL) {
    if (cur->isMarked) {
      cur->isMarked = false;
      prev = cur;
      cur = cur->next;
    } else {
      Obj *unreacheble = cur;
      cur = cur->next;
      if (prev == NULL) {
        vm.objectHeap = cur;
      } else {
        prev->next = cur;
      }
      freeObject(unreacheble);
    }
  }
}

static void markArray(ValueArray *array) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}

static void blackenObject(Obj *object) {
#ifdef DEBUG_LOG_GC
  printf("%p blacken %d\n", (void *)object, object->type);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif
  switch (object->type) {
  case OBJ_CLOSURE: {
    ObjClosure *closure = (ObjClosure *)object;
    markObject((Obj *)closure->function);
    for (int i = 0; i < closure->upvalueCount; i++) {
      markObject((Obj *)closure->upvalues[i]);
    }
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    markObject((Obj *)function->name);
    markArray(&function->chunk.constants);
    break;
  }
  case OBJ_UPVALUE:
    markValue(((ObjUpvalue *)object)->closed);
    break;
  case OBJ_NATIVE:
  case OBJ_STRING:
    break;
  }
}

static void traceReferences() {
  while (vm.grayCount > 0) {
    Obj *object = vm.grayStack[--vm.grayCount];
    blackenObject(object);
  }
}

void markObject(Obj *object) {
  if (object == NULL) {
    return;
  }
  if (object->isMarked) {
    return;
  }
#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void *)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif
  object->isMarked = true;

  if (vm.grayCap < vm.grayCount + 1) {
    vm.grayCap = GROW_CAPACITY(vm.grayCap);
    vm.grayStack = (Obj **)realloc(vm.grayStack, sizeof(Obj *) * vm.grayCap);
    if (vm.grayStack == NULL)
      exit(1);
  }
  vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
  if (IS_OBJ(value)) {
    markObject(AS_OBJ(value));
  }
}

void static markRoots() {
  for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }

  for (int i = 0; i < vm.frameCount; i++) {
    markObject((Obj *)vm.frames[i].closure);
  }

  for (ObjUpvalue *curUpvalue = vm.openUpvalues; curUpvalue != NULL;
       curUpvalue = curUpvalue->next) {
    markObject((Obj *)curUpvalue);
  }

  markTable(&vm.globals);
  markCompilerRoots();
}

void runGc() {
#ifdef DEBUG_LOG_STATS_GC
  printf("-- gc begin\n");
  size_t before = vm.bytesAllocated;

#endif

  markRoots();
  traceReferences();
  tableRemoveWhite(&vm.stringsPool);
  sweep();
  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;
#ifdef DEBUG_LOG_STATS_GC
  printf("-- gc end\n");
  printf("   collected %zu bytes, alocated: %zu next at %zu\n",
         before - vm.bytesAllocated, vm.bytesAllocated, vm.nextGC);
#endif
}
