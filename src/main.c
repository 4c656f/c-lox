#include <_stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_size_t.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

char *read_file_contents(const char *filename);

static void repl() {
  char line[1024];
  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(line);
  }
}

static char *readFile(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *fileBuffer = (char *)malloc(fileSize + 1);
  if (fileBuffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t readedBytes = fread(fileBuffer, sizeof(char), fileSize, file);
  if (readedBytes < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }

  fileBuffer[readedBytes] = '\0';

  fclose(file);

  return fileBuffer;
}

static void runFile(const char *path) {
  char *fileContent = readFile(path);
  InterpritationResult result = interpret(fileContent);
  free(fileContent);
  if (result == INTERPRET_COMPILE_ERROR)
    exit(65);
  if (result == INTERPRET_RUNTIME_ERROR)
    exit(70);
}

int main(int argc, char *argv[]) {
  initVm();
  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    fprintf(stderr, "Usage: clox [path]\n");
    exit(64);
  }

  freeVm();
  return 0;
}
