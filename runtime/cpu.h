#ifndef NES_CPU_H
#define NES_CPU_H

#include <stdint.h>

typedef struct {
    uint8_t a, x, y, sp;
    uint16_t pc;
    uint8_t n, v, d, i, z, c;
    uint64_t cycles;
} Cpu;

extern Cpu cpu;
void cpu_step(void);
void cpu_nmi(void);
void cpu_safe_point(void);

#endif
