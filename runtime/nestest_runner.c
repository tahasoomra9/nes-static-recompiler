#include "bus.h"
#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    FILE *rom = fopen(argv[1], "rb");
    if (rom == NULL) return 2;
    unsigned char header[6];
    int valid = fread(header, 1, sizeof(header), rom) == sizeof(header);
    fclose(rom);
    if (!valid || header[0] != 'N' || header[1] != 'E' || header[2] != 'S' ||
        header[3] != 0x1a || !bus_load_prg(argv[1], header[4])) return 2;
    cpu.pc = 0xc000; cpu.sp = 0xfd; cpu.i = 1; cpu.cycles = 7;
    for (unsigned long i = 0; i < 1000000; ++i) {
        printf("%04X A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%llu\n",
               cpu.pc, cpu.a, cpu.x, cpu.y,
               (unsigned)(cpu.c | cpu.z << 1 | cpu.i << 2 | cpu.d << 3 |
                          0x20 | cpu.v << 6 | cpu.n << 7),
               cpu.sp, (unsigned long long)cpu.cycles);
        cpu_step();
    }
    return 0;
}
