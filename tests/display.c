/* Public API checks for the configured theme and system DPI. */
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>
int main(int argc, char **argv)
{
    HDC dc;
    int dpi, expected, active;
    WCHAR file[MAX_PATH] = {0}, color[64] = {0}, size[64] = {0};
    if (argc != 3) return 2;
    expected = atoi(argv[1]);
    active = atoi(argv[2]);
    if (!SetProcessDPIAware()) return 3;
    dc = GetDC(NULL);
    dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    printf("system-dpi=%d theme-active=%d icon=%dx%d\n", dpi, IsThemeActive(),
           GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    if (dpi != expected || !!IsThemeActive() != !!active) return 4;
    if (active)
    {
        HTHEME theme;
        SIZE part = {0};
        HRESULT hr = GetCurrentThemeName(file, MAX_PATH, color, 64, size, 64);
        wprintf(L"theme=%ls color=%ls size=%ls\n", file, color, size);
        if (FAILED(hr) || !wcsstr(file, L"luna.msstyles")) return 5;
        theme = OpenThemeData(NULL, L"Taskbar");
        hr = GetThemePartSize(theme, NULL, 1, 1, NULL, TS_TRUE, &part);
        CloseThemeData(theme);
        if (FAILED(hr) || part.cx <= 0 || part.cy <= 0) return 6;
    }
    return 0;
}
