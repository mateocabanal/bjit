#include <stdint.h>
typedef struct {
  uint8_t *dest;
} microasm;

void asm_write(microasm *a, int n, ...);

void asm_arm64_immadd(microasm *a, uint8_t rd, uint8_t rn, uint8_t imm);
void asm_arm64_regadd(microasm *a, uint8_t rd, uint8_t rn, uint8_t rm,
                      uint8_t imm_shift);
void asm_arm64_immsub(microasm *a, uint8_t rd, uint8_t rn, uint8_t imm);
void asm_arm64_regldrb(microasm *a, uint8_t rt, uint8_t rn);
void asm_arm64_regstrb(microasm *a, uint8_t rt, uint8_t rn);
void asm_arm64_regldr(microasm *a, uint8_t rt, uint8_t rn);
void asm_arm64_regstr(microasm *a, uint8_t rt, uint8_t rn);
void asm_arm64_immmov(microasm *a, uint8_t rn, uint16_t imm);
void asm_arm64_syscall(microasm *a, uint16_t imm);
void asm_arm64_regmov(microasm *a, uint8_t rd, uint8_t rm);
void asm_arm64_immcmp(microasm *a, uint8_t rn, uint16_t imm);
void asm_arm64_pcrelbranch_ne(microasm *a, uint32_t imm);
void asm_arm64_pcrelbranch_ze(microasm *a, uint8_t rt, uint32_t imm);
void asm_arm64_br(microasm *a, uint8_t rn);
void asm_arm64_getpcval(microasm *a, uint8_t rd);
void asm_return(microasm *a);