/* Wine's move loop reports drags for window snapping (patch 0032): EVENT_SYSTEM_MOVESIZESTART
 * and EVENT_SYSTEM_MOVESIZEEND, "Win4WineSizeMove" (the hit test while the loop runs, then
 * with 0x1000 ended, 0x100 moved), and a window with "Win4WineSnapRestore" gets that size
 * back on the first mouse move, the pointer staying on its caption. With
 * WIN4WINE_WINDOW_SNAP=1 a maximized window dragged by its caption is restored
 * and moves on in its restored size; without it, it stays maximized.
 * Usage: size-move-snap-test. Run with WIN4WINE_EXTERNAL_SHELL, Luna or classic,
 * with and without WIN4WINE_WINDOW_SNAP=1. */
#define UNICODE
#include <windows.h>
#include <stdio.h>

static HWND window;
static UINT_PTR start_state, end_state;
static int starts, ends, step;
static RECT moved_rect;
static POINT moved_cursor;

static void CALLBACK on_event(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG object, LONG child,
                              DWORD thread, DWORD time)
{
    (void)hook; (void)object; (void)child; (void)thread; (void)time;
    if (hwnd != window) return;
    if (event == EVENT_SYSTEM_MOVESIZESTART) { starts++; start_state = (UINT_PTR)GetPropW(hwnd, L"Win4WineSizeMove"); }
    if (event == EVENT_SYSTEM_MOVESIZEEND) { ends++; end_state = (UINT_PTR)GetPropW(hwnd, L"Win4WineSizeMove"); }
}

static void mouse(DWORD flags, int dx, int dy)
{
    INPUT input = {.type = INPUT_MOUSE};
    input.mi.dwFlags = flags;
    input.mi.dx = dx;
    input.mi.dy = dy;
    SendInput(1, &input, sizeof(input));
}

/* Runs in the move loop: move the pointer twice, then release the button. */
static void CALLBACK drive(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
    (void)hwnd; (void)msg; (void)time;
    switch (step++)
    {
    case 0: mouse(MOUSEEVENTF_MOVE, 30, 20); break;
    case 1: mouse(MOUSEEVENTF_MOVE, 30, 20); break;
    case 2:
        GetWindowRect(window, &moved_rect);
        GetCursorPos(&moved_cursor);
        mouse(MOUSEEVENTF_LEFTUP, 0, 0);
        break;
    default: KillTimer(NULL, id);
    }
}

static void pump(DWORD ms)
{
    MSG msg;
    DWORD end = GetTickCount() + ms;
    while (GetTickCount() < end)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(10);
    }
}

/* Presses the caption 120 pixels from the window's left; with drag, the timer moves and releases. */
static void press_caption(BOOL drag)
{
    RECT rect;
    GetWindowRect(window, &rect);
    SetCursorPos(rect.left + 120, rect.top + GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION) / 2);
    pump(200);
    step = 0;
    if (drag) SetTimer(NULL, 0, 100, drive);
    mouse(MOUSEEVENTF_LEFTDOWN, 0, 0);
    if (!drag) mouse(MOUSEEVENTF_LEFTUP, 0, 0);
    pump(1500);
}

int main(void)
{
    RECT before, after;
    WCHAR value[8] = L"";
    BOOL window_snap;
    int failures = 0;
    WNDCLASSW cls = {.lpfnWndProc = DefWindowProcW, .lpszClassName = L"SizeMoveSnap"};
    cls.hCursor = LoadCursorW(NULL, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&cls);
    window = CreateWindowW(L"SizeMoveSnap", L"snap", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 400, 300,
                           NULL, NULL, NULL, NULL);
    SetForegroundWindow(window);
    pump(300);
    SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND, NULL, on_event, 0, 0, WINEVENT_OUTOFCONTEXT);

    /* A click on the caption: a loop that did not move. */
    GetWindowRect(window, &before);
    press_caption(FALSE);
    GetWindowRect(window, &after);
    if (starts != 1 || ends != 1) { printf("FAIL: click: %d start and %d end events, want 1 and 1\n", starts, ends); failures++; }
    else if (start_state != HTCAPTION || end_state != (HTCAPTION | 0x1000))
    { printf("FAIL: click: states %lx and %lx, want 2 and 1002\n", (unsigned long)start_state, (unsigned long)end_state); failures++; }
    if (!EqualRect(&before, &after)) { printf("FAIL: click moved the window\n"); failures++; }

    /* A drag of a snapped window: it gets its size back on the first move. */
    SetPropW(window, L"Win4WineSnapRestore", (HANDLE)(UINT_PTR)MAKELONG(200, 150));
    starts = ends = 0;
    press_caption(TRUE);
    GetWindowRect(window, &after);
    if (starts != 1 || ends != 1) { printf("FAIL: drag: %d start and %d end events, want 1 and 1\n", starts, ends); failures++; }
    else if (start_state != HTCAPTION || end_state != (HTCAPTION | 0x1000 | 0x100))
    { printf("FAIL: drag: states %lx and %lx, want 2 and 1102\n", (unsigned long)start_state, (unsigned long)end_state); failures++; }
    if (moved_rect.right - moved_rect.left != 200 || moved_rect.bottom - moved_rect.top != 150)
    { printf("FAIL: drag: size %ldx%ld, want 200x150\n", moved_rect.right - moved_rect.left, moved_rect.bottom - moved_rect.top); failures++; }
    if (!PtInRect(&moved_rect, moved_cursor) || moved_cursor.y - moved_rect.top > GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION))
    { printf("FAIL: drag: the pointer %ld,%ld left the caption of %ld,%ld-%ld,%ld\n", moved_cursor.x, moved_cursor.y,
             moved_rect.left, moved_rect.top, moved_rect.right, moved_rect.bottom); failures++; }
    if (GetPropW(window, L"Win4WineSnapRestore")) { printf("FAIL: drag: Win4WineSnapRestore stayed\n"); failures++; }
    if (!EqualRect(&moved_rect, &after)) { printf("FAIL: drag: the window changed after the release\n"); failures++; }

    /* A drag of a maximized window. */
    GetEnvironmentVariableW(L"WIN4WINE_WINDOW_SNAP", value, ARRAYSIZE(value));
    window_snap = !wcscmp(value, L"1");
    SetWindowPos(window, NULL, 150, 120, 320, 240, SWP_NOZORDER);
    ShowWindow(window, SW_MAXIMIZE);
    pump(300);
    GetWindowRect(window, &before);
    starts = ends = 0;
    press_caption(TRUE);
    GetWindowRect(window, &after);
    if (window_snap)
    {
        if (IsZoomed(window)) { printf("FAIL: maximized drag: the window stayed maximized\n"); failures++; }
        else if (moved_rect.right - moved_rect.left != 320 || moved_rect.bottom - moved_rect.top != 240)
        { printf("FAIL: maximized drag: size %ldx%ld, want 320x240\n", moved_rect.right - moved_rect.left,
                 moved_rect.bottom - moved_rect.top); failures++; }
        else if (!PtInRect(&moved_rect, moved_cursor) ||
                 moved_cursor.y - moved_rect.top > GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION))
        { printf("FAIL: maximized drag: the pointer %ld,%ld left the caption of %ld,%ld-%ld,%ld\n", moved_cursor.x,
                 moved_cursor.y, moved_rect.left, moved_rect.top, moved_rect.right, moved_rect.bottom); failures++; }
    }
    else if (!IsZoomed(window) || !EqualRect(&before, &after))
    { printf("FAIL: maximized drag without WIN4WINE_WINDOW_SNAP=1 changed the window\n"); failures++; }

    if (!failures)
        printf("PASS: events and states for a click (1002) and a drag (1102); the snapped window became %ldx%ld; "
               "a dragged maximized window %s\n", after.right - after.left, after.bottom - after.top,
               window_snap ? "was restored" : "stayed (no WIN4WINE_WINDOW_SNAP=1)");
    DestroyWindow(window);
    return failures != 0;
}
