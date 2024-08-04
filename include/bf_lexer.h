#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum token_t {
  ADD,
  SUB,
  INC_CUR,
  DEC_CUR,
  JUMP_IF_ZERO,
  JUMP_IF_NOT_ZERO,
  PRINT,
  INPUT
} token_t;

typedef struct Token {
  token_t token;
  uint32_t token_data;
} Token;
