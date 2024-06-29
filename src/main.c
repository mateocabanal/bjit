#include "bf.h"
#include "microasm.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define offsetof(type, field) ((unsigned long)&(((type *)0)->field))

#define JIT_MEM_SIZE (131072)

uint8_t *compile_bf(FILE *bf_file) {
  uint8_t *memory = mmap(NULL, JIT_MEM_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  microasm bin = {.dest = memory};

  const uint8_t pos_reg = 9;
  const uint8_t data_reg = 10;
  const uint8_t cur_loop_point_reg = 11;
  const uint8_t value_at_pos_reg = 12;
  const uint8_t loop_vec_loc_reg = 14;
  const uint8_t loop_pos_reg = 15;

  asm_arm64_regmov(&bin, data_reg, 0);
  asm_arm64_regmov(&bin, loop_vec_loc_reg, 1);
  asm_arm64_immmov(&bin, pos_reg, 0);

  int oper_bin;
  while ((oper_bin = fgetc(bf_file)) != EOF) {
    char oper = (char)oper_bin;

    // x0 = data
    // x1 = loop array
    switch (oper) {
    case '>':
      asm_arm64_immadd(&bin, pos_reg, pos_reg, 1);
      break;
    case '<':
      asm_arm64_immsub(&bin, pos_reg, pos_reg, 1);
      break;
    case '[':
      asm_arm64_getpcval(&bin, cur_loop_point_reg); // Get current location
      asm_arm64_immadd(&bin, loop_pos_reg, loop_pos_reg,
                       8); // Increment loop position
      asm_arm64_regadd(&bin, 12, loop_vec_loc_reg, loop_pos_reg,
                       0); // Calculate offset inside array
      asm_arm64_immadd(&bin, cur_loop_point_reg, cur_loop_point_reg, 20);
      asm_arm64_regstr(&bin, cur_loop_point_reg,
                       12); // Write position in memory
      break;
    case ']':
      asm_arm64_regadd(&bin, value_at_pos_reg, pos_reg, data_reg,
                       0);                           // Value at position
      asm_arm64_regldrb(&bin, 13, value_at_pos_reg); // Load value to x13
      asm_arm64_pcrelbranch_ze(&bin, 13, 4); // If x13 is zero, jump (3 * 4)
      asm_arm64_regadd(&bin, 12, loop_vec_loc_reg, loop_pos_reg, 0);
      asm_arm64_regldr(&bin, 13, 12); // Load saved address
      asm_arm64_br(&bin, 13);         // Branch to x12
      asm_arm64_immsub(&bin, loop_pos_reg, loop_pos_reg, 8);
      break;
    case '+':
      asm_arm64_regadd(&bin, value_at_pos_reg, pos_reg, data_reg,
                       0);                           // Value at position
      asm_arm64_regldrb(&bin, 13, value_at_pos_reg); // Load value to x13
      asm_arm64_immadd(&bin, 13, 13, 1);
      asm_arm64_regstrb(&bin, 13, value_at_pos_reg);
      asm_arm64_immmov(&bin, 13, 0); // Clear x13
      break;
    case '-':
      asm_arm64_regadd(&bin, value_at_pos_reg, pos_reg, data_reg,
                       0);                           // Value at position
      asm_arm64_regldrb(&bin, 13, value_at_pos_reg); // Load value to x2
      asm_arm64_immsub(&bin, 13, 13, 1);
      asm_arm64_regstrb(&bin, 13, value_at_pos_reg);
      asm_arm64_regmov(&bin, 13, 0); // Clear x13
      break;
    case '.':
      // asm_arm64_regmov(&bin, 20, 0);        // Copy position to 20
      // asm_arm64_regmov(&bin, 21, 1);        // Copy pointer to 21
      // asm_arm64_regmov(&bin, 22, 2);        // Copy 2 to 22
      asm_arm64_regadd(&bin, 1, pos_reg, data_reg, 0); // Value at position
      asm_arm64_immmov(&bin, 8, 64);                   // 0x40 is write syscall
      asm_arm64_immmov(&bin, 0, 1);                    // STDOUT
      asm_arm64_immmov(&bin, 2, 1);                    // Length, which is 1
      asm_arm64_syscall(&bin, 0);
      asm_arm64_immmov(&bin, 0, 0);
      asm_arm64_immmov(&bin, 1, 0);
      asm_arm64_immmov(&bin, 2, 0);
      break;

    case ',':
      asm_arm64_immmov(&bin, 8, 63);                   // Read syscall
      asm_arm64_immmov(&bin, 0, 0);                    // STDIN
      asm_arm64_regadd(&bin, 1, pos_reg, data_reg, 0); // Value at position
      asm_arm64_immmov(&bin, 2, 1);
      asm_arm64_syscall(&bin, 0);
      asm_arm64_immmov(&bin, 0, 0);
      asm_arm64_immmov(&bin, 1, 0);
      asm_arm64_immmov(&bin, 2, 0);
      // printf("bf file has user input, this is not supported yet!\n");
      // exit(-1);
      break;
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
  bf->loop_stack = malloc(8196 * 8); // Determines how deep nested loops can go
  bf->loop_pos = 0;
  memset(bf->data, 0, 30000);

  uint8_t *bin = compile_bf(bf_file);

  // Hmmm... So ARM64 calling convention returns x0??

  printf("Running...\n\n");
  uint32_t x0 =
      ((uint32_t(*)(uint8_t *, uint64_t *))bin)(bf->data, bf->loop_stack);

#if DEBUG
  printf("\n### DEBUG ###\n");
  printf("x0: %u\n", x0);
  printf("bf value at %u: %u\n", x0, bf->data[x0]);
  printf("bf loc: %p\n", bf->data);

  for (int i = 0; i < 16; i++) {
    printf("cell %i: %u\n", i, bf->data[i]);
  }
#endif

  munmap(bin, JIT_MEM_SIZE);
  free(bf->data);
  free(bf->loop_stack);
  free(bf);

  return 0;
}
