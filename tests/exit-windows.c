/* exit-windows-test.exe logoff|shutdown|reboot: call ExitWindowsEx as a shell's
 * dialog does (in the session, Wine patch 0024 records the request). */
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#include <windows.h>
#include <wchar.h>
int wmain(int argc, WCHAR **argv)
{
    UINT flags;
    if (argc != 2) return 2;
    if (!wcscmp(argv[1], L"logoff")) flags = EWX_LOGOFF;
    else if (!wcscmp(argv[1], L"shutdown")) flags = EWX_SHUTDOWN;
    else if (!wcscmp(argv[1], L"reboot")) flags = EWX_REBOOT;
    else return 2;
    return ExitWindowsEx(flags, 0) ? 0 : 1;
}
