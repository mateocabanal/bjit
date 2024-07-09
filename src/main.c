#include "bf.h"
#include "microasm.h"
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#ifdef __APPLE__
#include <libkern/OSCacheControl.h> // Apple only
#include <pthread.h>                // Apple only
#endif

#define JIT_MEM_SIZE ((1024 * 1024) / 8) // 128KB

typedef struct {
  uint32_t maxSize;
  uint32_t size;
  // Pointer to an array of ptrs
  void **data;
} Stack;

Stack stack_init(uint32_t size) {
  Stack stack = {sizeof(uint32_t) * size, 0, calloc(size, sizeof(uint32_t))};
  return stack;
}

// Adds item to the top of the stack
void stack_push(Stack *stack, void *item) {
  // If stack has reached it's max size, reallocate
  if (stack->size == stack->maxSize) {
    stack->data = realloc(stack->data, stack->maxSize * 2);
  }

  // Rotate the array right by 1
  for (uint32_t i = stack->size; i > 0; i--) {
    stack->data[i] = stack->data[i - 1];
  }
  stack->data[0] = item;
  stack->size++;
}

void *stack_pop(Stack *stack) {
  if (stack->size > 0) {
    void *res = stack->data[0];

    // If stack has more than 1 element, rotate array to the left by 1
    for (uint32_t i = 0; i < stack->size - 1; i++) {
      stack->data[i] = stack->data[i + 1];
    }
    stack->data[stack->size - 1] = NULL;
    stack->size--;
    return res;
  } else {
    printf("returning null...\n");
    return NULL;
  }
}

uint8_t *compile_bf(FILE *bf_file, uint64_t *lpos, uint64_t *rpos, bool debug,
                    bool dump) {

#ifdef __APPLE__
  uint8_t *memory = mmap(NULL, JIT_MEM_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, -1, 0);
#else
  uint8_t *memory = mmap(NULL, JIT_MEM_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

  microasm bin = {.dest = memory};

  Stack s_loops = stack_init(1024 * 8);

  uint32_t loop_count = 0;
  uint32_t depth = 0;

  const uint8_t pos_reg = 9;
  const uint8_t data_reg = 10;
  const uint8_t cur_loop_point_reg = 11;
  const uint8_t value_at_pos_reg = 12;
  const uint8_t loop_lpos = 14;
  const uint8_t loop_rpos = 15;

#ifdef __APPLE__
  const uint8_t write_syscall = 4;
#else
  const uint8_t write_syscall = 64;
#endif

  asm_arm64_regmov(&bin, data_reg, 0);
  asm_arm64_regmov(&bin, loop_lpos, 1);
  asm_arm64_regmov(&bin, loop_rpos, 2);
  asm_arm64_immmov(&bin, pos_reg, 0);

  int oper_bin;
  while ((oper_bin = fgetc(bf_file)) != EOF) {
    char oper = (char)oper_bin;

    // x0 = data
    // x1 = loop array
    switch (oper) {
    case '>': {
      int addptr_loop_count = 1;
      int addptr_ch_int = 0;
      while ((addptr_ch_int = fgetc(bf_file)) != EOF) {
        char ch = (char)addptr_ch_int;

        if (ch != '>') {
          ungetc(ch, bf_file);
          break;
        }

        addptr_loop_count++;
      }
      asm_arm64_immadd(&bin, pos_reg, pos_reg, addptr_loop_count);
      break;
    }
    case '<': {
      int subptr_loop_count = 1;
      int subptr_ch_int = 0;
      while ((subptr_ch_int = fgetc(bf_file)) != EOF) {
        char ch = (char)subptr_ch_int;

        if (ch != '<') {
          ungetc(ch, bf_file);
          break;
        }

        subptr_loop_count++;
      }
      asm_arm64_immsub(&bin, pos_reg, pos_reg, subptr_loop_count);
      break;
    }
    case '[': {
      loop_count++;
      lpos[loop_count] = (uint64_t)bin.dest + (6 * 4);

      if (debug) {
        printf("L: loop id: %i\n", loop_count);
      }

      uint32_t *loc = malloc(4);
      *loc = loop_count;
      stack_push(&s_loops, loc);

      asm_arm64_regadd(&bin, value_at_pos_reg, pos_reg, data_reg,
                       0);                           // Value at position
      asm_arm64_regldrb(&bin, 13, value_at_pos_reg); // Load value to x13
      asm_arm64_immadd(&bin, 2, loop_rpos, loop_count * 8);
      asm_arm64_regldr(&bin, 3, 2); // Load saved address
      asm_arm64_pcrelbranch_nz(
          &bin, 13,
          2); // If x13 is not zero, jump over br instruction
      asm_arm64_br(&bin, 3);

      // asm_arm64_getpcval(&bin, cur_loop_point_reg); // Get current location
      // asm_arm64_immadd(&bin, loop_rpos, loop_rpos,
      //                  8); // Increment loop position
      // asm_arm64_regadd(&bin, 12, loop_lpos, loop_rpos,
      //                  0); // Calculate offset inside array
      // asm_arm64_immadd(&bin, cur_loop_point_reg, cur_loop_point_reg, 20);
      // asm_arm64_regstr(&bin, cur_loop_point_reg,
      //                  12); // Write position in memory
      break;
    }
    case ']': {
      uint32_t *lpos_ptr = (uint32_t *)stack_pop(&s_loops);
      uint32_t lpos = *lpos_ptr;

      free(lpos_ptr);

      // Used for '[' to know where to jump if == 0
      rpos[lpos] = (uint64_t)bin.dest;

      if (debug) {
        printf("R: loop id: %i\n", lpos);
      }

      if (debug && dump) {
        asm_arm64_immmov(&bin, 0, (int)']');
      }
      asm_arm64_regadd(&bin, value_at_pos_reg, pos_reg, data_reg,
                       0);                           // Value at position
      asm_arm64_regldrb(&bin, 13, value_at_pos_reg); // Load value to x13
      asm_arm64_pcrelbranch_ze(&bin, 13, 4); // If x13 is zero, jump (3 * 4)
      asm_arm64_immadd(&bin, 4, loop_lpos,
                       lpos * 8);                  // sizeof(uint64_t) == 8
      asm_arm64_regldr(&bin, value_at_pos_reg, 4); // Load saved address
      asm_arm64_br(&bin, value_at_pos_reg);        // Branch to x12
      // asm_arm64_immsub(&bin, loop_rpos, loop_rpos, 8);

      break;
    }
    case '+': {
      // Combine multiple addition operations into 1 instruction
      int add_loop_count = 1;
      int add_ch_int = 0;
      while ((add_ch_int = fgetc(bf_file)) != EOF) {
        char ch = (char)add_ch_int;

        if (ch != '+') {
          ungetc(ch, bf_file);
          break;
        }

        add_loop_count++;
      }

      asm_arm64_regadd(&bin, value_at_pos_reg, pos_reg, data_reg,
                       0);                           // Value at position
      asm_arm64_regldrb(&bin, 13, value_at_pos_reg); // Load value to x13
      asm_arm64_immadd(&bin, 13, 13, add_loop_count);
      asm_arm64_regstrb(&bin, 13, value_at_pos_reg);
      asm_arm64_immmov(&bin, 13, 0); // Clear x13
      break;
    }
    case '-': {
      // Combine multiple addition operations into 1 instruction
      int sub_loop_count = 1;
      int sub_ch_int = 0;
      while ((sub_ch_int = fgetc(bf_file)) != EOF) {
        char ch = (char)sub_ch_int;

        if (ch != '-') {
          ungetc(ch, bf_file);
          break;
        }

        sub_loop_count++;
      }

      asm_arm64_regadd(&bin, value_at_pos_reg, pos_reg, data_reg,
                       0);                           // Value at position
      asm_arm64_regldrb(&bin, 13, value_at_pos_reg); // Load value to x2
      asm_arm64_immsub(&bin, 13, 13, sub_loop_count);
      asm_arm64_regstrb(&bin, 13, value_at_pos_reg);
      asm_arm64_immmov(&bin, 13, 0); // Clear x13
      break;
    }
    case '.': {
      asm_arm64_regadd(&bin, 1, pos_reg, data_reg, 0); // Value at position
#ifdef __APPLE__
      asm_arm64_immmov(&bin, 16, write_syscall);
#else
      asm_arm64_immmov(&bin, 8, write_syscall); // 0x40 is write syscall
#endif
      asm_arm64_immmov(&bin, 0, 1); // STDOUT
      asm_arm64_immmov(&bin, 2, 1); // Length, which is 1
      asm_arm64_syscall(&bin, 0);
      asm_arm64_immmov(&bin, 0, 0);
      asm_arm64_immmov(&bin, 1, 0);
      asm_arm64_immmov(&bin, 2, 0);
      break;
    }
    case ',': {
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
  }

  asm_return(&bin);

  if (rpos + loop_count * 8 == NULL) {
    printf("Missing '['\n");
    exit(-1);
  }

  free(s_loops.data);

  return memory;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("No brainfuck source file passed!\n");
    return -1;
  }

  const char *dump_path = NULL;
  bool dump_bin = false;
  bool debug = false;

  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      debug = true;
    }

    if (strcmp(argv[i], "-b") == 0) {
      dump_bin = true;
      if (argc - 1 == ++i) {
        printf("You did not pass an output for the binary dump!\n");
        return -1;
      }

      dump_path = argv[i];
    }
  }

  if (dump_bin) {
    printf("DEBUG: Dumping enabled\n");
    printf("DEBUG: Dumping at: %s\n", dump_path);
  }

  FILE *bf_file = fopen(argv[argc - 1], "r");
  if (bf_file == NULL) {
    printf("Could not open file: %s\n", argv[argc - 1]);
    return -1;
  }

  // Initialize BF struct
  bf = malloc(sizeof(bf_data));
  bf->position = 0;
  bf->data = malloc(30000);
  bf->loop_stack = malloc(1024 * 8); // Determines how deep nested loops can go
  bf->loop_pos = 0;
  memset(bf->data, 0, 30000);

#ifdef __APPLE__
  pthread_jit_write_protect_np(0); // Turn off so it is RW- (Apple only)
#endif

  uint64_t *loop_lpos = malloc(1024 * 8);
  uint64_t *loop_rpos = malloc(1024 * 8);
  printf("Compiling...\n");
  uint8_t *bin = compile_bf(bf_file, loop_lpos, loop_rpos, debug, dump_bin);

  if (debug) {
    printf("*** lpos ***\n");

    int i = 1;
    while (loop_lpos[i] != 0) {
      printf("L: 0x%lx, R: 0x%lx\n", loop_lpos[i], loop_rpos[i]);
      i++;
    }
  }

  fclose(bf_file);

  if (dump_bin && dump_path != NULL) {
    FILE *jit_file = fopen(dump_path, "w");

    if (jit_file == NULL) {
      printf("Could not open '%s'\n", dump_path);
      return -1;
    }

    fwrite(bin, 1, JIT_MEM_SIZE, jit_file);
    fclose(jit_file);
  }

#ifdef __APPLE__
  pthread_jit_write_protect_np(1);          // Turn on so it is R-X (Apple only)
  sys_icache_invalidate(bin, JIT_MEM_SIZE); // Invalidation  (Apple Sil. only)
#endif

  // Hmmm... So ARM64 calling convention returns x0??

  printf("Running...\n\n");
  uint32_t x0 = ((uint32_t(*)(uint8_t *, uint64_t *, uint64_t *))bin)(
      bf->data, loop_lpos, loop_rpos);

  if (debug) {
    printf("\n### DEBUG ###\n");
    printf("bf loc: %p\n", bf->data);

    for (int i = 0; i < 16; i++) {
      printf("cell %i: %u\n", i, bf->data[i]);
    }
  }

  munmap(bin, JIT_MEM_SIZE);
  free(loop_lpos);
  free(loop_rpos);
  free(bf->data);
  free(bf->loop_stack);
  free(bf);

  return 0;
}
