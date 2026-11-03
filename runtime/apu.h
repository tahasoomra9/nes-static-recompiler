#ifndef NES_APU_H
#define NES_APU_H

#include <stdint.h>

void apu_reset(void);
void apu_step(uint32_t cpu_cycles);
uint8_t apu_read(uint16_t addr);
void apu_write(uint16_t addr, uint8_t value);
int16_t apu_sample(void);
uint32_t apu_available_samples(void);
uint32_t apu_pop_samples(int16_t *samples, uint32_t count);
void apu_set_irq_callback(void (*callback)(void));
int apu_irq_pending(void);
void apu_clear_irq(void);

#endif
