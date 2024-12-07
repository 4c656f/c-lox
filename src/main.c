#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

char *read_file_contents(const char *filename);

int main(int argc, char *argv[]) {
  Chunk chunk;
  initVm();
  initChunk(&chunk);

  //lhs
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, addConstant(&chunk, 2), 123);
  
  //rhs 
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, addConstant(&chunk, 2), 123);
  
  writeChunk(&chunk, OP_ADD, 123);
  
  //rhs 
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, addConstant(&chunk, 2), 123);
  
  writeChunk(&chunk, OP_ADD, 123);
  
  writeChunk(&chunk, OP_RETURN, 123);
  disassembleChunk(&chunk, "test_chunk");
  interpret(&chunk);

  freeChunk(&chunk);
  freeVm();
  return 0;
}
