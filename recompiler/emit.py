"""Emit instruction-boundary generated C with interpreter fallback."""

from pathlib import Path

try:
    from .discover import Discovery
except ImportError:
    from discover import Discovery


def emit_game(discovery: Discovery, output: str | Path) -> Path:
    path = Path(output)
    instructions = {item.address: item for item in discovery.instructions}
    lines = [
        '#include "cpu.h"',
        '#include "bus.h"',
        '#include <stdint.h>',
        'void register_dispatch(void (*dispatch)(uint16_t addr));',
        'void interp_run(uint16_t addr);',
        '',
    ]
    for address in sorted(instructions):
        lines += [
            f"static void fn_{address:04X}(void)",
            "{",
            f"    cpu_step();",
            f"    cpu_trace_record(0x{address:04X});",
            "}",
            "",
        ]
    lines += ["void generated_dispatch(uint16_t addr)", "{", "    switch (addr) {"]
    for address in sorted(instructions):
        lines.append(f"    case 0x{address:04X}: fn_{address:04X}(); return;")
    lines += [
        "    default: cpu.pc = addr; cpu_step(); cpu_trace_record(addr); return;",
        "    }",
        "}",
        "",
        "void game_main(void)",
        "{",
        "    register_dispatch(generated_dispatch);",
        "    cpu.pc = (uint16_t)(bus_read(0xFFFC) | (bus_read(0xFFFD) << 8));",
        "}",
    ]
    path.write_text("\n".join(lines) + "\n", encoding="ascii")
    return path
