#include "bf.h"
#include "microasm.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define offsetof(type, field) ((unsigned long)&(((type *)0)->field))

uint8_t *compile_bf(FILE *bf_file) {
  uint8_t *memory = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  microasm bin = {.dest = memory};

  int oper_bin;
  while ((oper_bin = fgetc(bf_file)) != EOF) {
    char oper = (char)oper_bin;

    // x0 = position
    // x1 = data
    // x4 = loop point
    switch (oper) {
    case '>':
      asm_arm64_immadd(&bin, 0, 0, 1);
      break;
    case '<':
      asm_arm64_immsub(&bin, 0, 0, 1);
      break;
    case '[':
      asm_arm64_getpcval(&bin, 4);
      break;
    case ']':
      asm_arm64_regadd(&bin, 19, 0, 1, 0);  // Value at position
      asm_arm64_regldrb(&bin, 2, 19);       // Load value to x2
      asm_arm64_pcrelbranch_ze(&bin, 2, 2); // If x2 is zero, jump (2 * 4)
      asm_arm64_br(&bin, 4);                // Branch to x4
      break;
    case '+':
      asm_arm64_regadd(&bin, 19, 0, 1, 0); // Value at position
      asm_arm64_regldrb(&bin, 2, 19);      // Load value to x2
      asm_arm64_immadd(&bin, 2, 2, 1);
      asm_arm64_regstrb(&bin, 2, 19);
      asm_arm64_regmov(&bin, 2, 0); // Clear x2
      break;
    case '-':
      asm_arm64_regadd(&bin, 19, 0, 1, 0); // Value at position
      asm_arm64_regldrb(&bin, 2, 19);      // Load value to x2
      asm_arm64_immsub(&bin, 2, 2, 1);
      asm_arm64_regstrb(&bin, 2, 19);
      asm_arm64_regmov(&bin, 2, 0); // Clear x2
      break;
    case '.':
      asm_arm64_regmov(&bin, 20, 0);        // Copy position to 20
      asm_arm64_regmov(&bin, 21, 1);        // Copy pointer to 21
      asm_arm64_regmov(&bin, 22, 2);        // Copy 2 to 22
      asm_arm64_regadd(&bin, 1, 20, 21, 0); // Value at position
      asm_arm64_immmov(&bin, 8, 64);        // 0x40 is write syscall
      asm_arm64_immmov(&bin, 0, 1);         // STDOUT
      asm_arm64_immmov(&bin, 2, 1);         // Length, which is 1
      asm_arm64_syscall(&bin, 0);
      asm_arm64_regmov(&bin, 0, 20);
      asm_arm64_regmov(&bin, 1, 21);
      asm_arm64_regmov(&bin, 2, 22);

    case '$':
      asm_arm64_regmov(&bin, 20, 0);        // Copy position to 20
      asm_arm64_regmov(&bin, 21, 1);        // Copy pointer to 21
      asm_arm64_regmov(&bin, 22, 2);        // Copy 2 to 22
      asm_arm64_regadd(&bin, 1, 20, 21, 0); // Value at position
      asm_arm64_regmov(&bin, 0, 0);
      asm_arm64_immadd(&bin, 1, 0, 48);
      asm_arm64_immmov(&bin, 8, 64); // 0x40 is write syscall
      asm_arm64_immmov(&bin, 0, 1);  // STDOUT
      asm_arm64_immmov(&bin, 2, 1);  // Length, which is 1
      asm_arm64_syscall(&bin, 0);
      asm_arm64_regmov(&bin, 0, 20);
      asm_arm64_regmov(&bin, 1, 21);
      asm_arm64_regmov(&bin, 2, 22);
    }
  }

  asm_return(&bin);

  // TODO: Find out why this is needed
  printf("");

  return memory;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("No brainfuck source file passed!\n");
    return -1;
  }

  FILE *bf_file = fopen(argv[1], "r");
  if (bf_file == NULL) {
    printf("Could not open file: %s\n", argv[1]);
    return -1;
  }

  // Initialize BF struct
  bf = malloc(sizeof(bf_data));
  bf->position = 0;
  bf->data = malloc(30000);
  memset(bf->data, 0, 30000);

  uint8_t *bin = compile_bf(bf_file);

  // Hmmm... So ARM64 calling convention returns x0??

  printf("Running...\n");
  uint32_t x0 = ((uint32_t(*)(uint32_t, uint8_t *))bin)(bf->position, bf->data);

#if DEBUG
  printf("\n\n### DEBUG ###\n");
  printf("x0: %u\n", x0);
  printf("bf value at %u: %u\n", x0, bf->data[x0]);
  printf("bf loc: %p\n", bf->data);

  for (int i = 0; i < 16; i++) {
    printf("cell %i: %u\n", i, bf->data[i]);
  }
#endif

  munmap(bin, 4096);
  free(bf->data);
  free(bf);

  return 0;
}
