/* Apply themes through our Wine builtin implementation, never native uxtheme. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>
#include <wchar.h>

static BOOL initialize_classic_fonts(void)
{
    const WCHAR *names[]={L"CaptionFont",L"SmallCaptionFont",L"MenuFont",L"StatusFont",L"MessageFont",L"IconFont"};
    LOGFONTW fonts[6];
    NONCLIENTMETRICSW metrics={0};
    DPI_AWARENESS_CONTEXT previous;
    HKEY key;
    BOOL result;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Win4Wine\\ThemeProfiles\\XPClassic",0,KEY_READ,&key))return FALSE;
    for(unsigned i=0;i<6;i++)
    {
        DWORD type,bytes=sizeof(fonts[i]);
        if(RegQueryValueExW(key,names[i],NULL,&type,(BYTE*)&fonts[i],&bytes)||type!=REG_BINARY||bytes!=sizeof(fonts[i])
           ||fonts[i].lfHeight>=0||fonts[i].lfWeight<0||fonts[i].lfWeight>1000
           ||!fonts[i].lfFaceName[0]||fonts[i].lfFaceName[LF_FACESIZE-1])
        {RegCloseKey(key);return FALSE;}
    }
    RegCloseKey(key);
    previous=SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE);
    metrics.cbSize=sizeof(metrics);
    result=SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0);
    if(result)
    {
        metrics.lfCaptionFont=fonts[0];metrics.lfSmCaptionFont=fonts[1];metrics.lfMenuFont=fonts[2];
        metrics.lfStatusFont=fonts[3];metrics.lfMessageFont=fonts[4];
        result=SystemParametersInfoW(SPI_SETNONCLIENTMETRICS,sizeof(metrics),&metrics,SPIF_UPDATEINIFILE)
               &&SystemParametersInfoW(SPI_SETICONTITLELOGFONT,sizeof(fonts[5]),&fonts[5],SPIF_UPDATEINIFILE);
    }
    SetThreadDpiAwarenessContext(previous);
    return result;
}

int wmain(int argc, WCHAR **argv)
{
    HMODULE module;
    HRESULT (WINAPI *open_theme)(LPCWSTR, LPCWSTR, LPCWSTR, HANDLE *, DWORD);
    HRESULT (WINAPI *close_theme)(HANDLE);
    HRESULT (WINAPI *apply_theme)(HANDLE, char *, HWND);
    HANDLE file = NULL;
    HRESULT hr;
    BOOL luna,initialize;
    /* "msstyles PATH COLOR SIZE" applies another visual style (ReactOS's themes). */
    const WCHAR *path = L"C:\\windows\\Resources\\Themes\\Luna\\luna.msstyles";
    const WCHAR *color = L"NormalColor", *size = L"NormalSize";
    if (argc == 5 && !wcscmp(argv[1], L"msstyles"))
    {
        path = argv[2];
        color = argv[3];
        size = argv[4];
        argv[1] = (WCHAR *)L"luna";
        argc = 2;
    }
    if (argc != 2 || (wcscmp(argv[1], L"classic") && wcscmp(argv[1], L"luna") && wcscmp(argv[1],L"classic-initialize"))) return 2;
    luna = !wcscmp(argv[1], L"luna");
    initialize = !wcscmp(argv[1], L"classic-initialize");
    module = LoadLibraryW(L"uxtheme.dll");
    if (!module) return 1;
    /* Ordinals and signatures come from Wine's own uxtheme/system.c (the same in 10.0 and 11.0). */
    open_theme = (void *)GetProcAddress(module, MAKEINTRESOURCEA(2));
    close_theme = (void *)GetProcAddress(module, MAKEINTRESOURCEA(3));
    apply_theme = (void *)GetProcAddress(module, MAKEINTRESOURCEA(4));
    if (!open_theme || !close_theme || !apply_theme) return 1;
    /* Restore saved classic metrics first; a fresh theme then applies all metrics. */
    hr = apply_theme(NULL, NULL, NULL);
    if (FAILED(hr)) goto failed;
    if(initialize&&!initialize_classic_fonts())
    {hr=E_FAIL;goto failed;}
    if (luna||initialize)
    {
        hr = open_theme(path, color, size, &file, 0);
        if (SUCCEEDED(hr)) hr = apply_theme(file, NULL, NULL);
        if (file) close_theme(file);
        if (FAILED(hr)) goto failed;
    }
    /* Wine captures the new classic SPI baseline on the first Luna apply.
     * Restore it in this process, then later processes can read it normally. */
    if(initialize)
    {
        hr=apply_theme(NULL,NULL,NULL);
        if(FAILED(hr))goto failed;
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
