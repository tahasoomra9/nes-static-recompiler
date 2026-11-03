"""Official 6502 opcode metadata and byte decoding."""

from dataclasses import dataclass


@dataclass(frozen=True)
class Opcode:
    mnemonic: str
    mode: str
    bytes: int
    cycles: int


@dataclass(frozen=True)
class DecodedInstruction:
    address: int
    opcode: Opcode
    operand: bytes
    bank: int = 0


_MODES = {
    "imp": (1, 0), "acc": (1, 0), "imm": (2, 1), "zp": (2, 1),
    "zpx": (2, 1), "zpy": (2, 1), "rel": (2, 1), "abs": (3, 2),
    "abx": (3, 2), "aby": (3, 2), "ind": (3, 2), "inx": (2, 1),
    "iny": (2, 1),
}

# Each row contains the official 6502 table in opcode order. Entries are
# mnemonic/mode/cycles; absent entries remain unsupported illegal opcodes.
_ROWS = [
    "BRK imp 7 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0",
    "ORA inx 6 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 ORA zp 3 ASL zp 5 ??? imp 0 PHP imp 3 ORA imm 2 ASL acc 2 ??? imp 0 ??? imp 0 ORA abs 4 ASL abs 6 ??? imp 0 ??? imp 0",
    "JSR abs 6 AND inx 6 ??? imp 0 ??? imp 0 BIT zp 3 AND zp 3 ROL zp 5 ??? imp 0 PLP imp 4 AND imm 2 ROL acc 2 ??? imp 0 BIT abs 4 AND abs 4 ROL abs 6 ??? imp 0 ??? imp 0",
    "BMI rel 2 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 AND zpx 4 ROL zpx 6 ??? imp 0 SEC imp 2 AND aby 4 ??? imp 0 ??? imp 0 ??? imp 0 AND abx 4 ROL abx 7 ??? imp 0 ??? imp 0",
    "RTI imp 6 EOR inx 6 ??? imp 0 ??? imp 0 ??? imp 0 EOR zp 3 LSR zp 5 ??? imp 0 PHA imp 3 EOR imm 2 LSR acc 2 ??? imp 0 JMP abs 3 EOR abs 4 LSR abs 6 ??? imp 0 ??? imp 0",
    "BVC rel 2 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 EOR zpx 4 LSR zpx 6 ??? imp 0 CLI imp 2 EOR aby 4 ??? imp 0 ??? imp 0 ??? imp 0 EOR abx 4 LSR abx 7 ??? imp 0 ??? imp 0",
    "RTS imp 6 ADC inx 6 ??? imp 0 ??? imp 0 ??? imp 0 ADC zp 3 ROR zp 5 ??? imp 0 PLA imp 4 ADC imm 2 ROR acc 2 ??? imp 0 JMP ind 5 ADC abs 4 ROR abs 6 ??? imp 0 ??? imp 0",
    "BVS rel 2 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 ADC zpx 4 ROR zpx 6 ??? imp 0 SEI imp 2 ADC aby 4 ??? imp 0 ??? imp 0 ??? imp 0 ADC abx 4 ROR abx 7 ??? imp 0 ??? imp 0",
    "??? imp 0 STA inx 6 ??? imp 0 ??? imp 0 STY zp 3 STA zp 3 STX zp 3 ??? imp 0 DEY imp 2 ??? imp 0 TXA imp 2 ??? imp 0 STY abs 4 STA abs 4 STX abs 4 ??? imp 0 ??? imp 0",
    "BCC rel 2 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 STA zpx 4 ??? imp 0 ??? imp 0 TYA imp 2 STA aby 5 TXS imp 2 ??? imp 0 ??? imp 0 STA abx 5 ??? imp 0 ??? imp 0",
    "LDY imm 2 LDA inx 6 LDX imm 2 ??? imp 0 LDY zp 3 LDA zp 3 LDX zp 3 ??? imp 0 TAY imp 2 LDA imm 2 TAX imp 2 ??? imp 0 LDY abs 4 LDA abs 4 LDX abs 4 ??? imp 0 ??? imp 0",
    "BCS rel 2 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 LDA zpx 4 LDY zpx 4 LDX zpy 4 ??? imp 0 CLV imp 2 LDA aby 4 TSX imp 2 ??? imp 0 LDY abx 4 LDA abx 4 LDX aby 4 ??? imp 0 ??? imp 0",
    "CPY imm 2 CMP inx 6 ??? imp 0 ??? imp 0 CPY zp 3 CMP zp 3 DEC zp 5 ??? imp 0 INY imp 2 CMP imm 2 DEX imp 2 ??? imp 0 CPY abs 4 CMP abs 4 DEC abs 6 ??? imp 0 ??? imp 0",
    "BNE rel 2 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 CMP zpx 4 DEC zpx 6 ??? imp 0 CLD imp 2 CMP aby 4 ??? imp 0 ??? imp 0 ??? imp 0 CMP abx 4 DEC abx 7 ??? imp 0 ??? imp 0",
    "CPX imm 2 SBC inx 6 ??? imp 0 ??? imp 0 CPX zp 3 SBC zp 3 INC zp 5 ??? imp 0 INX imp 2 SBC imm 2 NOP imp 2 ??? imp 0 CPX abs 4 SBC abs 4 INC abs 6 ??? imp 0 ??? imp 0",
    "BEQ rel 2 ??? imp 0 ??? imp 0 ??? imp 0 ??? imp 0 SBC zpx 4 INC zpx 6 ??? imp 0 SED imp 2 SBC aby 4 ??? imp 0 ??? imp 0 ??? imp 0 SBC abx 4 INC abx 7 ??? imp 0 ??? imp 0",
]


def _build_table() -> tuple[Opcode | None, ...]:
    table: list[Opcode | None] = [None] * 256
    for row_index, row in enumerate(_ROWS):
        tokens = row.split()[:48]
        tokens.extend(("???", "imp", "0") * (16 - len(tokens) // 3))
        for column in range(16):
            index = column * 3
            mnemonic, mode, cycles_text = tokens[index:index + 3]
            if mnemonic != "???":
                size, _ = _MODES[mode]
                table[row_index * 16 + column] = Opcode(
                    mnemonic, mode, size, int(cycles_text)
                )
    # The first source row stores BRK separately; its remaining entries are
    # the first fifteen entries of the next compact line.
    first = _ROWS[1].split()
    for column in range(1, 16):
        mnemonic, mode, cycles_text = first[column * 3:column * 3 + 3]
        if mnemonic != "???":
            size, _ = _MODES[mode]
            table[column] = Opcode(mnemonic, mode, size, int(cycles_text))
    row_one = (
        "BPL rel 2 ORA iny 5 ??? imp 0 ??? imp 0 ??? imp 0 "
        "ORA zpx 4 ASL zpx 6 ??? imp 0 CLC imp 2 ORA aby 4 "
        "??? imp 0 ??? imp 0 ??? imp 0 ORA abx 4 ASL abx 7 ??? imp 0"
    ).split()
    for column in range(16):
        mnemonic, mode, cycles_text = row_one[column * 3:column * 3 + 3]
        if mnemonic != "???":
            size, _ = _MODES[mode]
            table[0x10 + column] = Opcode(mnemonic, mode, size, int(cycles_text))
    for code, mnemonic, mode, cycles in (
        (0x05, "ORA", "zp", 3), (0x08, "PHP", "imp", 3),
        (0x0D, "ORA", "abs", 4), (0x31, "AND", "iny", 5),
        (0x51, "EOR", "iny", 5), (0x71, "ADC", "iny", 5),
        (0x91, "STA", "iny", 6), (0x94, "STY", "zpx", 4),
        (0x96, "STX", "zpy", 4), (0xB1, "LDA", "iny", 5),
        (0xB4, "LDY", "zpx", 4), (0xB8, "CLV", "imp", 2),
    ):
        table[code] = Opcode(mnemonic, mode, _MODES[mode][0], cycles)
    unofficial = {
        0xA3: ("LAX", "inx", 6), 0xA7: ("LAX", "zp", 3),
        0xAF: ("LAX", "abs", 4), 0xB3: ("LAX", "iny", 5),
        0xB7: ("LAX", "zpy", 4), 0xBF: ("LAX", "aby", 4),
        0x83: ("SAX", "inx", 6), 0x87: ("SAX", "zp", 3),
        0x8F: ("SAX", "abs", 4), 0x97: ("SAX", "zpy", 4),
        0xC3: ("DCP", "inx", 8), 0xC7: ("DCP", "zp", 5),
        0xCF: ("DCP", "abs", 6), 0xD3: ("DCP", "iny", 8),
        0xD7: ("DCP", "zpx", 6), 0xDB: ("DCP", "aby", 7),
        0xDF: ("DCP", "abx", 7),
        0xE3: ("ISB", "inx", 8), 0xE7: ("ISB", "zp", 5),
        0xEF: ("ISB", "abs", 6), 0xF3: ("ISB", "iny", 8),
        0xF7: ("ISB", "zpx", 6), 0xFB: ("ISB", "aby", 7),
        0xFF: ("ISB", "abx", 7),
        0x03: ("SLO", "inx", 8), 0x07: ("SLO", "zp", 5),
        0x0F: ("SLO", "abs", 6), 0x13: ("SLO", "iny", 8),
        0x17: ("SLO", "zpx", 6), 0x1B: ("SLO", "aby", 7),
        0x1F: ("SLO", "abx", 7),
        0x43: ("SRE", "inx", 8), 0x47: ("SRE", "zp", 5),
        0x4F: ("SRE", "abs", 6), 0x53: ("SRE", "iny", 8),
        0x57: ("SRE", "zpx", 6), 0x5B: ("SRE", "aby", 7),
        0x5F: ("SRE", "abx", 7),
        0x23: ("RLA", "inx", 8), 0x27: ("RLA", "zp", 5),
        0x2F: ("RLA", "abs", 6), 0x33: ("RLA", "iny", 8),
        0x37: ("RLA", "zpx", 6), 0x3B: ("RLA", "aby", 7),
        0x3F: ("RLA", "abx", 7),
        0x63: ("RRA", "inx", 8), 0x67: ("RRA", "zp", 5),
        0x6F: ("RRA", "abs", 6), 0x73: ("RRA", "iny", 8),
        0x77: ("RRA", "zpx", 6), 0x7B: ("RRA", "aby", 7),
        0x7F: ("RRA", "abx", 7),
    }
    nop_modes = {
        0x1A: ("imp", 2), 0x3A: ("imp", 2), 0x5A: ("imp", 2),
        0x7A: ("imp", 2), 0xDA: ("imp", 2), 0xFA: ("imp", 2),
        0x04: ("zp", 3), 0x44: ("zp", 3), 0x64: ("zp", 3),
        0x14: ("zpx", 4), 0x34: ("zpx", 4), 0x54: ("zpx", 4),
        0x74: ("zpx", 4), 0xD4: ("zpx", 4), 0xF4: ("zpx", 4),
        0x80: ("imm", 2), 0x82: ("imm", 2), 0x89: ("imm", 2),
        0xC2: ("imm", 2), 0xE2: ("imm", 2),
        0x0C: ("abs", 4),
        0x1C: ("abx", 4), 0x3C: ("abx", 4), 0x5C: ("abx", 4),
        0x7C: ("abx", 4), 0xDC: ("abx", 4), 0xFC: ("abx", 4),
    }
    for code, (mnemonic, mode, cycles) in unofficial.items():
        table[code] = Opcode(mnemonic, mode, _MODES[mode][0], cycles)
    for code, (mode, cycles) in nop_modes.items():
        table[code] = Opcode("NOP", mode, _MODES[mode][0], cycles)
    return tuple(table)


OPCODES = _build_table()


def decode(data: bytes, address: int = 0) -> DecodedInstruction:
    if not data:
        raise ValueError("cannot decode an empty byte sequence")
    opcode = OPCODES[data[0]]
    if opcode is None:
        raise ValueError(f"unsupported illegal opcode ${data[0]:02X}")
    if len(data) < opcode.bytes:
        raise ValueError(f"truncated {opcode.mnemonic} instruction")
    return DecodedInstruction(address, opcode, data[1:opcode.bytes])
