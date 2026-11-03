#include <stdint.h>
#include <stdio.h>
#include "apu.h"
#include "bus.h"

int main(void)
{
    int16_t samples[32];
    apu_reset();
    bus_write(0x4000, 0x9f);
    bus_write(0x4002, 0x20);
    bus_write(0x4003, 0x08);
    bus_write(0x4015, 0x01);
    apu_step(20000);
    if ((bus_read(0x4015) & 1) == 0)
        return fprintf(stderr, "pulse length status is inactive\n"), 1;
    if (apu_pop_samples(samples, 32) == 0)
        return fprintf(stderr, "APU produced no samples\n"), 1;
    bus_write(0x4015, 0x00);
    if (bus_read(0x4015) & 1)
        return fprintf(stderr, "pulse channel did not disable\n"), 1;
    bus_write(0x4017, 0x80);
    bus_write(0x4010, 0x8f);
    bus_write(0x4011, 0x40);
    bus_write(0x4012, 0x00);
    bus_write(0x4013, 0x01);
    bus_write(0x4015, 0x10);
    apu_step(32);
    if ((bus_read(0x4015) & 0x10) == 0)
        return fprintf(stderr, "DMC status is inactive\n"), 1;
    apu_reset();
    bus_write(0x4017, 0x00);
    apu_step(14915);
    if ((bus_read(0x4015) & 0x40) == 0)
        return fprintf(stderr, "frame IRQ did not assert\n"), 1;
    bus_write(0x4017, 0x40);
    apu_step(14915);
    if (bus_read(0x4015) & 0x40)
        return fprintf(stderr, "frame IRQ inhibit failed\n"), 1;
    puts("P3 runtime checks passed");
    return 0;
}
