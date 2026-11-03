"""Small C lifter for discovered official 6502 instructions."""

try:
    from .decoder import DecodedInstruction
except ImportError:
    from decoder import DecodedInstruction


def _operand(instruction: DecodedInstruction) -> int:
    return int.from_bytes(instruction.operand, "little")


def lift(instruction: DecodedInstruction) -> str | None:
    op = instruction.opcode
    value = _operand(instruction)
    if op.mode == "rel":
        displacement = instruction.operand[0]
        if displacement & 0x80:
            displacement -= 0x100
        target = (instruction.address + 2 + displacement) & 0xFFFF
        condition = {
            "BPL": "!cpu.n", "BMI": "cpu.n", "BVC": "!cpu.v",
            "BVS": "cpu.v", "BCC": "!cpu.c", "BCS": "cpu.c",
            "BNE": "!cpu.z", "BEQ": "cpu.z",
        }[op.mnemonic]
        return f"cpu.pc = ({condition}) ? 0x{target:04X} : 0x{instruction.address + 2:04X}; return;"
    if op.mnemonic == "LDA" and op.mode == "imm":
        return f"cpu.a = bus_read(cpu.pc++); cpu.z = cpu.a == 0; cpu.n = cpu.a >> 7;"
    if op.mnemonic == "LDX" and op.mode == "imm":
        return f"cpu.x = bus_read(cpu.pc++); cpu.z = cpu.x == 0; cpu.n = cpu.x >> 7;"
    if op.mnemonic == "LDY" and op.mode == "imm":
        return f"cpu.y = bus_read(cpu.pc++); cpu.z = cpu.y == 0; cpu.n = cpu.y >> 7;"
    if op.mnemonic == "NOP":
        return ""
    if op.mnemonic == "CLC":
        return "cpu.c = 0;"
    if op.mnemonic == "SEC":
        return "cpu.c = 1;"
    if op.mnemonic == "CLI":
        return "cpu.i = 0;"
    if op.mnemonic == "SEI":
        return "cpu.i = 1;"
    if op.mnemonic == "CLD":
        return "cpu.d = 0;"
    if op.mnemonic == "SED":
        return "cpu.d = 1;"
    if op.mnemonic == "CLV":
        return "cpu.v = 0;"
    if op.mnemonic == "INX":
        return "cpu.x++; cpu.z = cpu.x == 0; cpu.n = cpu.x >> 7;"
    if op.mnemonic == "DEX":
        return "cpu.x--; cpu.z = cpu.x == 0; cpu.n = cpu.x >> 7;"
    if op.mnemonic == "INY":
        return "cpu.y++; cpu.z = cpu.y == 0; cpu.n = cpu.y >> 7;"
    if op.mnemonic == "DEY":
        return "cpu.y--; cpu.z = cpu.y == 0; cpu.n = cpu.y >> 7;"
    if op.mnemonic == "TAX":
        return "cpu.x = cpu.a; cpu.z = cpu.x == 0; cpu.n = cpu.x >> 7;"
    if op.mnemonic == "TXA":
        return "cpu.a = cpu.x; cpu.z = cpu.a == 0; cpu.n = cpu.a >> 7;"
    if op.mnemonic == "TAY":
        return "cpu.y = cpu.a; cpu.z = cpu.y == 0; cpu.n = cpu.y >> 7;"
    if op.mnemonic == "TYA":
        return "cpu.a = cpu.y; cpu.z = cpu.a == 0; cpu.n = cpu.a >> 7;"
    if op.mnemonic == "JSR" and op.mode == "abs":
        return (
            f"bus_write((uint16_t)(0x100 | cpu.sp--), (uint8_t)((cpu.pc + 1) >> 8)); "
            f"bus_write((uint16_t)(0x100 | cpu.sp--), (uint8_t)(cpu.pc + 1)); "
            f"cpu.pc = 0x{value:04X}; return;"
        )
    if op.mnemonic == "JMP" and op.mode == "abs":
        return f"cpu.pc = 0x{value:04X}; return;"
    if op.mnemonic == "RTS":
        return "cpu.pc = (uint16_t)(bus_read((uint16_t)(0x100 | ++cpu.sp)) | (bus_read((uint16_t)(0x100 | ++cpu.sp)) << 8)); cpu.pc++; return;"
    if op.mnemonic == "RTI":
        return "return;"
    if op.mnemonic == "BRK":
        return "return;"
    return None
