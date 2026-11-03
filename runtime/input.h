#ifndef NES_INPUT_H
#define NES_INPUT_H

#include <stdint.h>

void input_write(uint8_t value);
uint8_t input_read(void);
void input_set_button(unsigned button, int pressed);

#endif
