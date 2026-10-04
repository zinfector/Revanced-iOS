"""Conservative thin ARM64 Mach-O inspection and load-command insertion.

Never relocates sections, chained fixups, or __LINKEDIT. Refuses insufficient
zero-filled header padding rather than attempting a general binary rewrite.
"""
from dataclasses import dataclass
import struct

MH_MAGIC_64 = 0xFEEDFACF
CPU_ARM64 = 0x0100000C
LC_SEGMENT_64 = 0x19
LC_CODE_SIGNATURE = 0x1D
LC_LOAD_DYLIB = 0xC
LC_ID_DYLIB = 0xD
LC_UUID = 0x1B
LC_ENCRYPTION_INFO_64 = 0x2C
LC_BUILD_VERSION = 0x32
LC_DYLD_CHAINED_FIXUPS = 0x80000034

class PatchError(ValueError):
    pass

@dataclass(frozen=True)
class Command:
    kind: int
    offset: int
    raw: bytes

class MachO:
    def __init__(self, data: bytes):
        self.data = data
        if len(data) < 32:
            raise PatchError('Truncated Mach-O header')
        magic, cpu, sub, self.filetype, count, size, self.flags, _ = struct.unpack_from('<8I', data)
        if magic != MH_MAGIC_64 or cpu != CPU_ARM64 or sub & 0xFFFFFF != 0:
            raise PatchError('Expected a thin little-endian ARM64 image (not arm64e/fat)')
        self.end = 32 + size
        if self.end > len(data) or count > size // 8:
            raise PatchError('Invalid Mach-O load-command table')
        self.commands = []
        off = 32
        self.sections = []
        self.uuid = None
        self.encrypted = False
        self.platform = None
        self.minimum_os = None
        for _ in range(count):
            if off + 8 > self.end:
                raise PatchError('Truncated load command')
            kind, n = struct.unpack_from('<2I', data, off)
            if n < 8 or n % 8 or off + n > self.end:
                raise PatchError('Invalid load-command size/alignment')
            raw = data[off:off+n]
            self.commands.append(Command(kind, off, raw))
            if kind == LC_UUID:
                if n != 24:
                    raise PatchError('Invalid UUID command')
                self.uuid = raw[8:24].hex()
            elif kind == LC_ENCRYPTION_INFO_64:
                if n < 24:
                    raise PatchError('Invalid encryption command')
                self.encrypted |= struct.unpack_from('<I', raw, 16)[0] != 0
            elif kind == LC_BUILD_VERSION:
                if n < 24:
                    raise PatchError('Invalid build-version command')
                self.platform, self.minimum_os = struct.unpack_from('<2I', raw, 8)
            elif kind == LC_SEGMENT_64:
                if n < 72:
                    raise PatchError('Invalid segment command')
                ns = struct.unpack_from('<I', raw, 64)[0]
                if 72 + ns * 80 > n:
                    raise PatchError('Invalid section table')
                for i in range(ns):
                    pos = 72 + i * 80
                    name = raw[pos:pos+16].split(b'\0')[0].decode('ascii')
                    addr, length, fileoff = struct.unpack_from('<QQI', raw, pos+32)
                    flags = struct.unpack_from('<I', raw, pos+64)[0]
                    self.sections.append((name, addr, length, fileoff, flags))
            off += n
        if off != self.end:
            raise PatchError('Load-command size does not match header')

    def dylibs(self):
        result = []
        for cmd in self.commands:
            if cmd.kind in (LC_LOAD_DYLIB, LC_ID_DYLIB, 0x80000018, 0x8000001F):
                if len(cmd.raw) < 24:
                    raise PatchError('Invalid dylib command')
                p = struct.unpack_from('<I', cmd.raw, 8)[0]
                if p < 24 or p >= len(cmd.raw) or b'\0' not in cmd.raw[p:]:
                    raise PatchError('Invalid dylib path')
                result.append((cmd.kind, cmd.raw[p:].split(b'\0')[0].decode('utf-8')))
        return result

    def va_offset(self, address, size=1):
        for name, va, n, fileoff, flags in self.sections:
            if va <= address and address + size <= va+n and fileoff:
                offset = fileoff + address-va
                if offset+size <= len(self.data):
                    return offset
        raise PatchError(f'Unmapped address {address:#x}')

    @property
    def first_content(self):
        offsets = [s[3] for s in self.sections if s[2] and s[3] and (s[4] & 255) not in (1, 12, 18)]
        if not offsets:
            raise PatchError('No file-backed section found')
        return min(offsets)

def inject_library(data: bytes, path: str) -> bytes:
    image = MachO(data)
    if image.filetype != 2 or image.encrypted:
        raise PatchError('Injection requires a decrypted MH_EXECUTE image')
    if not path.startswith('@executable_path/') or '\0' in path:
        raise PatchError('Library must use an @executable_path-relative install path')
    if any(name == path for _, name in image.dylibs()):
        raise PatchError('Payload is already loaded; patch the original IPA')
    name = path.encode('utf-8') + b'\0'
    n = (24 + len(name) + 7) & ~7
    command = struct.pack('<6I', LC_LOAD_DYLIB, n, 24, 0, 0x10000, 0x10000) + name
    command += bytes(n - len(command))
    kept = [c.raw for c in image.commands if c.kind != LC_CODE_SIGNATURE]
    table = b''.join(kept) + command
    end = 32 + len(table)
    if end > image.first_content:
        raise PatchError('Insufficient load-command padding; image was not modified')
    if end > image.end and any(data[image.end:end]):
        raise PatchError('Header padding contains data; image was not modified')
    patched = bytearray(data)
    struct.pack_into('<2I', patched, 16, len(kept)+1, len(table))
    patched[32:max(end, image.end)] = table + bytes(max(0, image.end-end))
    result = bytes(patched)
    after = MachO(result)
    if after.uuid != image.uuid or after.sections != image.sections or result[image.first_content:] != data[image.first_content:]:
        raise PatchError('Internal error: injection changed section data or identity')
    before_fixups = [c.raw for c in image.commands if c.kind == LC_DYLD_CHAINED_FIXUPS]
    after_fixups = [c.raw for c in after.commands if c.kind == LC_DYLD_CHAINED_FIXUPS]
    if before_fixups != after_fixups:
        raise PatchError('Internal error: chained fixup command changed')
    return result
