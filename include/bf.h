#include <stdint.h>

typedef struct bf_data {
  uint8_t *data;
  uint32_t position;
} bf_data;

extern bf_data *bf;

uint8_t bf_get_data(uint8_t pos);
void bf_set_data(uint8_t pos, uint8_t value);
