#include "bus.h"
#include "input.h"
#include "ppu.h"
#include "apu.h"

static uint8_t ram[0x800];

int mapper0_load_rom(const char *path);
void mapper0_set_prg(const uint8_t *data, uint32_t size);
void mapper0_save(void);

uint8_t bus_read(uint16_t addr)
{
    if (addr < 0x2000) return ram[addr & 0x07ff];
    if (addr >= 0x2000 && addr < 0x4000) {
        ppu_step(1);
        return ppu_read(addr);
    }
    if (addr == 0x4016) return input_read();
    if (addr == 0x4015) return apu_read(addr);
    if (addr >= 0x4000 && addr <= 0x4017) return apu_read(addr);
    if (addr >= 0x6000) return mapper_cpu_read(addr);
    return 0;
}

void bus_write(uint16_t addr, uint8_t val)
{
    if (addr < 0x2000) {
        ram[addr & 0x07ff] = val;
    } else if (addr >= 0x2000 && addr < 0x4000) {
        ppu_step(1);
        ppu_write(addr, val);
    } else if (addr == 0x4014) {
        uint8_t page[256];
        uint16_t base = (uint16_t)val << 8;
        for (unsigned i = 0; i < 256; ++i)
            page[i] = bus_read((uint16_t)(base + i));
        ppu_dma(page);
    } else if (addr == 0x4016) {
        input_write(val);
    } else if (addr >= 0x4000 && addr <= 0x4017) {
        apu_write(addr, val);
    } else if (addr >= 0x6000) {
        mapper_cpu_write(addr, val);
    }
}

int bus_load_prg(const char *path, uint8_t prg_banks)
{
    (void)prg_banks;
    return mapper0_load_rom(path);
}

void bus_set_prg(const uint8_t *data, uint32_t size)
{
    mapper0_set_prg(data, size);
}

void bus_save_prg_ram(void)
{
    mapper0_save();
}
