#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

void initChunk(Chunk *chunk) {
  chunk->capacity = 0;
  chunk->count = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
}

void writeChunk(Chunk *chunk, uint8_t byte, int line) {
  // need to grow
  if (chunk->capacity <= chunk->count) {
    int oldCap = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCap);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCap, chunk->capacity);
    chunk->lines = GROW_ARRAY(int, chunk->lines, oldCap, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}

void freeChunk(Chunk *chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(int, chunk->lines, chunk->capacity);
  freeValueArray(&chunk->constants);
  initChunk(chunk);
}

int addConstant(Chunk *chunk, Value value) {
  push(value);
  writeValueArray(&chunk->constants, value);
  pop();
  return chunk->constants.count - 1;
}
