/* Apply themes through our Wine builtin implementation, never native uxtheme. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>
#include <wchar.h>

int wmain(int argc, WCHAR **argv)
{
    HMODULE module;
    HRESULT (WINAPI *open_theme)(LPCWSTR, LPCWSTR, LPCWSTR, HANDLE *, DWORD);
    HRESULT (WINAPI *close_theme)(HANDLE);
    HRESULT (WINAPI *apply_theme)(HANDLE, char *, HWND);
    HANDLE file = NULL;
    HRESULT hr;
    BOOL luna;
    if (argc != 2 || (wcscmp(argv[1], L"classic") && wcscmp(argv[1], L"luna"))) return 2;
    luna = !wcscmp(argv[1], L"luna");
    module = LoadLibraryW(L"uxtheme.dll");
    if (!module) return 1;
    /* Ordinals and signatures come from Wine 10.0's own uxtheme/system.c. */
    open_theme = (void *)GetProcAddress(module, MAKEINTRESOURCEA(2));
    close_theme = (void *)GetProcAddress(module, MAKEINTRESOURCEA(3));
    apply_theme = (void *)GetProcAddress(module, MAKEINTRESOURCEA(4));
    if (!open_theme || !close_theme || !apply_theme) return 1;
    /* Restore saved classic metrics first; a fresh theme then applies all metrics. */
    hr = apply_theme(NULL, NULL, NULL);
    if (FAILED(hr)) goto failed;
    if (luna)
    {
        hr = open_theme(L"C:\\windows\\Resources\\Themes\\Luna\\luna.msstyles",
                        L"NormalColor", L"NormalSize", &file, 0);
        if (SUCCEEDED(hr)) hr = apply_theme(file, NULL, NULL);
        if (file) close_theme(file);
        if (FAILED(hr)) goto failed;
    }
    if (!!IsThemeActive() != luna) return 1;
    printf("Theme applied: %s\n", luna ? "luna" : "classic");
    FreeLibrary(module);
    return 0;
failed:
    fprintf(stderr, "Wine theme application failed: %#lx\n", (unsigned long)hr);
    FreeLibrary(module);
    return 1;
}
