#include <assert.h>
#include <stdint.h>
#include "bus.h"
#include "cpu.h"
#include "ppu.h"

int main(void)
{
    cpu.pc = 0x8123;
    cpu.sp = 0xfd;
    cpu.i = 1;
    bus_write(0x2000, 0x80);
    ppu_step(30000);
    assert(nmi_pending());
    cpu_safe_point();
    assert(cpu.pc != 0x8123);
    assert(cpu.i == 1);

    cpu.i = 0;
    ppu_step(30000);
    assert(nmi_pending());
    cpu_safe_point();
    assert(!nmi_pending());
    assert(cpu.i == 1);
    assert(cpu.pc == 0x0000);

    ppu_step(1);
    assert(!nmi_pending());
    return 0;
}
