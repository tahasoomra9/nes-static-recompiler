#include "cpu.h"
#include "bus.h"
#include "ppu.h"
#include "apu.h"
#include <stdio.h>

Cpu cpu = {0};
static uint64_t trace_limit;
static uint64_t trace_count;
static FILE *trace_file;
static uint8_t external_irq_line;
static uint8_t observed_i = 1;
static uint8_t irq_delay;

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
    if (nmi_pending()) {
        cpu_nmi();
        ppu_nmi_ack();
    }
    if (cpu.i != observed_i) {
        observed_i = cpu.i;
        irq_delay = 1;
    }
    if (irq_delay != 0) {
        --irq_delay;
        return;
    }
    if (cpu_irq_line() && !cpu.i) {
        bus_write((uint16_t)(0x0100 | cpu.sp--), (uint8_t)(cpu.pc >> 8));
        bus_write((uint16_t)(0x0100 | cpu.sp--), (uint8_t)cpu.pc);
        bus_write((uint16_t)(0x0100 | cpu.sp--), status_byte());
        cpu.i = 1;
        cpu.pc = (uint16_t)(bus_read(0xfffe) | (bus_read(0xffff) << 8));
        cpu.cycles += 7;
        if (apu_irq_pending()) apu_clear_irq();
    }
}

void cpu_set_irq_line(uint8_t asserted)
{
    external_irq_line = asserted != 0;
}

uint8_t cpu_irq_line(void)
{
    return (uint8_t)(external_irq_line || apu_irq_pending());
}

void cpu_trace_configure(uint64_t limit, const char *path)
{
    trace_limit = limit;
    trace_count = 0;
    trace_file = stdout;
    if (path != NULL) {
        trace_file = fopen(path, "w");
        if (trace_file == NULL) trace_file = stdout;
    }
}

void cpu_trace_record(uint16_t instruction_pc)
{
    if (trace_count >= trace_limit || trace_file == NULL) return;
    fprintf(trace_file, "%04X A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%llu\n",
        instruction_pc, cpu.a, cpu.x, cpu.y, status_byte(), cpu.sp,
        (unsigned long long)cpu.cycles);
    ++trace_count;
}

void cpu_trace_close(void)
{
    if (trace_file != NULL && trace_file != stdout) fclose(trace_file);
    trace_file = NULL;
}
