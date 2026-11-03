"""Emit a buildable generated C translation unit."""

from pathlib import Path

try:
    from .discover import Discovery
    from .lifter import lift
except ImportError:
    from discover import Discovery
    from lifter import lift


def emit_game(discovery: Discovery, output: str | Path) -> Path:
    path = Path(output)
    lines = [
        '#include "cpu.h"',
        '#include "bus.h"',
        '#include <stdint.h>',
        'void call_addr(uint16_t addr);',
        'void interp_run(uint16_t addr);',
        'void register_dispatch(void (*dispatch)(uint16_t addr));',
        '',
    ]
    for function in discovery.functions:
        if not function:
            continue
        entry = function[0]
        lines.append(f"static void fn_{entry:04X}(void)")
        lines.append("{")
        lines.append(f"    cpu.pc = 0x{entry:04X};")
        for instruction in (item for item in discovery.instructions if item.address in function):
            lines.append("    cpu.pc++;")
            body = lift(instruction)
            if body is None:
                lines.append(f"    cpu.pc = 0x{instruction.address:04X}; cpu_step(); return;")
            else:
                lines.append(f"    {body}")
            lines.append(f"    cpu.cycles += {instruction.opcode.cycles};")
        lines.append("}")
        lines.append("")
    lines += ["void generated_dispatch(uint16_t addr)", "{", "    switch (addr) {"]
    for function in discovery.functions:
        if function:
            lines.append(f"    case 0x{function[0]:04X}: fn_{function[0]:04X}(); return;")
    lines += ["    default: interp_run(addr); return;", "    }", "}", ""]
    lines += [
        "void game_main(void)",
        "{",
        "    register_dispatch(generated_dispatch);",
        "    cpu.pc = (uint16_t)(bus_read(0xFFFC) | (bus_read(0xFFFD) << 8));",
        "}",
    ]
    path.write_text("\n".join(lines) + "\n", encoding="ascii")
    return path
