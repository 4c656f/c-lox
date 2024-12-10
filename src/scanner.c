#include "scanner.h"
#include <stdbool.h>
#include <stdio.h>
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

static TokenType matchKeywoard(int start, int length, const char *rest,
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
    return matchKeywoard(1, 2, "nd", TOKEN_AND);
  case 'c':
    return matchKeywoard(1, 4, "lass", TOKEN_CLASS);
  case 'e':
    return matchKeywoard(1, 3, "lse", TOKEN_ELSE);
  case 'i':
    return matchKeywoard(1, 1, "f", TOKEN_IF);
  case 'n':
    return matchKeywoard(1, 2, "il", TOKEN_NIL);
  case 'o':
    return matchKeywoard(1, 1, "r", TOKEN_OR);
  case 'p':
    return matchKeywoard(1, 4, "rint", TOKEN_PRINT);
  case 'r':
    return matchKeywoard(1, 5, "eturn", TOKEN_RETURN);
  case 's':
    return matchKeywoard(1, 4, "uper", TOKEN_SUPER);
  case 'v':
    return matchKeywoard(1, 2, "ar", TOKEN_VAR);
  case 'w':
    return matchKeywoard(1, 4, "hile", TOKEN_WHILE);
  case 'f':
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'a':
        return matchKeywoard(2, 3, "lse", TOKEN_FALSE);
      case 'o':
        return matchKeywoard(2, 1, "r", TOKEN_FOR);
      case 'u':
        return matchKeywoard(2, 1, "n", TOKEN_FUN);
      }
    }
    break;
  case 't':
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'h':
        return matchKeywoard(2, 2, "is", TOKEN_THIS);
      case 'r':
        return matchKeywoard(2, 2, "ue", TOKEN_TRUE);
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
    return newToken(EOF);
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
  case '*': {
    return newToken(TOKEN_STAR);
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

  return newErrorToken("Unexpected character.");
}

void initScanner(const char *source) {
  scanner.current = source;
  scanner.start = source;
  scanner.line = 1;
}
