#!/usr/bin/env python3
"""Read local XP hives without modifying them; export a bounded shell allowlist."""
import argparse
from pathlib import Path

NAMESPACE = ('{20D04FE0-3AEA-1069-A2D8-08002B30309D}',
             '{450D8FBA-AD25-11D0-98A8-0800361B1103}',
             '{645FF040-5081-101B-9F08-00AA002F954E}',
             '{21EC2020-3AEA-1069-A2DD-08002B30309D}')


def node_at(hive, path):
    node = hive.root()
    for component in path.split('\\'):
        node = hive.node_get_child(node, component)
        if not node:
            return None
    return node


def read_value(hive, path, name):
    node = node_at(hive, path)
    if not node:
        return None
    value = hive.node_get_value(node, name)
    return hive.value_value(value) if value else None


def quote(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"').replace('\r', '').replace('\n', '') + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--software', type=Path)
    parser.add_argument('--system', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if not args.software and not args.system:
        parser.error('Supply a local SOFTWARE and/or SYSTEM hive')
    if args.output.exists():
        parser.error('Refusing existing output')
    try:
        import hivex
    except ImportError:
        raise SystemExit('Install python3-hivex to read supplied registry hives')
    lines = ['Windows Registry Editor Version 5.00', '']
    def emit(hive, src, dest, names):
        values = []
        for name in names:
            result = read_value(hive, src, name)
            if result is None:
                continue
            kind, data = result
            label = quote(name) if name else '@'
            if kind == 1:
                text = data.decode('utf-16le').rstrip('\0')
                # Keep shell COM registrations pointed at the prefix's local DLL.
                if src.lower().endswith('inprocserver32') and not name:
                    dll = text.replace('/', '\\').split('\\')[-1].lower()
                    if dll != 'shell32.dll':
                        continue
                    text = dll
                values.append(label + '=' + quote(text))
            elif kind == 4 and len(data) == 4:
                values.append(label + '=dword:' + format(int.from_bytes(data, 'little'), '08x'))
        if values:
            lines.extend(['[' + dest + ']', *values, ''])
    if args.software:
        hive = hivex.Hivex(str(args.software.resolve(strict=True)), write=False)
        for clsid in NAMESPACE:
            base = 'Classes\\CLSID\\' + clsid
            emit(hive, base, 'HKEY_LOCAL_MACHINE\\Software\\' + base, ('', 'LocalizedString'))
            emit(hive, base + '\\InprocServer32', 'HKEY_LOCAL_MACHINE\\Software\\' + base + '\\InprocServer32', ('', 'ThreadingModel'))
    if args.system:
        hive = hivex.Hivex(str(args.system.resolve(strict=True)), write=False)
        selected = read_value(hive, 'Select', 'Current')
        number = int.from_bytes(selected[1], 'little') if selected and selected[0] == 4 else 1
        base = f'ControlSet{number:03d}\\Control\\Nls\\'
        for suffix, names in (('CodePage', ('ACP', 'OEMCP', 'MACCP')), ('Language', ('Default', 'InstallLanguage'))):
            emit(hive, base + suffix, 'HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Nls\\' + suffix, names)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-16') as stream:
        stream.write('\r\n'.join(lines))
    print(f'Wrote bounded shell settings to {args.output}; review before wine reg import')

if __name__ == '__main__':
    main()
