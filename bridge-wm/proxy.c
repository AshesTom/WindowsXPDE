/* Win32 identities and genuine nonclient frames for native X11 clients.
 * Uses public Win32 APIs only; X11 clients remain root children, paired below
 * their frame by the WM. No Microsoft image/code is copied into this helper. */
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CLIENTS 2048
struct client { unsigned long xid; HWND hwnd; int seen; int hidden; int active; int x, y, width, height;
    RECT workarea;
    HICON icon; int icon_checked, icon_present; FILETIME icon_time; DWORD icon_size;
};
static struct client clients[MAX_CLIENTS];
/* WM_TIMER of a frame whose caption is painted again after an activation change. */
#define CAPTION_REPAINT_TIMER 0x574e
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
static BOOL dirty = TRUE;
static char frames_text[MAX_CLIENTS * 96];
static BOOL frames_written;

/* Commands for the WM end in .cmd, those for the tray host in .tray. */
static void write_file_command(const char *data, const WCHAR *suffix)
{
    WCHAR temp[MAX_PATH + 64], target[MAX_PATH + 64];
    fprintf(stderr, "bridge command: %s", data);
    DWORD written;
    HANDLE file;
    swprintf(temp, MAX_PATH + 64, L"%ls\\%lu-%u.tmp", directory, GetCurrentProcessId(), sequence);
    swprintf(target, MAX_PATH + 64, L"%ls\\%lu-%u.%ls", directory, GetCurrentProcessId(), sequence++, suffix);
    file = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    WriteFile(file, data, (DWORD)strlen(data), &written, NULL);
    CloseHandle(file);
    if (!MoveFileExW(temp, target, MOVEFILE_REPLACE_EXISTING)) DeleteFileW(temp);
}

static void write_command(const char *data)
{
    write_file_command(data, L"cmd");
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
    if (msg == WM_GETMINMAXINFO && xid)
    {
        unsigned i;
        for (i = 0; i < MAX_CLIENTS; ++i)
            if (clients[i].xid == xid && clients[i].workarea.right > clients[i].workarea.left)
            {
                MINMAXINFO *limits = (MINMAXINFO *)lp;
                WINDOWINFO info = {.cbSize = sizeof(info)};
                MONITORINFO monitor = {.cbSize = sizeof(monitor)};
                RECT area = clients[i].workarea;
                LRESULT result = DefWindowProcW(hwnd, msg, wp, lp);
                if (!GetWindowInfo(hwnd, &info) ||
                    !GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &monitor)) return result;
                limits->ptMaxPosition.x = area.left - monitor.rcMonitor.left - info.cxWindowBorders;
                limits->ptMaxPosition.y = area.top - monitor.rcMonitor.top - info.cyWindowBorders;
                limits->ptMaxSize.x = area.right - area.left + 2 * info.cxWindowBorders;
                limits->ptMaxSize.y = area.bottom - area.top + 2 * info.cyWindowBorders;
                limits->ptMaxTrackSize.x = max(limits->ptMaxTrackSize.x, limits->ptMaxSize.x);
                limits->ptMaxTrackSize.y = max(limits->ptMaxTrackSize.y, limits->ptMaxSize.y);
                return result;
            }
    }
    /* While its client has the X focus the frame stays active: a Win32
     * activation of another process that arrives after the WM's report (a
     * window the user had activated just before) deactivates this frame. */
    if (msg == WM_NCACTIVATE && !wp && xid)
    {
        unsigned i;
        for (i = 0; i < MAX_CLIENTS; ++i)
            if (clients[i].xid == xid && clients[i].active) return DefWindowProcW(hwnd, msg, TRUE, lp);
    }
    if (msg == WM_TIMER && wp == CAPTION_REPAINT_TIMER)
    {
        KillTimer(hwnd, CAPTION_REPAINT_TIMER);
        RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
        return 0;
    }
    /* Border metrics written to frames.tsv follow theme and settings changes. */
    if (msg == WM_THEMECHANGED || msg == WM_SETTINGCHANGE || msg == WM_DISPLAYCHANGE) dirty = TRUE;
    if (!syncing && xid)
    {
        if (msg == WM_WINDOWPOSCHANGED) dirty = TRUE;
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

/* The WM writes shellkeys.tsv when the Windows key or Ctrl+Esc is pressed
 * under a native client: "NUMBER windows VK..." with the keys pressed while
 * the Windows key was down, or "NUMBER ctrl-escape". Enter them into Wine as
 * the keyboard would; Wine's foreground then handles XP's hotkeys and the
 * Start menu keys as for its own windows. */
static void key_input(INPUT *input, WORD vk, BOOL up)
{
    memset(input, 0, sizeof(*input));
    input->type = INPUT_KEYBOARD;
    input->ki.wVk = vk;
    input->ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    input->ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    if (vk == VK_LWIN) input->ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
}

static void shell_key_events(BOOL start)
{
    static unsigned seen;
    WCHAR path[MAX_PATH + 32];
    char kind[16] = "";
    unsigned number = 0, vk, count = 0;
    INPUT inputs[2 + 2 * 16 + 2];
    FILE *file;
    swprintf(path, MAX_PATH + 32, L"%ls\\shellkeys.tsv", directory);
    if (!(file = _wfopen(path, L"rb"))) return;
    if (fscanf(file, "%u %15s", &number, kind) != 2) number = seen;
    if (start || number == seen)
    {
        seen = number;
        fclose(file);
        return;
    }
    seen = number;
    if (!strcmp(kind, "ctrl-escape"))
    {
        key_input(&inputs[count++], VK_CONTROL, FALSE);
        key_input(&inputs[count++], VK_ESCAPE, FALSE);
        key_input(&inputs[count++], VK_ESCAPE, TRUE);
        key_input(&inputs[count++], VK_CONTROL, TRUE);
    }
    else if (!strcmp(kind, "windows"))
    {
        key_input(&inputs[count++], VK_LWIN, FALSE);
        while (count < ARRAYSIZE(inputs) - 3 && fscanf(file, "%x", &vk) == 1 && vk && vk < 0xff)
        {
            key_input(&inputs[count++], (WORD)vk, FALSE);
            key_input(&inputs[count++], (WORD)vk, TRUE);
        }
        key_input(&inputs[count++], VK_LWIN, TRUE);
    }
    fclose(file);
    if (count) SendInput(count, inputs, sizeof(INPUT));
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
    /* A native client with X focus is the active task, whether or not its
     * proxy could take the Win32 foreground. */
    for (i = 0; i < MAX_CLIENTS; ++i)
        if (clients[i].xid && clients[i].active && contains(current, current_count, clients[i].hwnd))
            foreground = clients[i].hwnd;
    if (foreground && !contains(current, current_count, foreground))
    {
        /* An activation can create an XP task too. Only activate identities
         * included in this snapshot; owned dialogs highlight their app owner. */
        HWND owner = foreground;
        unsigned depth;
        for (depth = 0; owner && depth < MAX_CLIENTS && !contains(current, current_count, owner); ++depth)
            owner = GetWindow(owner, GW_OWNER);
        foreground = contains(current, current_count, owner) ? owner : NULL;
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
    RECT outer, inner, wanted;
    POINT origin = {0, 0};
    GetWindowRect(c->hwnd, &outer);
    GetClientRect(c->hwnd, &inner);
    ClientToScreen(c->hwnd, &origin);
    wanted.left = c->x - (origin.x - outer.left);
    wanted.top = c->y - (origin.y - outer.top);
    wanted.right = wanted.left + c->width + (outer.right - outer.left - inner.right);
    wanted.bottom = wanted.top + c->height + (outer.bottom - outer.top - inner.bottom);
    if (EqualRect(&wanted, &outer)) return;
    SetWindowPos(c->hwnd, NULL, wanted.left, wanted.top, wanted.right - wanted.left,
                 wanted.bottom - wanted.top, SWP_NOACTIVATE | SWP_NOZORDER);
}

static void frame_geometry(void)
{
    WCHAR path[MAX_PATH + 32], target[MAX_PATH + 32];
    FILE *file;
    unsigned long xid;
    long long x, y, width, height, maximized, ax, ay, aw, ah;
    char line[240];
    unsigned i;
    swprintf(path, MAX_PATH + 32, L"%ls\\geometry.tsv", directory);
    file = _wfopen(path, L"rb");
    if (!file) return;
    while (fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "%lx %lld %lld %lld %lld %lld %lld %lld %lld %lld", &xid, &x, &y, &width, &height,
                   &maximized, &ax, &ay, &aw, &ah) != 10 || width <= 0 || height <= 0 ||
            width > 65535 || height > 65535 || aw <= 0 || ah <= 0 || aw > 65535 || ah > 65535 ||
            x < -65535 || x > 65535 || y < -65535 || y > 65535 ||
            ax < -65535 || ax > 65535 || ay < -65535 || ay > 65535 ||
            (maximized != 0 && maximized != 1)) continue;
        for (i = 0; i < MAX_CLIENTS; ++i)
            if (clients[i].xid == xid && !clients[i].hidden)
            {
                struct client *c = &clients[i];
                SetRect(&c->workarea, ax, ay, ax + aw, ay + ah);
                if (!!GetPropW(c->hwnd, L"Win4WineMaximized") != !!maximized)
                {
                    if (maximized) SetPropW(c->hwnd, L"Win4WineMaximized", (HANDLE)1);
                    else RemovePropW(c->hwnd, L"Win4WineMaximized");
                    RedrawWindow(c->hwnd, NULL, NULL, RDW_INVALIDATE | RDW_FRAME);
                }
                if (!!IsZoomed(c->hwnd) != !!maximized)
                    ShowWindow(c->hwnd, maximized ? SW_MAXIMIZE : SW_RESTORE);
                c->x = x; c->y = y; c->width = width; c->height = height;
                place_frame(c);
                break;
            }
    }
    fclose(file);
    {
        static char text[sizeof(frames_text)];
        size_t length = 0;
        text[0] = 0;
        for (i = 0; i < MAX_CLIENTS; ++i)
            if (clients[i].xid && length < sizeof(text))
            {
                RECT outer, inner;
                WINDOWINFO info = {.cbSize = sizeof(info)};
                POINT origin = {0, 0};
                GetWindowRect(clients[i].hwnd, &outer);
                GetClientRect(clients[i].hwnd, &inner);
                ClientToScreen(clients[i].hwnd, &origin);
                GetWindowInfo(clients[i].hwnd, &info);
                length += snprintf(text + length, sizeof(text) - length, "%lx %lx %ld %ld %ld %ld %d %d %u %u\n",
                        (unsigned long)(uintptr_t)clients[i].hwnd,
                        clients[i].xid, origin.x-outer.left, outer.right-origin.x-inner.right,
                        origin.y-outer.top, outer.bottom-origin.y-inner.bottom, !!GetPropW(clients[i].hwnd, L"Win4WineMaximized"), !!IsIconic(clients[i].hwnd), info.cxWindowBorders, info.cyWindowBorders);
            }
        /* Rewriting unchanged pairs would wake the WM for nothing. */
        if (frames_written && !strcmp(text, frames_text)) return;
        swprintf(path, MAX_PATH + 32, L"%ls\\frames.tmp", directory);
        swprintf(target, MAX_PATH + 32, L"%ls\\frames.tsv", directory);
        if (!(file = _wfopen(path, L"wb"))) return;
        fputs(text, file);
        fclose(file);
        if (MoveFileExW(path, target, MOVEFILE_REPLACE_EXISTING))
        {
            strcpy(frames_text, text);
            frames_written = TRUE;
        }
    }
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
        if (active != c->active)
        {
            /* The frame looks active while its client has the X focus. Win32
             * activation cannot always follow: like Windows, Wine refuses
             * SetForegroundWindow to a process that did not get the last
             * input, and the input goes to the native client, not to this
             * process. So the frame's caption is set directly, both ways;
             * it would otherwise keep the look of its last real activation. */
            c->active = active; /* before WM_NCACTIVATE, which proxy_proc checks */
            if (active) SetForegroundWindow(c->hwnd);
            SendMessageW(c->hwnd, WM_NCACTIVATE, active, 0);
            /* Activations of other processes' windows around this one
             * sometimes leave its caption painted in the previous state,
             * though WM_NCACTIVATE's state is right (seen in the classic
             * theme; a repaint shows it). Paint the caption again once
             * they are over. */
            SetTimer(c->hwnd, CAPTION_REPAINT_TIMER, 250, NULL);
        }
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

static HWND previous_tray;
static DWORD previous_pid;
static BOOL attempted;

/* shell_layout runs on a 200 ms timer only while a visible taskbar awaits it. */
static BOOL shell_layout_pending(void)
{
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL);
    DWORD pid = 0;
    if (!tray || !IsWindowVisible(tray)) return FALSE;
    GetWindowThreadProcessId(tray, &pid);
    return !attempted || tray != previous_tray || pid != previous_pid;
}

/* Explorer's initial WM_SIZE can run before its child layout is ready.
 * Synchronize once after the mapped taskbar and its controls settle. The shell
 * decides all child sizes; this message carries only its current client area. */
static void shell_layout(void)
{
    static HWND previous_start, previous_rebar;
    static RECT previous_rect, previous_start_rect, previous_rebar_rect;
    static BOOL stable;
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL), start, rebar;
    DWORD pid = 0;
    RECT rect, start_rect, rebar_rect;
    DWORD_PTR result;
    if (tray) GetWindowThreadProcessId(tray, &pid);
    if (tray != previous_tray || pid != previous_pid)
    {
        previous_tray = tray;
        previous_pid = pid;
        previous_start = previous_rebar = NULL;
        stable = attempted = FALSE;
    }
    if (!tray || attempted) return;
    /* A temporarily hidden taskbar keeps its completed startup state. */
    if (!IsWindowVisible(tray)) { stable = FALSE; return; }
    start = FindWindowExW(tray, NULL, L"Win4WineStartButton", NULL);
    if (!start) start = FindWindowExW(tray, NULL, L"Button", NULL);
    rebar = FindWindowExW(tray, NULL, L"ReBarWindow32", NULL);
    if (!start || !rebar || !IsWindowVisible(start) || !IsWindowVisible(rebar) ||
        !GetClientRect(tray, &rect) || rect.right <= 0 || rect.bottom <= 0 ||
        !GetWindowRect(start, &start_rect) || start_rect.right <= start_rect.left || start_rect.bottom <= start_rect.top ||
        !GetWindowRect(rebar, &rebar_rect) || rebar_rect.right <= rebar_rect.left || rebar_rect.bottom <= rebar_rect.top)
    { stable = FALSE; return; }
    if (!stable || start != previous_start || rebar != previous_rebar || !EqualRect(&rect, &previous_rect) ||
        !EqualRect(&start_rect, &previous_start_rect) || !EqualRect(&rebar_rect, &previous_rebar_rect))
    {
        previous_start = start;
        previous_rebar = rebar;
        previous_rect = rect;
        previous_start_rect = start_rect;
        previous_rebar_rect = rebar_rect;
        stable = TRUE;
        return;
    }
    /* One bounded attempt per shell lifetime, including an unresponsive shell. */
    attempted = TRUE;
    if (!IsWindow(tray) || GetWindowThreadProcessId(tray, &pid) == 0 || pid != previous_pid) return;
    fprintf(stderr, "shell layout sync hwnd=%p pid=%lu client=%ld,%ld\n",
            tray, pid, rect.right, rect.bottom);
    SendMessageTimeoutW(tray, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rect.right, rect.bottom),
                        SMTO_ABORTIFHUNG, 500, &result);
}

/* XP's Alt+Tab window, as measured on the reference XP machine: 331 px wide,
 * seven 43 px cells per row, centred while there is only one row, a 2 px
 * highlight frame around the selected icon and its title in a sunken field
 * below; the window is centred in the work area. The WM owns the keyboard
 * while Alt is held and records the session, the number of Tab steps and
 * whether the switch is open, committed or cancelled in switch.tsv. */
#define SWITCH_COLUMNS 7
#define SWITCH_ROWS 3
#define SWITCH_CELL 43
static struct
{
    unsigned session;
    int steps, count, selected;
    char state;
    HWND window, items[SWITCH_COLUMNS * SWITCH_ROWS];
    HFONT font;
    BOOL reactos;
} switcher;

/* ReactOS's Alt+Tab window (win32ss/user/user32/controls/appswitch.c), for the
 * ReactOS shell (WIN4WINE_SWITCHER=reactos): 48 px cells in a bordered dialog
 * frame with an 8 px margin, the selected icon in a 2 px highlight frame, and
 * the title centred in DEFAULT_GUI_FONT on a pushed button face. */
#define ROS_MARGIN 8
#define ROS_ITEM 40
#define ROS_SPACE 48
#define ROS_STYLE (WS_POPUP | WS_BORDER)
#define ROS_EXSTYLE (WS_EX_TOPMOST | WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW)

static HICON switch_icon(HWND hwnd);

static int ros_text_height(void)
{
    TEXTMETRICW tm;
    HDC dc = GetDC(NULL);
    GetTextMetricsW(dc, &tm);
    ReleaseDC(NULL, dc);
    return tm.tmHeight + 8;
}

static void ros_switch_paint(HDC dc, const RECT *rect)
{
    int shown = switcher.count < SWITCH_COLUMNS ? switcher.count : SWITCH_COLUMNS, i;
    int rows = switcher.count > SWITCH_COLUMNS ? (switcher.count + SWITCH_COLUMNS - 1) / SWITCH_COLUMNS : 1;
    RECT text;
    WCHAR title[512] = L"";
    HGDIOBJ old_font;

    FillRect(dc, rect, GetSysColorBrush(COLOR_3DFACE));
    for (i = 0; i < switcher.count; ++i)
    {
        int x = ROS_MARGIN + ROS_SPACE * (i % SWITCH_COLUMNS) + 4, y = ROS_MARGIN + ROS_SPACE * (i / SWITCH_COLUMNS) + 4;
        if (switcher.count < SWITCH_COLUMNS) x += (SWITCH_COLUMNS - shown) * ROS_SPACE / 2;
        if (i == switcher.selected)
        {
            HPEN pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_HIGHLIGHT));
            HGDIOBJ old_pen = SelectObject(dc, pen), old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, x, y, x + ROS_ITEM, y + ROS_ITEM);
            Rectangle(dc, x + 1, y + 1, x + ROS_ITEM - 1, y + ROS_ITEM - 1);
            SelectObject(dc, old_pen);
            SelectObject(dc, old_brush);
            DeleteObject(pen);
        }
        DrawIconEx(dc, x + 4, y + 4, switch_icon(switcher.items[i]), 32, 32, 0, NULL, DI_NORMAL);
    }
    SetRect(&text, ROS_MARGIN, ROS_MARGIN + rows * ROS_SPACE, rect->right - ROS_MARGIN, rect->bottom - ROS_MARGIN);
    DrawFrameControl(dc, &text, DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_PUSHED);
    if (switcher.selected < switcher.count)
        GetWindowTextW(switcher.items[switcher.selected], title, ARRAYSIZE(title));
    old_font = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, title, -1, &text, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old_font);
}

static BOOL CALLBACK switch_candidate(HWND hwnd, LPARAM param)
{
    DWORD style = GetWindowLongW(hwnd, GWL_STYLE), ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
    WCHAR name[64];
    (void)param;
    if (switcher.count >= SWITCH_COLUMNS * SWITCH_ROWS) return FALSE;
    if (hwnd == switcher.window || !(style & WS_VISIBLE) || (style & WS_DISABLED)) return TRUE;
    if (!(ex & WS_EX_APPWINDOW) && ((ex & WS_EX_TOOLWINDOW) || GetWindow(hwnd, GW_OWNER))) return TRUE;
    if (ex & WS_EX_NOACTIVATE) return TRUE;
    GetClassNameW(hwnd, name, ARRAYSIZE(name));
    if (!wcscmp(name, L"Progman") || !wcscmp(name, L"Shell_TrayWnd")) return TRUE;
    switcher.items[switcher.count++] = hwnd;
    return TRUE;
}

static void switch_collect(void)
{
    HWND foreground = GetForegroundWindow();
    int i;
    switcher.count = 0;
    EnumWindows(switch_candidate, 0);
    /* Start from the active window, which is not always the topmost one. */
    for (i = 0; i < switcher.count; ++i)
        if (switcher.items[i] == foreground && i)
        {
            memmove(switcher.items + 1, switcher.items, i * sizeof(HWND));
            switcher.items[0] = foreground;
            break;
        }
}

static HICON switch_icon(HWND hwnd)
{
    DWORD_PTR icon = 0;
    if (!SendMessageTimeoutW(hwnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 100, &icon)) icon = 0;
    if (!icon) icon = GetClassLongPtrW(hwnd, GCLP_HICON);
    if (!icon && !SendMessageTimeoutW(hwnd, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 100, &icon)) icon = 0;
    if (!icon) icon = GetClassLongPtrW(hwnd, GCLP_HICONSM);
    return icon ? (HICON)icon : default_icon;
}

static int switch_rows(void)
{
    return switcher.count > SWITCH_COLUMNS ? (switcher.count + SWITCH_COLUMNS - 1) / SWITCH_COLUMNS : 1;
}

static void switch_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rect, field;
    int shown = switcher.count < SWITCH_COLUMNS ? switcher.count : SWITCH_COLUMNS;
    int left = 15 + (SWITCH_COLUMNS - shown) * SWITCH_CELL / 2, i, rows = switch_rows();
    WCHAR title[512] = L"";
    HGDIOBJ old_font;

    GetClientRect(hwnd, &rect);
    if (switcher.reactos)
    {
        ros_switch_paint(dc, &rect);
        EndPaint(hwnd, &ps);
        return;
    }
    FillRect(dc, &rect, GetSysColorBrush(COLOR_3DFACE));
    DrawEdge(dc, &rect, EDGE_RAISED, BF_RECT);
    for (i = 0; i < switcher.count; ++i)
    {
        int x = left + i % SWITCH_COLUMNS * SWITCH_CELL, y = 19 + i / SWITCH_COLUMNS * SWITCH_CELL;
        if (i == switcher.selected)
        {
            RECT frame = {x + 1, y, x + 1 + SWITCH_CELL, y + SWITCH_CELL};
            HBRUSH brush = GetSysColorBrush(COLOR_HIGHLIGHT);
            FrameRect(dc, &frame, brush);
            InflateRect(&frame, -1, -1);
            FrameRect(dc, &frame, brush);
        }
        DrawIconEx(dc, x + 6, y + 5, switch_icon(switcher.items[i]), 32, 32, 0, NULL, DI_NORMAL);
    }
    SetRect(&field, 15, 72 + (rows - 1) * SWITCH_CELL, 316, 97 + (rows - 1) * SWITCH_CELL);
    DrawEdge(dc, &field, EDGE_SUNKEN, BF_RECT);
    if (switcher.selected < switcher.count)
        GetWindowTextW(switcher.items[switcher.selected], title, ARRAYSIZE(title));
    InflateRect(&field, -2, -2);
    field.left += 2;
    old_font = SelectObject(dc, switcher.font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawTextW(dc, title, -1, &field, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(dc, old_font);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK switch_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) { switch_paint(hwnd); return 0; }
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void switch_show(void)
{
    RECT area;
    int width = 30 + SWITCH_COLUMNS * SWITCH_CELL, height = 64 + switch_rows() * SWITCH_CELL;
    if (!switcher.font)
    {
        NONCLIENTMETRICSW metrics = {.cbSize = sizeof(metrics)};
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
        metrics.lfCaptionFont.lfWeight = FW_BOLD;
        switcher.font = CreateFontIndirectW(&metrics.lfCaptionFont);
    }
    if (switcher.reactos)
    {
        /* Centred on the screen, as ReactOS does. */
        int rows = switch_rows();
        RECT frame;
        SetRect(&frame, 0, 0, SWITCH_COLUMNS * ROS_SPACE + 2 * ROS_MARGIN,
                rows * ROS_SPACE + 2 * ROS_MARGIN + ros_text_height());
        OffsetRect(&frame, (GetSystemMetrics(SM_CXSCREEN) - frame.right) / 2,
                   (GetSystemMetrics(SM_CYSCREEN) - frame.bottom) / 2);
        AdjustWindowRectEx(&frame, ROS_STYLE, FALSE, ROS_EXSTYLE);
        if (!switcher.window)
            switcher.window = CreateWindowExW(ROS_EXSTYLE, L"Win4WineSwitch", L"", ROS_STYLE,
                                              0, 0, 0, 0, NULL, NULL, GetModuleHandleW(NULL), NULL);
        if (!switcher.window) return;
        SetWindowPos(switcher.window, HWND_TOPMOST, frame.left, frame.top, frame.right - frame.left,
                     frame.bottom - frame.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(switcher.window, NULL, FALSE);
        UpdateWindow(switcher.window);
        return;
    }
    if (!switcher.window)
        switcher.window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"Win4WineSwitch", L"", WS_POPUP,
                                          0, 0, width, height, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!switcher.window) return;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    SetWindowPos(switcher.window, HWND_TOPMOST, area.left + (area.right - area.left - width) / 2,
                 area.top + (area.bottom - area.top - height) / 2, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(switcher.window, NULL, FALSE);
    UpdateWindow(switcher.window);
}

static void switch_commit(void)
{
    HWND target = switcher.selected < switcher.count ? switcher.items[switcher.selected] : NULL;
    unsigned i;
    if (switcher.window) ShowWindow(switcher.window, SW_HIDE);
    if (!target) return;
    /* The WM activates the chosen window as for a click; Wine gives a
     * managed window X focus only through the WM. Linux windows are named by
     * their own X window, Wine windows by the one Wine created. */
    for (i = 0; i < MAX_CLIENTS; ++i)
        if (clients[i].xid && clients[i].hwnd == target)
        {
            command("switch", clients[i].xid);
            return;
        }
    if (IsIconic(target)) ShowWindowAsync(target, SW_RESTORE);
    command("switch", (unsigned long)(uintptr_t)GetPropW(target, L"__wine_x11_whole_window"));
}

static void switch_events(void)
{
    WCHAR path[MAX_PATH + 32];
    unsigned session;
    int steps;
    char state;
    FILE *file;
    swprintf(path, MAX_PATH + 32, L"%ls\\switch.tsv", directory);
    if (!(file = _wfopen(path, L"rb"))) return;
    if (fscanf(file, "%u %d %c", &session, &steps, &state) != 3) state = 0;
    fclose(file);
    if (!state || (session == switcher.session && steps == switcher.steps && state == switcher.state)) return;
    if (session != switcher.session)
    {
        if (switcher.window) ShowWindow(switcher.window, SW_HIDE);
        switch_collect();
    }
    else if (switcher.state != 'o') return; /* This session already ended. */
    switcher.session = session;
    switcher.steps = steps;
    switcher.state = state;
    if (!switcher.count) return;
    switcher.selected = ((steps % switcher.count) + switcher.count) % switcher.count;
    if (state == 'o' && switcher.count > 1) switch_show();
    else if (state == 'c') switch_commit();
    else if (switcher.window) ShowWindow(switcher.window, SW_HIDE);
}

/* Linux tray icons. bridge-wm/tray.py hosts StatusNotifierItems and lists
 * them in tray.tsv with a W4I1 image each; they join the XP notification
 * area through the public Shell_NotifyIcon. Clicks go back as *.tray files,
 * and a context menu that tray.py fetched over D-Bus is shown here. */
#define MAX_TRAY 64
#define TRAY_CALLBACK (WM_APP + 0x10)
#define MAX_MENU 256
static struct tray_icon
{
    unsigned id;
    int seen, added, failed, menu_pending;
    char status;
    HICON icon, retired;
    FILETIME icon_time;
    DWORD icon_size;
    WCHAR tip[128];
    POINT menu_point;
    unsigned menu_shown;
} tray_icons[MAX_TRAY];
static HWND tray_window;
static UINT taskbar_created;

static void tray_notify(struct tray_icon *t, DWORD op)
{
    NOTIFYICONDATAW data = {0};
    data.cbSize = sizeof(data);
    data.hWnd = tray_window;
    data.uID = t->id;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = TRAY_CALLBACK;
    data.hIcon = t->icon ? t->icon : default_icon;
    lstrcpynW(data.szTip, t->tip, ARRAYSIZE(data.szTip));
    if (op == NIM_DELETE) data.uFlags = 0;
    /* Before the shell's taskbar exists every attempt fails; the next
     * change or TaskbarCreated tries again. Report only the first. An add
     * the shell took while still starting may have answered FALSE: then
     * the icon exists and only NIM_MODIFY succeeds. */
    if (!Shell_NotifyIconW(op, &data) && (op != NIM_ADD || !Shell_NotifyIconW(NIM_MODIFY, &data)))
    {
        if (!t->failed) fprintf(stderr, "Shell_NotifyIcon %lu for tray icon %u failed\n", op, t->id);
        t->failed = 1;
        return;
    }
    t->failed = 0;
    if (op == NIM_ADD) t->added = 1;
}

static void tray_remove(struct tray_icon *t)
{
    if (t->added) tray_notify(t, NIM_DELETE);
    if (t->icon) DestroyIcon(t->icon);
    if (t->retired) DestroyIcon(t->retired);
    memset(t, 0, sizeof(*t));
}

/* Returns whether the image changed. */
static int tray_icon_image(struct tray_icon *t)
{
    WCHAR path[MAX_PATH + 40];
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    HICON icon;
    swprintf(path, MAX_PATH + 40, L"%ls\\tray-%u.argb", directory, t->id);
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attrs) || attrs.nFileSizeHigh ||
        attrs.nFileSizeLow > 12 + 64 * 64 * 4)
        return 0;
    if (t->icon && !CompareFileTime(&attrs.ftLastWriteTime, &t->icon_time) && attrs.nFileSizeLow == t->icon_size)
        return 0;
    if (!(icon = read_icon(path, attrs.nFileSizeLow))) return 0;
    /* The shell keeps showing the previous image until NIM_MODIFY has the
     * new handle; destroying it first could hand the same value back. */
    if (t->retired) DestroyIcon(t->retired);
    t->retired = t->icon;
    t->icon = icon;
    t->icon_time = attrs.ftLastWriteTime;
    t->icon_size = attrs.nFileSizeLow;
    return 1;
}

static void tray_menu(struct tray_icon *t)
{
    WCHAR path[MAX_PATH + 40], label[256];
    char line[1024];
    unsigned seq = 0, count = 0, items[MAX_MENU];
    HMENU stack[9] = {0};
    int depth, item, offset, chosen;
    char flags[8];
    FILE *file;
    swprintf(path, MAX_PATH + 40, L"%ls\\menu-%u.tsv", directory, t->id);
    if (!(file = _wfopen(path, L"rb"))) return;
    if (!fgets(line, sizeof(line), file) || sscanf(line, "%u", &seq) != 1 || seq <= t->menu_shown)
    {
        fclose(file);
        return;
    }
    t->menu_pending = 0;
    t->menu_shown = seq;
    stack[0] = CreatePopupMenu();
    while (stack[0] && count < MAX_MENU && fgets(line, sizeof(line), file))
    {
        UINT mf = MF_STRING;
        offset = 0;
        line[strcspn(line, "\r\n")] = 0;
        if (sscanf(line, "%d\t%d\t%7s\t%n", &depth, &item, flags, &offset) != 3 || depth < 0 || depth > 7) continue;
        if (!offset) offset = strlen(line);
        if (!MultiByteToWideChar(CP_UTF8, 0, line + offset, -1, label, ARRAYSIZE(label))) label[0] = 0;
        while (depth > 0 && !stack[depth]) --depth;
        if (strchr(flags, 's')) { AppendMenuW(stack[depth], MF_SEPARATOR, 0, NULL); continue; }
        if (strchr(flags, 'd')) mf |= MF_GRAYED;
        if (strchr(flags, 'c') || strchr(flags, 'r')) mf |= MF_CHECKED;
        items[count] = item;
        /* An entry followed by deeper ones opens a submenu; the next line decides. */
        AppendMenuW(stack[depth], mf, ++count, label);
        stack[depth + 1] = NULL;
        {
            long position = ftell(file);
            char next[64];
            int next_depth;
            if (fgets(next, sizeof(next), file) && sscanf(next, "%d", &next_depth) == 1 && next_depth == depth + 1)
            {
                HMENU sub = CreatePopupMenu();
                MENUITEMINFOW info = {.cbSize = sizeof(info), .fMask = MIIM_SUBMENU, .hSubMenu = sub};
                SetMenuItemInfoW(stack[depth], count, FALSE, &info);
                stack[depth + 1] = sub;
            }
            fseek(file, position, SEEK_SET);
        }
    }
    fclose(file);
    if (!stack[0]) return;
    /* The usual notification-area idiom: the owner must be foreground for
     * the menu to close when the user clicks elsewhere. */
    SetForegroundWindow(tray_window);
    chosen = TrackPopupMenuEx(stack[0], TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                              t->menu_point.x, t->menu_point.y, tray_window, NULL);
    PostMessageW(tray_window, WM_NULL, 0, 0);
    DestroyMenu(stack[0]);
    if (chosen > 0 && (unsigned)chosen <= count)
    {
        char data[80];
        snprintf(data, sizeof(data), "event %u %u %u\n", t->id, seq, items[chosen - 1]);
        write_file_command(data, L"tray");
    }
}

static void tray_sync(void)
{
    WCHAR path[MAX_PATH + 32];
    char line[1024];
    FILE *file;
    unsigned i;
    if (!tray_window) return;
    for (i = 0; i < MAX_TRAY; ++i) tray_icons[i].seen = 0;
    swprintf(path, MAX_PATH + 32, L"%ls\\tray.tsv", directory);
    if ((file = _wfopen(path, L"rb")))
    {
        while (fgets(line, sizeof(line), file))
        {
            unsigned id;
            char status;
            int offset = 0, changed;
            WCHAR tip[128];
            struct tray_icon *t = NULL;
            line[strcspn(line, "\r\n")] = 0;
            if (sscanf(line, "%u\t%c\t%n", &id, &status, &offset) != 2 || !id) continue;
            if (!offset) offset = strlen(line);
            if (!MultiByteToWideChar(CP_UTF8, 0, line + offset, -1, tip, ARRAYSIZE(tip))) tip[0] = 0;
            for (i = 0; i < MAX_TRAY && !t; ++i) if (tray_icons[i].id == id) t = &tray_icons[i];
            for (i = 0; i < MAX_TRAY && !t; ++i) if (!tray_icons[i].id) { t = &tray_icons[i]; t->id = id; }
            if (!t) continue;
            t->seen = 1;
            changed = tray_icon_image(t) | (wcscmp(t->tip, tip) != 0) | (t->status != status);
            wcscpy(t->tip, tip);
            t->status = status;
            if (!t->added) tray_notify(t, NIM_ADD);
            else if (changed) tray_notify(t, NIM_MODIFY);
            if (t->retired && t->added)
            {
                DestroyIcon(t->retired);
                t->retired = NULL;
            }
        }
        fclose(file);
    }
    for (i = 0; i < MAX_TRAY; ++i)
    {
        if (tray_icons[i].id && !tray_icons[i].seen) tray_remove(&tray_icons[i]);
        else if (tray_icons[i].menu_pending) tray_menu(&tray_icons[i]);
    }
}

static LRESULT CALLBACK tray_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == TRAY_CALLBACK)
    {
        const char *op = NULL;
        unsigned i;
        POINT pt;
        GetCursorPos(&pt);
        switch (LOWORD(lp))
        {
        case WM_LBUTTONUP: op = "activate"; break;
        case WM_MBUTTONUP: op = "secondary"; break;
        case WM_RBUTTONUP: op = "context"; break;
        }
        for (i = 0; op && i < MAX_TRAY; ++i)
            if (tray_icons[i].id == wp)
            {
                char data[80];
                if (!strcmp(op, "context"))
                {
                    tray_icons[i].menu_pending = 1;
                    tray_icons[i].menu_point = pt;
                }
                snprintf(data, sizeof(data), "%s %u %ld %ld\n", op, tray_icons[i].id, pt.x, pt.y);
                write_file_command(data, L"tray");
                break;
            }
        return 0;
    }
    if (msg == taskbar_created && msg)
    {
        /* A restarted shell has an empty notification area. */
        unsigned i;
        for (i = 0; i < MAX_TRAY; ++i) tray_icons[i].added = 0;
        dirty = TRUE;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void tray_start(void)
{
    WNDCLASSW cls = {0};
    WCHAR path[MAX_PATH + 32];
    char size[16];
    FILE *file;
    taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    cls.lpfnWndProc = tray_proc;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"Win4WineTray";
    if (!RegisterClassW(&cls)) return;
    tray_window = CreateWindowExW(WS_EX_TOOLWINDOW, L"Win4WineTray", L"", WS_POPUP, 0, 0, 0, 0,
                                  NULL, NULL, GetModuleHandleW(NULL), NULL);
    /* tray.py renders the images at the notification area's icon size. */
    swprintf(path, MAX_PATH + 32, L"%ls\\tray-size.tmp", directory);
    snprintf(size, sizeof(size), "%d\n", GetSystemMetrics(SM_CXSMICON));
    if ((file = _wfopen(path, L"wb")))
    {
        WCHAR target[MAX_PATH + 32];
        fputs(size, file);
        fclose(file);
        swprintf(target, MAX_PATH + 32, L"%ls\\tray-size", directory);
        if (!MoveFileExW(path, target, MOVEFILE_REPLACE_EXISTING)) DeleteFileW(path);
    }
}

/* Out-of-context WinEvents arrive as messages for this thread. Only top-level
 * windows can be tasks; a destroyed one is matched against the last list. */
static void CALLBACK win_event(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG object, LONG child,
                               DWORD thread, DWORD time)
{
    (void)hook; (void)thread; (void)time;
    if (event == EVENT_SYSTEM_FOREGROUND) dirty = TRUE;
    else if (object != OBJID_WINDOW || child != CHILDID_SELF || !hwnd) return;
    else if (event == EVENT_OBJECT_DESTROY) dirty |= contains(previous, previous_count, hwnd);
    else if (GetAncestor(hwnd, GA_PARENT) == GetDesktopWindow()) dirty = TRUE;
}

int wmain(int argc, WCHAR **argv)
{
    WNDCLASSW cls = {0};
    MSG msg;
    HANDLE changes, lock;
    WCHAR path[MAX_PATH + 32];
    UINT_PTR layout_timer = 0;
    if (argc != 2 || wcslen(argv[1]) >= MAX_PATH) return 2;
    wcscpy(directory, argv[1]);
    {
        WCHAR style[16];
        DWORD len = GetEnvironmentVariableW(L"WIN4WINE_SWITCHER", style, ARRAYSIZE(style));
        switcher.reactos = len && len < ARRAYSIZE(style) && !wcscmp(style, L"reactos");
    }
    shell_message = RegisterWindowMessageW(L"SHELLHOOK");
    default_icon = LoadIconW(NULL, IDI_APPLICATION);
    cls.hIcon = default_icon;
    cls.lpfnWndProc = proxy_proc;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"Win4WineProxy";
    if (!RegisterClassW(&cls)) return 3;
    cls.hIcon = NULL;
    cls.hCursor = LoadCursorW(NULL, IDC_ARROW);
    cls.lpfnWndProc = switch_proc;
    cls.lpszClassName = L"Win4WineSwitch";
    if (!RegisterClassW(&cls)) return 3;
    tray_start();
    /* The WM tests this lock to know whether the proxy is running.
     * wineserver backs it with a POSIX lock that ends with this process. */
    swprintf(path, MAX_PATH + 32, L"%ls\\proxy.lock", directory);
    lock = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (lock == INVALID_HANDLE_VALUE || !LockFile(lock, 0, 0, 1, 0))
        fprintf(stderr, "proxy lock unavailable: %lu\n", GetLastError());
    /* Wake when the WM writes a file or a Win32 window changes; never poll. */
    changes = FindFirstChangeNotificationW(directory, FALSE,
                                           FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (changes == INVALID_HANDLE_VALUE) return 4;
    SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, win_event, 0, 0, WINEVENT_OUTOFCONTEXT);
    SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND, NULL, win_event, 0, 0, WINEVENT_OUTOFCONTEXT);
    SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE, NULL, win_event, 0, 0, WINEVENT_OUTOFCONTEXT);
    SetWinEventHook(EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE, NULL, win_event, 0, 0, WINEVENT_OUTOFCONTEXT);
    shell_key_events(TRUE);
    for (;;)
    {
        switch_events();
        shell_key_events(FALSE);
        if (dirty)
        {
            dirty = FALSE;
            snapshot();
            shell_events();
            tray_sync();
        }
        if (shell_layout_pending() != !!layout_timer)
        {
            if (layout_timer) { KillTimer(NULL, layout_timer); layout_timer = 0; }
            else layout_timer = SetTimer(NULL, 0, 200, NULL);
        }
        /* Paint and other messages wait for the frames to move: a native
         * client that moved first exposes part of its frame, and repainting
         * that first would hold back the move. */
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
                for (i = 0; i < MAX_TRAY; ++i) if (tray_icons[i].id) tray_remove(&tray_icons[i]);
                return 0;
            }
            if (msg.message == WM_TIMER && !msg.hwnd && msg.wParam == layout_timer)
            {
                shell_layout();
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (dirty) continue; /* Changes made by this pass. */
        if (MsgWaitForMultipleObjects(1, &changes, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0)
        {
            FindNextChangeNotification(changes);
            dirty = TRUE;
        }
    }
}
