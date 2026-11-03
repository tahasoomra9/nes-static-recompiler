#include "input.h"

static uint8_t buttons;
static uint8_t shift;
static uint8_t strobe;

void input_write(uint8_t value)
{
    strobe = value & 1;
    if (strobe) shift = buttons;
}
uint8_t input_read(void)
{
    uint8_t value = shift & 1;
    if (strobe) shift = buttons;
    else shift = (uint8_t)((shift >> 1) | 0x80);
    return value;
}
void input_set_button(unsigned button, int pressed)
{
    if (button < 8) {
        if (pressed) buttons |= (uint8_t)(1u << button);
        else buttons &= (uint8_t)~(1u << button);
    }
}
