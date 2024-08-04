#include "bf.h"
#include "microasm.h"
#include "stack.h"
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#define ANSI_DEBUG_MSG "\x1b[38;5;255m\x1b[48;5;68mDEBUG\x1b[0m: "
#define ANSI_RESET_COLOR "\x1b[0m"

#ifdef __APPLE__
#include <libkern/OSCacheControl.h> // Apple only
#include <pthread.h>                // Apple only
#endif

uint8_t *compile_bf(FILE *bf_file, bool debug, bool dump, char *dump_path) {

#ifdef __APPLE__
  uint8_t *memory = mmap(NULL, JIT_MEM_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, -1, 0);
#else
  uint8_t *memory = mmap(NULL, JIT_MEM_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

  microasm bin = {.count = 0,
                  .dest = memory,
                  .dest_end = (uint64_t)memory + JIT_MEM_SIZE,
                  .dest_size = JIT_MEM_SIZE};

  Stack s_loops = stack_init(16384 * 8);

  uint32_t loop_count = 0;
  uint32_t depth = 0;

  uint64_t *lpos = malloc(16384 * 8);
  uint64_t *rpos = malloc(16384 * 8);

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
  asm_arm64_immmov(&bin, pos_reg, 0);

  int oper_bin;
  while ((oper_bin = fgetc(bf_file)) != EOF) {
    char oper = (char)oper_bin;

    // x0 = data
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
      lpos[loop_count] = (uint64_t)bin.dest + (4 * 4);

      if (debug) {
        printf("L: loop id: %i\n", loop_count);
      }

      uint32_t *loop_id = malloc(4);
      *loop_id = loop_count;
      stack_push(&s_loops, loop_id);

      asm_arm64_regadd(&bin, value_at_pos_reg, pos_reg, data_reg,
                       0);                           // Value at position
      asm_arm64_regldrb(&bin, 13, value_at_pos_reg); // Load value to x13
      // asm_arm64_immadd(&bin, 2, loop_rpos, loop_count * 8);
      // asm_arm64_regldr(&bin, 3, 2); // Load saved address
      asm_arm64_pcrelbranch_nz(
          &bin, 13,
          2);               // If x13 is not zero, jump over br instruction
      asm_arm64_b(&bin, 0); // Will be backpatched later

      loop_count++;

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
      uint32_t *loop_id_ptr = (uint32_t *)stack_pop(&s_loops);
      if (loop_id_ptr == NULL) {
        printf("extra ']' in bf code\n");
        exit(-1);
      }
      uint32_t loop_id = *loop_id_ptr;

      free(loop_id_ptr);

      // Used for '[' to know where to jump if == 0
      rpos[loop_id] = (uint64_t)bin.dest + (4 * 4);

      if (debug) {
        printf("R: loop id: %i\n", loop_id);
      }

      asm_arm64_regadd(&bin, value_at_pos_reg, pos_reg, data_reg,
                       0);                           // Value at position
      asm_arm64_regldrb(&bin, 13, value_at_pos_reg); // Load value to x13
      asm_arm64_pcrelbranch_ze(&bin, 13, 2); // If x13 is zero, jump (3 * 4)
      // asm_arm64_immadd(&bin, 4, loop_lpos,
      //                  loop_id * 8);               // sizeof(uint64_t) == 8
      // asm_arm64_regldr(&bin, value_at_pos_reg, 4); // Load saved address
      // asm_arm64_br(&bin, value_at_pos_reg); // Branch to x12
      asm_arm64_b(&bin, 0);
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

  // NOTE: Backpatching loop
  for (int i = loop_count - 1; i >= 0; i--) {
    uint32_t *l_brack = (uint32_t *)lpos[i];
    uint32_t *r_brack = (uint32_t *)rpos[i];

    int64_t offset = (r_brack - l_brack);

    uint32_t lpos_b_ins = 0x14000000;
    lpos_b_ins |= (offset + 1) & ((1 << 26) - 1);
    *(l_brack - 1) = lpos_b_ins;

    uint32_t rpos_b_ins = 0x14000000;
    rpos_b_ins |= (-offset + 1) & ((1 << 26) - 1);
    *(r_brack - 1) = rpos_b_ins;
  }

  if (rpos + loop_count * 8 == NULL) {
    printf("Missing '['\n");
    exit(-1);
  }

  if (s_loops.size != 0) {
    printf("Missing ']'\n");
    exit(-1);
  }

  if (debug) {
    printf("*** loops ***\n");

    int i = 1;
    while (lpos[i] != 0) {
      printf("L: 0x%lx, R: 0x%lx\n", lpos[i], rpos[i]);
      i++;
    }
  }

  if (dump && dump_path != NULL) {
    asm_write_exec(dump_path, &bin);
  }

  free(s_loops.data);
  free(lpos);
  free(rpos);

  return memory;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("No brainfuck source file passed!\n");
    return -1;
  }

  char *dump_path = NULL;
  bool dump_bin = false;
  bool debug = false;

  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      debug = true;
    }

    if (strcmp(argv[i], "-c") == 0) {
      dump_bin = true;
      if (argc - 1 == ++i) {
        printf("You did not provide a path to save executable!\n");
        return -1;
      }

      dump_path = argv[i];
    }
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

  printf("Compiling...\n");

  clock_t t;
  t = clock();

  uint8_t *bin = compile_bf(bf_file, debug, dump_bin, dump_path);

  if (debug) {
    printf(ANSI_DEBUG_MSG);
    printf("Compilation took %f seconds\n", ((double)t) / CLOCKS_PER_SEC);
  }

  fclose(bf_file);

  if (dump_bin && dump_path != NULL) {
    return 0;
  }

#ifdef __APPLE__
  pthread_jit_write_protect_np(1);          // Turn on so it is R-X (Apple only)
  sys_icache_invalidate(bin, JIT_MEM_SIZE); // Invalidation  (Apple Sil. only)
#endif

  printf("Running...\n\n");

  t = clock();

  uint32_t x0 = ((uint32_t(*)(uint8_t *))bin)(bf->data);

  t = clock() - t;
  double time_taken = ((double)t) / CLOCKS_PER_SEC;

  if (debug) {
    printf("\n### DEBUG ###\n");
    printf("bf loc: %p\n", bf->data);

    for (int i = 0; i < 16; i++) {
      printf("cell %i: %u\n", i, bf->data[i]);
    }

    printf("The program took %f seconds to execute\n", time_taken);
  }

  munmap(bin, JIT_MEM_SIZE);
  free(bf->data);
  free(bf->loop_stack);
  free(bf);

  return 0;
}
