#ifndef NES_BUS_H
#define NES_BUS_H

#include <stdint.h>

uint8_t bus_read(uint16_t addr);
void bus_write(uint16_t addr, uint8_t val);
int bus_load_prg(const char *path, uint8_t prg_banks);
void bus_set_prg(const uint8_t *data, uint32_t size);

#endif
