#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bus.h"
#include "cpu.h"
#include "ppu.h"

static int fail(const char *message)
{
    fprintf(stderr, "P0 failure: %s\n", message);
    return 1;
}

int main(void)
{
    static const uint8_t chr[0x2000] = {0};
    uint8_t program[0x4000];
    uint8_t header[16] = {'N', 'E', 'S', 0x1a, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    FILE *rom;
    memset(program, 0, sizeof(program));
    program[0] = 0xa9;
    program[1] = 0x3c;
    program[2] = 0x85;
    program[3] = 0x10;
    program[4] = 0xa7;
    program[5] = 0x10;
    program[6] = 0x60;
    bus_set_prg(program, sizeof(program));
    cpu.pc = 0x8000;
    cpu.sp = 0xfd;
    cpu_step();
    cpu_step();
    cpu_step();
    if (cpu.a != 0x3c || cpu.x != 0x3c || bus_read(0x10) != 0x3c)
        return fail("LAX did not load A and X");

    ppu_load_chr(chr, sizeof(chr));
    ppu_set_chr_writable(1);
    mapper_ppu_write(0x0012, 0xa5);
    if (mapper_ppu_read(0x0012) != 0xa5)
        return fail("CHR-RAM is not writable");

    mapper_cpu_write(0x6000, 0x5a);
    if (mapper_cpu_read(0x6000) != 0x5a)
        return fail("PRG-RAM is not mapped");
    if (current_prg_bank(0x8000) != 0)
        return fail("NROM bank identity is not stable");

    rom = fopen("build/p0-save-test.nes", "wb");
    if (rom == NULL) return fail("could not create save test ROM");
    fwrite(header, 1, sizeof(header), rom);
    fwrite(program, 1, sizeof(program), rom);
    fclose(rom);
    if (!bus_load_prg("build/p0-save-test.nes", 1))
        return fail("could not load save test ROM");
    bus_write(0x6000, 0x6b);
    bus_save_prg_ram();
    if (!bus_load_prg("build/p0-save-test.nes", 1) ||
        bus_read(0x6000) != 0x6b)
        return fail("battery PRG-RAM did not persist");
    remove("build/p0-save-test.nes");
    remove("build/p0-save-test.nes.sav");
    puts("P0 runtime checks passed");
    return 0;
}
