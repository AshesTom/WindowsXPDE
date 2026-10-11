"""Read bitmap and string resources from a PE file (no code is read or run).

The greeter draws the Welcome screen from the user's own XP files at run
time; this module only walks the documented resource directory.
"""
import struct

RT_BITMAP, RT_STRING = 2, 6


class Resources:
    def __init__(self, path):
        self.data = open(path, 'rb').read()
        pe = struct.unpack_from('<I', self.data, 0x3c)[0]
        if self.data[pe:pe + 4] != b'PE\0\0':
            raise ValueError(f'{path}: not a PE file')
        sections, optional = struct.unpack_from('<H12xH', self.data, pe + 6)
        magic = struct.unpack_from('<H', self.data, pe + 24)[0]
        directories = pe + 24 + (96 if magic == 0x10b else 112)
        self.rva = struct.unpack_from('<I', self.data, directories + 2 * 8)[0]
        self.sections = []
        table = pe + 24 + optional
        for i in range(sections):
            size, address, raw_size, raw = struct.unpack_from('<IIII', self.data, table + 40 * i + 8)
            self.sections.append((address, max(size, raw_size), raw))
        self.base = self.offset(self.rva)

    def offset(self, rva):
        for address, size, raw in self.sections:
            if address <= rva < address + size:
                return rva - address + raw
        raise ValueError(f'RVA {rva:#x} outside the sections')

    def entries(self, offset):
        named, numbered = struct.unpack_from('<12xHH', self.data, offset)
        for i in range(named + numbered):
            name, target = struct.unpack_from('<II', self.data, offset + 16 + 8 * i)
            yield name, target

    def name(self, entry):
        if not entry & 0x80000000:
            return entry
        offset = self.base + (entry & 0x7fffffff)
        length = struct.unpack_from('<H', self.data, offset)[0]
        return self.data[offset + 2:offset + 2 + 2 * length].decode('utf-16-le')

    def find(self, rtype, name):
        """The data of the first language of resource TYPE/NAME (numbers or names), or None."""
        level = self.base
        for key in (rtype, name, None):
            for entry, target in self.entries(level):
                if key is None or self.name(entry) == key:
                    break
            else:
                return None
            level = self.base + (target & 0x7fffffff)
            if not target & 0x80000000:
                rva, size = struct.unpack_from('<II', self.data, level)
                start = self.offset(rva)
                return self.data[start:start + size]
        return None

    def bitmap(self, name):
        """A BITMAP resource as the bytes of a .bmp file."""
        dib = self.find(RT_BITMAP, name)
        if dib is None:
            raise KeyError(f'bitmap {name}')
        header, _, _, _, bits, compression, _, _, _, used = struct.unpack_from('<IiiHHIIiiI', dib)
        colors = used or (1 << bits if bits <= 8 else 0)
        masks = 12 if compression == 3 and header == 40 else 0
        offset = 14 + header + masks + 4 * colors
        return b'BM' + struct.pack('<IHHI', 14 + len(dib), 0, 0, offset) + dib

    def string(self, number):
        block = self.find(RT_STRING, number // 16 + 1)
        if block is None:
            return None
        position = 0
        for _ in range(number % 16):
            position += 2 + 2 * struct.unpack_from('<H', block, position)[0]
        length = struct.unpack_from('<H', block, position)[0]
        return block[position + 2:position + 2 + 2 * length].decode('utf-16-le') or None
