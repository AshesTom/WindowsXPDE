/* DefWindowProc keyboard-cue UI state: query, inheritance, change propagation.
 * Usage: ui-state [COM1]. Also run on the reference XP machine. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int updates;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_UPDATEUISTATE) updates++;
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static HANDLE out = INVALID_HANDLE_VALUE;

static int report(const char *format, ...)
{
    char line[256];
    DWORD written;
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (out == INVALID_HANDLE_VALUE) fputs(line, stdout);
    else WriteFile(out, line, (DWORD)strlen(line), &written, NULL);
    return 0;
}

static int scenario(const WNDCLASSW *wc, BOOL cues)
{
    HWND top, child, grandchild;
    UINT hidden = cues ? 0 : UISF_HIDEFOCUS | UISF_HIDEACCEL, state;
    /* Measured on XP: windows start with every cue shown. */
    int failures = 0;

    SystemParametersInfoW(SPI_SETKEYBOARDCUES, 0, (void *)(INT_PTR)cues, 0);
    top = CreateWindowW(wc->lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc->hInstance, NULL);
    child = CreateWindowW(wc->lpszClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, 50, 50, top, NULL, wc->hInstance, NULL);
    grandchild = CreateWindowW(L"Button", L"&OK", WS_CHILD | WS_VISIBLE, 0, 0, 40, 20, child, NULL, wc->hInstance, NULL);
    if (!top || !child || !grandchild) return 100;

    state = (UINT)SendMessageW(grandchild, WM_QUERYUISTATE, 0, 0);
    report("UISTATE cues=%d initial=%u\r\n", cues, state);
    failures += state != 0;

    updates = 0;
    SendMessageW(grandchild, WM_CHANGEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEACCEL), 0);
    state = (UINT)SendMessageW(grandchild, WM_QUERYUISTATE, 0, 0);
    report("clear-accel top=%u child=%u grandchild=%u updates=%d\r\n",
           (UINT)SendMessageW(top, WM_QUERYUISTATE, 0, 0), (UINT)SendMessageW(child, WM_QUERYUISTATE, 0, 0),
           state, updates);
    failures += state != 0 || updates != 0;

    updates = 0;
    SendMessageW(top, WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEACCEL | UISF_HIDEFOCUS), 0);
    state = (UINT)SendMessageW(grandchild, WM_QUERYUISTATE, 0, 0);
    report("set-both grandchild=%u updates=%d\r\n", state, updates);
    failures += state != hidden || updates != (cues ? 0 : 2);

    SendMessageW(child, WM_CHANGEUISTATE, MAKEWPARAM(7, UISF_HIDEFOCUS), 0);
    failures += (UINT)SendMessageW(grandchild, WM_QUERYUISTATE, 0, 0) != hidden;
    DestroyWindow(top);
    return failures;
}

int wmain(int argc, WCHAR **argv)
{
    WNDCLASSW wc = {0};
    BOOL cues = FALSE;
    int failures;

    if (argc > 1)
    {
        out = CreateFileW(argv[1], GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (out != INVALID_HANDLE_VALUE)
        {
            DCB dcb = {.DCBlength = sizeof(dcb)};
            GetCommState(out, &dcb);
            dcb.BaudRate = CBR_115200;
            dcb.ByteSize = 8;
            dcb.Parity = NOPARITY;
            dcb.StopBits = ONESTOPBIT;
            SetCommState(out, &dcb);
        }
    }
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"UiStateProbe";
    if (!RegisterClassW(&wc)) return 2;
    /* Temporarily change only the in-memory preference; restore it below. */
    SystemParametersInfoW(SPI_GETKEYBOARDCUES, 0, &cues, 0);
    failures = scenario(&wc, TRUE) + scenario(&wc, FALSE);
    SystemParametersInfoW(SPI_SETKEYBOARDCUES, 0, (void *)(INT_PTR)cues, 0);
    report(failures ? "FAIL\r\n" : "PASS\r\n");
    return failures != 0;
}
