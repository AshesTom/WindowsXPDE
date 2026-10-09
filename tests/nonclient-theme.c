/* Public hit regions must match the theme's actual caption button assets. */
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>

int wmain(void)
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    HTHEME theme;
    SIZE image;
    RECT window;
    int x, y, first[3] = {-1, -1, -1}, last[3] = {0}, top = -1, bottom = 0;
    int roles[3] = {HTMINBUTTON, HTMAXBUTTON, HTCLOSE};
    int width, height, i, gap, padding;
    UINT dpi;
    if (!SetProcessDPIAware() || !IsThemeActive()) return 2;
    theme = OpenThemeData(NULL, L"Window");
    if (!theme || FAILED(GetThemePartSize(theme, NULL, WP_CLOSEBUTTON, CBS_NORMAL,
                                          NULL, TS_TRUE, &image))) return 3;
    wc.lpfnWndProc = DefWindowProcW;
    wc.lpszClassName = L"NC geometry probe";
    wc.hInstance = GetModuleHandleW(NULL);
    if (!RegisterClassW(&wc)) return 4;
    hwnd = CreateWindowW(wc.lpszClassName, L"Caption geometry", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                          80, 80, 400, 200, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) return 5;
    dpi = GetDpiForWindow(hwnd);
    width = MulDiv(image.cx, dpi, 96); height = MulDiv(image.cy, dpi, 96);
    gap = MulDiv(2, dpi, 96);
    padding = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION);
    GetWindowRect(hwnd, &window);
    for (y = 0; y < padding; y++)
        for (x = 0; x < window.right - window.left; x++)
        {
            LRESULT hit = SendMessageW(hwnd, WM_NCHITTEST, 0,
                                      MAKELPARAM(window.left + x, window.top + y));
            for (i = 0; i < 3; i++) if (hit == roles[i])
            {
                if (first[i] < 0 || x < first[i]) first[i] = x;
                if (x >= last[i]) last[i] = x + 1;
                if (i == 2)
                {
                    if (top < 0 || y < top) top = y;
                    if (y >= bottom) bottom = y + 1;
                }
            }
        }
    printf("dpi=%u source=%ldx%ld asset=%dx%d min=%d..%d max=%d..%d close=%d..%d y=%d..%d\n",
           dpi, image.cx, image.cy, width, height, first[0], last[0], first[1], last[1], first[2], last[2], top, bottom);
    for (i = 0; i < 3; i++) if (last[i] - first[i] != width) return 6;
    if (bottom - top != height || first[1] - last[0] != gap || first[2] - last[1] != gap) return 7;
    SetWindowLongW(hwnd, GWL_STYLE, GetWindowLongW(hwnd, GWL_STYLE) & ~WS_MAXIMIZEBOX);
    if (SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(window.left + (first[1] + last[1]) / 2,
                    window.top + (top + bottom) / 2)) != HTCAPTION) return 8;
    DestroyWindow(hwnd);
    CloseThemeData(theme);
    puts("PASS intrinsic caption button bounds, spacing and disabled hit region");
    return 0;
}
