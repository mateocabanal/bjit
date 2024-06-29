#include "bf.h"

bf_data *bf = 0;

uint8_t bf_get_data(uint8_t pos) { return bf->data[pos]; }
void bf_set_data(uint8_t pos, uint8_t value) { bf->data[pos] = value; }
