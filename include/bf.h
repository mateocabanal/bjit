#include <stdint.h>

typedef struct bf_data {
  uint8_t *data;
  uint32_t position;
  uint64_t *loop_stack;
  uint32_t loop_pos;
} bf_data;

extern bf_data *bf;

uint8_t bf_get_data(uint8_t pos);
void bf_set_data(uint8_t pos, uint8_t value);
