#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bus.h"
#include "cpu.h"

static void put_word(uint8_t *rom, uint16_t address, uint16_t value)
{
    uint32_t offset = (uint32_t)(address - 0x8000);
    rom[offset] = (uint8_t)value;
    rom[offset + 1] = (uint8_t)(value >> 8);
}

static void fake_mapper_tick(void)
{
    if (cpu.cycles == 2)
        cpu_set_irq_line(1);
}

int main(void)
{
    uint8_t rom[0x8000];
    memset(rom, 0xea, sizeof rom);
    rom[0x0000] = 0x58; /* CLI at $8000 */
    rom[0x0001] = 0xea; /* delayed instruction at $8001 */
    rom[0x0002] = 0xea;
    rom[0x1000] = 0xea; /* IRQ handler at $9000 */
    put_word(rom, 0xfffc, 0x8000);
    put_word(rom, 0xfffe, 0x9000);
    bus_set_prg(rom, sizeof rom);

    memset(&cpu, 0, sizeof cpu);
    cpu.pc = 0x8000;
    cpu.sp = 0xfd;
    cpu.i = 1;
    cpu_set_irq_line(0);

    cpu_step();
    fake_mapper_tick();
    cpu_safe_point();
    if (cpu.pc != 0x8001)
        return fprintf(stderr, "IRQ fired during CLI latency at %04X\n", cpu.pc), 1;

    cpu_step();
    cpu_safe_point();
    if (cpu.pc != 0x9000 || cpu.sp != 0xfa)
        return fprintf(stderr, "IRQ vector/stack mismatch: PC=%04X SP=%02X\n",
            cpu.pc, cpu.sp), 1;
    if (bus_read(0x01fb) & 0x10)
        return fprintf(stderr, "IRQ pushed status with B set\n"), 1;
    if (bus_read(0x01fd) != 0x80 || bus_read(0x01fc) != 0x02)
        return fprintf(stderr, "IRQ pushed wrong return PC\n"), 1;

    cpu_set_irq_line(0);
    puts("Q0 runtime checks passed");
    return 0;
}
