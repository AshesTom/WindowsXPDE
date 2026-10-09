/* Public cross-process icon contracts, using only generated test pixels. */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static HICON icons[80];
static int failures;
static BOOL callback_done;
static HICON callback_icon;
#define check(c, s) do { if (!(c)) { printf("FAIL: %s (error %lu)\n", s, GetLastError()); ++failures; } } while (0)

static void CALLBACK icon_callback(HWND hwnd, UINT msg, ULONG_PTR data, LRESULT result)
{
    (void)hwnd; (void)msg; (void)data;
    callback_icon = (HICON)result;
    callback_done = TRUE;
}

static HICON make_icon(unsigned int seed)
{
    BITMAPINFO info = {0};
    ICONINFO icon = {0};
    DWORD *bits;
    BYTE mask[64] = {0};
    unsigned int x, y;
    HICON result;
    if (seed == 77) return LoadImageW(NULL, MAKEINTRESOURCEW(32512), IMAGE_ICON, 16, 16, LR_SHARED);
    if (seed == 78)
    {
        BYTE and_bits[32], xor_bits[32];
        for (x = 0; x < 32; ++x) { and_bits[x] = (BYTE)(x * 7); xor_bits[x] = (BYTE)(x * 13); }
        return CreateIcon(NULL, 16, 16, 1, 1, and_bits, xor_bits);
    }
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 16;
    info.bmiHeader.biHeight = -16;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    icon.fIcon = TRUE;
    icon.hbmColor = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    if (!icon.hbmColor) return NULL;
    for (y = 0; y < 16; ++y)
        for (x = 0; x < 16; ++x)
            bits[y * 16 + x] = ((x < 8 ? 255u : 128u) << 24) |
                              (((seed * 19 + x * 11) & 255) << 16) |
                              (((y * 13 + seed) & 255) << 8) | 63;
    icon.hbmMask = CreateBitmap(16, 16, 1, 1, mask);
    result = CreateIconIndirect(&icon);
    DeleteObject(icon.hbmColor);
    DeleteObject(icon.hbmMask);
    return result;
}

static DWORD draw_hash(HICON icon)
{
    BITMAPINFO info = {0};
    HDC dc = CreateCompatibleDC(NULL);
    DWORD *bits, hash = 2166136261u;
    HBITMAP bitmap, old;
    unsigned int i;
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 16;
    info.bmiHeader.biHeight = -16;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    old = SelectObject(dc, bitmap);
    for (i = 0; i < 256; ++i) bits[i] = 0xff305070;
    if (!DrawIconEx(dc, 0, 0, icon, 16, 16, 0, NULL, DI_NORMAL)) hash = 0;
    else { GdiFlush(); for (i = 0; i < 256; ++i) hash = (hash ^ bits[i]) * 16777619u; }
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return hash;
}

static HICON make_large_icon(unsigned int size)
{
    BITMAPINFO info = {0};
    ICONINFO icon = {0};
    DWORD *bits;
    BYTE *mask = calloc(((size + 15) / 16 * 2) * size, 1);
    HICON result;
    unsigned int i;
    if (!mask) return NULL;
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -(LONG)size;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    icon.fIcon = TRUE;
    icon.hbmColor = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    if (!icon.hbmColor) { free(mask); return NULL; }
    for (i = 0; i < size * size; ++i) bits[i] = 0x806080a0;
    icon.hbmMask = CreateBitmap(size, size, 1, 1, mask);
    result = CreateIconIndirect(&icon);
    DeleteObject(icon.hbmColor);
    DeleteObject(icon.hbmMask);
    free(mask);
    return result;
}

static BOOL inspect_icon(HICON icon)
{
    ICONINFO info;
    BITMAP bitmap;
    BOOL valid = GetIconInfo(icon, &info);
    if (!valid) return FALSE;
    valid = info.fIcon && GetObjectW(info.hbmColor ? info.hbmColor : info.hbmMask, sizeof(bitmap), &bitmap) &&
            bitmap.bmWidth == 16 && bitmap.bmHeight == (info.hbmColor ? 16 : 32);
    DeleteObject(info.hbmColor);
    DeleteObject(info.hbmMask);
    return valid;
}

static void check_image_list(HICON foreign, HICON local)
{
    typedef HANDLE (WINAPI *create_fn)(INT, INT, UINT, INT, INT);
    typedef INT (WINAPI *replace_fn)(HANDLE, INT, HICON);
    typedef HICON (WINAPI *get_fn)(HANDLE, INT, UINT);
    typedef BOOL (WINAPI *destroy_fn)(HANDLE);
    WCHAR path[MAX_PATH] = L"comctl32.dll";
    HMODULE module;
    DWORD length = GetEnvironmentVariableW(L"XP_ICON_COMCTL", path, MAX_PATH);
    if (!length) lstrcpyW(path, L"comctl32.dll");
    if (length >= MAX_PATH) { check(FALSE, "common controls test path fits buffer"); return; }
    module = LoadLibraryW(path);
    create_fn create;
    replace_fn replace;
    get_fn get;
    destroy_fn destroy;
    HANDLE a, b;
    HICON icon_a, icon_b;
    if (!module) { check(FALSE, "load public common controls API"); return; }
    create = (create_fn)(void *)GetProcAddress(module, "ImageList_Create");
    replace = (replace_fn)(void *)GetProcAddress(module, "ImageList_ReplaceIcon");
    get = (get_fn)(void *)GetProcAddress(module, "ImageList_GetIcon");
    destroy = (destroy_fn)(void *)GetProcAddress(module, "ImageList_Destroy");
    check(create && replace && get && destroy, "public ImageList exports");
    if (!(create && replace && get && destroy)) { FreeLibrary(module); return; }
    a = create(16, 16, 0x21 /* ILC_COLOR32 | ILC_MASK */, 1, 1);
    b = create(16, 16, 0x21, 1, 1);
    check(a && b, "create public image lists");
    if (!a || !b) { if (a) destroy(a); if (b) destroy(b); FreeLibrary(module); return; }
    check(replace(a, -1, foreign) == 0 && replace(b, -1, local) == 0, "ImageList accepts foreign and local icons");
    icon_a = get(a, 0, 0);
    icon_b = get(b, 0, 0);
    check(icon_a && icon_b && draw_hash(icon_a) == draw_hash(icon_b), "ImageList preserves foreign pixels like local pixels");
    if (icon_a) DestroyIcon(icon_a);
    if (icon_b) DestroyIcon(icon_b);
    if (a) destroy(a);
    if (b) destroy(b);
    FreeLibrary(module);
}

static DWORD WINAPI concurrent_reader(void *data)
{
    HICON *handles = data;
    unsigned int i;
    for (i = 0; i < 160; ++i)
        if (!inspect_icon(handles[i % 80]) || !draw_hash(handles[i % 80])) return 1;
    return 0;
}

static LRESULT CALLBACK owner_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_APP + 1 && wp < 80) return (LRESULT)icons[wp];
    if (msg == WM_APP + 2 && wp < 80)
    {
        DestroyIcon(icons[wp]);
        icons[wp] = NULL;
        return 1;
    }
    if (msg == WM_APP + 4 && wp < 80)
    {
        if (icons[wp]) DestroyIcon(icons[wp]);
        icons[wp] = make_icon(1000 + (unsigned int)lp);
        return (LRESULT)icons[wp];
    }
    if (msg == WM_APP + 7 && wp < 80)
    {
        if (icons[wp]) DestroyIcon(icons[wp]);
        icons[wp] = make_large_icon((unsigned int)lp);
        return (LRESULT)icons[wp];
    }
    if (msg == WM_APP + 8 && wp < 80)
    {
        ICONINFO info;
        if (!GetIconInfo(icons[wp], &info)) return FALSE;
        DeleteObject(info.hbmColor);
        DeleteObject(info.hbmMask);
        return TRUE;
    }
    if (msg == WM_APP + 5) ExitProcess(0); /* server must free remaining owner snapshots */
    if (msg == WM_APP + 3)
    {
        PostQuitMessage(0);
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int wmain(int argc, WCHAR **argv)
{
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION process;
    WCHAR exe[MAX_PATH], command[MAX_PATH + 64], title[64];
    HWND hwnd;
    HICON borrowed, copy, local;
    DWORD expected, start;
    unsigned int i;
    BOOL enabled = GetEnvironmentVariableW(L"WIN4WINE_EXTERNAL_SHELL", NULL, 0) != 0;
    if (argc > 1)
    {
        WNDCLASSW cls = {0};
        MSG message;
        cls.lpfnWndProc = owner_proc;
        cls.lpszClassName = L"ForeignIconOwner";
        RegisterClassW(&cls);
        for (i = 0; i < 80; ++i) icons[i] = make_icon(i);
        hwnd = CreateWindowW(cls.lpszClassName, argv[1], WS_OVERLAPPEDWINDOW,
                             0, 0, 100, 100, NULL, NULL, NULL, NULL);
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icons[0]);
        while (GetMessageW(&message, NULL, 0, 0) > 0) DispatchMessageW(&message);
        for (i = 0; i < 80; ++i) if (icons[i] && i != 77) DestroyIcon(icons[i]);
        return 0;
    }
    startup.cb = sizeof(startup);
    swprintf(title, 64, L"ForeignIcon-%lu", GetCurrentProcessId());
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    swprintf(command, MAX_PATH + 64, L"\"%ls\" %ls", exe, title);
    check(CreateProcessW(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process), "create owner process");
    if (failures) return 1;
    start = GetTickCount();
    do { hwnd = FindWindowW(L"ForeignIconOwner", title); if (!hwnd) Sleep(10); }
    while (!hwnd && GetTickCount() - start < 10000);
    check(hwnd != NULL, "owner ready");
    if (!hwnd) { TerminateProcess(process.hProcess, 1); return 1; }
    borrowed = (HICON)SendMessageW(hwnd, WM_APP + 1, 0, 0);
    if (!enabled)
    {
        check(!SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0), "normal Wine rejects cross-process WM_GETICON");
        check(!inspect_icon(borrowed), "normal Wine rejects foreign icon reads");
    }
    else
    {
        HICON handles[80];
        HANDLE readers[2];
        DWORD code;
        check((HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0) == borrowed, "WM_GETICON preserves borrowed HICON identity");
        {
            MSG message;
            DWORD_PTR result = 0;
            check(SendMessageTimeoutW(hwnd, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 1000, &result) &&
                  (HICON)result == borrowed, "timeout WM_GETICON preserves borrowed identity");
            check(SendMessageCallbackW(hwnd, WM_GETICON, ICON_SMALL2, 0, icon_callback, 0), "queue asynchronous WM_GETICON");
            start = GetTickCount();
            while (!callback_done && GetTickCount() - start < 1000)
            {
                while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
                Sleep(1);
            }
            check(callback_done && callback_icon == borrowed && inspect_icon(callback_icon),
                  "asynchronous callback returns usable borrowed icon");
        }
        local = make_icon(0);
        expected = draw_hash(local);
        check(inspect_icon(borrowed), "GetIconInfo creates usable caller-owned bitmaps");
        check(expected && draw_hash(borrowed) == expected, "DrawIconEx matches local pixels including half alpha");
        check_image_list(borrowed, local);
        {
            HICON scaled = CopyImage(borrowed, IMAGE_ICON, 16, 16, LR_COPYFROMRESOURCE | LR_COPYRETURNORG);
            check(scaled && draw_hash(scaled) == expected, "taskbar-style CopyImage preserves foreign pixels");
            if (scaled && scaled != borrowed) DestroyIcon(scaled);
        }
        copy = CopyIcon(borrowed);
        check(copy && copy != borrowed && draw_hash(copy) == expected, "CopyIcon produces independent owned icon");
        check(!DestroyIcon(borrowed), "foreign borrowed handle cannot be destroyed");
        check(inspect_icon(borrowed), "failed foreign destruction preserves owner");
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)local);
        check((HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0) == borrowed,
              "unsupported foreign WM_SETICON cannot replace owner icon");
        for (i = 0; i < 80; ++i) handles[i] = (HICON)SendMessageW(hwnd, WM_APP + 1, i, 0);
        readers[0] = CreateThread(NULL, 0, concurrent_reader, handles, 0, NULL);
        readers[1] = CreateThread(NULL, 0, concurrent_reader, handles, 0, NULL);
        check(readers[0] && readers[1], "start concurrent foreign cache readers");
        for (i = 1; i < 80; ++i)
        {
            HICON other = (HICON)SendMessageW(hwnd, WM_APP + 1, i, 0);
            HICON reference = make_icon(i);
            check(inspect_icon(other) && draw_hash(other) == draw_hash(reference), "bounded cache pressure preserves pixels");
            if (i != 77) DestroyIcon(reference);
        }
        for (i = 0; i < 2; ++i)
            if (readers[i])
            {
                check(WaitForSingleObject(readers[i], 10000) == WAIT_OBJECT_0 &&
                      GetExitCodeThread(readers[i], &code) && !code,
                      "concurrent cache publication, reads and eviction preserve pins");
                CloseHandle(readers[i]);
            }
        check(draw_hash(borrowed) == expected, "evicted live icon rematerializes");
        SendMessageW(hwnd, WM_APP + 2, 0, 0);
        check(!inspect_icon(borrowed) && !draw_hash(borrowed), "owner destruction invalidates cached borrowed icon");
        check(draw_hash(copy) == expected, "owned copy survives original destruction");
        for (i = 0; i < 100; ++i)
        {
            HICON replaced = (HICON)SendMessageW(hwnd, WM_APP + 4, 0, i);
            check(replaced != borrowed && inspect_icon(replaced), "reused slot carries a new generation");
            check(!inspect_icon(borrowed), "old generation never aliases replacement");
            borrowed = replaced;
            SendMessageW(hwnd, WM_APP + 2, 0, 0);
        }
        {
            unsigned int accepted = 0;
            ICONINFO info;
            HICON oversized = (HICON)SendMessageW(hwnd, WM_APP + 7, 0, 257);
            check(oversized && SendMessageW(hwnd, WM_APP + 8, 0, 0), "oversized icon remains usable by its owner");
            check(!GetIconInfo(oversized, &info), "foreign snapshot dimensions are bounded");
            for (i = 1; i <= 40; ++i)
            {
                HICON large = (HICON)SendMessageW(hwnd, WM_APP + 7, i, 256);
                check(large && SendMessageW(hwnd, WM_APP + 8, i, 0), "snapshot budget does not break local icon creation");
                if (GetIconInfo(large, &info))
                {
                    ++accepted;
                    DeleteObject(info.hbmColor);
                    DeleteObject(info.hbmMask);
                }
            }
            check(accepted && accepted <= 32, "per-owner snapshot memory budget is enforced");
            printf("foreign-icons: %u / 40 large snapshots admitted\n", accepted);
        }
        borrowed = (HICON)SendMessageW(hwnd, WM_APP + 1, 79, 0);
        check(inspect_icon(borrowed), "cache owner exit fixture");
        SendMessageW(hwnd, WM_APP + 5, 0, 0);
        check(WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0, "owner exits");
        check(!inspect_icon(borrowed), "owner exit invalidates cached icon");
        check(draw_hash(copy) == expected && DestroyIcon(copy), "copy remains owned after owner exit");
        DestroyIcon(local);
    }
    if (!enabled) SendMessageW(hwnd, WM_APP + 3, 0, 0);
    WaitForSingleObject(process.hProcess, 10000);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    printf("foreign-icons: %s (%s)\n", failures ? "FAIL" : "PASS", enabled ? "external shell" : "normal Wine");
    return !!failures;
}
