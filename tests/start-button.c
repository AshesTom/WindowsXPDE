/* Exercise Wine's isolated themed Start control without native Explorer code. */
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    WNDCLASSW wc = {0};
    HWND parent, button;
    WCHAR name[64];
    SIZE ideal = {0}, image = {0};
    HTHEME theme;
    HIMAGELIST native_images = NULL;
    HMODULE native_controls = NULL;
    BOOL enabled = argc >= 2 && !wcscmp(argv[1], L"enabled");
    wc.lpfnWndProc = DefWindowProcW;
    wc.lpszClassName = L"Shell_TrayWnd";
    wc.hInstance = GetModuleHandleW(NULL);
    if (!RegisterClassW(&wc)) return 2;
    parent = CreateWindowW(wc.lpszClassName, L"Start control probe", WS_OVERLAPPEDWINDOW,
                           50, 50, 400, 200, NULL, NULL, wc.hInstance, NULL);
    button = CreateWindowW(L"Button", L"Start", WS_CHILD | WS_VISIBLE,
                           0, 0, 99, 33, parent, NULL, wc.hInstance, NULL);
    if (!parent || !button) return 3;
    GetClassNameW(button, name, ARRAY_SIZE(name));
    wprintf(L"class=%ls isolated=%d\n", name, enabled);
    if (wcscmp(name, enabled ? L"Win4WineStartButton" : L"Button")) return 4;
    if (enabled && argc >= 3 && !wcscmp(argv[2], L"native-images"))
    {
        HIMAGELIST (WINAPI *create_images)(int, int, UINT, int, int);
        int (WINAPI *add_image)(HIMAGELIST, HBITMAP, HBITMAP);
        HDC screen = GetDC(NULL), dc = CreateCompatibleDC(screen);
        HBITMAP image = CreateCompatibleBitmap(screen, 16, 16);
        HGDIOBJ old = SelectObject(dc, image);
        HBRUSH red = CreateSolidBrush(RGB(255, 0, 0));
        RECT r = {0, 0, 16, 16};
        BUTTON_IMAGELIST list = {0};
        native_controls = LoadLibraryW(L"C:\\xp\\comctl32.dll");
        if (!native_controls) return 8;
        create_images = (void *)GetProcAddress(native_controls, "ImageList_Create");
        add_image = (void *)GetProcAddress(native_controls, "ImageList_Add");
        if (!create_images || !add_image) return 9;
        FillRect(dc, &r, red);
        SelectObject(dc, old);
        native_images = create_images(16, 16, ILC_COLOR32, 1, 1);
        if (!native_images || add_image(native_images, image, NULL) != 0) return 10;
        list.himl = native_images;
        list.margin.right = 4;
        list.uAlign = BUTTON_IMAGELIST_ALIGN_LEFT;
        if (!SendMessageW(button, BCM_SETIMAGELIST, 0, (LPARAM)&list)) return 11;
        DeleteObject(red); DeleteObject(image); DeleteDC(dc); ReleaseDC(NULL, screen);
    }
    if (enabled && IsThemeActive())
    {
        SetWindowTheme(button, L"Start", NULL);
        SendMessageW(button, WM_THEMECHANGED, 0, 0);
        theme = GetWindowTheme(button);
        if (!theme || FAILED(GetThemePartSize(theme, NULL, BP_PUSHBUTTON, PBS_NORMAL,
                                              NULL, TS_TRUE, &image))) return 5;
        if (!SendMessageW(button, BCM_GETIDEALSIZE, 0, (LPARAM)&ideal)
            || ideal.cx < image.cx || ideal.cy < image.cy) return 6;
        printf("ideal=%ld,%ld image=%ld,%ld\n", ideal.cx, ideal.cy, image.cx, image.cy);
    }
    if (enabled)
    {
        HDC screen = GetDC(NULL), a = CreateCompatibleDC(screen), b = CreateCompatibleDC(screen);
        HBITMAP ba = CreateCompatibleBitmap(screen, 99, 33), bb = CreateCompatibleBitmap(screen, 130, 60);
        HGDIOBJ old_a = SelectObject(a, ba), old_b = SelectObject(b, bb);
        RECT fill = {0, 0, 130, 60};
        int x, y, red_pixels = 0;
        FillRect(a, &fill, GetSysColorBrush(COLOR_WINDOW));
        FillRect(b, &fill, GetSysColorBrush(COLOR_WINDOW));
        SetViewportOrgEx(b, 13, 9, NULL);
        SendMessageW(button, WM_PRINTCLIENT, (WPARAM)a, PRF_CLIENT | PRF_ERASEBKGND);
        SendMessageW(button, WM_PRINTCLIENT, (WPARAM)b, PRF_CLIENT | PRF_ERASEBKGND);
        for (y = 0; y < 33; y++) for (x = 0; x < 99; x++)
        {
            COLORREF pixel = GetPixel(a, x, y);
            if (pixel != GetPixel(b, x, y)) return 7;
            if (pixel == RGB(255, 0, 0)) red_pixels++;
        }
        if (native_images && red_pixels < 64) return 12;
        SelectObject(a, old_a); SelectObject(b, old_b);
        DeleteObject(ba); DeleteObject(bb); DeleteDC(a); DeleteDC(b); ReleaseDC(NULL, screen);
        puts("PASS translated Start Button print DC");
    }
    DestroyWindow(parent);
    if (native_images)
    {
        BOOL (WINAPI *destroy_images)(HIMAGELIST) = (void *)GetProcAddress(native_controls, "ImageList_Destroy");
        if (destroy_images) destroy_images(native_images);
        puts("PASS native opaque image list on Wine Start Button");
    }
    puts("PASS isolated Start Button");
    return 0;
}
