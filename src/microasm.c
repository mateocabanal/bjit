#define _GNU_SOURCE

#include "microasm.h"
#include <elf.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

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
  if ((uint64_t)a->dest == a->dest_end) {
    mremap(a->dest, a->dest_size, a->dest_size + JIT_MEM_SIZE, 0);
    a->dest_end = (uint64_t)a->dest + JIT_MEM_SIZE;
    a->dest_size += JIT_MEM_SIZE;
  }
  asm_write(
      a, 4, instruction & ((1 << 8) - 1), instruction >> 8 & ((1 << 8) - 1),
      instruction >> 16 & ((1 << 8) - 1), instruction >> 24 & ((1 << 8) - 1));
  a->count++;
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

void asm_arm64_immmovk(microasm *a, uint8_t rd, uint16_t imm) {
  uint32_t instruction = 0xF2800000;

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

// NOTE: Jumps to imm * 4
void asm_arm64_b(microasm *a, uint32_t imm) {
  uint32_t instruction = 0x14000000;
  instruction |= imm & ((1 << 26) - 1);

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

// This man is the goat: https://www.youtube.com/watch?v=JM9jX2aqkog
void asm_write_exec(char *filename, microasm *bin) {
  const uint8_t mapper_bin[] = {
      0xc8, 0x1b, 0x80, 0xd2, 0x00, 0x00, 0x80, 0xd2, 0x01, 0xa6, 0x8e, 0xd2,
      0x62, 0x00, 0x80, 0xd2, 0x43, 0x04, 0x80, 0xd2, 0x04, 0x00, 0x80, 0x92,
      0x05, 0x00, 0x80, 0xd2, 0x01, 0x00, 0x00, 0xd4, 0x05, 0x00, 0x00, 0x94,
      0xe8, 0x1a, 0x80, 0xd2, 0x01, 0x00, 0x00, 0xd4, 0xa8, 0x0b, 0x80, 0xd2,
      0x00, 0x00, 0x80, 0xd2, 0x01, 0x00, 0x00, 0xd4};

  const size_t prog_len = sizeof(mapper_bin) + (bin->count * 4);

  Elf64_Ehdr elf_header = {.e_ident = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3,
                                       ELFCLASS64, ELFDATA2LSB, EV_CURRENT,
                                       ELFOSABI_SYSV, 0, 0, 0, 0, 0, 0, 0, 0},
                           .e_type = ET_EXEC,
                           .e_machine = EM_AARCH64,
                           .e_entry = 0x400078,
                           .e_phoff = 64,
                           .e_shoff = 0,
                           .e_flags = 0,
                           .e_ehsize = 64,
                           .e_phentsize = 56,
                           .e_phnum = 1,
                           .e_shnum = 0,
                           .e_shentsize = 0,
                           .e_shstrndx = SHN_UNDEF};

  Elf64_Phdr elf_phdr = {.p_type = PT_LOAD,
                         .p_offset = 0x78,
                         .p_vaddr = 0x400078,
                         .p_paddr = 0x400078,
                         .p_filesz = prog_len,
                         .p_memsz = prog_len,
                         .p_flags = PF_X | PF_R,
                         .p_align = 0x8};

  FILE *f = fopen(filename, "w");
  if (!f) {
    printf("failed to write binary!\n");
    exit(-1);
  }

  fwrite(&elf_header, 1, sizeof(elf_header), f);
  fwrite(&elf_phdr, 1, sizeof(elf_phdr), f);
  fwrite(mapper_bin, 1, sizeof(mapper_bin), f);
  fwrite(bin->dest - bin->count * 4, 1, bin->count * 4, f);

  chmod(filename, S_IRUSR | S_IWUSR | S_IXUSR);
  fclose(f);
}
