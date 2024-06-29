#include "microasm.h"
#include <stdarg.h>
#include <stdio.h>

// https://github.com/spencertipping/jit-tutorial
void asm_write(microasm *a, int n, ...) {
  va_list bytes;
  va_start(bytes, n);
  for (int i = 0; i < n; i++) {
    *(a->dest++) = (uint8_t)va_arg(bytes, int);
  }
  va_end(bytes);
}

void asm_write_32bit(microasm *a, uint32_t instruction) {
  asm_write(
      a, 4, instruction & ((1 << 8) - 1), instruction >> 8 & ((1 << 8) - 1),
      instruction >> 16 & ((1 << 8) - 1), instruction >> 24 & ((1 << 8) - 1));
}

void asm_arm64_immadd(microasm *a, uint8_t rd, uint8_t rn, uint8_t imm) {
  uint32_t instruction = 0x91000000;
  instruction |= (rn << 5) | rd;
  instruction |= (imm << 10);

  asm_write_32bit(a, instruction);
}

void asm_arm64_regadd(microasm *a, uint8_t rd, uint8_t rn, uint8_t rm,
                      uint8_t imm_shift) {
  uint32_t instruction = 0x8B000000;
  instruction |= (rn << 5) | rd;
  instruction |= (imm_shift << 10) & ((1 << 5) - 1);
  instruction |= (rm << 16);

  asm_write_32bit(a, instruction);
}

void asm_arm64_immsub(microasm *a, uint8_t rd, uint8_t rn, uint8_t imm) {
  uint32_t instruction = 0xD1000000;
  instruction |= (rn << 5) | rd;
  instruction |= (imm << 10);

  asm_write_32bit(a, instruction);
}

void asm_arm64_regstrb(microasm *a, uint8_t rt, uint8_t rn) {
  uint32_t instruction = 0x39000000;
  instruction |= (rn << 5) | rt;

  asm_write_32bit(a, instruction);
}

void asm_arm64_regstr(microasm *a, uint8_t rt, uint8_t rn) {
  uint32_t instruction = 0xF9000000;
  instruction |= (rn << 5) | rt;

  asm_write_32bit(a, instruction);
}

void asm_arm64_regldrb(microasm *a, uint8_t rt, uint8_t rn) {
  uint32_t instruction = 0x39400000;
  instruction |= (rn << 5) | rt;

  asm_write_32bit(a, instruction);
}

void asm_arm64_regldr(microasm *a, uint8_t rt, uint8_t rn) {
  uint32_t instruction = 0xF9400000;
  instruction |= (rn << 5) | rt;

  asm_write_32bit(a, instruction);
}

void asm_arm64_immmov(microasm *a, uint8_t rd, uint16_t imm) {
  uint32_t instruction = 0xD2800000;
  instruction |= rd & ((1 << 5) - 1);
  instruction |= imm << 5;

  asm_write_32bit(a, instruction);
}

void asm_arm64_regmov(microasm *a, uint8_t rd, uint8_t rm) {
  uint32_t instruction = 0xAA0003E0;
  instruction |= rd;
  instruction |= rm << 16;

  asm_write_32bit(a, instruction);
}

void asm_arm64_syscall(microasm *a, uint16_t imm) {
  uint32_t instruction = 0xD4000001;
  instruction |= imm << 5;

  asm_write_32bit(a, instruction);
}

void asm_arm64_immcmp(microasm *a, uint8_t rn, uint16_t imm) {
  uint32_t instruction = 0xF100001F;
  instruction |= rn << 5;
  instruction |= imm << 10;

  asm_write_32bit(a, instruction);
}

// NOTE: Jumps to imm * 4
void asm_arm64_pcrelbranch_nz(microasm *a, uint8_t rt, uint32_t imm) {
  uint32_t instruction = 0xB5000000;
  instruction |= rt;
  instruction |= imm << 5;

  asm_write_32bit(a, instruction);
}

// NOTE: Jumps to imm * 4
void asm_arm64_pcrelbranch_ze(microasm *a, uint8_t rt, uint32_t imm) {
  uint32_t instruction = 0xB4000000;
  instruction |= rt;
  instruction |= imm << 5;

  asm_write_32bit(a, instruction);
}

void asm_arm64_br(microasm *a, uint8_t rn) {
  uint32_t instruction = 0xD61F0000;
  instruction |= rn << 5;

  asm_write_32bit(a, instruction);
}

void asm_arm64_getpcval(microasm *a, uint8_t rd) {
  uint32_t instruction = 0x10000000;
  instruction |= rd;

  asm_write_32bit(a, instruction);
}

void asm_return(microasm *a) {
  uint32_t instruction = 0xd65f03c0;

  asm_write_32bit(a, instruction);
}
