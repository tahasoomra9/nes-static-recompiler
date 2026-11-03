#include "ppu.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"
#include "bus.h"

static uint8_t regs[8];
static uint8_t vram[0x800];
static uint8_t palette[32];
static uint8_t oam[256];
static uint8_t chr[0x2000];
uint32_t framebuffer[NES_W * NES_H];
_Static_assert(sizeof(framebuffer) / sizeof(framebuffer[0]) == NES_W * NES_H,
    "framebuffer dimensions must match NES_W and NES_H");
static uint16_t v, t;
static uint8_t x, w, read_buffer;
static uint32_t dots;
static uint16_t scanline;
static int frame_ready;
static int nmi;
static int vertical_mirroring;
static int chr_writable;
static unsigned frame_number;
static unsigned write_log_frames;
static unsigned nametable_dump_frame;
static int nametable_dumped;
static unsigned ppm_dump_frame;
static const char *ppm_dump_path;
static int ppm_dump_complete;
static unsigned scroll_log_frame;
static int scroll_log_enabled;
static int frame_log_enabled;
static int bg_log_enabled;
static unsigned rendered_scanlines;
static unsigned frame_done_count;
static unsigned frame_min_y;
static unsigned frame_max_y;
static uint16_t line_render_v;
static const uint32_t nes_palette[64] = {
    0xff545454, 0xff001e74, 0xff081090, 0xff300088,
    0xff440064, 0xff5c0030, 0xff540400, 0xff3c1800,
    0xff202a00, 0xff083a00, 0xff004000, 0xff003c00,
    0xff00323c, 0xff000000, 0xff000000, 0xff000000,
    0xff989698, 0xff084cc4, 0xff3032ec, 0xff5c1ee4,
    0xff8814b0, 0xffa01464, 0xff982220, 0xff783c00,
    0xff545a00, 0xff287200, 0xff087c00, 0xff007628,
    0xff006678, 0xff000000, 0xff000000, 0xff000000,
    0xffeceeec, 0xff4c9aec, 0xff787cec, 0xffb062ec,
    0xffd454d4, 0xffe45484, 0xffec6a64, 0xffd48820,
    0xffa0aa00, 0xff74cc00, 0xff4cd020, 0xff38c878,
    0xff38b4cc, 0xff3c3c3c, 0xff000000, 0xff000000,
    0xffeceeec, 0xffa8ccec, 0xffbcbcec, 0xffd4b2ec,
    0xffe4b4ec, 0xffecb4b4, 0xffecbca0, 0xffe4c490,
    0xffccd080, 0xffb0d890, 0xffa8e0a8, 0xffa8d8c0,
    0xffa8ccd8, 0xffb0b0b0, 0xff000000, 0xff000000
};

static uint16_t increment(void) { return (regs[0] & 4) ? 32 : 1; }
static void increment_vertical(void)
{
    if ((v & 0x7000) != 0x7000) {
        v = (uint16_t)(v + 0x1000);
        return;
    }
    v &= (uint16_t)~0x7000;
    unsigned coarse_y = (v >> 5) & 0x1f;
    if (coarse_y == 29) {
        coarse_y = 0;
        v ^= 0x0800;
    } else if (coarse_y == 31) {
        coarse_y = 0;
    } else {
        ++coarse_y;
    }
    v = (uint16_t)((v & (uint16_t)~0x03e0) | (coarse_y << 5));
}
static uint16_t nametable_offset(uint16_t addr)
{
    addr &= 0x3fff;
    if (addr >= 0x3000 && addr < 0x3f00) addr = (uint16_t)(addr - 0x1000);
    uint16_t table = (uint16_t)((addr - 0x2000) >> 10);
    uint16_t offset = (uint16_t)((addr - 0x2000) & 0x03ff);
    uint16_t physical;
    if (vertical_mirroring < 2)
        physical = vertical_mirroring ? (table & 1) : (table >> 1);
    else if (vertical_mirroring == 2)
        physical = 0;
    else if (vertical_mirroring == 3)
        physical = 1;
    else
        physical = table & 1;
    return (uint16_t)(physical * 0x400 + offset);
}
static uint8_t ppu_mem_read(uint16_t addr)
{
    addr &= 0x3fff;
    if (addr < 0x2000) return chr[addr];
    if (addr < 0x3f00) return vram[nametable_offset(addr)];
    if (addr >= 0x3f20) addr = (uint16_t)(0x3f00 + ((addr - 0x3f00) & 31));
    addr = (uint16_t)((addr - 0x3f00) & 31);
    if ((addr & 3) == 0) addr &= 0x0f;
    return palette[addr];
}
uint8_t ppu_chr_read(uint16_t addr) { return chr[addr & 0x1fff]; }
void ppu_chr_write(uint16_t addr, uint8_t value)
{
    if (chr_writable) chr[addr & 0x1fff] = value;
}
static uint8_t background_pixel(uint16_t render_v, unsigned px, unsigned line)
{
    unsigned world_x = ((unsigned)(render_v & 0x1f) << 3) + x + px;
    unsigned world_y = (((unsigned)(render_v >> 12) & 7) +
        ((unsigned)((render_v >> 5) & 0x1f) << 3));
    unsigned nt_x = (world_x >> 8) & 1;
    unsigned nt_y = (world_y / NES_H) & 1;
    unsigned nt = ((render_v >> 10) & 3) ^ nt_x ^ (nt_y << 1);
    unsigned tile_x = (world_x >> 3) & 31;
    unsigned tile_y = (world_y >> 3) % 30;
    uint16_t name_addr = (uint16_t)(0x2000 + nt * 0x400 + tile_y * 32 + tile_x);
    uint8_t name = ppu_mem_read(name_addr);
    unsigned row = world_y & 7;
    unsigned col = world_x & 7;
    unsigned table = (regs[0] & 0x10) ? 0x1000 : 0;
    uint16_t pattern = (uint16_t)(table + name * 16 + row);
    uint8_t bits = (uint8_t)(((chr[pattern + 8] >> (7 - col)) & 1) << 1 |
        ((chr[pattern] >> (7 - col)) & 1));
    if (bits == 0) return 0;
    unsigned attr = 0x2000 + nt * 0x400 + 0x3c0 + (tile_y / 4) * 8 + tile_x / 4;
    uint8_t attribute = ppu_mem_read((uint16_t)attr);
    unsigned quadrant = ((tile_y & 2) ? 2 : 0) | (tile_x & 2 ? 1 : 0);
    return (uint8_t)(bits | (((attribute >> (quadrant * 2)) & 3) << 2));
}
static void log_bg_fetches(uint16_t render_v)
{
    if (!bg_log_enabled || scanline != 100) return;
    for (unsigned tile = 0; tile < 32; ++tile) {
        uint16_t tile_v = render_v;
        for (unsigned i = 0; i < tile; ++i) {
            if ((tile_v & 0x001f) == 31) {
                tile_v &= (uint16_t)~0x001f;
                tile_v ^= 0x0400;
            } else {
                ++tile_v;
            }
        }
        uint16_t address = (uint16_t)(0x2000 | (tile_v & 0x0fff));
        printf("PPU-BG frame=%u scanline=%u x=%u v=%04X coarse_x=%u nt=%04X tile=%02X\n",
            frame_number, scanline, tile * 8, tile_v, tile_v & 0x1f,
            address, ppu_mem_read(address));
    }
}
static void log_scroll_line(uint16_t render_v)
{
    if (!scroll_log_enabled || frame_number != scroll_log_frame) return;
    printf("PPU-SCROLL frame=%u scanline=%u v=%04X t=%04X x=%u bg=",
        frame_number, scanline, render_v, t, x);
    for (unsigned i = 0; i < 8; ++i)
        printf("%02X%s", background_pixel(render_v, i, scanline),
            i == 7 ? "\n" : " ");
}
static void ppu_mem_write(uint16_t addr, uint8_t value)
{
    addr &= 0x3fff;
    if (addr < 0x2000) {
        if (chr_writable) chr[addr] = value;
    } else if (addr < 0x3f00) vram[nametable_offset(addr)] = value;
    else {
        if (addr >= 0x3f20) addr = (uint16_t)(0x3f00 + ((addr - 0x3f00) & 31));
        addr = (uint16_t)((addr - 0x3f00) & 31);
        if ((addr & 3) == 0) addr &= 0x0f;
        palette[addr] = value & 0x3f;
    }
}
static void dump_nametable(void)
{
    if (nametable_dumped || frame_number != nametable_dump_frame) return;
    nametable_dumped = 1;
    printf("PPU nametable frame %u\n", frame_number);
    for (unsigned row = 0; row < 30; ++row) {
        printf("%02u:", row);
        for (unsigned col = 0; col < 32; ++col)
            printf(" %02X", vram[row * 32 + col]);
        putchar('\n');
    }
}
static void dump_ppm(void)
{
    FILE *file;
    if (ppm_dump_path == NULL || frame_number != ppm_dump_frame) return;
    file = fopen(ppm_dump_path, "wb");
    if (file == NULL) return;
    fprintf(file, "P6\n%u %u\n255\n", NES_W, NES_H);
    for (unsigned y = 0; y < NES_H; ++y) {
        for (unsigned x_pos = 0; x_pos < NES_W; ++x_pos) {
            uint32_t pixel = framebuffer[y * NES_W + x_pos];
            fputc((int)((pixel >> 16) & 0xff), file);
            fputc((int)((pixel >> 8) & 0xff), file);
            fputc((int)(pixel & 0xff), file);
        }
    }
    fclose(file);
    ppm_dump_path = NULL;
    ppm_dump_complete = 1;
}

static void increment_horizontal(void)
{
    if ((v & 0x001f) == 31) {
        v &= (uint16_t)~0x001f;
        v ^= 0x0400;
    } else {
        ++v;
    }
}

static void copy_horizontal(void)
{
    v = (uint16_t)((v & 0xfbe0) | (t & 0x041f));
}

static void copy_vertical(void)
{
    v = (uint16_t)((v & 0x041f) | (t & 0x7be0));
}

static void render_sprite_line(unsigned line, uint16_t render_v)
{
    for (unsigned sprite = 0; sprite < 64; ++sprite) {
        if (!(regs[1] & 0x10)) continue;
        uint8_t sy = oam[sprite * 4];
        uint8_t tile = oam[sprite * 4 + 1];
        uint8_t attr = oam[sprite * 4 + 2];
        uint8_t sx = oam[sprite * 4 + 3];
        unsigned height = (regs[0] & 0x20) ? 16 : 8;
        if (line < sy || line >= sy + height) continue;
        unsigned row = line - sy;
        if (attr & 0x80) row = height - 1 - row;
        uint16_t pattern = (uint16_t)(((regs[0] & 0x20) ?
            ((tile & 1) * 0x1000 + (tile & 0xfe) * 16) : tile * 16) + row);
        uint8_t lo = chr[pattern], hi = chr[pattern + 8];
        for (unsigned col = 0; col < 8; ++col) {
            unsigned screen_x = sx + col;
            if (screen_x >= NES_W) continue;
            unsigned bit = attr & 0x40 ? col : 7 - col;
            uint8_t color = (uint8_t)(((hi >> bit) & 1) << 1 | ((lo >> bit) & 1));
            if (color == 0) continue;
            if (sprite == 0 && screen_x < 255 && (regs[1] & 0x08) &&
                background_pixel(render_v, screen_x, line) != 0) {
                regs[2] |= 0x40;
            }
            if ((attr & 0x20) && background_pixel(render_v, screen_x, line) != 0)
                continue;
            uint8_t palette_index = (uint8_t)(0x10 + ((attr & 3) << 2) + color);
            framebuffer[line * NES_W + screen_x] =
                nes_palette[palette[palette_index & 31] & 0x3f];
        }
    }
}

void ppu_step(uint32_t cpu_cycles)
{
    for (uint32_t cycle = 0; cycle < cpu_cycles; ++cycle) {
        for (unsigned dot = 0; dot < 3; ++dot) {
            if (scanline == 241 && dots == 1) {
                regs[2] |= 0x80;
                if (regs[0] & 0x80) nmi = 1;
            }
            if (scanline == 261 && dots == 1) {
                regs[2] &= (uint8_t)~0xe0;
                nmi = 0;
            }
            int rendering = (regs[1] & 0x18) != 0;
            mapper_ppu_a12((uint8_t)(rendering && dots == 260), (uint8_t)rendering);
            if (scanline < NES_H && dots == 0) {
                line_render_v = v;
                log_scroll_line(v);
                log_bg_fetches(v);
                ++rendered_scanlines;
                if (scanline < frame_min_y) frame_min_y = scanline;
                if (scanline > frame_max_y) frame_max_y = scanline;
            }
            if (scanline < NES_H && rendering && dots >= 1 && dots <= 256) {
                unsigned px = dots - 1;
                uint8_t color = background_pixel(line_render_v, px, scanline);
                framebuffer[scanline * NES_W + px] =
                    nes_palette[palette[color ? color : 0] & 0x3f];
                if ((dots & 7) == 0) increment_horizontal();
            }
            if (scanline < NES_H && dots == 256 && rendering)
                increment_vertical();
            if (scanline < NES_H && dots == 257 && rendering) {
                copy_horizontal();
                render_sprite_line(scanline, line_render_v);
            }
            if (scanline == 261 && dots >= 280 && dots <= 304 && rendering)
                copy_vertical();
            ++dots;
            if (dots == 341) {
                dots = 0;
                mapper_scanline();
                if (scanline == NES_H - 1) {
                    frame_ready = 1;
                    ++frame_number;
                    ++frame_done_count;
                    if (frame_log_enabled) {
                        printf("PPU-FRAME frame=%u scanlines=%u min_y=%u max_y=%u frame_done=%u\n",
                            frame_number, rendered_scanlines, frame_min_y, frame_max_y,
                            frame_done_count);
                    }
                    dump_ppm();
                    dump_nametable();
                    rendered_scanlines = 0;
                    frame_min_y = NES_H;
                    frame_max_y = 0;
                }
                scanline = (uint16_t)((scanline + 1) % 262);
            }
        }
    }
}

bool nmi_pending(void) { return nmi != 0; }
uint8_t ppu_read(uint16_t addr)
{
    uint8_t result;
    switch (addr & 7) {
    case 2: result = regs[2]; regs[2] &= 0x7f; nmi = 0; w = 0; return result;
    case 4: return oam[regs[3]];
    case 7:
        if ((v & 0x3fff) >= 0x3f00) {
            result = ppu_mem_read(v);
            read_buffer = ppu_mem_read((uint16_t)(v - 0x1000));
        } else {
            result = read_buffer;
            read_buffer = ppu_mem_read(v);
        }
        v = (uint16_t)((v + increment()) & 0x3fff);
        return result;
    default: return regs[addr & 7];
    }
}
void ppu_write(uint16_t addr, uint8_t value)
{
    switch (addr & 7) {
    case 0:
        regs[0] = value;
        t = (uint16_t)((t & 0xf3ff) | ((value & 3) << 10));
        if ((value & 0x80) && (regs[2] & 0x80)) nmi = 1;
        break;
    case 1: regs[1] = value; break;
    case 3: regs[3] = value; break;
    case 4: oam[regs[3]++] = value; break;
    case 5: if (!w) { x = value & 7; t = (uint16_t)((t & 0xffe0) | (value >> 3)); } else t = (uint16_t)((t & 0x8c1f) | ((value & 7) << 12) | ((value & 0xf8) << 2)); w ^= 1; break;
    case 6: if (!w) t = (uint16_t)((t & 0x00ff) | ((value & 0x3f) << 8)); else { t = (uint16_t)((t & 0xff00) | value); v = t; } w ^= 1; break;
    case 7: ppu_mem_write(v, value); v = (uint16_t)((v + increment()) & 0x3fff); break;
    default: break;
    }
    if (write_log_frames > 0 && frame_number < write_log_frames &&
        ((addr & 7) == 0 || (addr & 7) == 1 || (addr & 7) == 5 ||
         (addr & 7) == 6 || (addr & 7) == 7)) {
        printf("PPU-WRITE %u %u %04X %04X %02X v=%04X t=%04X\n",
            frame_number, scanline, cpu.pc, (unsigned)(0x2000 + (addr & 7)),
            value, v, t);
    }
}
void ppu_dma(const uint8_t *page) { memcpy(oam, page, 256); }
const uint32_t *ppu_framebuffer(void) { return framebuffer; }
int ppu_frame_ready(void) { return frame_ready; }
void ppu_clear_frame_ready(void) { frame_ready = 0; }
void ppu_load_chr(const uint8_t *data, uint32_t size)
{
    if (size > sizeof(chr)) size = sizeof(chr);
    memcpy(chr, data, size);
}
void ppu_set_chr_writable(int writable) { chr_writable = writable != 0; }
void ppu_set_write_log_frames(unsigned frames) { write_log_frames = frames; }
void ppu_set_nametable_dump_frame(unsigned frame)
{
    nametable_dump_frame = frame;
    nametable_dumped = 0;
}
void ppu_set_ppm_dump(unsigned frame, const char *path)
{
    ppm_dump_frame = frame;
    ppm_dump_path = path;
    ppm_dump_complete = 0;
}
int ppu_ppm_dump_complete(void) { return ppm_dump_complete; }
void ppu_set_scroll_log_frame(unsigned frame)
{
    scroll_log_frame = frame;
    scroll_log_enabled = 1;
}
void ppu_set_bg_log(int enabled) { bg_log_enabled = enabled != 0; }
void ppu_set_frame_log(int enabled) { frame_log_enabled = enabled != 0; }
void ppu_set_mirroring(int vertical) { vertical_mirroring = vertical != 0; }
void ppu_set_mirroring_mode(int mode) { vertical_mirroring = mode; }
void ppu_nmi_ack(void) { nmi = 0; }
void ppu_fill_test_gradient(void)
{
    for (unsigned y = 0; y < NES_H; ++y) {
        for (unsigned x_pos = 0; x_pos < NES_W; ++x_pos) {
            uint8_t value = (uint8_t)((y * 255) / (NES_H - 1));
            if (x_pos == 0 || y == 0 || x_pos == NES_W - 1 || y == NES_H - 1)
                value = 0xff;
            framebuffer[y * NES_W + x_pos] =
                0xff000000u | ((uint32_t)value << 16) | ((uint32_t)value << 8) | value;
        }
    }
}
