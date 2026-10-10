"""Bounded descriptor adapter; only the validated GPB constructor/layout."""
import struct
import capstone
from capstone.arm64_const import ARM64_OP_REG, ARM64_OP_IMM, ARM64_OP_MEM
CONSTRUCTOR = "allocDescriptorForClass:messageName:runtimeSupport:fileDescription:fields:fieldCount:storageSize:flags:"
DIS = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_LITTLE_ENDIAN)
DIS.detail = True


def instructions(image, va, size):
    p = image.offset(va, size)
    return DIS.disasm(image.data[p:p + size], va)


def reg(ins, number):
    return ins.reg_name(number).replace('w', 'x', 1)


def assign(image, ins, values):
    ops = ins.operands
    if not ops or ops[0].type != ARM64_OP_REG:
        return
    dest = reg(ins, ops[0].reg)
    if ins.mnemonic in ('adrp', 'adr', 'mov') and len(ops) == 2 and ops[1].type == ARM64_OP_IMM:
        values[dest] = ops[1].imm
    elif ins.mnemonic == 'mov' and len(ops) == 2 and ops[1].type == ARM64_OP_REG:
        values[dest] = values.get(reg(ins, ops[1].reg))
    elif ins.mnemonic == 'add' and len(ops) == 3 and ops[1].type == ARM64_OP_REG and ops[2].type == ARM64_OP_IMM:
        prior = values.get(reg(ins, ops[1].reg))
        values[dest] = prior + ops[2].imm if isinstance(prior, int) and not ops[2].shift.value else None
    elif ins.mnemonic == 'ldr' and len(ops) == 2 and ops[1].type == ARM64_OP_MEM:
        mem = ops[1].mem
        prior = values.get(reg(ins, mem.base))
        values[dest] = None
        if isinstance(prior, int) and not mem.index:
            address = prior + mem.disp
            try:
                values[dest] = image.binds.get(address) or image.ptr(address)
            except ValueError:
                pass
    else:
        # A bounded constant tracker: discard every other written register.
        _, written = ins.regs_access()
        for number in written:
            values.pop(reg(ins, number), None)


def selector(image, target):
    values = {}
    try:
        for ins in instructions(image, target, 20):
            assign(image, ins, values)
        value = values.get('x1')
        return image.string(value) if isinstance(value, int) and value else None
    except ValueError:
        return None


def table(image, method):
    values = {}
    for ins in instructions(image, int(method['imp'], 16), 256):
        if ins.mnemonic == 'ret':
            break
        if ins.mnemonic == 'bl':
            target = ins.operands[0].imm
            if selector(image, target) == CONSTRUCTOR:
                receiver = values.get('x0')
                receiver_name = (image.string(image.ptr(image.ro(receiver) + 24))
                                 if isinstance(receiver, int) and receiver else receiver)
                if receiver_name not in ('GPBDescriptor', '_OBJC_CLASS_$_GPBDescriptor'):
                    raise ValueError('Unverified GPBDescriptor receiver')
                address, count = values.get('x6'), values.get('x7')
                if not isinstance(address, int) or not isinstance(count, int) or not 0 < count <= 4096:
                    raise ValueError('Unresolved field table/count')
                rows = []
                for i in range(count):
                    entry = address + 32 * i
                    # Observed GPBMessageFieldDescription ABI; layout is not universal.
                    _, _, number, has, storage, flags, typ = struct.unpack_from('<QQIiIHBx', image.data, image.offset(entry, 32))
                    name = image.string(image.ptr(entry))
                    if not name or not 0 < number <= 0x1fffffff or not 0 <= typ <= 17:
                        raise ValueError('Field schema validation failed')
                    rows.append(dict(name=name, number=number, has_index=has, storage_offset=storage,
                                     flags=flags, data_type=typ))
                if len({r['number'] for r in rows}) != count:
                    raise ValueError('Duplicate field numbers')
                return dict(descriptor_imp=method['imp'], constructor=CONSTRUCTOR, constructor_stub=hex(target),
                            table=hex(address), count=count, fields=rows)
            # Unrecognized call destroys caller-saved constant state.
            for i in range(19):
                values.pop('x' + str(i), None)
        else:
            assign(image, ins, values)
    raise ValueError('Observed constructor pattern not recovered')

