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
