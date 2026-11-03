#include <stdio.h>
#include "bus.h"

int main(void)
{
    unsigned char prg[0x8000] = {0};
    prg[0x0000] = 0x11;
    prg[0x4000] = 0x22;
    prg[0x7ffc] = 0x00;
    prg[0x7ffd] = 0xC0;
    bus_set_prg(prg, sizeof(prg));
    if (mapper_cpu_read(0x8000) != 0x11)
        return fprintf(stderr, "initial UxROM bank mismatch\n"), 1;
    mapper_cpu_write(0x8000, 1);
    if (mapper_cpu_read(0x8000) != 0x22)
        return fprintf(stderr, "UxROM bank switch mismatch\n"), 1;
    if (current_prg_bank(0x8000) != 1)
        return fprintf(stderr, "UxROM current bank mismatch\n"), 1;
    if (mapper_cpu_read(0xC000) != 0x22)
        return fprintf(stderr, "UxROM fixed bank mismatch\n"), 1;
    puts("P1 runtime checks passed");
    return 0;
}
