"""Bounded thin ARM64 metadata reader. Unsupported layouts fail closed."""
import struct

class Image:
    def __init__(self, data):
        self.data, self.segments, self.sections = data, [], []
        magic, cpu, sub, typ, count, size, _, _ = self.unpack('<8I', 0)
        if (magic, cpu, sub & 0xffffff, typ) != (0xfeedfacf, 0x100000c, 0, 2):
            raise ValueError('Expected thin ARM64 executable')
        self.uuid, self.fixup, self.encrypted = None, None, False
        p, end = 32, 32 + size
        if end > len(data) or count > size // 8:
            raise ValueError('Invalid load command count/range')
        for _ in range(count):
            cmd, n = self.unpack('<2I', p)
            if n < 8 or n % 8 or p + n > end:
                raise ValueError('Invalid load command')
            if cmd == 0x19:
                name, va, vs, fo, fs, _, protection, ns, _ = self.unpack('<16s4Q4I', p + 8)
                if fs > vs or fo + fs > len(data): raise ValueError('Invalid segment range')
                self.segments.append(dict(name=name.rstrip(b'\0').decode(), va=va,
                                          vmsize=vs, fileoff=fo, filesize=fs,
                                          protection=protection))
                if 72 + ns * 80 > n:
                    raise ValueError('Invalid sections')
                for i in range(ns):
                    sn, sg, sa, ss, so, _, _, _, flags, _, _, _ = self.unpack('<16s16sQQ8I', p + 72 + i * 80)
                    self.sections.append(dict(name=sn.rstrip(b'\0').decode(), segment=sg.rstrip(b'\0').decode(),
                                              va=sa, size=ss, fileoff=so, flags=flags))
            elif cmd == 0x1b:
                if n != 24 or self.uuid is not None: raise ValueError('Invalid/duplicate UUID')
                self.uuid = data[p + 8:p + 24].hex()
            elif cmd == 0x2c:
                self.encrypted |= self.unpack('<I', p + 16)[0] != 0
            elif cmd == 0x80000034:
                if n != 16 or self.fixup is not None: raise ValueError('Invalid/duplicate fixup command')
                self.fixup = self.unpack('<2I', p + 8)
            p += n
        if p != end or self.encrypted or not self.fixup or self.uuid is None:
            raise ValueError('Unsupported/encrypted image')
        self.base = next(s['va'] for s in self.segments if s['name'] == '__TEXT')
        self.pointers, self.binds, self.formats = {}, {}, set()
        self.read_fixups()

    def unpack(self, fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(self.data):
            raise ValueError('Read outside image')
        return struct.unpack_from(fmt, self.data, offset)

    def offset(self, va, size=1):
        for s in self.segments:
            if s['va'] <= va and va + size <= s['va'] + s['filesize']:
                p = s['fileoff'] + va - s['va']
                if p + size <= len(self.data):
                    return p
        raise ValueError(f'Unmapped VA {va:#x}')

    def u32(self, va):
        return self.unpack('<I', self.offset(va, 4))[0]

    def i32(self, va):
        return self.unpack('<i', self.offset(va, 4))[0]

    def ptr(self, va):
        if va in self.pointers:
            return self.pointers[va]
        if va in self.binds:
            return 0  # External references remain explicit in self.binds.
        raw = self.unpack('<Q', self.offset(va, 8))[0]
        if raw == 0:
            return 0
        raise ValueError(f'Nonzero pointer outside a recorded fixup slot {va:#x}')

    def string(self, va):
        if not va:
            return ''
        p = self.offset(va)
        section_end = next(s['fileoff'] + s['filesize'] for s in self.segments
                           if s['va'] <= va < s['va'] + s['filesize'])
        end = self.data.find(b'\0', p, min(p + 20000, section_end))
        if end < 0:
            raise ValueError('Unterminated string')
        return self.data[p:end].decode('utf-8', errors='strict')

    def read_fixups(self):
        f, length = self.fixup
        if f < 0 or length < 28 or f + length > len(self.data):
            raise ValueError('Invalid fixup range')
        version, starts, imports, symbols, count, fmt, symbol_fmt = self.unpack('<7I', f)
        if f + length > len(self.data) or version != 0 or symbol_fmt != 0 or fmt not in (1, 2, 3):
            raise ValueError('Unsupported fixup header')
        if count > 1000000 or not 28 <= starts < length or not 28 <= imports <= length or not 28 <= symbols < length:
            raise ValueError('Invalid fixup offsets/count')
        names = []
        stride = {1: 4, 2: 8, 3: 16}[fmt]
        if imports + count * stride > length:
            raise ValueError('Imports exceed fixup range')
        for i in range(count):
            p = f + imports + i * stride
            word = self.unpack('<Q' if fmt == 3 else '<I', p)[0]
            name_offset = word >> (32 if fmt == 3 else 9)
            p = f + symbols + name_offset
            if p >= f + length: raise ValueError('Import name outside fixup range')
            end = self.data.find(b'\0', p, f + length)
            if end < p:
                raise ValueError('Invalid import name')
            names.append(self.data[p:end].decode())
        start = f + starts
        n = self.unpack('<I', start)[0]
        if n > len(self.segments) or starts + 4 + n * 4 > length:
            raise ValueError('Invalid fixup segment count')
        for index, delta in enumerate(self.unpack('<' + str(n) + 'I', start + 4)):
            if not delta:
                continue
            p = start + delta
            if p < f or p + 22 > f + length: raise ValueError('Invalid chain start offset')
            struct_size, page_size, pointer_fmt, segment_offset, _, pages = self.unpack('<IHHQIH', p)
            self.formats.add(pointer_fmt)
            if pointer_fmt != 6:
                raise ValueError(f'Prototype requires pointer format 6, got {pointer_fmt}')
            segment = self.segments[index]
            if self.base + segment_offset != segment['va'] or 22 + pages * 2 > struct_size or p + struct_size > f + length or page_size not in (4096, 16384):
                raise ValueError('Invalid chain segment')
            for page in range(pages):
                first = self.unpack('<H', p + 22 + page * 2)[0]
                if first == 0xffff:
                    continue
                if first & 0x8000:
                    raise ValueError('Prototype does not support multiple chain starts')
                page_va = segment['va'] + page * page_size
                va = page_va + first
                while True:
                    if not page_va <= va or va + 8 > page_va + page_size:
                        raise ValueError('Fixup crossed page boundary')
                    raw = self.unpack('<Q', self.offset(va, 8))[0]
                    if raw >> 63:
                        ordinal = raw & 0xffffff
                        if ordinal >= len(names): raise ValueError('Invalid bind ordinal')
                        self.binds[va] = names[ordinal]
                    else:
                        if (raw >> 44) & 0x7f:
                            raise ValueError('Reserved rebase bits set')
                        target = (raw & ((1 << 36) - 1)) | (((raw >> 36) & 0xff) << 56)
                        self.pointers[va] = self.base + target
                    delta = ((raw >> 51) & 0xfff) * 4
                    if not delta:
                        break
                    va += delta

    def methods(self, va, kind, origin='class'):
        if not va:
            return []
        flags, count = self.u32(va), self.u32(va + 4)
        size = flags & 0xfffc
        if count > 20000 or size not in (12, 24):
            raise ValueError('Unsupported method list')
        rows = []
        for i in range(count):
            p = va + 8 + i * size
            if flags & 0x80000000:
                sel = p + self.i32(p)
                if not flags & 0x40000000:
                    sel = self.ptr(sel)
                typ, imp = p + 4 + self.i32(p + 4), p + 8 + self.i32(p + 8)
            else:
                sel, typ, imp = self.ptr(p), self.ptr(p + 8), self.ptr(p + 16)
            self.offset(imp, 4)
            rows.append(dict(kind=kind, selector=self.string(sel), encoding=self.string(typ),
                             imp=hex(imp), rva=hex(imp - self.base), origin=origin))
        return rows

    def ro(self, cls):
        return self.ptr(cls + 32) & ~7

    def classes(self):
        classes, addresses, errors = {}, {}, []
        sec = next(s for s in self.sections if s['name'] == '__objc_classlist')
        for i in range(sec['size'] // 8):
            a = self.ptr(sec['va'] + i * 8)
            try:
                r, meta, sup = self.ro(a), self.ptr(a), self.ptr(a + 8)
                name = self.string(self.ptr(r + 24))
                if not name or name in classes:
                    raise ValueError('Empty/duplicate class name')
                fields, iv = [], self.ptr(r + 48)
                if iv:
                    stride, count = self.u32(iv), self.u32(iv + 4)
                    if stride != 32 or count > 10000:
                        raise ValueError('Unsupported ivar list')
                    for j in range(count):
                        p = iv + 8 + j * stride
                        offset_storage = self.ptr(p)
                        field_offset, offset_status = None, 'runtime_required'
                        if offset_storage:
                            try:
                                field_offset = self.u32(offset_storage)
                                offset_status = 'file_backed_initial_value'
                            except ValueError:
                                if not any(s['va'] + s['filesize'] <= offset_storage and
                                           offset_storage + 4 <= s['va'] + s['vmsize'] for s in self.segments):
                                    raise
                                offset_status = 'zero_fill_runtime_required'
                        fields.append(dict(name=self.string(self.ptr(p + 8)), encoding=self.string(self.ptr(p + 16)),
                                           offset=field_offset, offset_status=offset_status,
                                           offset_storage=hex(offset_storage), size=self.u32(p + 28)))
                superclass = self.string(self.ptr(self.ro(sup) + 24)) if sup else None
                classes[name] = dict(name=name, address=hex(a), superclass=superclass,
                                     external_superclass=self.binds.get(a + 8),
                                     instance_size=self.u32(r + 8), ivars=fields,
                                     methods=self.methods(self.ptr(r + 32), '-') +
                                     self.methods(self.ptr(self.ro(meta) + 32), '+'))
                addresses[a] = name
            except Exception as ex:
                errors.append(dict(address=hex(a), error=str(ex)))
        category_count = 0
        for sec in (s for s in self.sections if s['name'] in ('__objc_catlist', '__objc_nlcatlist')):
            for i in range(sec['size'] // 8):
                a = self.ptr(sec['va'] + i * 8)
                try:
                    owner = addresses.get(self.ptr(a + 8))
                    if owner:
                        origin = 'category:' + self.string(self.ptr(a))
                        classes[owner]['methods'].extend(self.methods(self.ptr(a + 16), '-', origin) +
                                                         self.methods(self.ptr(a + 24), '+', origin))
                    category_count += 1
                except Exception as ex:
                    errors.append(dict(category=hex(a), error=str(ex)))
        return classes, errors, category_count

