#include "scanner.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Scanner scanner;

static bool isDigit(char target) { return target >= '0' && target <= '9'; }

static bool isAlpha(char target) {
  return (target >= 'a' && target <= 'z') || (target >= 'A' && target <= 'Z') ||
         (target == '_');
}

static bool isAtEnd() { return *scanner.current == '\0'; }

static Token newToken(TokenType type) {
  Token token;

  token.type = type;
  token.line = scanner.line;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);

  return token;
}

static Token newErrorToken(const char *msg) {
  Token token;

  token.type = TOKEN_ERROR;
  token.line = scanner.line;
  token.start = msg;
  token.length = (int)(strlen(msg));

  return token;
}

static char advance() {
  scanner.current++;
  return scanner.current[-1];
}

static char peek() { return *scanner.current; }

static char peekNext() {
  if (isAtEnd()) {
    return '\0';
  }
  return scanner.current[1];
}

// compare current char of scanner with target, if chars match advance scanner,
// return cmp result
static bool match(char target) {
  if (isAtEnd()) {
    return false;
  }
  if (*scanner.current != target) {
    return false;
  }
  advance();
  return true;
}

// compare current char of scanner with target, return cmp result
static bool check(char target) {
  if (isAtEnd()) {
    return false;
  }
  return *scanner.current != target;
}

static void skipWhiteSpaces() {
  for (;;) {
    char cur = peek();
    switch (cur) {
    case ' ':
    case '\r':
    case '\t':
      advance();
      break;
    case '\n':
      scanner.line++;
      advance();
      break;
    case '/':
      if (peekNext() == '/') {
        while (peek() != '\n' && !isAtEnd()) {
          advance();
        }
      } else {
        return;
      }
      break;
    default:
      return;
    }
  }
}

static Token scanString() {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n') {
      scanner.line++;
    }
    advance();
  }

  if (isAtEnd()) {
    return newErrorToken("Unterminated string.");
  }
  advance();
  return newToken(TOKEN_STRING);
}

static Token scanNumber() {
  while (isDigit(peek())) {
    advance();
  }
  if (peek() == '.' && isDigit(peekNext())) {
    advance();
    while (isDigit(peek())) {
      advance();
    }
  }
  return newToken(TOKEN_NUMBER);
}

static TokenType matchKeyword(int start, int length, const char *rest,
                               TokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }
  return TOKEN_IDENTIFIER;
}

static TokenType matchIdentifierType() {
  switch (scanner.start[0]) {
  case 'a':
    return matchKeyword(1, 2, "nd", TOKEN_AND);
  case 'c':
    return matchKeyword(1, 4, "lass", TOKEN_CLASS);
  case 'e':
    return matchKeyword(1, 3, "lse", TOKEN_ELSE);
  case 'i':
    return matchKeyword(1, 1, "f", TOKEN_IF);
  case 'n':
    return matchKeyword(1, 2, "il", TOKEN_NIL);
  case 'o':
    return matchKeyword(1, 1, "r", TOKEN_OR);
  case 'p':
    return matchKeyword(1, 4, "rint", TOKEN_PRINT);
  case 'r':
    return matchKeyword(1, 5, "eturn", TOKEN_RETURN);
  case 's':
    return matchKeyword(1, 4, "uper", TOKEN_SUPER);
  case 'v':
    return matchKeyword(1, 2, "ar", TOKEN_VAR);
  case 'w':
    return matchKeyword(1, 4, "hile", TOKEN_WHILE);
  case 'f':
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'a':
        return matchKeyword(2, 3, "lse", TOKEN_FALSE);
      case 'o':
        return matchKeyword(2, 1, "r", TOKEN_FOR);
      case 'u':
        return matchKeyword(2, 1, "n", TOKEN_FUN);
      }
    }
    break;
  case 't':
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'h':
        return matchKeyword(2, 2, "is", TOKEN_THIS);
      case 'r':
        return matchKeyword(2, 2, "ue", TOKEN_TRUE);
      }
    }
    break;
  }

  return TOKEN_IDENTIFIER;
}

static Token scanIdentifier() {
  while (isAlpha(peek()) || isDigit(peek())) {
    advance();
  }

  return newToken(matchIdentifierType());
}

Token scanToken() {
  skipWhiteSpaces();
  scanner.start = scanner.current;
  if (isAtEnd()) {
    return newToken(TOKEN_EOF);
  }
  char tokenChar = advance();

  if (isDigit(tokenChar)) {
    return scanNumber();
  }

  if (isAlpha(tokenChar)) {
    return scanIdentifier();
  }

  switch (tokenChar) {
  case '(': {
    return newToken(TOKEN_LEFT_PAREN);
  }
  case ')': {
    return newToken(TOKEN_RIGHT_PAREN);
  }
  case '{': {
    return newToken(TOKEN_LEFT_BRACE);
  }
  case '}': {
    return newToken(TOKEN_RIGHT_BRACE);
  }
  case ';': {
    return newToken(TOKEN_SEMICOLON);
  }
  case ',': {
    return newToken(TOKEN_COMMA);
  }
  case '+': {
    return newToken(TOKEN_PLUS);
  }
  case '-': {
    return newToken(TOKEN_MINUS);
  }
  case '.': {
    return newToken(TOKEN_DOT);
  }
  case '*': {
    return newToken(TOKEN_STAR);
  }
  case '/': {
    return newToken(TOKEN_SLASH);
  }
  case '!': {
    if (match('=')) {
      return newToken(TOKEN_BANG_EQUAL);
    }
    return newToken(TOKEN_BANG);
  }
  case '=': {
    if (match('=')) {
      return newToken(TOKEN_EQUAL_EQUAL);
    }
    return newToken(TOKEN_EQUAL);
  }
  case '<': {
    if (match('=')) {
      return newToken(TOKEN_LESS_EQUAL);
    }
    return newToken(TOKEN_LESS);
  }
  case '>': {
    if (match('=')) {
      return newToken(TOKEN_GREATER_EQUAL);
    }
    return newToken(TOKEN_GREATER);
  }
  case '"': {
    return scanString();
  }
  }
  char buffer[100];
  sprintf(buffer, "Unexpected character: %c", scanner.start[0]);
  return newErrorToken(buffer);
}

void initScanner(const char *source) {
  scanner.current = source;
  scanner.start = source;
  scanner.line = 1;
}

void printToken(Token token) {
  switch (token.type) {
  // Single-character tokens
  case TOKEN_LEFT_PAREN:
    printf("LEFT_PAREN ( null\n");
    break;
  case TOKEN_RIGHT_PAREN:
    printf("RIGHT_PAREN ) null\n");
    break;
  case TOKEN_LEFT_BRACE:
    printf("LEFT_BRACE { null\n");
    break;
  case TOKEN_RIGHT_BRACE:
    printf("RIGHT_BRACE } null\n");
    break;
  case TOKEN_COMMA:
    printf("COMMA , null\n");
    break;
  case TOKEN_DOT:
    printf("DOT . null\n");
    break;
  case TOKEN_MINUS:
    printf("MINUS - null\n");
    break;
  case TOKEN_PLUS:
    printf("PLUS + null\n");
    break;
  case TOKEN_SEMICOLON:
    printf("SEMICOLON ; null\n");
    break;
  case TOKEN_SLASH:
    printf("SLASH / null\n");
    break;
  case TOKEN_STAR:
    printf("STAR * null\n");
    break;

  // One or two character tokens
  case TOKEN_BANG:
    printf("BANG ! null\n");
    break;
  case TOKEN_BANG_EQUAL:
    printf("BANG_EQUAL != null\n");
    break;
  case TOKEN_EQUAL:
    printf("EQUAL = null\n");
    break;
  case TOKEN_EQUAL_EQUAL:
    printf("EQUAL_EQUAL == null\n");
    break;
  case TOKEN_GREATER:
    printf("GREATER > null\n");
    break;
  case TOKEN_GREATER_EQUAL:
    printf("GREATER_EQUAL >= null\n");
    break;
  case TOKEN_LESS:
    printf("LESS < null\n");
    break;
  case TOKEN_LESS_EQUAL:
    printf("LESS_EQUAL <= null\n");
    break;

  // Literals
  case TOKEN_IDENTIFIER: {
    printf("IDENTIFIER %.*s null\n", token.length, token.start);
    break;
  }
  case TOKEN_STRING: {
    printf("STRING %.*s %.*s\n", token.length, token.start, token.length - 2,
           token.start + 1);
    break;
  }
  case TOKEN_NUMBER: {
    double value = strtod(token.start, NULL);
    if (value == (int)value) {
      printf("NUMBER %.*s %g%s\n", token.length, token.start, value, ".0");
    } else {

      printf("NUMBER %.*s %g\n", token.length, token.start, value);
    }
    break;
  }

  // Keywords
  case TOKEN_AND:
    printf("AND and null\n");
    break;
  case TOKEN_CLASS:
    printf("CLASS class null\n");
    break;
  case TOKEN_ELSE:
    printf("ELSE else null\n");
    break;
  case TOKEN_FALSE:
    printf("FALSE false null\n");
    break;
  case TOKEN_FOR:
    printf("FOR for null\n");
    break;
  case TOKEN_FUN:
    printf("FUN fun null\n");
    break;
  case TOKEN_IF:
    printf("IF if null\n");
    break;
  case TOKEN_NIL:
    printf("NIL nil null\n");
    break;
  case TOKEN_OR:
    printf("OR or null\n");
    break;
  case TOKEN_PRINT:
    printf("PRINT print null\n");
    break;
  case TOKEN_RETURN:
    printf("RETURN return null\n");
    break;
  case TOKEN_SUPER:
    printf("SUPER super null\n");
    break;
  case TOKEN_THIS:
    printf("THIS this null\n");
    break;
  case TOKEN_TRUE:
    printf("TRUE true null\n");
    break;
  case TOKEN_VAR:
    printf("VAR var null\n");
    break;
  case TOKEN_WHILE:
    printf("WHILE while null\n");
    break;

  // Special tokens
  case TOKEN_ERROR:
    fprintf(stderr, "[line %d] Error: %.*s\n", token.line, token.length,
            token.start);
    break;
  case TOKEN_EOF:
    printf("EOF  null\n");
    break;
  default: {
    printf("UNKNOWN  null\n");
    break;
  }
  }
}
