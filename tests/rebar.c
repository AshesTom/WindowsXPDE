/* Public native-control layout regression; the DLL stays user supplied. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

typedef BOOL (WINAPI *init_fn)(const INITCOMMONCONTROLSEX *);
int wmain(void)
{
    HMODULE module = LoadLibraryW(L"C:\\xp\\comctl32.dll");
    init_fn init;
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_COOL_CLASSES};
    HWND parent;
    int index, result = 0;
    if (!module) return 2;
    init = (init_fn)(void *)GetProcAddress(module, "InitCommonControlsEx");
    if (!init || !init(&controls)) return 3;
    parent = CreateWindowW(L"Static", L"rebar parent", WS_OVERLAPPEDWINDOW,
                           100, 100, 600, 300, NULL, NULL, NULL, NULL);
    if (!parent) return 4;
    for (index = 0; index < 2; ++index)
    {
        DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | CCS_NOPARENTALIGN | CCS_NODIVIDER;
        RECT r;
        HWND rebar, child;
        REBARBANDINFOW band = {0};
        int y = index ? 12 : 0;
        if (index) style |= CCS_NOMOVEY;
        rebar = CreateWindowExW(WS_EX_TOOLWINDOW, REBARCLASSNAMEW, NULL, style,
                                0, y, 400, 36, parent, (HMENU)(UINT_PTR)(100 + index), module, NULL);
        child = CreateWindowW(L"Static", L"band", WS_CHILD | WS_VISIBLE, 0, 0, 200, 24,
                              rebar, NULL, NULL, NULL);
        band.cbSize = REBARBANDINFO_V6_SIZE;
        band.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE;
        band.hwndChild = child;
        band.cxMinChild = 200;
        band.cyMinChild = 24;
        band.cx = 300;
        if (!rebar || !child || !SendMessageW(rebar, RB_INSERTBANDW, 0, (LPARAM)&band))
        {
            fprintf(stderr, "failed to create band, layout %d\n", index);
            result = 1;
        }
        GetWindowRect(rebar, &r);
        MapWindowPoints(NULL, parent, (POINT *)&r, 2);
        if (r.top != y || r.bottom <= r.top)
        {
            fprintf(stderr, "layout %d: unexpected rebar rectangle %ld,%ld,%ld,%ld\n",
                    index, r.left, r.top, r.right, r.bottom);
            result = 1;
        }
        DestroyWindow(rebar);
    }
    DestroyWindow(parent);
    FreeLibrary(module);
    if (!result) puts("PASS: native rebar default top alignment, explicit no-move alignment, band layout");
    return result;
}
