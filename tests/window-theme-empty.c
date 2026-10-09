/* SetWindowTheme(hwnd, L"", L"") must turn visual styles off for one window. */
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>

static int theme_changed;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_THEMECHANGED) theme_changed++;
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int wmain(void)
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    HTHEME theme;
    HRESULT hr;
    int failures = 0;

    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"WindowThemeEmpty";
    if (!RegisterClassW(&wc)) return 2;
    hwnd = CreateWindowW(wc.lpszClassName, L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) return 2;
    if (!IsThemeActive())
    {
        puts("SKIP theme inactive");
        return 0;
    }

    theme = OpenThemeData(hwnd, L"Toolbar");
    printf("before theme=%d\n", theme != NULL);
    failures += !theme;
    if (theme) CloseThemeData(theme);

    hr = SetWindowTheme(hwnd, L"", L"");
    theme = OpenThemeData(hwnd, L"Toolbar");
    printf("empty hr=%08lx changed=%d theme=%d window_theme=%d\n", (unsigned long)hr,
           theme_changed, theme != NULL, GetWindowTheme(hwnd) != NULL);
    failures += hr != S_OK || theme_changed != 1 || theme || GetWindowTheme(hwnd);
    if (theme) CloseThemeData(theme);

    hr = SetWindowTheme(hwnd, NULL, NULL);
    theme = OpenThemeData(hwnd, L"Toolbar");
    printf("restored hr=%08lx changed=%d theme=%d\n", (unsigned long)hr, theme_changed, theme != NULL);
    failures += hr != S_OK || theme_changed != 2 || !theme;
    if (theme) CloseThemeData(theme);

    hr = SetWindowTheme(hwnd, L"", NULL);
    theme = OpenThemeData(hwnd, L"Toolbar");
    printf("empty-subapp hr=%08lx theme=%d\n", (unsigned long)hr, theme != NULL);
    failures += hr != S_OK || !theme;
    if (theme) CloseThemeData(theme);

    DestroyWindow(hwnd);
    puts(failures ? "FAIL" : "PASS");
    return failures != 0;
}
