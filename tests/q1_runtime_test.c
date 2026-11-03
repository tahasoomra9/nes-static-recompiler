#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bus.h"

static int fail(const char *message)
{
    fprintf(stderr, "Q1 failure: %s\n", message);
    return 1;
}

int main(void)
{
    uint8_t header[16] = {'N','E','S',0x1a,8,4,0x40,0,0,0,0,0,0,0,0,0};
    uint8_t prg[0x20000];
    uint8_t chr[0x8000];
    FILE *rom = fopen("build/q1-mmc3.nes", "wb");
    if (rom == NULL) return fail("could not create test ROM");
    for (unsigned bank = 0; bank < 16; ++bank)
        memset(prg + bank * 0x2000, (int)(0x10 + bank), 0x2000);
    for (unsigned bank = 0; bank < 32; ++bank)
        memset(chr + bank * 0x400, (int)(0x40 + bank), 0x400);
    fwrite(header, 1, sizeof header, rom);
    fwrite(prg, 1, sizeof prg, rom);
    fwrite(chr, 1, sizeof chr, rom);
    fclose(rom);

    if (!bus_load_prg("build/q1-mmc3.nes", 8))
        return fail("could not load MMC3 ROM");
    if (mapper_cpu_read(0x8000) != 0x10 ||
        mapper_cpu_read(0xa000) != 0x11 ||
        mapper_cpu_read(0xc000) != 0x1e ||
        mapper_cpu_read(0xe000) != 0x1f)
        return fail("MMC3 default PRG mapping mismatch");

    bus_write(0x8000, 0x06);
    bus_write(0x8001, 0x03);
    bus_write(0x8000, 0x07);
    bus_write(0x8001, 0x04);
    if (mapper_cpu_read(0x8000) != 0x13 || mapper_cpu_read(0xa000) != 0x14)
        return fail("MMC3 R6/R7 PRG mapping mismatch");

    bus_write(0x8000, 0x46);
    if (mapper_cpu_read(0x8000) != 0x1e ||
        mapper_cpu_read(0xc000) != 0x13)
        return fail("MMC3 PRG mode 1 mismatch");

    bus_write(0x8000, 0x00); bus_write(0x8001, 0x02);
    bus_write(0x8000, 0x01); bus_write(0x8001, 0x04);
    bus_write(0x8000, 0x02); bus_write(0x8001, 0x06);
    bus_write(0x8000, 0x03); bus_write(0x8001, 0x07);
    bus_write(0x8000, 0x04); bus_write(0x8001, 0x08);
    bus_write(0x8000, 0x05); bus_write(0x8001, 0x09);
    if (mapper_ppu_read(0x0000) != 0x42 ||
        mapper_ppu_read(0x0800) != 0x44 ||
        mapper_ppu_read(0x1000) != 0x46 ||
        mapper_ppu_read(0x1c00) != 0x49)
    {
        fprintf(stderr, "CHR got %02X %02X %02X %02X\n",
            mapper_ppu_read(0x0000), mapper_ppu_read(0x0800),
            mapper_ppu_read(0x1000), mapper_ppu_read(0x1c00));
        return fail("MMC3 CHR mapping mismatch");
    }
    bus_write(0x8000, 0x80);
    if (mapper_ppu_read(0x0000) != 0x46 ||
        mapper_ppu_read(0x0c00) != 0x49 ||
        mapper_ppu_read(0x1000) != 0x42)
        return fail("MMC3 CHR inversion mismatch");

    bus_write(0xa000, 1);
    bus_write(0xa001, 0x00);
    bus_write(0x6000, 0xaa);
    if (mapper_cpu_read(0x6000) != 0)
        return fail("MMC3 PRG-RAM disable mismatch");
    bus_write(0xa001, 0x80);
    bus_write(0x6000, 0x55);
    if (mapper_cpu_read(0x6000) != 0x55)
        return fail("MMC3 PRG-RAM enable mismatch");
    bus_write(0xa001, 0xc0);
    bus_write(0x6000, 0x33);
    if (mapper_cpu_read(0x6000) != 0x55)
        return fail("MMC3 PRG-RAM write protection mismatch");

    bus_write(0xc000, 7);
    bus_write(0xc001, 0);
    bus_write(0xe001, 0);
    bus_write(0xe000, 0);
    remove("build/q1-mmc3.nes");
    puts("Q1 runtime checks passed");
    return 0;
}
