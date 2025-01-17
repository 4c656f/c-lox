#include "compiler.h"
#include "chunk.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "value.h"
#include <_types/_uint8_t.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_off_t.h>
#include <sys/_types/_uid_t.h>

typedef enum { TYPE_FUNCTION, TYPE_SCRIPT } FunctionType;

typedef struct {
  Token name;
  int depth;
  bool isCaptured;
} Local;

typedef struct {
  uint8_t index;
  bool isLocal;
} Upvalue;

typedef struct Compiler {
  struct Compiler *enclosing;

  ObjFunction *function;
  FunctionType type;

  Local locals[UINT8_COUNT];
  Upvalue upvalues[UINT8_COUNT];
  int localCount;
  int scopeDepth;
} Compiler;

Parser parser;
Compiler *current = NULL;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static Chunk *getCurrentChunk() { return &current->function->chunk; }

static void initCompiler(Compiler *compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;

  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->function = newFunction();
  current = compiler;

  if (type != TYPE_SCRIPT) {
    current->function->name =
        copyString(parser.previous.start, parser.previous.length);
  }

  Local *local = &current->locals[current->localCount++];
  local->depth = 0;
  local->name.start = "";
  local->name.length = 0;
}

static void errorAt(Token *token, const char *message) {
  parser.isInPanic = true;
  parser.isOk = false;

  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
}

static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}

static void errorAtPrevius(const char *message) {
  errorAt(&parser.previous, message);
}

static void advance() {
  parser.previous = parser.current;
  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR) {
      break;
    }
    errorAtCurrent(parser.current.start);
  }
}

static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      return i;
    }
  }

  return -1;
}

static void consume(TokenType type, char *message) {
  if (parser.current.type != type) {
    errorAtCurrent(message);
  }
  advance();
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type)) {
    return false;
  }
  advance();
  return true;
}

static void emitByte(uint8_t byte) {
  writeChunk(getCurrentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static int emitJump(uint8_t jumpInstruction) {
  // emit jump instruction
  emitByte(jumpInstruction);
  // and then short 16 bit jump pointer
  emitByte(0xff);
  emitByte(0xff);
  // JUMP_INSTRUCTION
  // OFFSET 8 MSB               <-|
  // OFFSET 8 LSB                 |
  // return pointer to offset MSB |
  return getCurrentChunk()->count - 2;
}

static void patchJump(int offset) {
  // -2 is for encoded short offset itself, because after reading the offset, we
  // whant to jump next x inctructions
  int jumpTo = getCurrentChunk()->count - offset - 2;
  if (jumpTo > UINT16_MAX) {
    errorAtPrevius("To long jump");
  }
  getCurrentChunk()->code[offset] = (jumpTo >> 8) & 0xff;
  getCurrentChunk()->code[offset + 1] = jumpTo & 0xff;
}

static void emitLoop(int offset) {
  emitByte(OP_LOOP);
  // -2 is for encoded short offset itself, because after reading the offset, we
  // whant to jump next x inctructions
  int jumpTo = getCurrentChunk()->count - offset + 2;

  if (jumpTo > UINT16_MAX) {
    errorAtPrevius("To long loop");
  }
  emitByte((jumpTo >> 8) & 0xff);
  emitByte(jumpTo & 0xff);
}

static void beginScope() { current->scopeDepth++; }

static void endScope() {
  current->scopeDepth--;
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {
    if (current->locals[current->localCount - 1].isCaptured) {

      emitByte(OP_CLOSE_UPVALUE);
    } else {

      emitByte(OP_POP);
    }
    current->localCount--;
  }
}

static uint8_t makeConstant(Value value) {
  int constants = addConstant(getCurrentChunk(), value);
  if (constants > UINT8_MAX) {
    errorAtPrevius("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constants;
}

static void markInitialized() {
  if (current->scopeDepth == 0)
    return;
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t variableIdx) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }
  emitBytes(OP_DEFINE_GLOBAL, variableIdx);
}

static uint8_t identifierConstant(Token *token) {
  return makeConstant(OBJ_VAL(copyString(token->start, token->length)));
}

static void addLocal(Token name) {
  if (current->localCount == UINT8_COUNT) {
    errorAtPrevius("Too many local variables in function.");
    return;
  }
  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->isCaptured = false;
  local->depth = current->scopeDepth;
}

static void declareVariable() {
  if (current->scopeDepth == 0)
    return;

  Token *name = &parser.previous;
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      errorAtPrevius("Already a variable with this name in this scope.");
    }
  }
  addLocal(*name);
}

static uint8_t parseVariable(char *errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  declareVariable();
  if (current->scopeDepth > 0)
    return 0;

  return identifierConstant(&parser.previous);
}

static void emitConstant(Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value));
}

static void emitReturn() {
  emitByte(OP_NIL);
  emitByte(OP_RETURN);
}

static ObjFunction *endCompiler() {
  emitReturn();
  ObjFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
  if (parser.isOk) {
    disassembleChunk(getCurrentChunk(), current->function->name != NULL
                                            ? current->function->name->chars
                                            : "<script>");
  }
#endif
  current = current->enclosing;
  return function;
}

static void statemnt();
static void declaration();
static void expression();
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void binary(bool canAssign) {
  TokenType operator= parser.previous.type;
  ParseRule *rule = getRule(operator);
  parsePrecedence((Precedence)rule->precedence + 1);

  switch (operator) {
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT);
    break;
  case TOKEN_PLUS:
    emitByte(OP_ADD);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULT);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE);
    break;
  case TOKEN_BANG_EQUAL:
    emitBytes(OP_EQUAL, OP_NOT);
    break;
  case TOKEN_EQUAL_EQUAL:
    emitByte(OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emitByte(OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emitBytes(OP_LESS, OP_NOT);
    break;
  case TOKEN_LESS:
    emitByte(OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emitBytes(OP_GREATER, OP_NOT);
    break;
  default:
    return;
  }
}

static void number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
  emitConstant(OBJ_VAL(
      copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static uint8_t argumentList() {
  uint8_t argCount = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (argCount == 255) {
        errorAtPrevius("Can't have more than 255 arguments.");
      }
      argCount++;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return argCount;
}

static void call(bool canAssign) {
  uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
}

static void unary(bool canAssign) {
  TokenType operator= parser.previous.type;
  parsePrecedence(PREC_UNARY);
  switch (operator) {
  TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  TOKEN_BANG:
    emitByte(OP_NOT);
    break;
  default:
    return;
  }
}

static void literal(bool canAssign) {
  switch (parser.previous.type) {
  case TOKEN_FALSE:
    emitByte(OP_FALSE);
    break;
  case TOKEN_NIL:
    emitByte(OP_NIL);
    break;
  case TOKEN_TRUE:
    emitByte(OP_TRUE);
    break;
  default:
    return; // Unreachable.
  }
}

static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;

  for (int i = 0; i < upvalueCount; i++) {
    Upvalue *upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  if (upvalueCount == UINT8_COUNT) {
    errorAtPrevius("Too many closure variables in function.");
    return 0;
  }

  compiler->upvalues[upvalueCount].index = index;
  compiler->upvalues[upvalueCount].isLocal = isLocal;

  return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Token *name) {
  // top level code cannot contain closures
  if (compiler->enclosing == NULL) {
    return -1;
  }
  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(compiler, (uint8_t)local, true);
  }
  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t)upvalue, false);
  }

  return -1;
}

static void namedVariable(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }
  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitBytes(setOp, (uint8_t)arg);
    return;
  }
  emitBytes(getOp, (uint8_t)arg);
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void and_(bool canAssign) {
  int jump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  parsePrecedence(PREC_AND);
  patchJump(jump);
}

static void or_(bool canAssign) {
  int orJump = emitJump(OP_JUMP_IF_FALSE);
  int jump = emitJump(OP_JUMP);
  patchJump(orJump);
  emitByte(OP_POP);
  parsePrecedence(PREC_OR);
  patchJump(jump);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type) { return &rules[type]; }

static void parsePrecedence(Precedence precedence) {
  advance();
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    errorAtPrevius("Expect expression.");
    return;
  }
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }
  if (canAssign && match(TOKEN_EQUAL)) {
    errorAtPrevius("Invalid assignment target.");
  }
}

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Can't have more than 255 parameters.");
      }
      uint8_t constant = parseVariable("Expect parameter name.");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
  block();

  ObjFunction *func = endCompiler();

  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(func)));
  for (int i = 0; i < func->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void functionDeclaration() {
  uint8_t functionNameIdx = parseVariable("Expect function name.");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(functionNameIdx);
  return;
}

static void varDeclaration() {
  uint8_t variableIdx = parseVariable("Expect variable name.");
  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
  defineVariable(variableIdx);
  return;
}

static void expressionStatemnt() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OP_POP);
}

static void printStatemnt() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}

static void returnStatement() {
  if (current->type == TYPE_SCRIPT) {
    errorAtPrevius("Can't return from top-level code.");
  }
  if (match(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitByte(OP_RETURN);
  }
}

static void ifStatemnt() {
  consume(TOKEN_LEFT_PAREN, "Expect ( after if statemnt");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ) after if statemnt");
  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statemnt();
  int elseJump = emitJump(OP_JUMP);
  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(TOKEN_ELSE)) {
    statemnt();
  }
  patchJump(elseJump);
}

static void whileStatemnt() {
  int loopStartOffset = getCurrentChunk()->count;
  consume(TOKEN_LEFT_PAREN, "Expect ( before 'while' statemnt");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ) after 'while' statemnt");
  int thenJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP);
  statemnt();
  emitLoop(loopStartOffset);

  patchJump(thenJump);
  emitByte(OP_POP);
}

static void forStatemnt() {
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expect ( before for statemnt");
  if (match(TOKEN_SEMICOLON)) {

  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    expressionStatemnt();
  }
  int exitJump = -1;
  int incrementJump = getCurrentChunk()->count;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after epression");
    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
  }
  int jumpToBody = emitJump(OP_JUMP);

  int loopJump = getCurrentChunk()->count;
  if (!match(TOKEN_RIGHT_PAREN)) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ) after for statemnt");
    emitByte(OP_POP);
  }
  emitLoop(incrementJump);

  patchJump(jumpToBody);

  statemnt();

  emitLoop(loopJump);

  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP);
  }
  endScope();
}

static void synchronize() {
  parser.isInPanic = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;
    switch (parser.current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;

    default:; // Do nothing.
    }

    advance();
  }
}

static void statemnt() {
  if (match(TOKEN_PRINT)) {
    printStatemnt();
    return;
  } else if (match(TOKEN_IF)) {
    ifStatemnt();
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else if (match(TOKEN_WHILE)) {
    whileStatemnt();
  } else if (match(TOKEN_FOR)) {
    forStatemnt();
  } else {
    expressionStatemnt();
  }
}

static void declaration() {
  if (match(TOKEN_FUN)) {
    functionDeclaration();
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    statemnt();
  }
  if (parser.isInPanic) {
    synchronize();
  }
}

ObjFunction *compile(const char *source) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);
  parser.isInPanic = false;
  parser.isOk = true;

  advance();
  while (!match(TOKEN_EOF)) {
    declaration();
  }
  ObjFunction *function = endCompiler();
  if (parser.isOk) {
    return function;
  }
  return NULL;
}

void markCompilerRoots() {
  Compiler *compiler = current;
  while (compiler != NULL) {
    markObject((Obj *)compiler->function);
    compiler = compiler->enclosing;
  }
}
