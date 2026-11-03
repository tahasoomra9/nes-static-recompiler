#include "bus.h"
#include "ppu.h"
#include <stdio.h>
#include <string.h>
#include "cpu.h"

static uint8_t prg[0x200000];
static uint8_t chr_rom[0x20000];
static uint8_t prg_ram[0x2000];
static uint32_t prg_size;
static uint32_t mapper_type;
static uint32_t selected_bank;
static uint8_t mmc1_shift, mmc1_control, mmc1_chr0, mmc1_chr1, mmc1_prg;
static uint64_t mmc1_last_write_cycle = UINT64_MAX;
static uint8_t mmc3_bank_select, mmc3_bank[8];
static uint8_t mmc3_prg_mode, mmc3_chr_inversion;
static uint8_t mmc3_prg_ram_enable, mmc3_prg_ram_write_protect;
static uint8_t mmc3_irq_latch, mmc3_irq_reload, mmc3_irq_enable;
static uint8_t mmc3_irq_counter;
static unsigned mmc3_a12_low_dots;
static uint8_t mmc3_a12_high;
static int four_screen;

static void mmc1_apply_mirroring(void)
{
    switch (mmc1_control & 3) {
    case 0: ppu_set_mirroring_mode(2); break;
    case 1: ppu_set_mirroring_mode(3); break;
    case 2: ppu_set_mirroring_mode(1); break;
    default: ppu_set_mirroring_mode(0); break;
    }
}
static int battery;
static char save_path[1024];

static int supported_header(const uint8_t *header)
{
    uint16_t mapper = (uint16_t)((header[6] >> 4) | (header[7] & 0xf0));
    if ((header[7] & 0x0c) == 0x08)
        mapper |= (uint16_t)((header[8] & 0x0f) << 8);
    return (mapper == 0 && header[4] >= 1 && header[4] <= 2) ||
        (mapper == 1 && header[4] >= 2 && header[4] <= 128) ||
        (mapper == 2 && header[4] >= 2 && header[4] <= 128) ||
        (mapper == 4 && header[4] >= 2 && header[4] <= 128);
}

uint8_t mapper_cpu_read(uint16_t addr)
{
    if (addr >= 0x6000 && addr < 0x8000) {
        if (mapper_type == 4 && !mmc3_prg_ram_enable) return 0;
        return prg_ram[addr - 0x6000];
    }
    if (addr < 0x8000 || prg_size == 0)
        return 0;
    uint32_t bank_count = prg_size / 0x4000;
    uint32_t offset;
    if (mapper_type == 4) {
        uint32_t bank_count_8k = prg_size / 0x2000;
        uint32_t last = bank_count_8k - 1;
        uint32_t second_last = bank_count_8k - 2;
        uint32_t bank;
        switch ((addr - 0x8000) / 0x2000) {
        case 0: bank = mmc3_prg_mode ? second_last : mmc3_bank[6]; break;
        case 1: bank = mmc3_bank[7]; break;
        case 2: bank = mmc3_prg_mode ? mmc3_bank[6] : second_last; break;
        default: bank = last; break;
        }
        offset = (bank % bank_count_8k) * 0x2000 + (addr & 0x1fff);
    } else if (mapper_type == 1) {
        uint8_t mode = (uint8_t)((mmc1_control >> 2) & 3);
        if (mode <= 1) {
            offset = (mmc1_prg & 0x0e) * 0x2000 + (addr - 0x8000);
        } else if (mode == 2 && addr < 0xc000) {
            offset = addr - 0x8000;
        } else if (mode == 2) {
            offset = (mmc1_prg % bank_count) * 0x4000 + (addr - 0xc000);
        } else if (addr < 0xc000) {
            offset = (mmc1_prg % bank_count) * 0x4000 + (addr - 0x8000);
        } else {
            offset = (bank_count - 1) * 0x4000 + (addr - 0xc000);
        }
    } else if (mapper_type == 2) {
        if (addr < 0xc000)
            offset = selected_bank * 0x4000 + (addr - 0x8000);
        else
            offset = (bank_count - 1) * 0x4000 + (addr - 0xc000);
    } else {
        offset = (uint32_t)(addr - 0x8000);
        if (prg_size == 0x4000) offset &= 0x3fff;
    }
    return prg[offset];
}

void mapper_cpu_write(uint16_t addr, uint8_t value)
{
    if (mapper_type == 4 && addr >= 0x8000) {
        if (addr < 0xa000) {
            if ((addr & 1) == 0) {
                mmc3_bank_select = value & 7;
                mmc3_prg_mode = (value >> 6) & 1;
                mmc3_chr_inversion = (value >> 7) & 1;
            } else {
                mmc3_bank[mmc3_bank_select] = value;
            }
        } else if (addr < 0xc000) {
            if ((addr & 1) == 0) {
                if (!four_screen) ppu_set_mirroring(value & 1);
            } else {
                mmc3_prg_ram_enable = (value & 0x80) != 0;
                mmc3_prg_ram_write_protect = (value & 0x40) != 0;
            }
        } else if (addr < 0xe000) {
            if ((addr & 1) == 0) mmc3_irq_latch = value;
            else mmc3_irq_reload = 1;
        } else if ((addr & 1) == 0) {
            mmc3_irq_enable = 0;
            cpu_set_irq_line(0);
        } else {
            mmc3_irq_enable = 1;
        }
    } else if (mapper_type == 1 && addr >= 0x8000) {
        if (mmc1_last_write_cycle != UINT64_MAX &&
            cpu.cycles == mmc1_last_write_cycle + 1) return;
        mmc1_last_write_cycle = cpu.cycles;
        if (value & 0x80) {
            mmc1_shift = 0x10;
            mmc1_control |= 0x0c;
            return;
        }
        if (mmc1_shift & 1) {
            uint8_t latched = (uint8_t)((mmc1_shift >> 1) |
                ((value & 1) << 4));
            if (addr < 0xa000) {
                mmc1_control = latched;
                mmc1_apply_mirroring();
            }
            else if (addr < 0xc000) mmc1_chr0 = latched;
            else if (addr < 0xe000) mmc1_chr1 = latched;
            else mmc1_prg = latched;
            mmc1_shift = 0x10;
        } else {
            mmc1_shift = (uint8_t)((mmc1_shift >> 1) |
                ((value & 1) << 4));
        }
    } else if (mapper_type == 2 && addr >= 0x8000) {
        selected_bank = value % (prg_size / 0x4000);
    } else if (addr >= 0x6000 && addr < 0x8000 &&
               !(mapper_type == 4 && (!mmc3_prg_ram_enable ||
                                      mmc3_prg_ram_write_protect)) &&
               !(mapper_type == 1 && (mmc1_prg & 0x10)))
        prg_ram[addr - 0x6000] = value;
}

uint8_t mapper_ppu_read(uint16_t addr)
{
    if (mapper_type == 4) {
        uint32_t bank;
        unsigned slot = addr >> 10;
        if (!mmc3_chr_inversion) {
            if (slot < 2) bank = (mmc3_bank[0] & 0xfe) + slot;
            else if (slot < 4) bank = (mmc3_bank[1] & 0xfe) + slot - 2;
            else bank = mmc3_bank[slot - 2];
        } else {
            if (slot < 4) bank = mmc3_bank[slot + 2];
            else if (slot < 6) bank = (mmc3_bank[0] & 0xfe) + slot - 4;
            else bank = (mmc3_bank[1] & 0xfe) + slot - 6;
        }
        return chr_rom[(bank % (sizeof(chr_rom) / 0x400)) * 0x400 +
                       (addr & 0x03ff)];
    } else if (mapper_type == 1) {
        uint32_t offset;
        if (mmc1_control & 0x10)
            offset = (addr < 0x1000 ? mmc1_chr0 : mmc1_chr1) * 0x1000 +
                (addr & 0x0fff);
        else
            offset = (mmc1_chr0 & 0x1e) * 0x1000 + addr;
        return chr_rom[offset % sizeof(chr_rom)];
    }
    return ppu_chr_read(addr);
}

void mapper_ppu_write(uint16_t addr, uint8_t value)
{
    ppu_chr_write(addr, value);
}

void mapper_scanline(void) {}

void mapper_ppu_a12(uint8_t high, uint8_t rendering)
{
    if (!rendering || mapper_type != 4) {
        mmc3_a12_high = high != 0;
        if (!mmc3_a12_high) ++mmc3_a12_low_dots;
        return;
    }
    if (!high) {
        mmc3_a12_high = 0;
        ++mmc3_a12_low_dots;
        return;
    }
    if (mmc3_a12_high || mmc3_a12_low_dots < 9) {
        mmc3_a12_high = 1;
        return;
    }
    mmc3_a12_high = 1;
    mmc3_a12_low_dots = 0;
    if (mmc3_irq_counter == 0 || mmc3_irq_reload) {
        mmc3_irq_counter = mmc3_irq_latch;
        mmc3_irq_reload = 0;
    } else {
        --mmc3_irq_counter;
    }
    if (mmc3_irq_counter == 0 && mmc3_irq_enable)
        cpu_set_irq_line(1);
}

uint32_t current_prg_bank(uint16_t addr)
{
    if (mapper_type == 4) {
        uint32_t bank_count_8k = prg_size / 0x2000;
        uint32_t last = bank_count_8k - 1;
        uint32_t second_last = bank_count_8k - 2;
        switch ((addr - 0x8000) / 0x2000) {
        case 0: return mmc3_prg_mode ? second_last : mmc3_bank[6];
        case 1: return mmc3_bank[7];
        case 2: return mmc3_prg_mode ? mmc3_bank[6] : second_last;
        default: return last;
        }
    }
    if (mapper_type == 1 && addr < 0xc000)
        return mmc1_prg % (prg_size / 0x4000);
    if (mapper_type == 2 && addr < 0xc000)
        return selected_bank;
    if (mapper_type == 2)
        return (prg_size / 0x4000) - 1;
    return 0;
}

void mapper0_set_prg(const uint8_t *data, uint32_t size)
{
    if (size > sizeof(prg)) size = sizeof(prg);
    memcpy(prg, data, size);
    prg_size = size;
    mapper_type = size > 0x4000 ? 2 : 0;
    selected_bank = 0;
    mmc1_shift = 0x10;
    mmc1_control = 0x0c;
    mmc1_chr0 = mmc1_chr1 = mmc1_prg = 0;
    memset(mmc3_bank, 0, sizeof mmc3_bank);
    mmc3_bank[7] = 1;
    mmc3_prg_ram_enable = 1;
    mmc3_prg_ram_write_protect = 0;
    mmc3_irq_latch = mmc3_irq_reload = mmc3_irq_enable = 0;
    mmc3_irq_counter = 0;
    mmc3_a12_low_dots = 0;
    mmc3_a12_high = 0;
    mmc1_apply_mirroring();
}

int mapper0_load_rom(const char *path)
{
    FILE *file = fopen(path, "rb");
    uint8_t header[16];
    uint32_t offset, chr_size;
    if (file == NULL || fread(header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "NES\x1a", 4) != 0 || !supported_header(header)) {
        if (file != NULL) fclose(file);
        return 0;
    }
    offset = (header[6] & 0x04) ? 528 : 16;
    if (fseek(file, (long)offset, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    mapper_type = (header[6] >> 4) | (header[7] & 0xf0);
    if ((header[7] & 0x0c) == 0x08)
        mapper_type |= (uint32_t)((header[8] & 0x0f) << 8);
    prg_size = (uint32_t)header[4] * 0x4000;
    selected_bank = 0;
    mmc1_shift = 0x10;
    mmc1_control = 0x0c;
    mmc1_chr0 = mmc1_chr1 = mmc1_prg = 0;
    memset(mmc3_bank, 0, sizeof mmc3_bank);
    mmc3_bank[7] = 1;
    mmc3_prg_ram_enable = 1;
    mmc3_prg_ram_write_protect = 0;
    mmc3_irq_latch = mmc3_irq_reload = mmc3_irq_enable = 0;
    mmc3_irq_counter = 0;
    mmc3_a12_low_dots = 0;
    mmc3_a12_high = 0;
    mmc1_last_write_cycle = UINT64_MAX;
    chr_size = (uint32_t)header[5] * 0x2000;
    if (fread(prg, 1, prg_size, file) != prg_size) {
        fclose(file);
        return 0;
    }
    if (chr_size != 0) {
        if (chr_size > sizeof(chr_rom) ||
            fread(chr_rom, 1, chr_size, file) != chr_size) {
            fclose(file);
            return 0;
        }
        ppu_load_chr(chr_rom, chr_size > 0x2000 ? 0x2000 : chr_size);
        ppu_set_chr_writable(0);
    } else {
        ppu_set_chr_writable(1);
    }
    four_screen = (header[6] & 8) != 0;
    ppu_set_mirroring((header[6] & 1) != 0);
    if (mapper_type == 1) mmc1_apply_mirroring();
    battery = (header[6] & 2) != 0;
    snprintf(save_path, sizeof(save_path), "%s.sav", path);
    memset(prg_ram, 0, sizeof(prg_ram));
    if (battery) {
        FILE *save = fopen(save_path, "rb");
        if (save != NULL) {
            fread(prg_ram, 1, sizeof(prg_ram), save);
            fclose(save);
        }
    }
    fclose(file);
    return 1;
}

void mapper0_save(void)
{
    if (battery && save_path[0] != '\0') {
        FILE *save = fopen(save_path, "wb");
        if (save != NULL) {
            fwrite(prg_ram, 1, sizeof(prg_ram), save);
            fclose(save);
        }
    }
}
