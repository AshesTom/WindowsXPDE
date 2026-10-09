/* Win32 identities and genuine nonclient frames for native X11 clients.
 * Uses public Win32 APIs only; X11 clients remain root children, paired below
 * their frame by the WM. No Microsoft image/code is copied into this helper. */
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CLIENTS 2048
struct client { unsigned long xid; HWND hwnd; int seen; int hidden; int active; int x, y, width, height;
    HICON icon; int icon_checked, icon_present; FILETIME icon_time; DWORD icon_size;
};
static struct client clients[MAX_CLIENTS];
static HWND previous[MAX_CLIENTS];
static unsigned previous_count;
static WCHAR previous_titles[MAX_CLIENTS][512];
static WCHAR current_titles[MAX_CLIENTS][512];
static HWND current[MAX_CLIENTS];
static unsigned current_count;
static HWND last_foreground;
static WCHAR directory[MAX_PATH];
static UINT shell_message;
static HICON default_icon;
static int syncing;
static unsigned sequence;

static void write_command(const char *data)
{
    WCHAR temp[MAX_PATH + 64], target[MAX_PATH + 64];
    fprintf(stderr, "bridge command: %s", data);
    DWORD written;
    HANDLE file;
    swprintf(temp, MAX_PATH + 64, L"%ls\\%lu-%u.tmp", directory, GetCurrentProcessId(), sequence);
    swprintf(target, MAX_PATH + 64, L"%ls\\%lu-%u.cmd", directory, GetCurrentProcessId(), sequence++);
    file = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    WriteFile(file, data, (DWORD)strlen(data), &written, NULL);
    CloseHandle(file);
    if (!MoveFileExW(temp, target, MOVEFILE_REPLACE_EXISTING)) DeleteFileW(temp);
}

static void command(const char *op, unsigned long xid)
{
    char data[80];
    snprintf(data, sizeof(data), "%s %lx\n", op, xid);
    write_command(data);
}

static void geometry_command(HWND hwnd, unsigned long xid)
{
    RECT r;
    POINT pt = {0, 0};
    char data[160];
    GetClientRect(hwnd, &r);
    ClientToScreen(hwnd, &pt);
    snprintf(data, sizeof(data), "geometry %lx %ld %ld %ld %ld\n", xid,
             pt.x, pt.y, r.right, r.bottom);
    write_command(data);
}

/* Keep diagnostic readback usable with unpatched Wine10 too. This read-only
 * scalar query performs public icon/readback APIs in their owning process. */
static LRESULT icon_readback(HWND hwnd, WPARAM field)
{
    HICON icon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0);
    ICONINFO info;
    BITMAP bitmap;
    HDC dc;
    HBITMAP image, previous;
    RECT rect;
    DWORD hash = 2166136261u, first = 0, center = 0, last = 0;
    int x, y;
    if (field == 0) return (LRESULT)icon;
    if (field > 5) return 0;
    if (!icon || !GetIconInfo(icon, &info)) return 0;
    if (!GetObjectW(info.hbmColor ? info.hbmColor : info.hbmMask, sizeof(bitmap), &bitmap))
    {
        if (info.hbmColor) DeleteObject(info.hbmColor);
        DeleteObject(info.hbmMask);
        return 0;
    }
    rect.left = rect.top = 0;
    rect.right = bitmap.bmWidth;
    rect.bottom = bitmap.bmHeight / (info.hbmColor ? 1 : 2);
    if (rect.right <= 0 || rect.right > 256 || rect.bottom <= 0 || rect.bottom > 256)
    {
        if (info.hbmColor) DeleteObject(info.hbmColor);
        DeleteObject(info.hbmMask);
        return 0;
    }
    dc = CreateCompatibleDC(NULL);
    {
        HDC screen = GetDC(NULL);
        image = CreateCompatibleBitmap(screen, rect.right, rect.bottom);
        ReleaseDC(NULL, screen);
    }
    previous = SelectObject(dc, image);
    FillRect(dc, &rect, GetStockObject(WHITE_BRUSH));
    DrawIconEx(dc, 0, 0, icon, rect.right, rect.bottom, 0, NULL, DI_NORMAL);
    for (y = 0; y < rect.bottom; ++y) for (x = 0; x < rect.right; ++x)
    {
        COLORREF color = GetPixel(dc, x, y);
        DWORD rgb = GetRValue(color) << 16 | GetGValue(color) << 8 | GetBValue(color);
        hash = (hash ^ rgb) * 16777619u;
        if (!x && !y) first = rgb;
        if (x == rect.right / 2 && y == rect.bottom / 2) center = rgb;
        last = rgb;
    }
    SelectObject(dc, previous); DeleteObject(image); DeleteDC(dc);
    if (info.hbmColor) DeleteObject(info.hbmColor);
    DeleteObject(info.hbmMask);
    if (field == 1) return MAKELONG(rect.right, rect.bottom);
    if (field == 2) return hash;
    if (field == 3) return first;
    if (field == 4) return center;
    if (field == 5) return last;
    return 0;
}

static LRESULT CALLBACK proxy_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    unsigned long xid = (unsigned long)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_APP + 0x574 && lp == 0x57344931) return icon_readback(hwnd, wp);
    if (msg == WM_NCCREATE)
    {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lp;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    if (!syncing && xid)
    {
        if (msg == WM_CLOSE) { command("close", xid); return 0; }
        if (msg == WM_SYSCOMMAND)
        {
            if ((wp & 0xfff0) == SC_MINIMIZE) { command("minimize", xid); return 0; }
            if ((wp & 0xfff0) == SC_MAXIMIZE)
            { command(GetPropW(hwnd, L"Win4WineMaximized") ? "restore" : "maximize", xid); return 0; }
            if ((wp & 0xfff0) == SC_RESTORE)
            { command(IsIconic(hwnd) ? "activate" : "restore", xid); return 0; }
        }
        if (msg == WM_WINDOWPOSCHANGED && !IsIconic(hwnd)) geometry_command(hwnd, xid);
        if (msg == WM_ACTIVATE && LOWORD(wp) != WA_INACTIVE && !IsIconic(hwnd)) command("activate", xid);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

struct notification { WPARAM event; LPARAM window; };
static BOOL CALLBACK deliver(HWND hwnd, LPARAM parameter)
{
    struct notification *n = (struct notification *)parameter;
    if (GetPropW(hwnd, L"Win4WineShellHook")) PostMessageW(hwnd, shell_message, n->event, n->window);
    return TRUE;
}

static BOOL CALLBACK deliver_tree(HWND hwnd, LPARAM parameter)
{
    deliver(hwnd, parameter);
    /* XP registers its child task-switcher window, not the taskbar parent. */
    EnumChildWindows(hwnd, deliver, parameter);
    return TRUE;
}

static void notify(UINT event, HWND window)
{
    struct notification n = { event, (LPARAM)window };
    EnumWindows(deliver_tree, (LPARAM)&n);
}

static BOOL CALLBACK collect(HWND hwnd, LPARAM unused)
{
    WCHAR name[80];
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    (void)unused;
    if (!IsWindowVisible(hwnd) || ((ex & WS_EX_TOOLWINDOW) && !(ex & WS_EX_APPWINDOW))) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) && !(ex & WS_EX_APPWINDOW)) return TRUE;
    GetClassNameW(hwnd, name, 80);
    if (!wcsicmp(name, L"Progman") || !wcsicmp(name, L"Shell_TrayWnd") || !wcsicmp(name, L"WorkerW") || !wcsicmp(name, L"DV2ControlHost")) return TRUE;
    if (current_count < MAX_CLIENTS)
    {
        current[current_count] = hwnd;
        GetWindowTextW(hwnd, current_titles[current_count], 512);
        current_count++;
    }
    return TRUE;
}

static int contains(HWND *list, unsigned count, HWND hwnd)
{
    unsigned i;
    for (i = 0; i < count; ++i) if (list[i] == hwnd) return 1;
    return 0;
}

static void shell_events(void)
{
    unsigned i;
    HWND foreground;
    current_count = 0;
    EnumWindows(collect, 0);
    for (i = 0; i < current_count; ++i)
    {
        unsigned j;
        for (j = 0; j < previous_count && previous[j] != current[i]; ++j) {}
        if (j == previous_count) notify(HSHELL_WINDOWCREATED, current[i]);
        else if (wcscmp(previous_titles[j], current_titles[i])) notify(HSHELL_REDRAW, current[i]);
    }
    for (i = 0; i < previous_count; ++i)
        if (!contains(current, current_count, previous[i])) notify(HSHELL_WINDOWDESTROYED, previous[i]);
    foreground = GetForegroundWindow();
    if (foreground)
    {
        WCHAR cls[80];
        GetClassNameW(foreground, cls, 80);
        /* XP's Start popup is not an application task. Excluding it only from
         * WINDOWCREATED is insufficient: WINDOWACTIVATED can create a task too. */
        if (!wcsicmp(cls, L"DV2ControlHost")) foreground = NULL;
    }
    if (foreground != last_foreground)
    {
        notify(HSHELL_WINDOWACTIVATED, foreground);
        last_foreground = foreground;
    }
    memcpy(previous, current, current_count * sizeof(HWND));
    memcpy(previous_titles, current_titles, current_count * sizeof(current_titles[0]));
    previous_count = current_count;
}

/* The WM exports bounded, little-endian ARGB pixels from public X11 icon
 * properties. These are client-owned images, never bundled theme assets. */
static HICON read_icon(const WCHAR *path, DWORD expected_size)
{
    HANDLE file;
    DWORD header[3], count, bytes, i, width, height, stride;
    BITMAPINFO color_info = {0};
    struct { BITMAPINFOHEADER header; RGBQUAD colors[2]; } mask_info = {0};
    HBITMAP color = NULL, mask = NULL;
    DWORD *pixels;
    BYTE *mask_bits;
    ICONINFO info = {0};
    HICON icon = NULL;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    if (!ReadFile(file, header, sizeof(header), &bytes, NULL) || bytes != sizeof(header) ||
        memcmp(header, "W4I1", 4)) goto done;
    width = header[1]; height = header[2];
    if (!width || !height || width > 256 || height > 256) goto done;
    count = width * height;
    if (expected_size != sizeof(header) + count * sizeof(DWORD)) goto done;
    color_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    color_info.bmiHeader.biWidth = width;
    color_info.bmiHeader.biHeight = -(LONG)height;
    color_info.bmiHeader.biPlanes = 1;
    color_info.bmiHeader.biBitCount = 32;
    color_info.bmiHeader.biCompression = BI_RGB;
    color = CreateDIBSection(NULL, &color_info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (!color || !ReadFile(file, pixels, count * sizeof(DWORD), &bytes, NULL) ||
        bytes != count * sizeof(DWORD)) goto done;
    mask_info.header = color_info.bmiHeader;
    mask_info.header.biBitCount = 1;
    mask_info.colors[1].rgbRed = mask_info.colors[1].rgbGreen = mask_info.colors[1].rgbBlue = 255;
    mask = CreateDIBSection(NULL, (BITMAPINFO *)&mask_info, DIB_RGB_COLORS, (void **)&mask_bits, NULL, 0);
    if (!mask) goto done;
    stride = (width + 31) / 32 * 4;
    memset(mask_bits, 0, stride * height);
    for (i = 0; i < count; ++i)
        if (!(pixels[i] >> 24)) mask_bits[(i / width) * stride + (i % width) / 8] |= 0x80 >> (i % width % 8);
    info.fIcon = TRUE; info.hbmColor = color; info.hbmMask = mask;
    icon = CreateIconIndirect(&info);
 done:
    if (color) DeleteObject(color);
    if (mask) DeleteObject(mask);
    CloseHandle(file);
    return icon;
}

static void client_icon(struct client *c)
{
    WCHAR path[MAX_PATH + 40];
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    HICON icon = NULL, previous;
    int present;
    swprintf(path, MAX_PATH + 40, L"%ls\\icon-%lx.argb", directory, c->xid);
    present = GetFileAttributesExW(path, GetFileExInfoStandard, &attrs) &&
              !(attrs.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
    if (c->icon_checked && present == c->icon_present &&
        (!present || (!CompareFileTime(&attrs.ftLastWriteTime, &c->icon_time) &&
                      attrs.nFileSizeLow == c->icon_size && !attrs.nFileSizeHigh))) return;
    c->icon_checked = 1; c->icon_present = present;
    if (present)
    {
        c->icon_time = attrs.ftLastWriteTime; c->icon_size = attrs.nFileSizeLow;
        if (!attrs.nFileSizeHigh && attrs.nFileSizeLow <= 12 + 256 * 256 * 4)
            icon = read_icon(path, attrs.nFileSizeLow);
    }
    previous = c->icon;
    c->icon = icon;
    SendMessageW(c->hwnd, WM_SETICON, ICON_SMALL, (LPARAM)(icon ? icon : default_icon));
    SendMessageW(c->hwnd, WM_SETICON, ICON_BIG, (LPARAM)(icon ? icon : default_icon));
    if (previous) DestroyIcon(previous); /* IDI_APPLICATION is shared and never owned. */
    RedrawWindow(c->hwnd, NULL, NULL, RDW_INVALIDATE | RDW_FRAME);
    notify(HSHELL_REDRAW, c->hwnd);
}

static void place_frame(struct client *c)
{
    RECT outer, inner;
    POINT origin = {0, 0};
    GetWindowRect(c->hwnd, &outer);
    GetClientRect(c->hwnd, &inner);
    ClientToScreen(c->hwnd, &origin);
    SetWindowPos(c->hwnd, NULL, c->x - (origin.x - outer.left),
                 c->y - (origin.y - outer.top),
                 c->width + (outer.right - outer.left - inner.right),
                 c->height + (outer.bottom - outer.top - inner.bottom),
                 SWP_NOACTIVATE | SWP_NOZORDER);
}

static void frame_geometry(void)
{
    WCHAR path[MAX_PATH + 32], target[MAX_PATH + 32];
    FILE *file;
    unsigned long xid;
    int x, y, width, height, maximized;
    unsigned i;
    swprintf(path, MAX_PATH + 32, L"%ls\\geometry.tsv", directory);
    file = _wfopen(path, L"rb");
    if (!file) return;
    while (fscanf(file, "%lx %d %d %d %d %d", &xid, &x, &y, &width, &height, &maximized) == 6)
        for (i = 0; i < MAX_CLIENTS; ++i)
            if (clients[i].xid == xid && !clients[i].hidden)
            {
                struct client *c = &clients[i];
                if (!!GetPropW(c->hwnd, L"Win4WineMaximized") != !!maximized)
                {
                    if (maximized) SetPropW(c->hwnd, L"Win4WineMaximized", (HANDLE)1);
                    else RemovePropW(c->hwnd, L"Win4WineMaximized");
                    RedrawWindow(c->hwnd, NULL, NULL, RDW_INVALIDATE | RDW_FRAME);
                }
                c->x = x; c->y = y; c->width = width; c->height = height;
                place_frame(c);
                break;
            }
    fclose(file);
    swprintf(path, MAX_PATH + 32, L"%ls\\frames.tmp", directory);
    swprintf(target, MAX_PATH + 32, L"%ls\\frames.tsv", directory);
    file = _wfopen(path, L"wb");
    if (!file) return;
    for (i = 0; i < MAX_CLIENTS; ++i)
        if (clients[i].xid)
        {
            RECT outer, inner;
            POINT origin = {0, 0};
            GetWindowRect(clients[i].hwnd, &outer);
            GetClientRect(clients[i].hwnd, &inner);
            ClientToScreen(clients[i].hwnd, &origin);
            fprintf(file, "%lx %lx %ld %ld %ld %ld %d %d\n", (unsigned long)(uintptr_t)clients[i].hwnd,
                    clients[i].xid, origin.x-outer.left, outer.right-origin.x-inner.right,
                    origin.y-outer.top, outer.bottom-origin.y-inner.bottom, !!GetPropW(clients[i].hwnd, L"Win4WineMaximized"), !!IsIconic(clients[i].hwnd));
        }
    fclose(file);
    MoveFileExW(path, target, MOVEFILE_REPLACE_EXISTING);
}

static void snapshot(void)
{
    WCHAR path[MAX_PATH + 32];
    char line[4096];
    FILE *file;
    unsigned i;
    swprintf(path, MAX_PATH + 32, L"%ls\\windows.tsv", directory);
    file = _wfopen(path, L"rb");
    if (!file) return; /* A missing snapshot is not an empty client list. */
    for (i = 0; i < MAX_CLIENTS; ++i) clients[i].seen = 0;
    syncing = 1;
    while (fgets(line, sizeof(line), file))
    {
        unsigned long xid;
        int hidden, active, offset = 0;
        WCHAR title[1024];
        struct client *c = NULL;
        if (sscanf(line, "%lx\t%d\t%d\t%n", &xid, &hidden, &active, &offset) != 3 || !offset) continue;
        line[strcspn(line, "\r\n")] = 0;
        if (!MultiByteToWideChar(CP_UTF8, 0, line + offset, -1, title, 1024)) continue;
        for (i = 0; i < MAX_CLIENTS; ++i) if (clients[i].xid == xid) { c = &clients[i]; break; }
        if (!c) for (i = 0; i < MAX_CLIENTS; ++i) if (!clients[i].xid) { c = &clients[i]; break; }
        if (!c) continue;
        if (!c->xid)
        {
            c->xid = xid;
            c->hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"Win4WineProxy", title,
                WS_OVERLAPPEDWINDOW, -32000, -32000, 1, 1, NULL, NULL, GetModuleHandleW(NULL), (void *)(uintptr_t)xid);
            if (!c->hwnd) { c->xid = 0; continue; }
            ShowWindow(c->hwnd, SW_SHOWNOACTIVATE);
        }
        c->seen = 1;
        client_icon(c);
        WCHAR old[1024];
        GetWindowTextW(c->hwnd, old, 1024);
        if (wcscmp(old, title)) { SetWindowTextW(c->hwnd, title); notify(HSHELL_REDRAW, c->hwnd); }
        if (hidden == c->hidden && !!IsIconic(c->hwnd) != c->hidden)
        {
            command(IsIconic(c->hwnd) ? "minimize" : "activate", xid);
            continue; /* Wait for X11 to acknowledge a direct ShowWindow operation. */
        }
        c->hidden = hidden;
        if (hidden && !IsIconic(c->hwnd)) ShowWindow(c->hwnd, SW_SHOWMINNOACTIVE);
        else if (!hidden && IsIconic(c->hwnd)) ShowWindow(c->hwnd, SW_SHOWNOACTIVATE);
        if (active && !c->active)
        {
            SetForegroundWindow(c->hwnd);
        }
        c->active = active;
        if (active) notify(HSHELL_WINDOWACTIVATED, c->hwnd);
    }
    fclose(file);
    for (i = 0; i < MAX_CLIENTS; ++i)
        if (clients[i].xid && !clients[i].seen)
        {
            DestroyWindow(clients[i].hwnd);
            if (clients[i].icon) DestroyIcon(clients[i].icon);
            memset(&clients[i], 0, sizeof(clients[i]));
        }
    frame_geometry();
    syncing = 0;
}

int wmain(int argc, WCHAR **argv)
{
    WNDCLASSW cls = {0};
    MSG msg;
    ULONGLONG next = 0;
    if (argc != 2 || wcslen(argv[1]) >= MAX_PATH) return 2;
    wcscpy(directory, argv[1]);
    shell_message = RegisterWindowMessageW(L"SHELLHOOK");
    default_icon = LoadIconW(NULL, IDI_APPLICATION);
    cls.hIcon = default_icon;
    cls.lpfnWndProc = proxy_proc;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"Win4WineProxy";
    if (!RegisterClassW(&cls)) return 3;
    for (;;)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                unsigned i;
                for (i = 0; i < MAX_CLIENTS; ++i) if (clients[i].xid)
                {
                    DestroyWindow(clients[i].hwnd);
                    if (clients[i].icon) DestroyIcon(clients[i].icon);
                }
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (GetTickCount64() >= next)
        {
            snapshot();
            shell_events();
            next = GetTickCount64() + 200;
        }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);
    }
}
