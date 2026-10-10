/* Add a notification icon with the public Shell_NotifyIconW, check that the
 * shell's notification toolbar gains a button, and that the button of a
 * fully transparent icon shows the notification area's themed background.
 * Usage: notify-icon [OUTPUT] (XP: A:\NI.EXE COM1). The icon's pixels are
 * made here; nothing is read from the shell's files. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>

static void pump(DWORD milliseconds)
{
    DWORD end = GetTickCount() + milliseconds;
    MSG msg;
    while ((LONG)(end - GetTickCount()) > 0)
    {
        MsgWaitForMultipleObjects(0, NULL, FALSE, 50, QS_ALLINPUT);
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    }
}

static HWND notify_toolbar(HWND *notify)
{
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL), pager;
    *notify = tray ? FindWindowExW(tray, NULL, L"TrayNotifyWnd", NULL) : NULL;
    pager = *notify ? FindWindowExW(*notify, NULL, L"SysPager", NULL) : NULL;
    return pager ? FindWindowExW(pager, NULL, L"ToolbarWindow32", NULL) : NULL;
}

static HICON transparent_icon(void)
{
    BITMAPINFO info = {0};
    BYTE mask[16 * 4];
    ICONINFO icon = {0};
    void *bits;
    HICON result;
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = 16;
    info.bmiHeader.biHeight = -16;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    icon.fIcon = TRUE;
    icon.hbmColor = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    memset(bits, 0, 16 * 16 * 4);
    memset(mask, 0xff, sizeof(mask));
    icon.hbmMask = CreateBitmap(16, 16, 1, 1, mask);
    result = CreateIconIndirect(&icon);
    DeleteObject(icon.hbmColor);
    DeleteObject(icon.hbmMask);
    return result;
}

/* Whether some button of the toolbar shows exactly the expected background. */
static int background_button(HWND own, HWND notify, HWND toolbar, LRESULT count)
{
    RECT client, item;
    HTHEME theme;
    HDC screen, actual, expected;
    HBITMAP a, e, old_a, old_e;
    DWORD pid;
    HANDLE process;
    void *remote;
    POINT origin = {0, 0};
    int found = 0, x, y;
    LRESULT i;

    if (!IsThemeActive())
    {
        puts("classic: background not compared");
        return 1;
    }
    GetClientRect(notify, &client);
    /* The shell selects TrayNotifyHoriz or TrayNotifyVert for its window;
     * another process opens the same through a window of its own. */
    SetWindowTheme(own, client.right >= client.bottom ? L"TrayNotifyHoriz" : L"TrayNotifyVert", NULL);
    theme = OpenThemeData(own, L"TrayNotify");
    if (!theme)
    {
        puts("no TrayNotify theme data");
        return 0;
    }
    ClientToScreen(notify, &origin);
    screen = GetDC(NULL);
    actual = CreateCompatibleDC(screen);
    expected = CreateCompatibleDC(screen);
    a = CreateCompatibleBitmap(screen, client.right, client.bottom);
    e = CreateCompatibleBitmap(screen, client.right, client.bottom);
    old_a = SelectObject(actual, a);
    old_e = SelectObject(expected, e);
    BitBlt(actual, 0, 0, client.right, client.bottom, screen, origin.x, origin.y, SRCCOPY);
    ReleaseDC(NULL, screen);
    DrawThemeBackground(theme, expected, TNP_BACKGROUND, 0, &client, NULL);
    CloseThemeData(theme);

    /* TB_GETITEMRECT writes into the shell's own address space. */
    GetWindowThreadProcessId(toolbar, &pid);
    process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, pid);
    remote = process ? VirtualAllocEx(process, NULL, sizeof(RECT), MEM_COMMIT, PAGE_READWRITE) : NULL;
    if (!remote) printf("no memory in the shell's process: %lu\n", GetLastError());
    for (i = 0; remote && i < count; ++i)
    {
        int mismatches = 0;
        SIZE_T read = 0;
        if (!SendMessageW(toolbar, TB_GETITEMRECT, i, (LPARAM)remote) ||
            !ReadProcessMemory(process, remote, &item, sizeof(item), &read) || read != sizeof(item))
        {
            printf("button %ld: no rectangle\n", (long)i);
            continue;
        }
        MapWindowPoints(toolbar, notify, (POINT *)&item, 2);
        for (y = item.top; y < item.bottom; ++y)
            for (x = item.left; x < item.right; ++x)
                if (GetPixel(actual, x, y) != GetPixel(expected, x, y)) ++mismatches;
        printf("button %ld (%ld,%ld)-(%ld,%ld) mismatches=%d first=%06lx expected=%06lx\n", (long)i,
               item.left, item.top, item.right, item.bottom, mismatches,
               (unsigned long)GetPixel(actual, item.left + 1, item.top + 1),
               (unsigned long)GetPixel(expected, item.left + 1, item.top + 1));
        if (!mismatches && item.right > item.left && item.bottom > item.top) found = 1;
    }
    if (remote) VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    if (process) CloseHandle(process);
    SelectObject(actual, old_a);
    SelectObject(expected, old_e);
    DeleteObject(a);
    DeleteObject(e);
    DeleteDC(actual);
    DeleteDC(expected);
    return found;
}

int main(int argc, char **argv)
{
    WNDCLASSW cls = {0};
    NOTIFYICONDATAW data = {0};
    HWND notify, toolbar, window;
    LRESULT before, after, final;
    BOOL added, deleted;
    int background;

    if (argc > 1 && !freopen(argv[1], "w", stdout)) return 1;
    Sleep(1000);
    if (!(toolbar = notify_toolbar(&notify)))
    {
        puts("FAIL: no notification toolbar");
        return 2;
    }
    cls.lpfnWndProc = DefWindowProcW;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"NotifyIconTest";
    RegisterClassW(&cls);
    window = CreateWindowW(cls.lpszClassName, L"", WS_POPUP, 0, 0, 0, 0, NULL, NULL, cls.hInstance, NULL);

    before = SendMessageW(toolbar, TB_BUTTONCOUNT, 0, 0);
    data.cbSize = NOTIFYICONDATAW_V2_SIZE;
    data.hWnd = window;
    data.uID = 7;
    data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    data.uCallbackMessage = WM_APP + 7;
    data.hIcon = transparent_icon();
    lstrcpyW(data.szTip, L"notify-icon test");
    added = Shell_NotifyIconW(NIM_ADD, &data);
    pump(1500);
    after = SendMessageW(toolbar, TB_BUTTONCOUNT, 0, 0);
    background = background_button(window, notify, toolbar, after);
    deleted = Shell_NotifyIconW(NIM_DELETE, &data);
    pump(1000);
    final = SendMessageW(toolbar, TB_BUTTONCOUNT, 0, 0);
    printf("add=%d buttons %ld -> %ld, delete=%d -> %ld, background=%d\n", added, (long)before,
           (long)after, deleted, (long)final, background);
    DestroyIcon(data.hIcon);
    DestroyWindow(window);
    if (added && deleted && after == before + 1 && final == before && background)
    {
        puts("PASS: notification icon added, shown on the notification background and removed");
        return 0;
    }
    puts("FAIL: notification icon");
    return 1;
}
