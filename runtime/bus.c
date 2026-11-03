#include "bus.h"
#include <stdio.h>
#include "input.h"
#include "ppu.h"

static uint8_t ram[0x800];
static uint8_t prg[0x8000];
static uint32_t prg_size;

uint8_t bus_read(uint16_t addr)
{
    if (addr < 0x2000) {
        return ram[addr & 0x07ff];
    }
    if (addr >= 0x2000 && addr < 0x4000) {
        ppu_step(1);
        return ppu_read(addr);
    }
    if (addr == 0x4016) return input_read();
    if (addr >= 0x8000 && prg_size != 0) {
        uint32_t offset = (uint32_t)(addr - 0x8000);
        if (prg_size == 0x4000) {
            offset &= 0x3fff;
        }
        return prg[offset];
    }
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
        for (unsigned i = 0; i < 256; ++i) page[i] = bus_read((uint16_t)(base + i));
        ppu_dma(page);
    } else if (addr == 0x4016) {
        input_write(val);
    }
}

int bus_load_prg(const char *path, uint8_t prg_banks)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL || prg_banks < 1 || prg_banks > 2) {
        if (file != NULL) fclose(file);
        return 0;
    }
    uint8_t header[16];
    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        header[0] != 'N' || header[1] != 'E' || header[2] != 'S' ||
        header[3] != 0x1a) {
        fclose(file);
        return 0;
    }
    uint16_t mapper = (uint16_t)((header[6] >> 4) | (header[7] & 0xf0));
    if ((header[7] & 0x0c) == 0x08) {
        mapper |= (uint16_t)((header[8] & 0x0f) << 8);
    }
    if (mapper != 0 || header[4] < 1 || header[4] > 2) {
        fprintf(stderr,
            "Unsupported ROM: mapper %u, PRG banks %u; this runtime supports "
            "only mapper 0 with 1 or 2 PRG banks.\n", mapper, header[4]);
        fclose(file);
        return 0;
    }
    prg_banks = header[4];
    if (fseek(file, (header[6] & 0x04) ? 528 : 16, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    prg_size = (uint32_t)prg_banks * 0x4000;
    ppu_set_mirroring((header[6] & 1) != 0);
    size_t read = fread(prg, 1, prg_size, file);
    if (read == prg_size) {
        uint8_t chr_banks = header[5];
        uint8_t chr_data[0x2000];
        if (chr_banks > 0 &&
            fread(chr_data, 1, sizeof(chr_data), file) == sizeof(chr_data)) {
            ppu_load_chr(chr_data, sizeof(chr_data));
            ppu_set_chr_writable(0);
        } else if (chr_banks == 0) {
            ppu_set_chr_writable(1);
        }
    }
    fclose(file);
    return read == prg_size;
}
