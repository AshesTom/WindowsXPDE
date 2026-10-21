/* Follow the user's input languages for the session's keyboard layout.
 * Usage: keyboard-watch.exe SCRIPT, the Unix path of session/keyboard.py.
 * XP's Text Services and Input Languages (input.dll) writes HKCU\Keyboard
 * Layout\Preload, Substitutes and Toggle; each change runs
 * "python3 SCRIPT apply --klids KLID,... --toggle N", which sets the
 * session's XKB layout (labwc or setxkbmap). Wine runs a Unix executable
 * named in CreateProcess natively, as linux-launch.c does. A GUI-subsystem
 * program, so that it opens no console. Public Win32 APIs only. */
#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>

#define KEY L"Keyboard Layout"

/* "KLID,KLID,..." from Preload in order, each through Substitutes. */
static void layouts(WCHAR *out, size_t size)
{
    HKEY preload, substitutes = NULL;
    *out = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, KEY L"\\Preload", 0, KEY_READ, &preload)) return;
    RegOpenKeyExW(HKEY_CURRENT_USER, KEY L"\\Substitutes", 0, KEY_READ, &substitutes);
    for (int i = 1; i < 32; i++)
    {
        WCHAR name[12], klid[KL_NAMELENGTH], substitute[KL_NAMELENGTH];
        DWORD length = sizeof(klid), type;
        swprintf(name, ARRAYSIZE(name), L"%d", i);
        if (RegQueryValueExW(preload, name, NULL, &type, (BYTE *)klid, &length) || type != REG_SZ) break;
        klid[ARRAYSIZE(klid) - 1] = 0;
        length = sizeof(substitute);
        if (substitutes && !RegQueryValueExW(substitutes, klid, NULL, &type, (BYTE *)substitute, &length) &&
            type == REG_SZ)
        {
            substitute[ARRAYSIZE(substitute) - 1] = 0;
            wcscpy(klid, substitute);
        }
        if (wcslen(out) + wcslen(klid) + 2 >= size) break;
        if (*out) wcscat(out, L",");
        wcscat(out, klid);
    }
    if (substitutes) RegCloseKey(substitutes);
    RegCloseKey(preload);
}

/* XP's switching key: Toggle\Hotkey, "1" (Left Alt+Shift) when absent. */
static void toggle(WCHAR *out, size_t size)
{
    DWORD length = (DWORD)(size * sizeof(WCHAR));
    if (RegGetValueW(HKEY_CURRENT_USER, KEY L"\\Toggle", L"Hotkey", RRF_RT_REG_SZ, NULL, out, &length) || !*out)
        wcscpy(out, L"1");
}

static void apply(const WCHAR *script)
{
    WCHAR klids[512], hotkey[8], line[2048];
    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION process;

    layouts(klids, ARRAYSIZE(klids));
    if (!*klids) return;
    toggle(hotkey, ARRAYSIZE(hotkey));
    /* KLIDs and the hotkey are hexadecimal digits, the script a path without quotes. */
    swprintf(line, ARRAYSIZE(line), L"/usr/bin/env -u WINEDEBUG python3 \"%ls\" apply --klids %ls --toggle %ls",
             script, klids, hotkey);
    if (CreateProcessW(L"Z:\\usr\\bin\\env", line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                       &startup, &process))
    {
        WaitForSingleObject(process.hProcess, 30000);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, WCHAR *command, int show)
{
    HANDLE changed = CreateEventW(NULL, FALSE, FALSE, NULL);
    WCHAR last[512] = L"", current[512], hotkey[8], state[528];
    int argc;
    WCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    HKEY key;

    (void)instance; (void)previous; (void)command; (void)show;
    if (argc < 2 || !changed) return 2;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, KEY, 0, NULL, 0, KEY_NOTIFY | KEY_READ, NULL, &key, NULL)) return 1;
    for (;;)
    {
        if (RegNotifyChangeKeyValue(key, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, changed, TRUE))
            return 1;
        /* input.dll writes several values in a row: apply once they are all there. */
        layouts(current, ARRAYSIZE(current));
        toggle(hotkey, ARRAYSIZE(hotkey));
        swprintf(state, ARRAYSIZE(state), L"%ls/%ls", current, hotkey);
        if (*last && wcscmp(state, last)) apply(argv[1]);
        wcscpy(last, state);
        WaitForSingleObject(changed, INFINITE);
        Sleep(500);
    }
}
