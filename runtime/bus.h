#ifndef NES_BUS_H
#define NES_BUS_H

#include <stdint.h>

uint8_t bus_read(uint16_t addr);
void bus_write(uint16_t addr, uint8_t val);
int bus_load_prg(const char *path, uint8_t prg_banks);
void bus_set_prg(const uint8_t *data, uint32_t size);
void bus_save_prg_ram(void);
uint8_t mapper_cpu_read(uint16_t addr);
void mapper_cpu_write(uint16_t addr, uint8_t val);
uint8_t mapper_ppu_read(uint16_t addr);
void mapper_ppu_write(uint16_t addr, uint8_t val);
void mapper_scanline(void);
void mapper_ppu_a12(uint8_t high, uint8_t rendering);
uint32_t current_prg_bank(uint16_t addr);

#endif
