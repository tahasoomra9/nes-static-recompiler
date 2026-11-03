#include "apu.h"
#include "bus.h"
#include <string.h>

#define APU_RATE 48000u
#define CPU_RATE 1789773u
#define RING_SIZE 8192u

typedef struct {
    uint8_t enabled, duty, constant, loop, volume;
    uint8_t envelope, envelope_start, length_halt, sweep_enable;
    uint8_t sweep_period, sweep_negate, sweep_shift, sweep_reload;
    uint16_t timer, timer_period;
    uint8_t length, sequence;
} Pulse;

typedef struct {
    uint8_t enabled, control, linear_reload, linear_start;
    uint8_t length;
    uint16_t timer, timer_period;
    uint8_t sequence;
} Triangle;

typedef struct {
    uint8_t enabled, mode, constant, volume, envelope, envelope_start;
    uint8_t loop;
    uint8_t length_halt, length;
    uint16_t timer, timer_period;
    uint16_t shift;
} Noise;

typedef struct {
    uint8_t enabled, irq_enable, loop, irq;
    uint8_t output, sample_buffer, sample_empty, bits;
    uint16_t timer, timer_period;
    uint16_t sample_addr, current_addr;
    uint16_t sample_length, bytes_remaining;
} Dmc;

static Pulse pulse[2];
static Triangle triangle;
static Noise noise;
static Dmc dmc;
static uint8_t frame_mode, frame_irq_inhibit, frame_irq;
static uint32_t frame_cycle;
static uint64_t sample_accum;
static int16_t ring[RING_SIZE];
static uint32_t ring_read, ring_write, ring_count;
static void (*irq_callback)(void);

static const uint8_t length_table[32] = {
    10,254,20, 2,40, 4,80, 6,160, 8,60,10,14,12,26,14,
    12,16,24,18,48,20,96,22,192,24,72,26,16,28,32,30
};
static const uint16_t dmc_periods[16] = {
    428,380,340,320,286,254,226,214,190,160,142,128,106,84,72,54
};
static const uint8_t duty_table[4][8] = {
    {0,1,0,0,0,0,0,0}, {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,0,0,0}, {1,0,0,1,1,1,1,1}
};
static const uint8_t triangle_table[32] = {
    15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
};

static void raise_irq(void)
{
    if (irq_callback != NULL) irq_callback();
}

static void quarter_frame(void)
{
    for (unsigned i = 0; i < 2; ++i) {
        Pulse *p = &pulse[i];
        if (p->envelope_start) {
            p->envelope_start = 0;
            p->envelope = 15;
        } else if (p->envelope != 0) {
            --p->envelope;
        } else if (p->loop) {
            p->envelope = 15;
        }
        if (p->sweep_reload) p->sweep_reload = 0;
    }
    if (noise.envelope_start) {
        noise.envelope_start = 0;
        noise.envelope = 15;
    } else if (noise.envelope != 0) {
        --noise.envelope;
    } else if (noise.loop) {
        noise.envelope = 15;
    }
    if (triangle.linear_start) triangle.linear_start = 0;
}

static void half_frame(void)
{
    for (unsigned i = 0; i < 2; ++i) {
        Pulse *p = &pulse[i];
        if (!p->length_halt && p->length != 0) --p->length;
        if (p->sweep_enable && p->sweep_shift != 0) {
            uint16_t delta = (uint16_t)(p->timer_period >> p->sweep_shift);
            if (p->sweep_negate) p->timer_period -= delta + (i == 0);
            else p->timer_period += delta;
        }
    }
    if (!noise.length_halt && noise.length != 0) --noise.length;
    if (!triangle.control && triangle.length != 0) --triangle.length;
}

static void clock_frame(void)
{
    ++frame_cycle;
    if (frame_mode == 0) {
        if (frame_cycle == 3729 || frame_cycle == 11186) quarter_frame();
        if (frame_cycle == 7457 || frame_cycle == 14915) half_frame();
        if (frame_cycle >= 14915) {
            frame_cycle = 0;
            if (!frame_irq_inhibit) {
                frame_irq = 1;
                raise_irq();
            }
        }
    } else {
        if (frame_cycle == 3729 || frame_cycle == 11186) quarter_frame();
        if (frame_cycle == 7457 || frame_cycle == 14915) half_frame();
        if (frame_cycle >= 18641) {
            frame_cycle = 0;
            quarter_frame();
            half_frame();
        }
    }
}

static void clock_channels(void)
{
    for (unsigned i = 0; i < 2; ++i) {
        Pulse *p = &pulse[i];
        if (p->timer == 0) {
            p->timer = p->timer_period;
            p->sequence = (uint8_t)((p->sequence + 1) & 7);
        } else --p->timer;
    }
    if (triangle.timer == 0) {
        triangle.timer = triangle.timer_period;
        if (triangle.length != 0 && (triangle.control || !triangle.linear_start))
            triangle.sequence = (uint8_t)((triangle.sequence + 1) & 31);
    } else --triangle.timer;
    if (noise.timer == 0) {
        noise.timer = noise.timer_period;
        uint16_t feedback = (uint16_t)((noise.shift ^ (noise.shift >> (noise.mode ? 6 : 1))) & 1);
        noise.shift = (uint16_t)((noise.shift >> 1) | (feedback << 14));
    } else --noise.timer;
    if (dmc.timer == 0) {
        dmc.timer = dmc.timer_period;
        if (dmc.bits == 0) {
            if (dmc.sample_empty && dmc.bytes_remaining != 0) {
                dmc.sample_buffer = bus_read(dmc.current_addr);
                dmc.sample_empty = 0;
                dmc.current_addr = dmc.current_addr == 0xffff ? 0x8000 : dmc.current_addr + 1;
                --dmc.bytes_remaining;
            } else if (dmc.bytes_remaining == 0 && dmc.loop) {
                dmc.current_addr = dmc.sample_addr;
                dmc.bytes_remaining = dmc.sample_length;
            } else if (dmc.bytes_remaining == 0 && dmc.irq_enable) {
                dmc.irq = 1;
                raise_irq();
            }
            dmc.bits = 8;
        }
        if (!dmc.sample_empty) {
            if (dmc.sample_buffer & 1) {
                if (dmc.output <= 125) dmc.output += 2;
            } else if (dmc.output >= 2) dmc.output -= 2;
            dmc.sample_buffer >>= 1;
            --dmc.bits;
            if (dmc.bits == 0) dmc.sample_empty = 1;
        }
    } else --dmc.timer;
}

static uint8_t pulse_output(const Pulse *p)
{
    if (!p->enabled || p->length == 0 || p->timer_period < 8 ||
        (p->sweep_enable && p->timer_period > 0x7ff)) return 0;
    return duty_table[p->duty][p->sequence] ? (p->constant ? p->volume : p->envelope) : 0;
}

static uint8_t noise_output(void)
{
    if (!noise.enabled || noise.length == 0 || (noise.shift & 1)) return 0;
    return noise.constant ? noise.volume : noise.envelope;
}

static int16_t mix_sample(void)
{
    unsigned p = pulse_output(&pulse[0]) + pulse_output(&pulse[1]);
    unsigned t = triangle.length ? triangle_table[triangle.sequence] : 0;
    unsigned n = noise_output();
    unsigned d = dmc.output;
    double pulse_mix = p ? 95.88 / (8128.0 / p + 100.0) : 0.0;
    double tnd_mix = (t + n * 2 + d) ? 159.79 / (1.0 / ((t + n * 2 + d) / 8227.0) + 100.0) : 0.0;
    double sample = (pulse_mix + tnd_mix) * 2.0 - 1.0;
    if (sample > 1.0) sample = 1.0;
    if (sample < -1.0) sample = -1.0;
    return (int16_t)(sample * 30000.0);
}

void apu_reset(void)
{
    memset(pulse, 0, sizeof pulse);
    memset(&triangle, 0, sizeof triangle);
    memset(&noise, 0, sizeof noise);
    memset(&dmc, 0, sizeof dmc);
    noise.shift = 1;
    ring_read = ring_write = ring_count = 0;
    frame_cycle = 0;
    frame_mode = 0;
    frame_irq = 0;
    sample_accum = 0;
}

void apu_step(uint32_t cpu_cycles)
{
    for (uint32_t i = 0; i < cpu_cycles; ++i) {
        clock_frame();
        clock_channels();
        sample_accum += APU_RATE;
        if (sample_accum >= CPU_RATE) {
            sample_accum -= CPU_RATE;
            if (ring_count < RING_SIZE) {
                ring[ring_write] = mix_sample();
                ring_write = (ring_write + 1) % RING_SIZE;
                ++ring_count;
            }
        }
    }
}

uint8_t apu_read(uint16_t addr)
{
    if (addr != 0x4015) return 0;
    uint8_t result = (uint8_t)((pulse[0].length ? 1 : 0) |
        (pulse[1].length ? 2 : 0) | (triangle.length ? 4 : 0) |
        (noise.length ? 8 : 0) | (dmc.bytes_remaining ? 16 : 0) |
        (frame_irq ? 0x40 : 0) | (dmc.irq ? 0x80 : 0));
    frame_irq = 0;
    return result;
}

void apu_write(uint16_t addr, uint8_t value)
{
    if (addr >= 0x4000 && addr <= 0x4003) {
        Pulse *p = &pulse[0];
        unsigned r = addr & 3;
        if (r == 0) { p->duty = value >> 6; p->loop = p->length_halt = (value >> 5) & 1; p->constant = (value >> 4) & 1; p->volume = value & 15; }
        else if (r == 1) { p->sweep_enable = value & 0x80; p->sweep_period = ((value >> 4) & 7) + 1; p->sweep_negate = value & 8; p->sweep_shift = value & 7; p->sweep_reload = 1; }
        else if (r == 2) p->timer_period = (p->timer_period & 0x700) | value;
        else { p->timer_period = (p->timer_period & 0xff) | ((value & 7) << 8); p->length = length_table[value >> 3]; p->sequence = 0; p->envelope_start = 1; }
    } else if (addr >= 0x4004 && addr <= 0x4007) {
        Pulse *p = &pulse[1];
        unsigned r = addr & 3;
        if (r == 0) { p->duty = value >> 6; p->loop = p->length_halt = (value >> 5) & 1; p->constant = (value >> 4) & 1; p->volume = value & 15; }
        else if (r == 1) { p->sweep_enable = value & 0x80; p->sweep_period = ((value >> 4) & 7) + 1; p->sweep_negate = value & 8; p->sweep_shift = value & 7; p->sweep_reload = 1; }
        else if (r == 2) p->timer_period = (p->timer_period & 0x700) | value;
        else { p->timer_period = (p->timer_period & 0xff) | ((value & 7) << 8); p->length = length_table[value >> 3]; p->sequence = 0; p->envelope_start = 1; }
    } else if (addr == 0x4008) {
        triangle.control = value & 0x80; triangle.linear_reload = value & 0x7f;
    } else if (addr == 0x400a) triangle.timer_period = (triangle.timer_period & 0x700) | value;
    else if (addr == 0x400b) { triangle.timer_period = (triangle.timer_period & 0xff) | ((value & 7) << 8); triangle.length = length_table[value >> 3]; triangle.linear_start = 1; }
    else if (addr == 0x400c) { noise.loop = noise.length_halt = value & 0x20; noise.constant = value & 0x10; noise.volume = value & 15; }
    else if (addr == 0x400e) { noise.mode = value & 0x80; noise.timer_period = (uint16_t)(value & 15); }
    else if (addr == 0x400f) { noise.length = length_table[value >> 3]; noise.envelope_start = 1; }
    else if (addr == 0x4010) { dmc.irq_enable = value & 0x80; dmc.loop = value & 0x40; dmc.timer_period = dmc_periods[value & 15]; if (!dmc.irq_enable) dmc.irq = 0; }
    else if (addr == 0x4011) dmc.output = value & 0x7f;
    else if (addr == 0x4012) dmc.sample_addr = (uint16_t)(0xc000 | (value << 6));
    else if (addr == 0x4013) dmc.sample_length = (uint16_t)((value << 4) | 1);
    else if (addr == 0x4015) {
        pulse[0].enabled = value & 1; pulse[1].enabled = value & 2; triangle.enabled = value & 4; noise.enabled = value & 8;
        if (!pulse[0].enabled) pulse[0].length = 0;
        if (!pulse[1].enabled) pulse[1].length = 0;
        if (!triangle.enabled) triangle.length = 0;
        if (!noise.enabled) noise.length = 0;
        dmc.enabled = value & 0x10; if (!dmc.enabled) dmc.bytes_remaining = 0;
        if (dmc.enabled && dmc.bytes_remaining == 0) { dmc.current_addr = dmc.sample_addr; dmc.bytes_remaining = dmc.sample_length; }
        dmc.irq = 0;
    } else if (addr == 0x4017) {
        frame_mode = value >> 7; frame_irq_inhibit = (value >> 6) & 1; frame_cycle = 0;
        if (frame_irq_inhibit) frame_irq = 0;
        if (frame_mode) { quarter_frame(); half_frame(); }
    }
}

int16_t apu_sample(void)
{
    int16_t result = 0;
    apu_pop_samples(&result, 1);
    return result;
}

uint32_t apu_available_samples(void) { return ring_count; }

uint32_t apu_pop_samples(int16_t *samples, uint32_t count)
{
    uint32_t taken = 0;
    while (taken < count && ring_count != 0) {
        samples[taken++] = ring[ring_read];
        ring_read = (ring_read + 1) % RING_SIZE;
        --ring_count;
    }
    return taken;
}

void apu_set_irq_callback(void (*callback)(void)) { irq_callback = callback; }
int apu_irq_pending(void) { return frame_irq || dmc.irq; }
void apu_clear_irq(void) { frame_irq = dmc.irq = 0; }
