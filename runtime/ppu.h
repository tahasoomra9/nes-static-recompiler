#ifndef NES_PPU_H
#define NES_PPU_H

#include <stdbool.h>
#include <stdint.h>

#define NES_W 256
#define NES_H 240

extern uint32_t framebuffer[NES_W * NES_H];

void ppu_step(uint32_t cpu_cycles);
bool nmi_pending(void);
uint8_t ppu_read(uint16_t addr);
void ppu_write(uint16_t addr, uint8_t value);
void ppu_dma(const uint8_t *page);
const uint32_t *ppu_framebuffer(void);
int ppu_frame_ready(void);
void ppu_clear_frame_ready(void);
void ppu_load_chr(const uint8_t *data, uint32_t size);
void ppu_set_chr_writable(int writable);
uint8_t ppu_chr_read(uint16_t addr);
void ppu_chr_write(uint16_t addr, uint8_t value);
void ppu_set_write_log_frames(unsigned frames);
void ppu_set_nametable_dump_frame(unsigned frame);
void ppu_set_mirroring(int vertical);
void ppu_set_mirroring_mode(int mode);
void ppu_nmi_ack(void);
void ppu_fill_test_gradient(void);
void ppu_set_ppm_dump(unsigned frame, const char *path);
int ppu_ppm_dump_complete(void);
void ppu_set_scroll_log_frame(unsigned frame);
void ppu_set_bg_log(int enabled);
void ppu_set_frame_log(int enabled);
void ppu_set_frame_log(int enabled);

#endif
