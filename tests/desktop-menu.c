/* Print the desktop background context menu as the shell builds it, with
 * public shell calls only. Usage: desktop-menu [COM1|FILE [--trace]].
 *
 * folder:   IShellFolder::CreateViewObject(IID_IContextMenu) of the desktop.
 * view:     the background IContextMenu of a desktop IShellView created in a
 *           window of this program (IShellView::GetItemObject).
 * handler:  each Directory\Background\shellex\ContextMenuHandlers entry on its
 *           own, into an empty menu and after two items of this program.
 * Every item prints its position, type, state, command id, submenu, verb and
 * text. Only menu texts and registry names are printed.
 *
 * With --trace as the second argument, the user32 menu calls that shell32
 * and shlwapi make while building the view menu and the menus with the two
 * probe items (folder+, handler+) are printed too, with each call's
 * MENUITEMINFO. They are seen by pointing those modules' user32 import
 * entries in this process at logging wrappers; the module files are not
 * changed. Under Wine, load the XP shell family as XP Explorer does
 * (WINEDLLOVERRIDES=shell32,shdocvw,browseui,shlwapi,comctl32,linkinfo,
 * browselc,shdoclc=n); otherwise Wine's own shell32 builds the menus. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define FIRST 1
#define LAST 0x7fff

static HANDLE out = INVALID_HANDLE_VALUE;
static HWND window;
static IShellView *active_view;

static void emit(const WCHAR *text)
{
    char utf8[4096];
    DWORD written;
    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8), NULL, NULL);
    if (length <= 1) return;
    if (out == INVALID_HANDLE_VALUE) fputs(utf8, stdout);
    else WriteFile(out, utf8, length - 1, &written, NULL);
}

static int trace_enabled, tracing;

static void trace(const WCHAR *format, ...)
{
    WCHAR line[1024];
    va_list args;
    if (!tracing) return;
    va_start(args, format);
    vswprintf(line, ARRAYSIZE(line), format, args);
    va_end(args);
    emit(line);
}

static const WCHAR *caller(void *address)
{
    static WCHAR name[MAX_PATH];
    HMODULE module;
    WCHAR *slash;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            address, &module) || !GetModuleFileNameW(module, name, MAX_PATH))
        return L"?";
    slash = wcsrchr(name, L'\\');
    return slash ? slash + 1 : name;
}

#define CALLER caller(__builtin_return_address(0))

static void trace_info(const WCHAR *what, const MENUITEMINFOW *info, BOOL ansi)
{
    WCHAR text[128] = L"";
    if (!info) return;
    if (info->fMask & (MIIM_TYPE | MIIM_STRING) && info->dwTypeData && !(info->fType & (MFT_SEPARATOR | MFT_BITMAP | MFT_OWNERDRAW)))
    {
        if (ansi) MultiByteToWideChar(CP_ACP, 0, (const char *)info->dwTypeData, -1, text, ARRAYSIZE(text));
        else lstrcpynW(text, info->dwTypeData, ARRAYSIZE(text));
    }
    trace(L"    %ls cb=%u mask=%x type=%x state=%x id=%x sub=%ls cch=%u data=%ls '%ls'\r\n", what, info->cbSize,
          info->fMask, info->fType, info->fState, info->wID, info->hSubMenu ? L"yes" : L"no", info->cch,
          info->dwItemData ? L"set" : L"0", text);
}

static int (WINAPI *real_GetMenuItemCount)(HMENU);
static int WINAPI hook_GetMenuItemCount(HMENU menu)
{
    int ret = real_GetMenuItemCount(menu);
    trace(L"  %ls GetMenuItemCount(%p) = %d\r\n", CALLER, menu, ret);
    return ret;
}

static BOOL (WINAPI *real_GetMenuItemInfoA)(HMENU, UINT, BOOL, MENUITEMINFOA *);
static BOOL WINAPI hook_GetMenuItemInfoA(HMENU menu, UINT item, BOOL position, MENUITEMINFOA *info)
{
    BOOL ret;
    trace(L"  %ls GetMenuItemInfoA(%p, %u, %d)\r\n", CALLER, menu, item, position);
    trace_info(L"in ", (MENUITEMINFOW *)info, TRUE);
    ret = real_GetMenuItemInfoA(menu, item, position, info);
    trace(L"    = %d err=%lu\r\n", ret, ret ? 0 : GetLastError());
    trace_info(L"out", (MENUITEMINFOW *)info, TRUE);
    return ret;
}

static BOOL (WINAPI *real_GetMenuItemInfoW)(HMENU, UINT, BOOL, MENUITEMINFOW *);
static BOOL WINAPI hook_GetMenuItemInfoW(HMENU menu, UINT item, BOOL position, MENUITEMINFOW *info)
{
    BOOL ret;
    trace(L"  %ls GetMenuItemInfoW(%p, %u, %d)\r\n", CALLER, menu, item, position);
    trace_info(L"in ", info, FALSE);
    ret = real_GetMenuItemInfoW(menu, item, position, info);
    trace(L"    = %d err=%lu\r\n", ret, ret ? 0 : GetLastError());
    trace_info(L"out", info, FALSE);
    return ret;
}

static BOOL (WINAPI *real_SetMenuItemInfoW)(HMENU, UINT, BOOL, const MENUITEMINFOW *);
static BOOL WINAPI hook_SetMenuItemInfoW(HMENU menu, UINT item, BOOL position, const MENUITEMINFOW *info)
{
    BOOL ret = real_SetMenuItemInfoW(menu, item, position, info);
    trace(L"  %ls SetMenuItemInfoW(%p, %u, %d) = %d\r\n", CALLER, menu, item, position, ret);
    trace_info(L"in ", info, FALSE);
    return ret;
}

static BOOL (WINAPI *real_InsertMenuItemW)(HMENU, UINT, BOOL, const MENUITEMINFOW *);
static BOOL WINAPI hook_InsertMenuItemW(HMENU menu, UINT item, BOOL position, const MENUITEMINFOW *info)
{
    BOOL ret = real_InsertMenuItemW(menu, item, position, info);
    trace(L"  %ls InsertMenuItemW(%p, %u, %d) = %d\r\n", CALLER, menu, item, position, ret);
    trace_info(L"in ", info, FALSE);
    return ret;
}

static BOOL (WINAPI *real_InsertMenuW)(HMENU, UINT, UINT, UINT_PTR, const WCHAR *);
static BOOL WINAPI hook_InsertMenuW(HMENU menu, UINT position, UINT flags, UINT_PTR id, const WCHAR *text)
{
    BOOL ret = real_InsertMenuW(menu, position, flags, id, text);
    trace(L"  %ls InsertMenuW(%p, %u, %x, %lx, '%ls') = %d\r\n", CALLER, menu, position, flags, (unsigned long)id,
          (flags & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW)) || !text ? L"" : text, ret);
    return ret;
}

static BOOL (WINAPI *real_DeleteMenu)(HMENU, UINT, UINT);
static BOOL WINAPI hook_DeleteMenu(HMENU menu, UINT position, UINT flags)
{
    BOOL ret = real_DeleteMenu(menu, position, flags);
    trace(L"  %ls DeleteMenu(%p, %x, %x) = %d\r\n", CALLER, menu, position, flags, ret);
    return ret;
}

static BOOL (WINAPI *real_RemoveMenu)(HMENU, UINT, UINT);
static BOOL WINAPI hook_RemoveMenu(HMENU menu, UINT position, UINT flags)
{
    BOOL ret = real_RemoveMenu(menu, position, flags);
    trace(L"  %ls RemoveMenu(%p, %x, %x) = %d\r\n", CALLER, menu, position, flags, ret);
    return ret;
}

static UINT (WINAPI *real_GetMenuItemID)(HMENU, int);
static UINT WINAPI hook_GetMenuItemID(HMENU menu, int position)
{
    UINT ret = real_GetMenuItemID(menu, position);
    trace(L"  %ls GetMenuItemID(%p, %d) = %x\r\n", CALLER, menu, position, ret);
    return ret;
}

static UINT (WINAPI *real_GetMenuState)(HMENU, UINT, UINT);
static UINT WINAPI hook_GetMenuState(HMENU menu, UINT id, UINT flags)
{
    UINT ret = real_GetMenuState(menu, id, flags);
    trace(L"  %ls GetMenuState(%p, %x, %x) = %x\r\n", CALLER, menu, id, flags, ret);
    return ret;
}

static UINT (WINAPI *real_GetMenuDefaultItem)(HMENU, UINT, UINT);
static UINT WINAPI hook_GetMenuDefaultItem(HMENU menu, UINT position, UINT flags)
{
    UINT ret = real_GetMenuDefaultItem(menu, position, flags);
    trace(L"  %ls GetMenuDefaultItem(%p, %u, %x) = %x\r\n", CALLER, menu, position, flags, ret);
    return ret;
}

static BOOL (WINAPI *real_SetMenuDefaultItem)(HMENU, UINT, UINT);
static BOOL WINAPI hook_SetMenuDefaultItem(HMENU menu, UINT item, UINT position)
{
    BOOL ret = real_SetMenuDefaultItem(menu, item, position);
    trace(L"  %ls SetMenuDefaultItem(%p, %x, %u) = %d\r\n", CALLER, menu, item, position, ret);
    return ret;
}

static HMENU (WINAPI *real_GetSubMenu)(HMENU, int);
static HMENU WINAPI hook_GetSubMenu(HMENU menu, int position)
{
    HMENU ret = real_GetSubMenu(menu, position);
    trace(L"  %ls GetSubMenu(%p, %d) = %p\r\n", CALLER, menu, position, ret);
    return ret;
}

static BOOL (WINAPI *real_ModifyMenuW)(HMENU, UINT, UINT, UINT_PTR, const WCHAR *);
static BOOL WINAPI hook_ModifyMenuW(HMENU menu, UINT position, UINT flags, UINT_PTR id, const WCHAR *text)
{
    BOOL ret = real_ModifyMenuW(menu, position, flags, id, text);
    trace(L"  %ls ModifyMenuW(%p, %x, %x, %lx) = %d\r\n", CALLER, menu, position, flags, (unsigned long)id, ret);
    return ret;
}

static DWORD (WINAPI *real_CheckMenuItem)(HMENU, UINT, UINT);
static DWORD WINAPI hook_CheckMenuItem(HMENU menu, UINT id, UINT flags)
{
    DWORD ret = real_CheckMenuItem(menu, id, flags);
    trace(L"  %ls CheckMenuItem(%p, %x, %x) = %lx\r\n", CALLER, menu, id, flags, ret);
    return ret;
}

static BOOL (WINAPI *real_EnableMenuItem)(HMENU, UINT, UINT);
static BOOL WINAPI hook_EnableMenuItem(HMENU menu, UINT id, UINT flags)
{
    BOOL ret = real_EnableMenuItem(menu, id, flags);
    trace(L"  %ls EnableMenuItem(%p, %x, %x) = %d\r\n", CALLER, menu, id, flags, ret);
    return ret;
}

static const struct { const char *name; void *hook; void **real; } hooks[] = {
#define HOOK(name) {#name, (void *)hook_##name, (void **)&real_##name}
    HOOK(GetMenuItemCount), HOOK(GetMenuItemInfoA), HOOK(GetMenuItemInfoW), HOOK(SetMenuItemInfoW),
    HOOK(InsertMenuItemW), HOOK(InsertMenuW), HOOK(DeleteMenu), HOOK(RemoveMenu), HOOK(GetMenuItemID),
    HOOK(GetMenuState), HOOK(GetMenuDefaultItem), HOOK(SetMenuDefaultItem), HOOK(GetSubMenu),
    HOOK(ModifyMenuW), HOOK(CheckMenuItem), HOOK(EnableMenuItem),
#undef HOOK
};

/* Point the module's USER32 imports of the functions above at the hooks. */
static void hook_imports(const WCHAR *name)
{
    BYTE *base = (BYTE *)GetModuleHandleW(name);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    IMAGE_NT_HEADERS *nt;
    IMAGE_IMPORT_DESCRIPTOR *import;
    WCHAR line[128];
    int count = 0;
    if (!base) return;
    nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    if (!nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress) return;
    import = (IMAGE_IMPORT_DESCRIPTOR *)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; import->Name; import++)
    {
        if (lstrcmpiA((char *)base + import->Name, "user32.dll")) continue;
        for (IMAGE_THUNK_DATA *thunk = (IMAGE_THUNK_DATA *)(base + import->FirstThunk); thunk->u1.Function; thunk++)
        {
            for (size_t i = 0; i < ARRAYSIZE(hooks); i++)
            {
                void *target = (void *)GetProcAddress(user32, hooks[i].name);
                DWORD old;
                if ((void *)thunk->u1.Function != target) continue;
                *hooks[i].real = target;
                VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), PAGE_READWRITE, &old);
                thunk->u1.Function = (ULONG_PTR)hooks[i].hook;
                VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), old, &old);
                count++;
            }
        }
    }
    swprintf(line, ARRAYSIZE(line), L"trace %ls imports=%d\r\n", name, count);
    emit(line);
}

static void dump(const WCHAR *tag, HMENU menu, IContextMenu *context, int depth)
{
    WCHAR line[1024];
    int count = GetMenuItemCount(menu);
    swprintf(line, 1024, L"%ls%*ls count=%d\r\n", tag, depth * 2, L"", count);
    emit(line);
    for (int i = 0; i < count; i++)
    {
        MENUITEMINFOW info = {.cbSize = sizeof(info)};
        WCHAR text[256] = L"", verb[128] = L"";
        info.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU | MIIM_STRING | MIIM_DATA;
        info.dwTypeData = text;
        info.cch = ARRAYSIZE(text);
        if (!GetMenuItemInfoW(menu, i, TRUE, &info))
        {
            swprintf(line, 1024, L"%ls%*ls  %2d failed %lu\r\n", tag, depth * 2, L"", i, GetLastError());
            emit(line);
            continue;
        }
        if (context && !(info.fType & MFT_SEPARATOR) && info.wID >= FIRST && info.wID <= LAST)
        {
            if (FAILED(IContextMenu_GetCommandString(context, info.wID - FIRST, GCS_VERBW, NULL,
                                                     (char *)verb, ARRAYSIZE(verb))))
                verb[0] = 0;
        }
        swprintf(line, 1024, L"%ls%*ls  %2d type=%04x state=%04x id=%08lx sub=%ls data=%lx verb='%ls' '%ls'\r\n",
                 tag, depth * 2, L"", i, info.fType, info.fState, (unsigned long)info.wID,
                 info.hSubMenu ? L"yes" : L"no", (unsigned long)info.dwItemData, verb,
                 (info.fType & MFT_SEPARATOR) ? L"-" : text);
        emit(line);
        if (info.hSubMenu && depth < 2)
        {
            /* Handlers fill their submenus when the menu opens. */
            IContextMenu2 *context2;
            if (context && SUCCEEDED(IContextMenu_QueryInterface(context, &IID_IContextMenu2, (void **)&context2)))
            {
                IContextMenu2_HandleMenuMsg(context2, WM_INITMENUPOPUP, (WPARAM)info.hSubMenu, i);
                IContextMenu2_Release(context2);
            }
            dump(tag, info.hSubMenu, context, depth + 1);
        }
    }
}

static void query(const WCHAR *tag, IContextMenu *context, int prefill, UINT flags)
{
    HMENU menu = CreatePopupMenu();
    WCHAR line[256];
    HRESULT hr;
    if (prefill)
    {
        AppendMenuW(menu, MF_STRING, 0x7f01, L"probe-A");
        AppendMenuW(menu, MF_STRING, 0x7f02, L"probe-B");
    }
    tracing = trace_enabled && (prefill || !lstrcmpW(tag, L"view"));
    trace(L"%ls trace begin\r\n", tag);
    hr = IContextMenu_QueryContextMenu(context, menu, prefill ? 2 : 0, FIRST, prefill ? 0x7eff : LAST, flags);
    trace(L"%ls trace end\r\n", tag);
    tracing = 0;
    swprintf(line, 256, L"%ls flags=%x hr=%08lx\r\n", tag, flags, (unsigned long)hr);
    emit(line);
    dump(tag, menu, context, 0);
    DestroyMenu(menu);
}

/* A minimal browser that only owns the window the view is created in. */
static HRESULT WINAPI browser_query(IShellBrowser *self, REFIID iid, void **object)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IOleWindow) || IsEqualIID(iid, &IID_IShellBrowser))
    {
        *object = self;
        return S_OK;
    }
    *object = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI browser_addref(IShellBrowser *self) { (void)self; return 2; }
static ULONG WINAPI browser_release(IShellBrowser *self) { (void)self; return 1; }
static HRESULT WINAPI browser_window(IShellBrowser *self, HWND *hwnd) { (void)self; *hwnd = window; return S_OK; }
static HRESULT WINAPI browser_help(IShellBrowser *self, BOOL mode) { (void)self; (void)mode; return E_NOTIMPL; }
static HRESULT WINAPI browser_insert_menus(IShellBrowser *self, HMENU menu, LPOLEMENUGROUPWIDTHS widths)
{ (void)self; (void)menu; (void)widths; return E_NOTIMPL; }
static HRESULT WINAPI browser_set_menu(IShellBrowser *self, HMENU menu, HOLEMENU shared, HWND active)
{ (void)self; (void)menu; (void)shared; (void)active; return E_NOTIMPL; }
static HRESULT WINAPI browser_remove_menus(IShellBrowser *self, HMENU menu) { (void)self; (void)menu; return E_NOTIMPL; }
static HRESULT WINAPI browser_status(IShellBrowser *self, LPCWSTR text) { (void)self; (void)text; return S_OK; }
static HRESULT WINAPI browser_modeless(IShellBrowser *self, BOOL enable) { (void)self; (void)enable; return S_OK; }
static HRESULT WINAPI browser_accelerator(IShellBrowser *self, MSG *msg, WORD id)
{ (void)self; (void)msg; (void)id; return S_FALSE; }
static HRESULT WINAPI browser_browse(IShellBrowser *self, PCUIDLIST_RELATIVE pidl, UINT flags)
{ (void)self; (void)pidl; (void)flags; return E_NOTIMPL; }
static HRESULT WINAPI browser_stream(IShellBrowser *self, DWORD mode, IStream **stream)
{ (void)self; (void)mode; *stream = NULL; return E_NOTIMPL; }
static HRESULT WINAPI browser_control(IShellBrowser *self, UINT id, HWND *hwnd)
{ (void)self; (void)id; *hwnd = NULL; return E_NOTIMPL; }
static HRESULT WINAPI browser_send(IShellBrowser *self, UINT id, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT *result)
{ (void)self; (void)id; (void)msg; (void)wparam; (void)lparam; if (result) *result = 0; return E_NOTIMPL; }
static HRESULT WINAPI browser_active(IShellBrowser *self, IShellView **view)
{
    (void)self;
    *view = active_view;
    if (!active_view) return E_FAIL;
    IShellView_AddRef(active_view);
    return S_OK;
}
static HRESULT WINAPI browser_view_active(IShellBrowser *self, IShellView *view) { (void)self; (void)view; return S_OK; }
static HRESULT WINAPI browser_toolbar(IShellBrowser *self, LPTBBUTTONSB buttons, UINT count, UINT flags)
{ (void)self; (void)buttons; (void)count; (void)flags; return E_NOTIMPL; }

static IShellBrowserVtbl browser_vtbl = {
    browser_query, browser_addref, browser_release, browser_window, browser_help,
    browser_insert_menus, browser_set_menu, browser_remove_menus, browser_status, browser_modeless,
    browser_accelerator, browser_browse, browser_stream, browser_control, browser_send,
    browser_active, browser_view_active, browser_toolbar,
};
static IShellBrowser browser = {&browser_vtbl};

static void folder_menu(IShellFolder *desktop)
{
    IContextMenu *context;
    HRESULT hr = IShellFolder_CreateViewObject(desktop, window, &IID_IContextMenu, (void **)&context);
    WCHAR line[128];
    swprintf(line, 128, L"folder CreateViewObject hr=%08lx\r\n", (unsigned long)hr);
    emit(line);
    if (FAILED(hr)) return;
    query(L"folder", context, 0, CMF_NORMAL);
    query(L"folder+", context, 1, CMF_NORMAL);
    IContextMenu_Release(context);
}

static void view_menu(IShellFolder *desktop)
{
    FOLDERSETTINGS settings = {.ViewMode = FVM_ICON, .fFlags = FWF_DESKTOP | FWF_NOCLIENTEDGE};
    RECT rect = {0, 0, 400, 300};
    IContextMenu *context;
    HWND view_window = NULL;
    WCHAR line[128];
    HRESULT hr = IShellFolder_CreateViewObject(desktop, window, &IID_IShellView, (void **)&active_view);
    swprintf(line, 128, L"view CreateViewObject hr=%08lx\r\n", (unsigned long)hr);
    emit(line);
    if (FAILED(hr)) return;
    hr = IShellView_CreateViewWindow(active_view, NULL, &settings, &browser, &rect, &view_window);
    swprintf(line, 128, L"view CreateViewWindow hr=%08lx window=%ls\r\n", (unsigned long)hr, view_window ? L"yes" : L"no");
    emit(line);
    if (SUCCEEDED(hr))
    {
        IShellView_UIActivate(active_view, SVUIA_ACTIVATE_NOFOCUS);
        hr = IShellView_GetItemObject(active_view, SVGIO_BACKGROUND, &IID_IContextMenu, (void **)&context);
        swprintf(line, 128, L"view GetItemObject hr=%08lx\r\n", (unsigned long)hr);
        emit(line);
        if (SUCCEEDED(hr))
        {
            query(L"view", context, 0, CMF_NORMAL);
            IContextMenu_Release(context);
        }
        IShellView_UIActivate(active_view, SVUIA_DEACTIVATE);
        IShellView_DestroyViewWindow(active_view);
    }
    IShellView_Release(active_view);
    active_view = NULL;
}

static void handler_menus(void)
{
    static const WCHAR path[] = L"Directory\\Background\\shellex\\ContextMenuHandlers";
    LPITEMIDLIST pidl;
    HKEY handlers, background;
    WCHAR name[256], line[512];
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, path, 0, KEY_READ, &handlers))
    {
        emit(L"handler key missing\r\n");
        return;
    }
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, L"Directory\\Background", 0, KEY_READ, &background)) background = NULL;
    SHGetSpecialFolderLocation(NULL, CSIDL_DESKTOP, &pidl);
    for (DWORD index = 0;; index++)
    {
        DWORD length = ARRAYSIZE(name);
        WCHAR value[64] = L"";
        DWORD size = sizeof(value);
        CLSID clsid;
        IShellExtInit *init;
        IContextMenu *context;
        HRESULT hr;
        if (RegEnumKeyExW(handlers, index, name, &length, NULL, NULL, NULL, NULL)) break;
        HKEY entry;
        if (!RegOpenKeyExW(handlers, name, 0, KEY_READ, &entry))
        {
            if (RegQueryValueExW(entry, NULL, NULL, NULL, (BYTE *)value, &size)) value[0] = 0;
            RegCloseKey(entry);
        }
        if (!value[0]) lstrcpynW(value, name, ARRAYSIZE(value));
        swprintf(line, 512, L"handler '%ls' clsid=%ls\r\n", name, value);
        emit(line);
        if (FAILED(CLSIDFromString(value, &clsid))) continue;
        hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IShellExtInit, (void **)&init);
        if (SUCCEEDED(hr))
        {
            hr = IShellExtInit_Initialize(init, pidl, NULL, background);
            swprintf(line, 512, L"handler '%ls' Initialize hr=%08lx\r\n", name, (unsigned long)hr);
            emit(line);
            hr = IShellExtInit_QueryInterface(init, &IID_IContextMenu, (void **)&context);
            IShellExtInit_Release(init);
        }
        else
            hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IContextMenu, (void **)&context);
        swprintf(line, 512, L"handler '%ls' IContextMenu hr=%08lx\r\n", name, (unsigned long)hr);
        emit(line);
        if (FAILED(hr)) continue;
        swprintf(line, 512, L"handler:%ls", name);
        query(line, context, 0, CMF_NORMAL);
        swprintf(line, 512, L"handler+:%ls", name);
        query(line, context, 1, CMF_NORMAL);
        IContextMenu_Release(context);
    }
    if (background) RegCloseKey(background);
    RegCloseKey(handlers);
    CoTaskMemFree(pidl);
}

int wmain(int argc, WCHAR **argv)
{
    IShellFolder *desktop;
    if (argc > 1)
    {
        int serial = !lstrcmpiW(argv[1], L"COM1");
        out = CreateFileW(argv[1], GENERIC_WRITE, 0, NULL, serial ? OPEN_EXISTING : CREATE_ALWAYS, 0, NULL);
        if (serial && out != INVALID_HANDLE_VALUE)
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
    emit(L"DESKTOPMENU\r\n");
    OleInitialize(NULL);
    if (argc > 2 && !lstrcmpiW(argv[2], L"--trace"))
    {
        trace_enabled = 1;
        hook_imports(L"shell32.dll");
        hook_imports(L"shlwapi.dll");
    }
    window = CreateWindowExW(0, L"STATIC", L"desktop-menu", WS_OVERLAPPEDWINDOW, 0, 0, 400, 300,
                             NULL, NULL, NULL, NULL);
    if (SUCCEEDED(SHGetDesktopFolder(&desktop)))
    {
        folder_menu(desktop);
        view_menu(desktop);
        IShellFolder_Release(desktop);
    }
    handler_menus();
    DestroyWindow(window);
    OleUninitialize();
    emit(L"DESKTOPMENU complete\r\n");
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
