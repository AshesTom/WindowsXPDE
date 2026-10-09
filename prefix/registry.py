"""Minimal native shell COM registrations absent from Wine's generated prefix."""
import struct

VERSION = 6
FONT_PROFILE = r'HKCU\Software\Win4Wine\ThemeProfiles\JapaneseXPLuna'
CLASSIC_FONT_PROFILE = r'HKCU\Software\Win4Wine\ThemeProfiles\JapaneseXPClassic'


def font_profile_defaults():
    """Our opt-in profile, measured with public APIs on the user's Japanese XP.

    LOGFONTW has a documented 92-byte layout; heights are nominal 96 DPI pixels.
    These settings contain no font resources or Microsoft implementation code.
    """
    values = {
        'Enabled': ('REG_DWORD', '1'),
        'Version': ('REG_DWORD', '1'),
        'ThemePath': ('REG_SZ', r'C:\windows\Resources\Themes\Luna\luna.msstyles'),
        'ColorName': ('REG_SZ', 'NormalColor'),
        'SizeName': ('REG_SZ', 'NormalSize'),
    }
    face = 'MS UI Gothic'.encode('utf-16le').ljust(64, b'\0')
    for name, height, weight, italic in (
        ('CaptionFont', -13, 700, 0),
        ('SmallCaptionFont', -12, 700, 0),
        ('MenuFont', -12, 400, 0),
        ('StatusFont', -12, 400, 0),
        ('MessageFont', -12, 400, 0),
        ('IconFont', -12, 400, 0),
        ('StartFont', -16, 700, 1),
        ('ClockFont', -12, 400, 0),
    ):
        data = struct.pack('<5l8B64s', height, 0, 0, 0, weight,
                           italic, 0, 0, 1, 0, 0, 0, 0, face)
        values[name] = ('REG_BINARY', data.hex())
    return values


def install_classic_fonts(run, exists):
    """Fresh-prefix baseline only; existing-prefix migrations never call this."""
    face = 'MS UI Gothic'.encode('utf-16le').ljust(64, b'\0')
    for name, weight in (('CaptionFont', 700), ('SmallCaptionFont', 700),
                         ('MenuFont', 400), ('StatusFont', 400),
                         ('MessageFont', 400), ('IconFont', 400)):
        if not exists(CLASSIC_FONT_PROFILE, name):
            data = struct.pack('<5l8B64s', -12, 0, 0, 0, weight,
                               0, 0, 0, 1, 0, 0, 0, 0, face)
            run('reg', 'add', CLASSIC_FONT_PROFILE, '/v', name,
                '/t', 'REG_BINARY', '/d', data.hex(), '/f')

# Public class identifiers, cross-checked against the browseui assembly manifest:
# https://lists.reactos.org/hyperkitty/list/ros-diffs%40reactos.org/thread/ENB6UBPTHQSRZOG7GFIV2V4BJLWBTKYT/
# Execution logs require BandSite (ECD4FC4D) and ShellBrowser (AF604EFE).
BROWSEUI_CLASSES = (
    '{DD313E04-FEFF-11D1-8ECD-0000F87A470C}',
    '{0BE35200-8F91-11CE-9DE3-00AA004BB851}',
    '{00BB2765-6A77-11D0-A535-00C04FD7D062}',
    '{01E04581-4EEE-11D0-BFE9-00AA005B4383}',
    '{A08C11D2-A228-11D0-825B-00AA005B4383}',
    '{F61FFEC1-754F-11D0-80CA-00AA005B4383}',
    '{ECD4FC4D-521C-11D0-B792-00A0C90312E1}',
    '{ECD4FC4E-521C-11D0-B792-00A0C90312E1}',
    '{ECD4FC4F-521C-11D0-B792-00A0C90312E1}',
    '{E13EF4E4-D2F2-11D0-9816-00C04FD91972}',
    '{22BF0C20-6DA7-11D0-B373-00A0C9034938}',
    '{AF604EFE-8897-11D1-B944-00A0C90312E1}',
    '{EF8AD2D1-AE36-11D1-B2D2-006097DF8C11}',
    '{5E6AB780-7743-11CF-A12B-00AA004AE837}',
    '{AF4F6510-F982-11D0-8595-00AA004CD6D8}',
)


def install(run, exists):
    # Seed defaults once. Migrations preserve disabled profiles, custom fonts,
    # and even unrecognized values: the builtin reader validates or falls back.
    for name, (kind, data) in font_profile_defaults().items():
        if not exists(FONT_PROFILE, name):
            run('reg', 'add', FONT_PROFILE, '/v', name, '/t', kind, '/d', data, '/f')
    # Native My Computer defaults to explorer.exe's first icon. Our XP Explorer
    # has a separate name so Wine's explorer.exe can remain the desktop manager.
    run('reg', 'add', r'HKCR\CLSID\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\DefaultIcon',
        '/ve', '/t', 'REG_SZ', '/d', r'C:\xp\xp-explorer.exe,0', '/f')
    # Publicly documented Pin to Start Menu handler; users choose their own pins.
    for file_type in ('exefile', 'lnkfile'):
        run('reg', 'add', 'HKCR\\' + file_type + '\\shellex\\ContextMenuHandlers\\StartMenuPin',
            '/ve', '/t', 'REG_SZ', '/d', '{A2A9545D-A0C2-42B4-9708-A0B2BADD77C8}', '/f')
    # This alias contains only our source-built Wine controls, never XP code.
    run('reg', 'add', r'HKCU\Software\Wine\DllOverrides', '/v', 'win4wine-controls',
        '/d', 'builtin', '/f')
    servers = [(c, 'browseui.dll') for c in BROWSEUI_CLASSES]
    servers.append(('{A5E46E3A-8849-11D1-9D8C-00C04FC99D61}', 'shdocvw.dll'))
    # Providers verified with DllGetClassObject on the user's local XP SP3 DLLs.
    servers.extend((
        ('{26FDC864-BE88-46E7-9235-032D8EA5162E}', 'shell32.dll'),
        ('{7EB5FBE4-2100-49E6-8593-17E130122F91}', 'shell32.dll'),
        ('{5B4DAE26-B807-11D0-9815-00C04FD91972}', 'shell32.dll'),
        ('{4622AD11-FF23-11D0-8D34-00A0C90F2719}', 'shell32.dll'),
        ('{A2A9545D-A0C2-42B4-9708-A0B2BADD77C8}', 'shell32.dll'),
        ('{603D3800-BD81-11D0-A3A5-00C04FD706EC}', 'browseui.dll'),
        ('{603D3801-BD81-11D0-A3A5-00C04FD706EC}', 'browseui.dll'),
        ('{42AEDC87-2188-41FD-B9A3-0C966FEABEC1}', 'shdocvw.dll'),
        ('{7849596A-48EA-486E-8937-A2A3009F31A9}', 'shell32.dll'),
        ('{D912F8CF-0396-4915-884E-FB425D32943B}', 'shell32.dll'),
        ('{3F6953F0-5359-47FC-BD99-9F2CB95A62FD}', 'shell32.dll'),
    ))
    for clsid, dll in servers:
        key = 'HKCR\\CLSID\\' + clsid + '\\InprocServer32'
        run('reg', 'add', key, '/ve', '/t', 'REG_SZ', '/d', dll, '/f')
        run('reg', 'add', key, '/v', 'ThreadingModel', '/t', 'REG_SZ', '/d', 'Apartment', '/f')

    for dll in ('browselc', 'shdoclc'):
        run('reg', 'add', r'HKCU\Software\Wine\DllOverrides', '/v', dll, '/d', 'builtin', '/f')
        run('reg', 'add', r'HKCU\Software\Wine\AppDefaults\xp-explorer.exe\DllOverrides',
            '/v', dll, '/d', 'native', '/f')

    # Wine's builtin ShellExecute handles executable extensions directly;
    # native XP ShellExecute requires their explicit association.
    for extension in ('.exe', '.com'):
        run('reg', 'add', 'HKCR\\' + extension, '/ve', '/t', 'REG_SZ', '/d', 'exefile', '/f')
