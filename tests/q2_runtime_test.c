#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bus.h"
#include "cpu.h"

static int fail(const char *message)
{
    fprintf(stderr, "Q2 failure: %s\n", message);
    return 1;
}

int main(void)
{
    uint8_t header[16] = {'N','E','S',0x1a,2,1,0x40,0,0,0,0,0,0,0,0,0};
    uint8_t prg[0x8000];
    uint8_t chr[0x2000];
    FILE *rom = fopen("build/q2-mmc3.nes", "wb");
    if (rom == NULL) return fail("could not create test ROM");
    memset(prg, 0xea, sizeof prg);
    memset(chr, 0, sizeof chr);
    fwrite(header, 1, sizeof header, rom);
    fwrite(prg, 1, sizeof prg, rom);
    fwrite(chr, 1, sizeof chr, rom);
    fclose(rom);
    if (!bus_load_prg("build/q2-mmc3.nes", 2))
        return fail("could not load MMC3 ROM");

    cpu_set_irq_line(0);
    bus_write(0xc000, 1);
    bus_write(0xc001, 0);
    bus_write(0xe001, 0);

    mapper_ppu_a12(0, 1);
    for (unsigned i = 0; i < 7; ++i) mapper_ppu_a12(0, 1);
    mapper_ppu_a12(1, 1);
    if (cpu_irq_line())
        return fail("short A12 low interval clocked IRQ counter");
    for (unsigned i = 0; i < 9; ++i) mapper_ppu_a12(0, 1);
    mapper_ppu_a12(1, 1);
    if (cpu_irq_line())
        return fail("first valid A12 edge asserted IRQ too early");
    for (unsigned i = 0; i < 9; ++i) mapper_ppu_a12(0, 1);
    mapper_ppu_a12(1, 1);
    if (!cpu_irq_line())
        return fail("filtered A12 edge did not assert IRQ");

    bus_write(0xe000, 0);
    if (cpu_irq_line())
        return fail("E000 did not acknowledge IRQ");
    bus_write(0xc001, 0);
    for (unsigned i = 0; i < 9; ++i) mapper_ppu_a12(0, 1);
    mapper_ppu_a12(1, 0);
    if (cpu_irq_line())
        return fail("A12 clocked while rendering disabled");

    remove("build/q2-mmc3.nes");
    puts("Q2 runtime checks passed");
    return 0;
}
