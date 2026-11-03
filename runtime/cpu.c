#include "cpu.h"
#include "bus.h"
#include "ppu.h"

Cpu cpu = {0};

static uint8_t status_byte(void)
{
    return (uint8_t)(cpu.c | (cpu.z << 1) | (cpu.i << 2) |
        (cpu.d << 3) | 0x20 | (cpu.v << 6) | (cpu.n << 7));
}

void cpu_nmi(void)
{
    bus_write((uint16_t)(0x0100 | cpu.sp--), (uint8_t)(cpu.pc >> 8));
    bus_write((uint16_t)(0x0100 | cpu.sp--), (uint8_t)cpu.pc);
    bus_write((uint16_t)(0x0100 | cpu.sp--), status_byte());
    cpu.i = 1;
    cpu.pc = (uint16_t)(bus_read(0xfffa) | (bus_read(0xfffb) << 8));
    cpu.cycles += 7;
}

void cpu_safe_point(void)
{
    if (nmi_pending() && !cpu.i) {
        cpu_nmi();
        ppu_nmi_ack();
    }
}
