/* Play an event of XP's sound scheme: playsound.exe EVENT (SystemStart,
 * WindowsLogoff, ...), as XP plays them at logon and log off. The scheme is
 * the user's (Control Panel, Sounds and Audio Devices), and an event without
 * a sound plays nothing. Waits until the sound ends. Public Win32 APIs only. */
#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, WCHAR *command, int show)
{
    int argc;
    WCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    (void)instance; (void)previous; (void)command; (void)show;
    if (!argv || argc < 2) return 2;
    return PlaySoundW(argv[1], NULL, SND_ALIAS | SND_SYNC | SND_NODEFAULT) ? 0 : 1;
}
