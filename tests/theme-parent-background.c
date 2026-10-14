/* Where DrawThemeParentBackground puts the parent's painting for a child.
 * Usage: theme-parent-background [COM1|FILE]
 * A parent window records the DC's viewport origin and clip box that its
 * WM_ERASEBKGND and WM_PRINTCLIENT receive when its child at (30,20) calls
 * DrawThemeParentBackground with no rectangle and with (40,5)-(60,15). On
 * XP the origin is the child's position whatever the rectangle, which only
 * clips. Prints the results; changes nothing. */
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>
#include <stdlib.h>

static HANDLE out;
static void say(const char *format, ...)
{
    char line[512];
    DWORD written;
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (out != INVALID_HANDLE_VALUE) WriteFile(out, line, (DWORD)strlen(line), &written, NULL);
    else fputs(line, stdout);
}

static LRESULT CALLBACK parent_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ERASEBKGND || msg == WM_PRINTCLIENT)
    {
        POINT origin;
        RECT clip;
        int kind;
        GetViewportOrgEx((HDC)wp, &origin);
        kind = GetClipBox((HDC)wp, &clip);
        say("  %s: viewport origin %ld,%ld clip box %ld,%ld-%ld,%ld (%d)\n",
            msg == WM_ERASEBKGND ? "WM_ERASEBKGND" : "WM_PRINTCLIENT",
            origin.x, origin.y, clip.left, clip.top, clip.right, clip.bottom, kind);
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int wmain(int argc, WCHAR **argv)
{
    WNDCLASSW cls = {0};
    HWND parent, child;
    HDC dc;
    RECT part = {40, 5, 60, 15};

    out = INVALID_HANDLE_VALUE;
    if (argc > 1)
        out = CreateFileW(!lstrcmpiW(argv[1], L"COM1") ? L"\\\\.\\COM1" : argv[1], GENERIC_WRITE, 0, NULL,
                          !lstrcmpiW(argv[1], L"COM1") ? OPEN_EXISTING : CREATE_ALWAYS, 0, NULL);
    cls.lpfnWndProc = parent_proc;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"ThemeParentProbe";
    RegisterClassW(&cls);
    parent = CreateWindowW(L"ThemeParentProbe", L"probe", WS_OVERLAPPEDWINDOW, 100, 100, 300, 200,
                           NULL, NULL, cls.hInstance, NULL);
    child = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 30, 20, 100, 50, parent, NULL, cls.hInstance, NULL);
    dc = GetDC(child);
    say("theme-parent-background: child at 30,20\n");
    say(" no rectangle:\n");
    DrawThemeParentBackground(child, dc, NULL);
    say(" rectangle 40,5-60,15:\n");
    DrawThemeParentBackground(child, dc, &part);
    ReleaseDC(child, dc);
    DestroyWindow(parent);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
