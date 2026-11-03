#include <stdio.h>
#include "bus.h"
#include "input.h"
#include "ppu.h"

static int fail(const char *message)
{
    fprintf(stderr, "%s\n", message);
    return 1;
}

int main(void)
{
    input_set_button(0, 1);
    input_set_button(7, 1);
    input_write(1);
    input_write(0);
    if (input_read() != 1 || input_read() != 0) return fail("controller A/right mismatch");
    for (int i = 0; i < 6; ++i) input_read();
    if (input_read() != 1) return fail("controller right mismatch");

    ppu_write(0x2006, 0x3f);
    ppu_write(0x2006, 0x00);
    ppu_write(0x2007, 0x2a);
    ppu_write(0x2006, 0x3f);
    ppu_write(0x2006, 0x10);
    if (ppu_read(0x2007) != 0x2a) return fail("palette mirror mismatch");

    ppu_write(0x2000, 0x04);
    ppu_write(0x2006, 0x20);
    ppu_write(0x2006, 0x00);
    ppu_write(0x2007, 0x11);
    ppu_write(0x2007, 0x22);
    ppu_write(0x2006, 0x20);
    ppu_write(0x2006, 0x00);
    (void)ppu_read(0x2007);
    if (ppu_read(0x2007) != 0x11) return fail("PPU increment mismatch");
    if (ppu_read(0x2007) != 0x22) return fail("PPU increment second read mismatch");

    ppu_write(0x2006, 0x20);
    ppu_write(0x2006, 0x40);
    for (int i = 0; i < 8; ++i) ppu_write(0x2007, (uint8_t)(0xa0 + i));
    ppu_write(0x2006, 0x20);
    ppu_write(0x2006, 0x40);
    (void)ppu_read(0x2007);
    for (int i = 0; i < 8; ++i)
        if (ppu_read(0x2007) != (uint8_t)(0xa0 + i))
            return fail("PPU sequential write/read mismatch");

    ppu_write(0x2006, 0x20);
    ppu_write(0x2006, 0x00);
    ppu_write(0x2005, 0x12);
    (void)ppu_read(0x2002);
    ppu_write(0x2006, 0x21);
    ppu_write(0x2006, 0x00);

    ppu_set_mirroring(1);
    ppu_write(0x2006, 0x20);
    ppu_write(0x2006, 0x00);
    ppu_write(0x2007, 0x31);
    ppu_write(0x2006, 0x28);
    ppu_write(0x2006, 0x00);
    (void)ppu_read(0x2007);
    if (ppu_read(0x2007) != 0x31) return fail("vertical nametable mirror mismatch");
    ppu_set_mirroring(0);
    ppu_write(0x2006, 0x20);
    ppu_write(0x2006, 0x01);
    ppu_write(0x2007, 0x42);
    ppu_write(0x2006, 0x24);
    ppu_write(0x2006, 0x01);
    (void)ppu_read(0x2007);
    if (ppu_read(0x2007) != 0x42) return fail("horizontal nametable mirror mismatch");

    ppu_set_chr_writable(0);
    ppu_write(0x2006, 0x00);
    ppu_write(0x2006, 0x00);
    ppu_write(0x2007, 0x5a);
    ppu_set_chr_writable(1);
    ppu_write(0x2006, 0x00);
    ppu_write(0x2006, 0x00);
    ppu_write(0x2007, 0x5a);

    bus_write(0x0200, 0xa5);
    bus_write(0x4014, 0x02);
    ppu_write(0x2003, 0x00);
    if (ppu_read(0x2004) != 0xa5) return fail("OAM DMA page mismatch");
    return 0;
}
