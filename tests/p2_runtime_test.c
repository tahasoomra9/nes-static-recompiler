#include <stdio.h>
#include "bus.h"
#include "ppu.h"
#include "cpu.h"

static int fail(const char *message)
{
    fprintf(stderr, "P2 failure: %s\n", message);
    return 1;
}

static void mmc1_write(uint16_t address, uint8_t value)
{
    for (unsigned bit = 0; bit < 5; ++bit) {
        bus_write(address, (uint8_t)((value >> bit) & 1));
        cpu.cycles += 2;
    }
}

int main(void)
{
    uint8_t header[16] = {'N','E','S',0x1a,4,2,0x10,0,0,0,0,0,0,0,0,0};
    uint8_t prg[0x10000];
    uint8_t chr[0x4000];
    FILE *rom;
    for (unsigned bank = 0; bank < 4; ++bank)
        for (unsigned i = 0; i < 0x4000; ++i)
            prg[bank * 0x4000 + i] = (uint8_t)(0x10 + bank);
    for (unsigned bank = 0; bank < 4; ++bank)
        for (unsigned i = 0; i < 0x1000; ++i)
            chr[bank * 0x1000 + i] = (uint8_t)(0x40 + bank);
    rom = fopen("build/p2-mmc1.nes", "wb");
    if (rom == NULL) return fail("could not create test ROM");
    fwrite(header, 1, sizeof(header), rom);
    fwrite(prg, 1, sizeof(prg), rom);
    fwrite(chr, 1, sizeof(chr), rom);
    fclose(rom);
    if (!bus_load_prg("build/p2-mmc1.nes", 4))
        return fail("could not load MMC1 ROM");

    mmc1_write(0x8000, 0x0c);
    mmc1_write(0xe000, 1);
    if (mapper_cpu_read(0x8000) != 0x11 ||
        mapper_cpu_read(0xc000) != 0x13) {
        fprintf(stderr, "got %02X %02X bank %u\n",
            mapper_cpu_read(0x8000), mapper_cpu_read(0xc000),
            current_prg_bank(0x8000));
        return fail("MMC1 PRG mode 3 mapping mismatch");
    }

    mmc1_write(0x8000, 0x00);
    mmc1_write(0xe000, 2);
    if (mapper_cpu_read(0x8000) != 0x11 ||
        mapper_cpu_read(0xc000) != 0x12) {
        return fail("MMC1 32 KiB PRG mapping mismatch");
    }

    mmc1_write(0x8000, 0x10);
    mmc1_write(0xa000, 1);
    if (mapper_ppu_read(0x0000) != 0x41)
        return fail("MMC1 4 KiB CHR mapping mismatch");

    cpu.cycles = 100;
    for (unsigned i = 0; i < 4; ++i) {
        bus_write(0xe000, 0);
        cpu.cycles += 2;
    }
    cpu.cycles--;
    bus_write(0xe000, 1);
    if (mapper_cpu_read(0x8000) != 0x11)
        return fail("MMC1 consecutive write was not ignored");

    remove("build/p2-mmc1.nes");
    puts("P2 runtime checks passed");
    return 0;
}
