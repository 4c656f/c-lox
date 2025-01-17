#include "vm.h"
#include "chunk.h"
#include "compiler.h"
#include "debug.h"
#include "hash_table.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

VM vm;

static Value clockNative(int argCount, Value *args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    ObjFunction *function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  resetStack();
}

static void defineNative(const char *name, NativeFn function) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  setTableValue(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool call(ObjClosure *closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d.", closure->function->arity,
                 argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_CLOSURE:
      return call(AS_CLOSURE(callee), argCount);
    case OBJ_NATIVE: {
      NativeFn native = AS_NATIVE(callee);
      Value result = native(argCount, vm.stackTop - argCount);
      vm.stackTop -= argCount + 1;
      push(result);
      return true;
    }
    default:
      break; // Non-callable object type.
    }
  }
  runtimeError("Can only call functions and classes.");
  return false;
}

static ObjUpvalue *captureUpvalue(Value *local) {
  ObjUpvalue *prev = NULL;
  ObjUpvalue *cur = vm.openUpvalues;
  while (cur != NULL && cur->location > local) {
    prev = cur;
    cur = cur->next;
  }

  if (cur != NULL && cur->location == local) {
    return cur;
  }
  ObjUpvalue *createdUpvalue = newUpvalue(local);
  createdUpvalue->next = cur;
  if (prev == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prev->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalues(Value *last) {
  while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
    ObjUpvalue *upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  ObjString *rhs = AS_STRING(peek(0));
  ObjString *lhs = AS_STRING(peek(1));
  int newLen = lhs->length + rhs->length;
  char *newString = ALLOCATE(char, newLen + 1);
  memcpy(newString, lhs->chars, lhs->length);
  memcpy(newString + lhs->length, rhs->chars, rhs->length);
  newString[newLen] = '\0';
  ObjString *obj = takeString(newString, newLen);
  pop();
  pop();
  push(OBJ_VAL(obj));
}

static void numberToString() {
  Value rhs = peek(0);
  Value lhs = peek(1);
  ObjString *string = NULL;
  double number = 0;

  if (IS_STRING(rhs)) {
    string = AS_STRING(rhs);
    number = AS_NUMBER(lhs);
  } else {
    string = AS_STRING(lhs);
    number = AS_NUMBER(rhs);
  }

  // Buffer for the number conversion
  char numberStr[32];
  int numberLen = snprintf(numberStr, sizeof(numberStr), "%.14g", number);

  // Calculate total length
  int totalLen = string->length + numberLen;

  // Allocate new buffer for concatenated string
  char *newString = ALLOCATE(char, totalLen + 1); // +1 for null terminator

  if (IS_STRING(rhs)) {
    // Copy the existing string
    memcpy(newString, numberStr, numberLen);

    // Copy the number string
    memcpy(newString + numberLen, string->chars, string->length);
  } else {
    // Copy the existing string
    memcpy(newString, string->chars, string->length);

    // Copy the number string
    memcpy(newString + string->length, numberStr, numberLen);
  }
  // Null terminate the string
  newString[totalLen] = '\0';

  ObjString *obj = takeString(newString, totalLen);
  pop();
  pop();
  push(OBJ_VAL(obj));
}

void initVm() {
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;
  vm.objectHeap = NULL;
  vm.grayCap = 0;
  vm.grayCount = 0;
  vm.grayStack = NULL;
  resetStack();
  initHashTable(&vm.stringsPool);
  initHashTable(&vm.globals);

  defineNative("clock", clockNative);
}
#ifdef DEBUG_LOG_STATS_GC
static void printRemainingObjects() {
  for (Obj *cur = vm.objectHeap; cur != NULL; cur = cur->next) {
    printf("heap obj: ");
    printObject(cur);
    printf("\n");
  }
}
#endif
void freeVm() {
  freeObjectPool();
  freeHashTable(&vm.stringsPool);
  freeHashTable(&vm.globals);
#ifdef DEBUG_LOG_STATS_GC
  printf("-- free vm: %zu\n", vm.bytesAllocated);
#endif
}

InterpritationResult static run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT()                                                        \
  (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)(frame->ip[-2] << 8 | frame->ip[-1]))
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtimeError("Operands must be numbers.");                               \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop());                                               \
    double a = AS_NUMBER(pop());                                               \
    push(valueType(a op b));                                                   \
  } while (false)

  uint8_t instruction;
  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(
        &frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
#endif
    switch (instruction = READ_BYTE()) {
    case OP_RETURN: {
      Value result = pop();
      closeUpvalues(frame->slots);
      vm.frameCount--;
      if (vm.frameCount == 0) {
        pop();
        return INTERPRET_OK;
      }

      vm.stackTop = frame->slots;
      push(result);
      frame = &vm.frames[vm.frameCount - 1];
      break;
    }
    case OP_NEGATE: {
      if (!IS_NUMBER(peek(0))) {
        runtimeError("Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(NUMBER_VAL(-AS_NUMBER(pop())));
      break;
    }
    case OP_ADD: {
      if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
        concatenate();
      } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double rhs = AS_NUMBER(pop());
        double lhs = AS_NUMBER(pop());
        push(NUMBER_VAL(lhs + rhs));
      } else if (IS_NUMBER(peek(0)) && IS_STRING(peek(1)) ||
                 IS_STRING(peek(0)) && IS_NUMBER(peek(1))) {
        numberToString();
      } else {
        runtimeError("Operands must be two numbers or two strings.");
        return INTERPRET_RUNTIME_ERROR;
      }

      break;
    }
    case OP_NOT: {
      push(BOOL_VAL(isFalsey(pop())));
      break;
    }
    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(BOOL_VAL(valuesEqual(a, b)));
      break;
    }
    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;
    case OP_SUBTRACT: {
      BINARY_OP(NUMBER_VAL, -);
      break;
    }
    case OP_NIL:
      push(NIL_VAL);
      break;
    case OP_TRUE:
      push(BOOL_VAL(true));
      break;
    case OP_FALSE:
      push(BOOL_VAL(false));
      break;
    case OP_MULT: {
      BINARY_OP(NUMBER_VAL, *);
      break;
    }
    case OP_DIVIDE: {
      BINARY_OP(NUMBER_VAL, /);
      break;
    }
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    }
    case OP_PRINT: {
      printValue(pop());
      printf("\n");
      break;
    }
    case OP_POP: {
      pop();
      break;
    }
    case OP_DEFINE_GLOBAL: {
      ObjString *name = READ_STRING();
      setTableValue(&vm.globals, name, peek(0));
      // pop variable value
      pop();
      break;
    }
    case OP_GET_GLOBAL: {
      ObjString *name = READ_STRING();
      Value variableValue;
      if (!getTableValue(&vm.globals, name, &variableValue)) {
        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      push(variableValue);
      break;
    }
    case OP_SET_GLOBAL: {
      ObjString *name = READ_STRING();
      Value variableValue;
      if (!getTableValue(&vm.globals, name, &variableValue)) {
        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      setTableValue(&vm.globals, name, peek(0));
      break;
    }
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(frame->slots[slot]);
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = peek(0);
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (isFalsey(peek(0))) {
        frame->ip += offset;
      }
      break;
    }
    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      frame->ip += offset;
      break;
    }
    case OP_LOOP: {
      uint16_t offset = READ_SHORT();
      frame->ip -= offset;
      break;
    }
    case OP_CALL: {
      int argCount = READ_BYTE();
      if (!callValue(peek(argCount), argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      frame = &vm.frames[vm.frameCount - 1];
      break;
    }
    case OP_CLOSURE: {
      ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
      ObjClosure *closure = newClosure(function);
      push(OBJ_VAL(closure));
      for (int i = 0; i < closure->upvalueCount; i++) {
        uint8_t isLocal = READ_BYTE();
        uint8_t index = READ_BYTE();
        if (isLocal) {
          closure->upvalues[i] = captureUpvalue(frame->slots + index);
        } else {
          closure->upvalues[i] = frame->closure->upvalues[index];
        }
      }
      break;
    }
    case OP_GET_UPVALUE: {
      uint8_t index = READ_BYTE();
      push(*frame->closure->upvalues[index]->location);
      break;
    }
    case OP_SET_UPVALUE: {
      uint8_t index = READ_BYTE();
      *frame->closure->upvalues[index]->location = peek(0);
      break;
    }
    case OP_CLOSE_UPVALUE: {
      closeUpvalues(vm.stackTop - 1);
      pop();
      break;
    }
    }
  }
#undef BINARY_OP
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef READ_SHORT
}

InterpritationResult interpret(char *source) {
  ObjFunction *function = compile(source);
  if (function == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }
  push(OBJ_VAL(function));

  ObjClosure *closure = newClosure(function);
  pop();
  push(OBJ_VAL(closure));

  call(closure, 0);

  InterpritationResult res = run();
  runGc();
#ifdef DEBUG_LOG_STATS_GC
  printRemainingObjects();
  printf("-- last gc: %zu\n", vm.bytesAllocated);
#endif
  return res;
}
