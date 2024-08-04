#include "stack.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
    printf("STACK: returning null...\n");
    return NULL;
  }
}
