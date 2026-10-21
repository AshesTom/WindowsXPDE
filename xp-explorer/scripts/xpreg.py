#!/usr/bin/env python3
"""Registry entries of XP's own self-registration, for chosen classes.

XP's shell DLLs (shell32, shdocvw, browseui, ...) carry their registration as
an INF in a REGINST resource, which DllRegisterServer installs. Installing
it whole would replace Wine's own registrations, so this reads the user's
local XP files and keeps only the HKCR\\CLSID\\{...} keys of the classes asked
for (and their Shell Extensions Approved values), as a .reg file.

Usage: xpreg.py OUTPUT.reg DLL:{CLSID}[,{CLSID}...] ...
"""
import re
import struct
import sys
from pathlib import Path


def read_inf(path):
    """An INF's text: Unicode (UTF-16 with its byte order mark, as Japanese XP's
    are) or ANSI (English XP's; Latin-1 keeps every byte)."""
    data = Path(path).read_bytes()
    if data[:2] in (b'\xff\xfe', b'\xfe\xff'):
        return data.decode('utf-16')
    return data.decode('latin-1')


def reginst(path):
    """The REGINST resource of a PE file, as text."""
    data = Path(path).read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    count, optional = struct.unpack_from('<H12xH', data, pe + 6)
    magic = struct.unpack_from('<H', data, pe + 24)[0]
    directories = pe + 24 + (96 if magic == 0x10b else 112)
    sections = [struct.unpack_from('<IIII', data, pe + 24 + optional + 40 * i + 8) for i in range(count)]

    def offset(rva):
        for size, address, raw_size, raw in sections:
            if address <= rva < address + max(size, raw_size):
                return rva - address + raw
        raise ValueError('RVA outside the image')

    base = offset(struct.unpack_from('<I', data, directories + 16)[0])

    def name(value):
        if value & 0x80000000:
            at = base + (value & 0x7fffffff)
            length = struct.unpack_from('<H', data, at)[0]
            return data[at + 2:at + 2 + 2 * length].decode('utf-16le')
        return value

    def entries(at):
        named, ids = struct.unpack_from('<HH', data, at + 12)
        return [struct.unpack_from('<II', data, at + 16 + 8 * i) for i in range(named + ids)]

    for kind, kind_dir in entries(base):
        if name(kind) != 'REGINST':
            continue
        for _, name_dir in entries(base + (kind_dir & 0x7fffffff)):
            for _, leaf in entries(base + (name_dir & 0x7fffffff)):
                rva, size = struct.unpack_from('<II', data, base + leaf)
                raw = data[offset(rva):offset(rva) + size]
                return raw.decode('utf-16le') if raw[1:2] == b'\0' else raw.decode('latin-1')
    raise SystemExit(f'{path}: no REGINST resource')


def fields(line):
    """The comma-separated fields of an INF line, with quotes removed."""
    out, current, quoted = [], '', False
    for c in line:
        if c == '"':
            quoted = not quoted
        elif c == ',' and not quoted:
            out.append(current.strip())
            current = ''
        else:
            current += c
    out.append(current.strip())
    return out


def convert(text, module, classes, renames=()):
    strings, lines = {}, []
    section = None
    for raw in text.splitlines():
        line = raw.split(';', 1)[0].strip() if not raw.lstrip().startswith('"') else raw.strip()
        if not line:
            continue
        if line.startswith('['):
            section = line.strip('[]').lower()
            continue
        if section == 'strings' and '=' in line:
            key, value = line.split('=', 1)
            strings[key.strip().lower()] = value.strip().strip('"')
    builtin = {'11': r'C:\windows\system32', '25': r'C:\windows', '10': r'C:\windows',
               '_sys_mod_path': rf'C:\windows\system32\{module}', '_mod_path': rf'C:\windows\system32\{module}'}

    def expand(value):
        return re.sub(r'%([^%]+)%', lambda m: builtin.get(m.group(1).lower(), strings.get(m.group(1).lower(), m.group(0))), value)

    wanted = {c.lower() for c in classes}
    keys = {}
    for raw in text.splitlines():
        if not raw.upper().startswith(('HKCR,', 'HKLM,')):
            continue
        root, key, name, flags, value = (fields(raw) + [''] * 5)[:5]
        key = expand(key)
        match = re.match(r'CLSID\\(\{[0-9a-fA-F-]+\})', key)
        approved = root.upper() == 'HKLM' and 'shell extensions\\approved' in key.lower()
        clsid = match.group(1).lower() if match else (expand(name).lower() if approved else None)
        if clsid not in wanted:
            continue
        name, value, flags = expand(name), expand(value), expand(flags)
        for old, new in renames:
            value = re.sub(re.escape(old), lambda m: new, value, flags=re.IGNORECASE)
        flag = int(flags, 0) if flags else 0
        hive = 'HKEY_CLASSES_ROOT' if root.upper() == 'HKCR' else 'HKEY_LOCAL_MACHINE'
        values = keys.setdefault(f'{hive}\\{key}', [])
        if not name and not value and not flags:
            continue  # the key alone
        left = '@' if not name else '"' + name.replace('\\', '\\\\').replace('"', '\\"') + '"'
        if flag & 0x00010001 == 0x00010001:
            values.append(f'{left}=dword:{int(value, 0):08x}')
        elif flag & 0x00020000:
            data = (value + '\0').encode('utf-16le')
            values.append(f'{left}=hex(2):' + ','.join(f'{b:02x}' for b in data))
        else:
            values.append(f'{left}="' + value.replace('\\', '\\\\').replace('"', '\\"') + '"')
    return keys


HIVES = {'HKCU': 'HKEY_CURRENT_USER', 'HKLM': 'HKEY_LOCAL_MACHINE', 'HKCR': 'HKEY_CLASSES_ROOT'}


def hive_entries(text, root, prefixes, exclude=(), extra=None, renames=(), install=None):
    """The ROOT lines of an XP setup hive INF (HIVEDEF.INF, HIVESFT.INF) under
    one of PREFIXES (and none of EXCLUDE), as .reg text. INF AddReg flags:
    0x1 binary (its type in the high word), 0x10 the key alone, 0x10000
    REG_MULTI_SZ, 0x20000 REG_EXPAND_SZ, 0x10001 REG_DWORD; [Strings] are
    expanded, %SystemRoot% and the like are left for REG_EXPAND_SZ. An empty
    prefix takes every key; EXTRA adds strings (a REGINST's %_SYS_MOD_PATH%,
    an INF's directory IDs); INSTALL names the install section of an INF."""
    strings, section = {}, None
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith('['):
            section = line.strip('[]').lower()
        elif section == 'strings' and '=' in line and not line.startswith(';'):
            key, value = line.split('=', 1)
            strings[key.strip().lower()] = value.strip().strip('"')
    strings.update({k.lower(): v for k, v in (extra or {}).items()})

    def expand(value):
        # Renames first: they may name an INF string (%MSIEXPLORE%).
        for old, new in renames:
            value = re.sub(re.escape(old), lambda m: new, value, flags=re.IGNORECASE)
        value = re.sub(r'%([^%]+)%', lambda m: strings.get(m.group(1).lower(), m.group(0)), value)
        value = value.replace('%%', '%')
        for old, new in renames:
            value = re.sub(re.escape(old), lambda m: new, value, flags=re.IGNORECASE)
        return value

    def quote(value):
        return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'

    # Join continuation lines (ending in a backslash).
    logical, current = [], ''
    for raw in text.splitlines():
        line = raw.rstrip()
        if line.endswith('\\'):
            current += line[:-1]
            continue
        logical.append(current + line)
        current = ''
    # As XP's setup: [AddReg] adds and [DelReg...] deletes in a hive INF; in a
    # REGINST, the sections its install section names (AddReg=, DelReg=).
    sections = {(l.strip()[1:].split(']')[0]).lower() for l in logical if l.strip().startswith('[')}
    add, delete = {'addreg'}, {s for s in sections if s.startswith('delreg')}
    if install or 'addreg' not in sections:
        install_name, add, delete = None, set(), set()

        def section_ok(name, wanted):
            return name in ([w.lower() for w in wanted] if wanted else ('regdll', 'defaultinstall'))

        for raw in logical:
            line = raw.strip()
            if line.startswith('['):
                install_name = line[1:].split(']')[0].lower()
            elif section_ok(install_name, install) and '=' in line:
                directive, names = (part.strip() for part in line.split('=', 1))
                target = add if directive.lower() == 'addreg' else delete if directive.lower() == 'delreg' else None
                if target is not None:
                    target.update(n.strip().lower() for n in names.split(','))
    keys, deleted, section = {}, [], None
    for line in logical:
        if line.strip().startswith('['):
            section = line.strip()[1:].split(']')[0].lower()
            continue
        if not line.upper().startswith(root.upper() + ',') or section not in add | delete:
            continue
        if section in delete:
            parts = fields(line) + ['', '']
            key = expand(parts[1]).replace('\\\\', '\\').rstrip('\\')
            lower = key.lower()
            if any(not p or lower == p.lower() or lower.startswith(p.lower() + '\\') for p in prefixes) and \
                    not any(lower == e.lower() or lower.startswith(e.lower() + '\\') for e in exclude):
                full = HIVES[root.upper()] + '\\' + key
                name = expand(parts[2])
                deleted.append(f'[{full}]\r\n' + ('@' if not name else quote(name)) + '=-' if name else f'[-{full}]')
            continue
        parts = fields(line)
        parts += [''] * (4 - min(4, len(parts)))
        _, key, name, flags, *data = parts
        key = expand(key).replace('\\\\', '\\').rstrip('\\')
        lower = key.lower()
        if not any(not p or lower == p.lower() or lower.startswith(p.lower() + '\\') for p in prefixes):
            continue
        if any(lower == e.lower() or lower.startswith(e.lower() + '\\') for e in exclude):
            continue
        values = keys.setdefault(HIVES[root.upper()] + '\\' + key, [])
        named = {'REG_SZ': 0, 'REG_EXPAND_SZ': 0x20000, 'REG_MULTI_SZ': 0x10000, 'REG_DWORD': 0x10001,
                 'REG_BINARY': 0x1, 'REG_NONE': 0x20001}
        flag = 0
        # A REGINST may use advpack's names for them without defining them.
        named.update({'%REGSZ%': 0, '%REGEXSZ%': 0x20000, '%REGDW%': 0x10001, '%REGMULTISZ%': 0x10000,
                      '%REGBIN%': 0x1})
        for token in expand(flags).replace(' ', '').split('|'):
            if token:
                flag |= named[token.upper()] if token.upper() in named else int(token, 0)
        name = expand(name)
        if flag & 0x10 or (not name and not data and not flags):
            continue  # the key alone
        left = '@' if not name else quote(name)
        data = [d for d in data if d != ''] if flag & 0x1 else data
        if flag & 0x00010001 == 0x00010001 and not (flag >> 16) & ~1:
            number = data[0] if data else '0'
            values.append(f'{left}=dword:{int(number, 0) & 0xffffffff:08x}')
        elif flag & 0x1:
            kind = flag >> 16
            hexes = ','.join(f'{int(b, 16):02x}' for b in data)
            values.append(f'{left}=hex' + (f'({kind:x})' if kind not in (0, 3) else '') + f':{hexes}')
        elif flag & 0x00010000:
            raw = ''.join(expand(d) + '\0' for d in data) + '\0'
            values.append(f'{left}=hex(7):' + ','.join(f'{b:02x}' for b in raw.encode('utf-16le')))
        elif flag & 0x00020000:
            raw = expand(data[0] if data else '') + '\0'
            values.append(f'{left}=hex(2):' + ','.join(f'{b:02x}' for b in raw.encode('utf-16le')))
        else:
            values.append(f'{left}=' + quote(expand(data[0] if data else '')))
    lines = ['Windows Registry Editor Version 5.00', '']
    for entry in deleted:
        lines += [entry, '']
    for key, values in keys.items():
        lines += [f'[{key}]', *values, '']
    return '\r\n'.join(lines) + '\r\n'


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    keys = {}
    for argument in sys.argv[2:]:
        path, classes = argument.rsplit(':', 1)
        for key, values in convert(reginst(path), Path(path).name, classes.split(',')).items():
            keys.setdefault(key, []).extend(values)
    lines = ['Windows Registry Editor Version 5.00', '']
    for key, values in keys.items():
        lines += [f'[{key}]', *values, '']
    Path(sys.argv[1]).write_text('\r\n'.join(lines) + '\r\n', encoding='utf-16')
    print(f'{len(keys)} keys')


if __name__ == '__main__':
    main()
