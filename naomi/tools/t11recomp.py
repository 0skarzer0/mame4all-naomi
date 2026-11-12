#!/usr/bin/env python3
"""Static recompiler for the DEC T-11 code of an Atari System 2 game.

Usage: t11recomp.py T11OPS_H T11TABLE_H EVEN_ROM ODD_ROM OUT_H [EXTRA_ENTRIES]

EVEN_ROM / ODD_ROM hold the low / high bytes of the fixed program at
0x8000-0xffff (Super Sprint rev. 1: 136042-138.7l / 136042-139.7n).
EXTRA_ENTRIES is an optional text file with more entry addresses (hex, one
per line), for code reached only through tables (found with the run-time
miss counter, -DNAOMI_T11RC_MISSES).

The output is C to be included at the end of src/t11/t11.cpp. Each block of
code (from an entry to the next jump, at most MAXLEN instructions) becomes a
function that runs the same instruction bodies as MAME's interpreter
(t11ops.h), with the opcode a constant, so the behaviour is the same and only
fetching, decoding and dispatching go away. A block hands back after any
instruction that leaves t11_ICount <= 0 or makes an interrupt ready, as the
interpreter would, so the emulation stays instruction for instruction the same.
Addresses with no block run in the interpreter.
"""
import re
import sys

MAXLEN = 64
BASE = 0x8000

CONTROL_PREFIXES = ('br', 'bne', 'beq', 'bge', 'blt', 'bgt', 'ble', 'bpl', 'bmi', 'bhi', 'blos',
                    'bvc', 'bvs', 'bcc', 'bcs', 'jmp_', 'jsr_', 'rts', 'op_0000', 'illegal',
                    'emt', 'trap', 'sob', 'mark', 'mtps_')
# after these the next address is code too (a return lands there)
FALLTHROUGH_OK = ('bne', 'beq', 'bge', 'blt', 'bgt', 'ble', 'bpl', 'bmi', 'bhi', 'blos', 'bvc',
                  'bvs', 'bcc', 'bcs', 'jsr_', 'sob', 'emt', 'trap', 'mtps_')
MODES = ('rgd', 'rg', 'ind', 'in', 'ded', 'de', 'ixd', 'ix')


def load(path):
    return open(path, encoding='latin-1').read()


def parse_table(src):
    i = src.index('opcode_table[65536 >> 3]')
    body = src[src.index('{', i) + 1:src.index('};', i)]
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    names = [n.strip() for n in body.split(',') if n.strip()]
    assert len(names) == 8192, len(names)
    return names


def parse_handlers(src):
    """name -> body text of every 'static void name(void) {...}'."""
    out = {}
    for m in re.finditer(r'static void (\w+)\(void\)\s*\{', src):
        name = m.group(1)
        i = m.end()
        depth = 1
        j = i
        while depth:
            c = src[j]
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
            j += 1
        out[name] = src[i:j - 1]
    return out


def opc_macros(src):
    """#define lines of t11ops.h that read state.op, rewritten to use OPC."""
    lines = []
    for line in src.split('\n'):
        m = re.match(r'#define (\w+)', line)
        if m and 'state.op' in line:
            lines.append('#undef ' + m.group(1))
            lines.append(line.replace('state.op', 'OPC'))
    return lines


def modes_of(name):
    parts = name.split('_')[1:]
    return [p for p in parts if p in MODES]


def decode(rom, table, addr):
    """-> (length in bytes, handler, op, list of static targets, immediates)"""
    if addr < BASE or addr > 0xfffe:
        return None
    op = word(rom, addr)
    h = table[op >> 3]
    length = 2
    imms = []
    ms = modes_of(h)
    if len(ms) == 2:
        regs = [(op >> 6) & 7, op & 7]
    elif len(ms) == 1:
        regs = [op & 7]
    else:
        regs = []
    absolute = None
    for m, r in zip(ms, regs):
        if (m in ('in', 'ind') and r == 7) or m in ('ix', 'ixd'):
            if addr + length > 0xfffe:
                return None
            w = word(rom, addr + length)
            imms.append(w)
            if m == 'ind' and r == 7:
                absolute = w
            elif m == 'ix' and r == 7:
                absolute = (addr + length + 2 + w) & 0xffff
            length += 2
    targets = []
    if h in ('br', 'bne', 'beq', 'bge', 'blt', 'bgt', 'ble', 'bpl', 'bmi', 'bhi', 'blos', 'bvc',
             'bvs', 'bcc', 'bcs'):
        off = op & 0xff
        if off >= 0x80:
            off -= 0x100
        targets.append((addr + 2 + 2 * off) & 0xffff)
    elif h == 'sob':
        targets.append((addr + 2 - 2 * (op & 0x3f)) & 0xffff)
    elif (h.startswith('jmp_') or h.startswith('jsr_')) and absolute is not None:
        targets.append(absolute)
    return length, h, op, targets, imms


def word(rom, addr):
    i = addr - BASE
    return rom[i] | (rom[i + 1] << 8)


def is_control(h, op, ms):
    if h.startswith(CONTROL_PREFIXES):
        return True
    # anything that writes the PC through a register mode
    regs = [(op >> 6) & 7, op & 7] if len(ms) == 2 else [op & 7]
    for m, r in zip(ms, regs):
        if r == 7 and m in ('rg', 'de', 'ded'):
            return True
    return False


def main():
    ops_src = load(sys.argv[1])
    table = parse_table(load(sys.argv[2]))
    even = open(sys.argv[3], 'rb').read()
    odd = open(sys.argv[4], 'rb').read()
    rom = bytearray(0x8000)
    for i in range(0x4000):
        rom[2 * i] = even[i]
        rom[2 * i + 1] = odd[i]
    extra = []
    if len(sys.argv) > 6:
        for line in open(sys.argv[6]):
            line = line.split('#')[0].strip()
            if line:
                extra.append(int(line, 16))
    handlers = parse_handlers(ops_src)

    entries = [0x8000] + extra
    seen = set()
    blocks = {}
    queue = list(entries)
    while queue:
        a = queue.pop()
        if a in seen or a < BASE or a > 0xfffe or a & 1:
            continue
        seen.add(a)
        insns = []
        pc = a
        while len(insns) < MAXLEN:
            d = decode(rom, table, pc)
            if d is None:
                break
            length, h, op, targets, imms = d
            if h == 'illegal':
                break
            insns.append((pc, op, h, length))
            for t in targets:
                queue.append(t)
            for w in imms:
                # immediates that look like code addresses (vectors, tables, handlers)
                if BASE <= w <= 0xfffe and not w & 1:
                    dd = decode(rom, table, w)
                    if dd and dd[1] != 'illegal':
                        queue.append(w)
            pc += length
            if is_control(h, op, modes_of(h)):
                if h.startswith(FALLTHROUGH_OK):
                    queue.append(pc)
                break
        else:
            queue.append(pc)   # block cut at MAXLEN: carry on in the next one
        if insns:
            blocks[a] = insns

    out = []
    out.append('/* Generated by naomi/tools/t11recomp.py: do not edit. */')
    out.append('/* %d blocks, %d instructions */' % (len(blocks), sum(len(b) for b in blocks.values())))
    out.extend(opc_macros(ops_src))
    # compact code: memory through out-of-line functions, operands as constants
    out.append('#undef RBYTE')
    out.append('#undef WBYTE')
    out.append('#define RBYTE(a) rc_rbyte (a)')
    out.append('#define WBYTE(a,d) rc_wbyte ((a), (d))')
    out.append('#define RWORD(a) rc_rword (a)')
    out.append('#define WWORD(a,d) rc_wword ((a), (d))')
    out.append('#define PUSH(v) rc_push (v)')
    out.append('#define POP() rc_pop ()')
    out.append('#define ROPCODE() (PC += 2, rk++ ? I1 : I0)')
    out.append('#define RC_STEP if (t11_ICount <= 0 || (state.pending_interrupts && t11_irq_ready ())) return')
    for a in sorted(blocks):
        out.append('static void rc_%04x (void)\n{' % a)
        insns = blocks[a]
        for k, (pc, op, h, length) in enumerate(insns):
            body = handlers[h].replace('state.op', 'OPC').strip()
            i0 = word(rom, pc + 2) if length > 2 else 0
            i1 = word(rom, pc + 4) if length > 4 else 0
            out.append('\t/* %04x: %06o %s */' % (pc, op, h))
            out.append('\tPC += 2; RC_INS; { const int OPC = 0x%04x, I0 = 0x%04x, I1 = 0x%04x; int rk = 0; (void)rk; %s } t11_ICount -= 22;'
                       % (op, i0, i1, body))
            if k + 1 < len(insns):
                out.append('\tRC_STEP;')
        out.append('}')
    out.append('#undef RC_STEP')
    out.append('#undef ROPCODE')
    out.append('#undef RWORD')
    out.append('#undef WWORD')
    out.append('#undef PUSH')
    out.append('#undef POP')
    out.append('static void (*const t11_rc_table[0x4000])(void) = {')
    row = []
    for i in range(0x4000):
        a = BASE + 2 * i
        row.append('rc_%04x' % a if a in blocks else '0')
    for i in range(0, 0x4000, 8):
        out.append('\t' + ', '.join(row[i:i + 8]) + ',')
    out.append('};')
    open(sys.argv[5], 'w').write('\n'.join(out) + '\n')
    print('%d blocks, %d instructions' % (len(blocks), sum(len(b) for b in blocks.values())))


if __name__ == '__main__':
    main()
