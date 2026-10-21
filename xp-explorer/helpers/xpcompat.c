/* Run-time compatibility fixes for XP Explorer on a stock, unpatched Wine.
 *
 * win4wine fixes these in Wine itself (patches 0012-0015, 0023, 0066). Here
 * xpstart.exe loads this DLL into XP Explorer and calls xpcompat_init, which
 * redirects, in the import tables of XP's own modules or at Wine's stub
 * entry points, the few calls that a stock Wine does not answer the way XP's
 * shell needs. Only public exports and import tables are touched. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <string.h>

/* setupapi/cfgmgr32: no Windows dock station here (Wine: unimplemented stub,
 * which raises when the taskbar asks before offering "Eject PC"). */
static DWORD WINAPI dock_station_present(BOOL *present)
{
    if (!present) return 0x1f; /* CR_INVALID_POINTER */
    *present = FALSE;
    return 0; /* CR_SUCCESS */
}

/* wuaueng: no Automatic Updates waiting at shut down (Wine: stub). */
static BOOL WINAPI check_updates_at_shutdown(DWORD flags)
{
    (void)flags;
    return FALSE;
}

/* user32: XP's shut down dialog frees a reason list Wine never builds (Wine:
 * no such export). */
static BOOL WINAPI destroy_reasons(void *reasons)
{
    (void)reasons;
    return TRUE;
}

/* advapi32: Software Restriction Policies logging (Wine: no such export). */
static BOOL WINAPI safer_record_event(void *level, const WCHAR *target, void *reserved)
{
    (void)level; (void)target; (void)reserved;
    return TRUE;
}

/* The XP shell was made for 32-bit Windows: under WOW64, XP's shlwapi and
 * shell32 take other paths (folder icons in the Start panel, for example). */
typedef NTSTATUS (WINAPI *query_process_fn)(HANDLE, PROCESSINFOCLASS, void *, ULONG, ULONG *);
static query_process_fn real_query_process;

static NTSTATUS WINAPI query_process(HANDLE process, PROCESSINFOCLASS class, void *info, ULONG size, ULONG *done)
{
    if (class == ProcessWow64Information && (process == GetCurrentProcess()
        || GetProcessId(process) == GetCurrentProcessId()) && info && size >= sizeof(ULONG_PTR))
    {
        *(ULONG_PTR *)info = 0;
        if (done) *done = sizeof(ULONG_PTR);
        return 0;
    }
    return real_query_process(process, class, info, size, done);
}

static BOOL WINAPI is_wow64_process(HANDLE process, BOOL *wow64)
{
    (void)process;
    if (!wow64) return FALSE;
    *wow64 = FALSE;
    return TRUE;
}


/* ---- The Start button (win4wine patch 0066) ----------------------------
 * XP's taskbar creates its Start button as a Button child of Shell_TrayWnd,
 * gives it the "Start" window theme and an image list (BCM_SETIMAGELIST), and
 * sizes itself from BCM_GETIDEALSIZE. user32's classic Button knows neither
 * message, so the button and the whole taskbar came out 0 pixels wide/high.
 * Such buttons are created from XPStartButton instead: user32's Button for
 * behaviour, drawn and measured here with the theme and XP's own image list
 * (opaque: only XP comctl32's public functions touch it). */
#define START_PROP L"XPStartButton.Data"

typedef struct
{
    BUTTON_IMAGELIST images;
    BOOL hot;
} START_DATA;

static WNDPROC button_proc;
static int button_extra;

static START_DATA *start_data(HWND hwnd)
{
    START_DATA *data = GetPropW(hwnd, START_PROP);
    if (!data && (data = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*data))))
        SetPropW(hwnd, START_PROP, data);
    return data;
}

static void *xp_imagelist_function(const char *name)
{
    HMODULE module = GetModuleHandleW(L"comctl32.dll");
    return module ? (void *)GetProcAddress(module, name) : NULL;
}

static SIZE start_image_size(START_DATA *data)
{
    BOOL (WINAPI *icon_size)(HIMAGELIST, int *, int *) = xp_imagelist_function("ImageList_GetIconSize");
    int cx = 0, cy = 0;
    SIZE size = {0, 0};
    if (data && data->images.himl && icon_size && icon_size(data->images.himl, &cx, &cy))
    {
        size.cx = cx;
        size.cy = cy;
    }
    return size;
}

static SIZE start_text_size(HWND hwnd, HDC hdc, HTHEME theme)
{
    WCHAR text[64];
    RECT rect = {0, 0, 0, 0};
    SIZE size = {0, 0};
    int length = GetWindowTextW(hwnd, text, 64);
    if (!length) return size;
    if (!theme || FAILED(GetThemeTextExtent(theme, hdc, BP_PUSHBUTTON, PBS_NORMAL, text, length,
                                            DT_SINGLELINE | DT_LEFT, NULL, &rect)))
        DrawTextW(hdc, text, length, &rect, DT_SINGLELINE | DT_LEFT | DT_CALCRECT);
    size.cx = rect.right - rect.left;
    size.cy = rect.bottom - rect.top;
    return size;
}

/* Label in the theme font, one pixel of text margin on each side, the image,
 * then the theme's padding (Japanese XP: 51x16 + 2 + 25x20 + 34x6 = 112x26). */
static SIZE start_ideal_size(HWND hwnd)
{
    START_DATA *data = start_data(hwnd);
    HTHEME theme = IsThemeActive() ? GetWindowTheme(hwnd) : NULL;
    HDC hdc = GetDC(hwnd);
    HFONT font = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0), old = font ? SelectObject(hdc, font) : NULL;
    SIZE text = start_text_size(hwnd, hdc, theme), image = start_image_size(data), size;
    RECT content, extent;
    if (old) SelectObject(hdc, old);
    ReleaseDC(hwnd, hdc);
    size.cx = image.cx + (text.cx ? text.cx + 2 : 0) + (data ? data->images.margin.left + data->images.margin.right : 0);
    size.cy = max(image.cy, text.cy + 2);
    SetRect(&content, 0, 0, size.cx, size.cy);
    if (theme && SUCCEEDED(GetThemeBackgroundExtent(theme, NULL, BP_PUSHBUTTON, PBS_NORMAL, &content, &extent)))
    {
        size.cx = extent.right - extent.left;
        size.cy = extent.bottom - extent.top;
    }
    else
    {
        /* XP Classic: five pixels wider and seven higher than the label. */
        size.cx += 5 + 2 * GetSystemMetrics(SM_CXEDGE);
        size.cy = max(size.cy, text.cy + 7);
    }
    return size;
}

static void start_paint(HWND hwnd, HDC hdc)
{
    START_DATA *data = start_data(hwnd);
    HTHEME theme = IsThemeActive() ? GetWindowTheme(hwnd) : NULL;
    LRESULT state = CallWindowProcW(button_proc, hwnd, BM_GETSTATE, 0, 0);
    BOOL pushed = (state & BST_PUSHED) != 0;
    int part_state = !IsWindowEnabled(hwnd) ? PBS_DISABLED : pushed ? PBS_PRESSED
                   : data && data->hot ? PBS_HOT : PBS_NORMAL;
    HFONT font = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0), old_font;
    int (WINAPI *image_count)(HIMAGELIST) = xp_imagelist_function("ImageList_GetImageCount");
    BOOL (WINAPI *image_draw)(HIMAGELIST, int, HDC, int, int, UINT) = xp_imagelist_function("ImageList_Draw");
    RECT client, content;
    SIZE image = start_image_size(data);
    WCHAR text[64];
    int length = GetWindowTextW(hwnd, text, 64), x;

    GetClientRect(hwnd, &client);
    content = client;
    if (theme)
    {
        DrawThemeParentBackground(hwnd, hdc, &client);
        DrawThemeBackground(theme, hdc, BP_PUSHBUTTON, part_state, &client, NULL);
        GetThemeBackgroundContentRect(theme, hdc, BP_PUSHBUTTON, part_state, &client, &content);
    }
    else
    {
        DrawFrameControl(hdc, &client, DFC_BUTTON, DFCS_BUTTONPUSH | (pushed ? DFCS_PUSHED : 0));
        InflateRect(&content, -GetSystemMetrics(SM_CXEDGE), -GetSystemMetrics(SM_CYEDGE));
        if (pushed) OffsetRect(&content, 1, 1);
    }
    x = content.left + (data ? data->images.margin.left : 0);
    if (data && data->images.himl && image_draw)
    {
        int count = image_count ? image_count(data->images.himl) : 1;
        int index = count == 1 ? 0 : min(count - 1, part_state - 1);
        image_draw(data->images.himl, index, hdc, x, content.top + (content.bottom - content.top - image.cy) / 2,
                   ILD_NORMAL);
        x += image.cx + (data ? data->images.margin.right : 0);
    }
    if (length)
    {
        RECT label = {x + 1, content.top, content.right, content.bottom};
        old_font = font ? SelectObject(hdc, font) : NULL;
        SetBkMode(hdc, TRANSPARENT);
        if (theme) DrawThemeText(theme, hdc, BP_PUSHBUTTON, part_state, text, length,
                                 DT_SINGLELINE | DT_LEFT | DT_VCENTER, 0, &label);
        else
        {
            SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
            DrawTextW(hdc, text, length, &label, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        }
        if (old_font) SelectObject(hdc, old_font);
    }
    if (!theme && GetFocus() == hwnd && !(SendMessageW(hwnd, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS))
    {
        RECT focus = client;
        InflateRect(&focus, -3, -3);
        DrawFocusRect(hdc, &focus);
    }
}

static void start_redraw(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);
    if (!hdc) return;
    start_paint(hwnd, hdc);
    ReleaseDC(hwnd, hdc);
}

static LRESULT CALLBACK start_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    START_DATA *data;
    LRESULT result;
    switch (message)
    {
    case BCM_SETIMAGELIST:
        if (!lparam || !(data = start_data(hwnd))) return FALSE;
        data->images = *(BUTTON_IMAGELIST *)lparam;
        InvalidateRect(hwnd, NULL, TRUE);
        return TRUE;
    case BCM_GETIMAGELIST:
        if (!lparam || !(data = start_data(hwnd))) return FALSE;
        *(BUTTON_IMAGELIST *)lparam = data->images;
        return TRUE;
    case BCM_GETIDEALSIZE:
        if (!lparam) return FALSE;
        *(SIZE *)lparam = start_ideal_size(hwnd);
        return TRUE;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    case WM_PRINTCLIENT:
    {
        PAINTSTRUCT ps;
        HDC hdc = message == WM_PAINT ? (wparam ? (HDC)wparam : BeginPaint(hwnd, &ps)) : (HDC)wparam;
        start_paint(hwnd, hdc);
        if (message == WM_PAINT && !wparam) EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if ((data = start_data(hwnd)) && !data->hot)
        {
            TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, hwnd, 0};
            data->hot = TRUE;
            TrackMouseEvent(&track);
            start_redraw(hwnd);
        }
        break;
    case WM_MOUSELEAVE:
        if ((data = start_data(hwnd))) data->hot = FALSE;
        start_redraw(hwnd);
        break;
    case WM_THEMECHANGED:
        InvalidateRect(hwnd, NULL, TRUE);
        break;
    case WM_NCDESTROY:
        if ((data = RemovePropW(hwnd, START_PROP))) HeapFree(GetProcessHeap(), 0, data);
        break;
    }
    result = CallWindowProcW(button_proc, hwnd, message, wparam, lparam);
    /* user32's Button paints its classic face itself on these; paint ours over it. */
    switch (message)
    {
    case BM_SETSTATE: case BM_SETCHECK: case BM_SETSTYLE: case WM_SETFOCUS: case WM_KILLFOCUS:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK: case WM_KEYDOWN: case WM_KEYUP:
    case WM_CAPTURECHANGED: case WM_ENABLE: case WM_SETTEXT: case WM_UPDATEUISTATE: case WM_MOUSEMOVE:
        if (IsWindowVisible(hwnd)) start_redraw(hwnd);
        break;
    }
    return result;
}

static BOOL register_start_button(void)
{
    WNDCLASSW button, start;
    if (!GetClassInfoW(NULL, L"Button", &button)) return FALSE;
    button_proc = button.lpfnWndProc;
    button_extra = button.cbWndExtra;
    start = button;
    start.style = (button.style & ~CS_PARENTDC) | CS_GLOBALCLASS;
    start.lpfnWndProc = start_proc;
    start.hInstance = GetModuleHandleW(NULL);
    start.lpszClassName = L"XPStartButton";
    return RegisterClassW(&start) != 0;
}

typedef HWND (WINAPI *create_window_fn)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
static create_window_fn real_create_window;

static HWND WINAPI create_window(DWORD ex_style, LPCWSTR class_name, LPCWSTR title, DWORD style, int x, int y,
                                 int width, int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param)
{
    WCHAR parent_class[32];
    if (button_proc && parent && !IS_INTRESOURCE(class_name) && !_wcsicmp(class_name, L"Button")
        && GetClassNameW(parent, parent_class, 32) && !wcscmp(parent_class, L"Shell_TrayWnd"))
        class_name = L"XPStartButton";
    return real_create_window(ex_style, class_name, title, style, x, y, width, height, parent, menu, instance, param);
}

/* Replaces the import FUNCTION from DLL in MODULE's import table. */
static void patch_import(HMODULE module, const char *dll, const char *function, void *replacement)
{
    BYTE *base = (BYTE *)module;
    IMAGE_NT_HEADERS *nt;
    IMAGE_DATA_DIRECTORY *directory;
    IMAGE_IMPORT_DESCRIPTOR *import;
    if (!module) return;
    nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    directory = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory->VirtualAddress) return;
    for (import = (IMAGE_IMPORT_DESCRIPTOR *)(base + directory->VirtualAddress); import->Name; import++)
    {
        IMAGE_THUNK_DATA *names, *slots;
        if (_stricmp((char *)base + import->Name, dll)) continue;
        names = (IMAGE_THUNK_DATA *)(base + (import->OriginalFirstThunk ? import->OriginalFirstThunk : import->FirstThunk));
        slots = (IMAGE_THUNK_DATA *)(base + import->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++)
        {
            DWORD old;
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            if (strcmp((char *)((IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData))->Name, function)) continue;
            if (VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), PAGE_READWRITE, &old))
            {
                slots->u1.Function = (ULONG_PTR)replacement;
                VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), old, &old);
            }
        }
    }
}

/* Makes the export FUNCTION of DLL jump to REPLACEMENT; for Wine stubs that
 * XP's modules look up with GetProcAddress. */
static void patch_export(const WCHAR *dll, const char *function, void *replacement)
{
    HMODULE module = LoadLibraryW(dll);
    BYTE *entry;
    DWORD old;
    if (!module || !(entry = (BYTE *)GetProcAddress(module, function))) return;
    if (!VirtualProtect(entry, 5, PAGE_EXECUTE_READWRITE, &old)) return;
    entry[0] = 0xe9; /* jmp rel32 */
    *(DWORD *)(entry + 1) = (DWORD)((BYTE *)replacement - (entry + 5));
    VirtualProtect(entry, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), entry, 5);
}

static void patch_module(HMODULE module)
{
    patch_import(module, "ntdll.dll", "NtQueryInformationProcess", query_process);
    patch_import(module, "kernel32.dll", "IsWow64Process", is_wow64_process);
    patch_import(module, "user32.dll", "DestroyReasons", destroy_reasons);
    patch_import(module, "advapi32.dll", "SaferRecordEventLogEntry", safer_record_event);
    patch_import(module, "user32.dll", "CreateWindowExW", create_window);
}

__declspec(dllexport) DWORD WINAPI xpcompat_init(void *unused)
{
    static const WCHAR *modules[] = {
        L"shell32.dll", L"shlwapi.dll", L"browseui.dll", L"shdocvw.dll", L"comctl32.dll", L"linkinfo.dll",
        L"msgina.dll", L"mydocs.dll", L"browselc.dll", L"shdoclc.dll",
    };
    (void)unused;
    real_create_window = (create_window_fn)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "CreateWindowExW");
    register_start_button();
    real_query_process = (query_process_fn)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
    patch_module(GetModuleHandleW(NULL));
    for (unsigned i = 0; i < sizeof(modules) / sizeof(*modules); i++)
        patch_module(LoadLibraryW(modules[i]));
    patch_export(L"setupapi.dll", "CM_Is_Dock_Station_Present", dock_station_present);
    patch_export(L"cfgmgr32.dll", "CM_Is_Dock_Station_Present", dock_station_present);
    patch_export(L"wuaueng.dll", "WUCheckForUpdatesAtShutdown", check_updates_at_shutdown);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance);
    return TRUE;
}
