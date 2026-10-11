/* Start a Linux program from an XP Start menu shortcut.
 * Usage: linux-launch.exe PROGRAM [ARGUMENTS...], quoted as a Windows command line.
 * A GUI-subsystem program, so that starting it opens no console. Wine runs a
 * Unix executable named in CreateProcess natively; /usr/bin/env receives the
 * Windows form of its own path as argv[0] and looks PROGRAM up in PATH. The
 * shortcut's working directory becomes the program's. Public Win32 APIs only. */
#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <wchar.h>

/* The command line after the first argument, by the rules CommandLineToArgvW uses for it. */
static WCHAR *arguments(WCHAR *line)
{
    BOOL quoted = FALSE;
    for (; *line && (quoted || (*line != ' ' && *line != '\t')); line++)
        if (*line == '"') quoted = !quoted;
    while (*line == ' ' || *line == '\t') line++;
    return line;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, WCHAR *unused, int show)
{
    static const WCHAR env[] = L"Z:\\usr\\bin\\env";
    /* The XP session's debug channels are for its own logs, not for Linux programs. */
    static const WCHAR prefix[] = L"/usr/bin/env -u WINEDEBUG ";
    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION process;
    WCHAR *rest = arguments(GetCommandLineW()), *line;
    size_t length = wcslen(prefix) + wcslen(rest) + 1;
    (void)instance; (void)previous; (void)unused; (void)show;
    if (!*rest || !(line = malloc(length * sizeof(WCHAR)))) return 2;
    wcscpy(line, prefix);
    wcscat(line, rest);
    if (!CreateProcessW(env, line, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process))
    {
        WCHAR message[160];
        DWORD error = GetLastError();
        _snwprintf(message, 160, L"The Linux program could not be started (error %lu).", error);
        message[159] = 0;
        MessageBoxW(NULL, message, L"Linux", MB_OK | MB_ICONERROR);
        free(line);
        return 1;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    free(line);
    return 0;
}
