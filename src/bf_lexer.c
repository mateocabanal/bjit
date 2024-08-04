#include "bf_lexer.h"
#include "stack.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#define ARR_INC_SIZE (1024)

Token *tokenize_bf(FILE *bf_file) {

  int next_tok_loc = 0;
  int cur_bf_tok_size = 1024;
  Token *bf_tokens = malloc(sizeof(Token) * cur_bf_tok_size);

  int oper_bin;

  while ((oper_bin = fgetc(bf_file)) != EOF) {
    if (next_tok_loc == cur_bf_tok_size) {
      bf_tokens = realloc(bf_tokens, cur_bf_tok_size + ARR_INC_SIZE);
      cur_bf_tok_size += ARR_INC_SIZE;
    }

    char oper = (char)oper_bin;
    Stack s_loops = stack_init(1024 * 8);

    uint32_t loop_id = 0;

    switch (oper) {
    case '>': {
      int count = 1;
      int ch_int = 0;
      while ((ch_int = fgetc(bf_file)) != EOF) {
        char ch = (char)ch_int;

        if (ch != '>') {
          ungetc(ch, bf_file);
          break;
        }

        count++;
      }

      Token token = {.token = INC_CUR, .token_data = count};
      bf_tokens[next_tok_loc] = token;
      break;
    }

    case '<': {
      int count = 1;
      int ch_int = 0;
      while ((ch_int = fgetc(bf_file)) != EOF) {
        char ch = (char)ch_int;

        if (ch != '<') {
          ungetc(ch, bf_file);
          break;
        }

        count++;
      }

      Token token = {.token = DEC_CUR, .token_data = count};
      bf_tokens[next_tok_loc] = token;
      break;
    }

    case '+': {
      int count = 1;
      int ch_int = 0;
      while ((ch_int = fgetc(bf_file)) != EOF) {
        char ch = (char)ch_int;

        if (ch != '+') {
          ungetc(ch, bf_file);
          break;
        }

        count++;
      }

      Token token = {.token = ADD, .token_data = count};
      bf_tokens[next_tok_loc] = token;
      break;
    }

    case '-': {
      int count = 1;
      int ch_int = 0;
      while ((ch_int = fgetc(bf_file)) != EOF) {
        char ch = (char)ch_int;

        if (ch != '-') {
          ungetc(ch, bf_file);
          break;
        }

        count++;
      }

      Token token = {.token = SUB, .token_data = count};
      bf_tokens[next_tok_loc] = token;
      break;
    }

    case '[': {
      uint32_t *cur_loop_pos = malloc(4);
      *cur_loop_pos = loop_id++;
      stack_push(&s_loops, &cur_loop_pos);

      Token token = {.token = JUMP_IF_ZERO, .token_data = loop_id};
      bf_tokens[next_tok_loc] = token;
      break;
    }

    case ']': {
      uint32_t *relative_loop_pos = stack_pop(&s_loops);

      Token token = {.token = JUMP_IF_NOT_ZERO,
                     .token_data = *relative_loop_pos};
      bf_tokens[next_tok_loc] = token;
      break;
    }
    }

    next_tok_loc++;
  }

  return NULL;
}
