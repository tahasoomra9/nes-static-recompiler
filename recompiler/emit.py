"""Emit bank-aware instruction-boundary generated C with interpreter fallback."""

from pathlib import Path

try:
    from .discover import Discovery
except ImportError:
    from discover import Discovery


def emit_game(discovery: Discovery, output: str | Path) -> Path:
    path = Path(output)
    instructions = {(item.bank, item.address): item for item in discovery.instructions}
    lines = [
        '#include "cpu.h"',
        '#include "bus.h"',
        '#include <stdint.h>',
        'void register_dispatch(void (*dispatch)(uint16_t addr));',
        'void interp_run(uint16_t addr);',
        '',
    ]
    def function_name(bank: int, address: int) -> str:
        return f"fn_B_{address:04X}" if bank == 0 else f"fn_{bank}_{address:04X}"

    for bank, address in sorted(instructions):
        lines += [
            f"static void {function_name(bank, address)}(void)",
            "{",
            f"    cpu_step();",
            f"    cpu_trace_record(0x{address:04X});",
            "}",
            "",
        ]
    lines += [
        "void generated_dispatch(uint16_t addr)",
        "{",
        "    switch (current_prg_bank(addr)) {",
    ]
    banks = sorted({bank for bank, _ in instructions})
    for bank in banks:
        lines += [f"    case {bank}:", "        switch (addr) {"]
        for item_bank, address in sorted(instructions):
            if item_bank == bank:
                lines.append(
                    f"        case 0x{address:04X}: "
                    f"{function_name(bank, address)}(); return;"
                )
        lines.append("        default: break;")
        lines.append("        }")
    lines += [
        "    default: break;",
        "    }",
        "    cpu.pc = addr; cpu_step(); cpu_trace_record(addr); return;",
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
