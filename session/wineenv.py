"""The shells a session can start, and the environment its Wine programs run in.

Shared by the session (supervisor.py) and win4wine-wine (wine.py), so that a
program started with "win4wine-wine" outside the session gets the session's
prefix, Wine and opt-ins, and looks and behaves as one started from the shell."""
import os
from pathlib import Path

# The shells a session can start. The prefix is made for one of them
# (prefix/create.py or prefix/reactos.py); its staging step runs before each
# start. XP's Welcome screen uses images extracted from the XP media.
SHELLS = {
    'xp': dict(marker='xp-shell.json', module='xp-explorer', command=r'C:\xp\xp-explorer.exe',
               stage=('prefix/stage.py',), welcome='greeter/xp-splash.py', folders=False, name=None, switcher=None),
    'reactos': dict(marker='reactos-shell.json', module='ros-explorer', command=r'C:\ros\ros-explorer.exe',
                    stage=('prefix/reactos.py', 'stage'), welcome=None, folders=True, name='reactos',
                    switcher='reactos'),
    # Experimental: Windows 7's x64 explorer (prefix/win7.py).
    'win7': dict(marker='win7-shell.json', module='w7-explorer', command=r'C:\w7\w7-explorer.exe',
                 stage=('prefix/win7.py', 'stage'), welcome='greeter/w7-splash.py', folders=True, name='win7',
                 switcher='win7'),
}
# Each shell's session.json, in ~/.config/win4wine (setup.sh)
CONFIGS = {'xp': 'session.json', 'reactos': 'reactos-session.json', 'win7': 'win7-session.json'}


def config_dir():
    return Path(os.environ.get('XDG_CONFIG_HOME') or Path.home() / '.config') / 'win4wine'


def environment(base, config, shell):
    """BASE with the variables every Wine program of SHELL's session needs
    (CONFIG: its session.json; SHELL: an entry of SHELLS)."""
    wine = Path(config['wine']).resolve(strict=True)
    prefix = Path(config['prefix']).resolve(strict=True)
    # WIN4WINE_EXTERNAL_SHELL is the opt-in of win4wine's Wine changes: without
    # it a program gets Wine's own behaviour (no Windows 7 Basic frames, for one).
    env = dict(base, WINEPREFIX=str(prefix), WIN4WINE_EXTERNAL_SHELL='1',
               WINEDLLOVERRIDES=f'explorer=b;{shell["module"]}=n;mscoree,mshtml=;uxtheme=b')
    if shell['folders']:
        # Wine's explorer.exe hands folder windows to the shell's own explorer.
        env['WIN4WINE_FOLDER_SHELL'] = shell['command']
    if shell['switcher']:
        # The proxy draws ReactOS's Alt+Tab window instead of XP's.
        env['WIN4WINE_SWITCHER'] = shell['switcher']
    if shell['name']:
        # Wine leaves ReactOS's own controls to it (Start button, dialogs);
        # with Windows 7's shell the XP shell's taskbar replacements are
        # left out and the Windows 7 changes apply (patches 0120 to 0137).
        env['WIN4WINE_SHELL'] = shell['name']
    # The prefix decides its architecture; the recovery prefix takes Wine's default.
    env.pop('WINEARCH', None)
    # "wine" in the session's prefix must be this Wine, not another one in
    # PATH, which would update the prefix to its own version.
    env['PATH'] = os.pathsep.join(filter(None, (str(wine.parent), env.get('PATH'))))
    # Wine derives its user locale and ANSI code page from the Unix locale.
    # A Japanese XP shell expects Japanese formats and code page 932 even
    # when the Linux session itself runs in another locale.
    if config.get('locale'):
        env.update(LANG=config['locale'], LC_ALL=config['locale'])
    return env
