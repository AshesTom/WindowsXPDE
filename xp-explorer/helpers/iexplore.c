/* Internet Explorer for the XP shell: opens the Linux desktop's web browser.
 *
 * XP's own Internet Explorer 6 cannot open today's web (no current TLS), so
 * this stands in for iexplore.exe (Wine DllOverrides iexplore.exe=native):
 * the desktop's Internet icon, links and "Internet" in the Start menu all
 * reach it. With an address it runs xdg-open on it; without one it starts
 * the default browser (xdg-settings) through gtk-launch.
 * Usage: iexplore.exe [-new] [-nohome] [-k] [URL] */
#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <wchar.h>

static BOOL run(const WCHAR *script, const WCHAR *argument)
{
    static const WCHAR prefix[] = L"/usr/bin/env -u WINEDEBUG -u WINEDLLOVERRIDES /bin/sh -c \"";
    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION process;
    size_t length = wcslen(prefix) + wcslen(script) + (argument ? wcslen(argument) * 2 + 8 : 0) + 4;
    WCHAR *line = malloc(length * sizeof(WCHAR)), *out;
    BOOL ok;
    if (!line) return FALSE;
    out = line + swprintf(line, length, L"%ls%ls\"", prefix, script);
    if (argument)
    {
        /* One more argument for the script's $1, quoted as a Windows command line. */
        *out++ = ' ';
        *out++ = '"';
        for (const WCHAR *c = argument; *c; c++)
        {
            if (*c == '"') *out++ = '\\';
            *out++ = *c;
        }
        *out++ = '"';
        *out = 0;
    }
    ok = CreateProcessW(L"Z:\\usr\\bin\\env", line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                        &startup, &process);
    free(line);
    if (!ok) return FALSE;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return TRUE;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, WCHAR *command_line, int show)
{
    int count;
    WCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &count);
    const WCHAR *url = NULL;
    (void)instance; (void)previous; (void)command_line; (void)show;

    for (int i = 1; argv && i < count; i++)
        if (argv[i][0] != '-' && argv[i][0] != '/') url = argv[i];
    if (url && *url)
    {
        /* Windows paths (local files) become file URLs Linux can open. */
        if (url[1] == ':' && (url[2] == '\\' || url[2] == '/'))
            run(L"exec xdg-open \\\"$(winepath -u \\\"$0\\\" 2>/dev/null || echo \\\"$0\\\")\\\"", url);
        else
            run(L"exec xdg-open \\\"$0\\\"", url);
    }
    else
        run(L"b=$(xdg-settings get default-web-browser); exec gtk-launch \\\"${b%.desktop}\\\"", NULL);
    LocalFree(argv);
    return 0;
}
