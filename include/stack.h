#pragma once
#include <stdint.h>

typedef struct {
  uint32_t maxSize;
  uint32_t size;
  // Pointer to an array of ptrs
  void **data;
} Stack;

Stack stack_init(uint32_t size);
void stack_push(Stack *stack, void *item);
void *stack_pop(Stack *stack);
