/* msgbox-layout: message box layout through public APIs.
 *   msgbox-layout-test OUT
 * Shows several message boxes; a timer in each one's modal loop writes the box's
 * window and client rectangles and its children (class, id, rectangle in client
 * coordinates, text) and ends it. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static FILE *out;
static int current;

static BOOL CALLBACK child(HWND hwnd, LPARAM lp)
{
    HWND box = (HWND)lp;
    WCHAR cls[64], text[256];
    char ucls[128], utext[768];
    RECT rc;
    GetClassNameW(hwnd, cls, 64);
    GetWindowTextW(hwnd, text, 256);
    GetWindowRect(hwnd, &rc);
    MapWindowPoints(NULL, box, (POINT *)&rc, 2);
    WideCharToMultiByte(CP_UTF8, 0, cls, -1, ucls, sizeof(ucls), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, text, -1, utext, sizeof(utext), NULL, NULL);
    fprintf(out, "  %s id %d rect %ld,%ld,%ld,%ld style %08lx '%s'\n", ucls, GetDlgCtrlID(hwnd), rc.left, rc.top,
            rc.right, rc.bottom, GetWindowLongW(hwnd, GWL_STYLE), utext);
    return TRUE;
}

static BOOL CALLBACK find_box(HWND hwnd, LPARAM lp)
{
    WCHAR cls[16];
    if (GetClassNameW(hwnd, cls, 16) && !wcscmp(cls, L"#32770")) *(HWND *)lp = hwnd;
    return TRUE;
}

static void CALLBACK timer(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
    HWND box = NULL;
    (void)hwnd; (void)msg; (void)time;
    EnumThreadWindows(GetCurrentThreadId(), find_box, (LPARAM)&box);
    RECT rc, client;
    POINT origin = {0, 0};
    KillTimer(NULL, id);
    GetWindowRect(box, &rc);
    GetClientRect(box, &client);
    ClientToScreen(box, &origin);
    fprintf(out, "box %d window %ld,%ld,%ld,%ld client %ldx%ld at %ld,%ld screen %dx%d\n", current, rc.left, rc.top,
            rc.right, rc.bottom, client.right, client.bottom, origin.x, origin.y,
            GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    EnumChildWindows(box, child, (LPARAM)box);
    fflush(out);
    PostMessageW(box, WM_COMMAND, IDCANCEL, 0);
    PostMessageW(box, WM_COMMAND, IDOK, 0);
}

static void show(const WCHAR *text, const WCHAR *title, UINT type)
{
    SetTimer(NULL, 0, 800, timer);
    MessageBoxW(NULL, text, title, type);
    current++;
}

int wmain(int argc, WCHAR **argv)
{
    out = argc > 1 ? _wfopen(argv[1], L"w") : stdout;
    show(L"メッセージの本文です。", L"確認", MB_OKCANCEL | MB_ICONINFORMATION);
    show(L"a", L"t", MB_OK);
    show(L"a", L"t", MB_OK | MB_ICONERROR);
    show(L"二行目のある\n本文です。", L"確認", MB_YESNOCANCEL | MB_ICONQUESTION);
    show(L"本文", L"確認", MB_ABORTRETRYIGNORE | MB_ICONWARNING);
    show(L"本文", L"確認", MB_CANCELTRYCONTINUE | MB_HELP);
    show(L"本文", L"確認", MB_OKCANCEL | MB_HELP | MB_ICONINFORMATION);
    show(L"abcdefghij", L"t", MB_OK);
    show(L"abcdefghijabcdefghij", L"t", MB_OK);
    show(L"abcdefghijabcdefghijabcdefghij", L"t", MB_OK);
    show(L"abcdefghijabcdefghijabcdefghijabcdefghij", L"t", MB_OK | MB_ICONINFORMATION);
    show(L"abcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghij", L"t", MB_OKCANCEL);
    show(L"a\nb\nc", L"t", MB_OK);
    show(L"a\nb\nc", L"t", MB_OK | MB_ICONINFORMATION);
    {
        static WCHAR text[4096];
        int n;
        for (n = 0; n < 4; n++)
        {
            int k, len = (n + 1) * 150;
            for (k = 0; k < len; k++) text[k] = (k % 7 == 6) ? ' ' : 'a' + k % 26;
            text[len] = 0;
            show(text, L"t", MB_OK);
        }
        for (n = 0; n < 2; n++)
        {
            int k, len = (n + 1) * 80;
            for (k = 0; k < len; k++) text[k] = 0x3042 + k % 40;
            text[len] = 0;
            show(text, L"t", MB_OK | MB_ICONINFORMATION);
        }
    }
    fclose(out);
    return 0;
}
