/* Apply Wine's builtin local theme and verify actual public system fonts. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
static int compare(HTHEME theme, int id, const LOGFONTW *actual, int dpi)
{
    LOGFONTW expected={0},base={0};
    HRESULT hr=GetThemeSysFont(theme,id,&expected);
    DPI_AWARENESS_CONTEXT previous=SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE);
    HRESULT base_hr=GetThemeSysFont(theme,id,&base);
    SetThreadDpiAwarenessContext(previous);
    /* SPI stores the nominal 96 DPI font and scales it. Direct point-to-pixel
     * theme lookup may differ by one pixel after rounding at 144/192 DPI. */
    LONG applied_height=MulDiv(base.lfHeight,dpi,96);
    if(FAILED(hr)||FAILED(base_hr)||actual->lfHeight!=applied_height||actual->lfHeight>=0
       ||abs((int)(actual->lfHeight-expected.lfHeight))>1
       ||(id==TMT_CAPTIONFONT&&dpi<=144&&actual->lfHeight!=expected.lfHeight)
       ||actual->lfWeight!=expected.lfWeight||actual->lfItalic!=expected.lfItalic
       ||lstrcmpW(actual->lfFaceName,expected.lfFaceName))
    {
        fprintf(stderr,"metric %d: applied height=%ld weight=%ld, theme height=%ld weight=%ld hr=%lx\n",
                id,actual->lfHeight,actual->lfWeight,expected.lfHeight,expected.lfWeight,hr);
        return 1;
    }
    return 0;
}
static int read_fonts(LOGFONTW fonts[6])
{
    NONCLIENTMETRICSW n={0};n.cbSize=sizeof(n);
    if(!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(n),&n,0)
       ||!SystemParametersInfoW(SPI_GETICONTITLELOGFONT,sizeof(fonts[5]),&fonts[5],0))return 0;
    fonts[0]=n.lfCaptionFont;fonts[1]=n.lfSmCaptionFont;fonts[2]=n.lfMenuFont;
    fonts[3]=n.lfStatusFont;fonts[4]=n.lfMessageFont;
    return 1;
}
static int equal_font(const LOGFONTW *a,const LOGFONTW *b)
{
    return !memcmp(a,b,offsetof(LOGFONTW,lfFaceName))&&!lstrcmpW(a->lfFaceName,b->lfFaceName);
}
int wmain(int argc,WCHAR **argv)
{
    HMODULE module=LoadLibraryW(L"uxtheme.dll");
    HRESULT (WINAPI *open_theme)(LPCWSTR,LPCWSTR,LPCWSTR,HANDLE *,DWORD);
    HRESULT (WINAPI *close_theme)(HANDLE);
    HRESULT (WINAPI *apply_theme)(HANDLE,char *,HWND);
    int round,failed=0,dpi;
    HDC dc;
    LOGFONTW classic_before[6],classic_after[6];
    if(argc!=2||!module||!SetProcessDPIAware())return 2;
    dc=GetDC(NULL);dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(NULL,dc);
    if(dpi!=_wtoi(argv[1])){fprintf(stderr,"system DPI %d, expected %d\n",dpi,_wtoi(argv[1]));return 2;}
    open_theme=(void *)GetProcAddress(module,MAKEINTRESOURCEA(2));
    close_theme=(void *)GetProcAddress(module,MAKEINTRESOURCEA(3));
    apply_theme=(void *)GetProcAddress(module,MAKEINTRESOURCEA(4));
    if(!open_theme||!close_theme||!apply_theme)return 2;
    if(FAILED(apply_theme(NULL,NULL,NULL))||!read_fonts(classic_before))return 2;
    for(round=0;round<2;++round)
    {
        HANDLE file=NULL;
        HTHEME theme;
        NONCLIENTMETRICSW n={0};
        LOGFONTW icon={0};
        if(FAILED(apply_theme(NULL,NULL,NULL))||IsThemeActive())return 2;
        if(FAILED(open_theme(L"C:\\windows\\Resources\\Themes\\Luna\\luna.msstyles",L"NormalColor",L"NormalSize",&file,0)))return 2;
        HRESULT hr=apply_theme(file,NULL,NULL);close_theme(file);
        if(FAILED(hr)||!IsThemeActive())return 2;
        theme=OpenThemeData(NULL,L"Window");
        n.cbSize=sizeof(n);
        if(!theme||!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(n),&n,0)
           ||!SystemParametersInfoW(SPI_GETICONTITLELOGFONT,sizeof(icon),&icon,0))return 2;
        failed|=compare(theme,TMT_CAPTIONFONT,&n.lfCaptionFont,dpi);
        failed|=compare(theme,TMT_SMALLCAPTIONFONT,&n.lfSmCaptionFont,dpi);
        failed|=compare(theme,TMT_MENUFONT,&n.lfMenuFont,dpi);
        failed|=compare(theme,TMT_STATUSFONT,&n.lfStatusFont,dpi);
        failed|=compare(theme,TMT_MSGBOXFONT,&n.lfMessageFont,dpi);
        failed|=compare(theme,TMT_ICONTITLEFONT,&icon,dpi);
        CloseThemeData(theme);
    }
    if(FAILED(apply_theme(NULL,NULL,NULL)))return 2;
    if(!read_fonts(classic_after))return 2;
    for(int i=0;i<6;i++)if(!equal_font(&classic_before[i],&classic_after[i]))
    {fprintf(stderr,"Classic font %d changed during theme roundtrip\n",i);failed=1;}
    if(!failed)printf("PASS: classic/Luna roundtrip applies six system fonts at %d DPI\n",dpi);
    FreeLibrary(module);
    return failed ? 1 : 0;
}
