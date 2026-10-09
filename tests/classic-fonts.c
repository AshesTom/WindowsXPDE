/* Public SPI verification of fresh Japanese Classic defaults and user fonts. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>
#include <wchar.h>
int wmain(int argc,WCHAR **argv)
{
    NONCLIENTMETRICSW n={0};LOGFONTW icon={0},*fonts[6];
    BOOL custom,set;
    if(argc!=2)return 2;
    set=!wcscmp(argv[1],L"--set-custom");
    custom=set||!wcscmp(argv[1],L"--custom");
    if(!custom&&wcscmp(argv[1],L"--japanese"))return 2;
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE);
    n.cbSize=sizeof(n);
    if(IsThemeActive()||!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(n),&n,0)
       ||!SystemParametersInfoW(SPI_GETICONTITLELOGFONT,sizeof(icon),&icon,0))return 2;
    fonts[0]=&n.lfCaptionFont;fonts[1]=&n.lfSmCaptionFont;fonts[2]=&n.lfMenuFont;
    fonts[3]=&n.lfStatusFont;fonts[4]=&n.lfMessageFont;fonts[5]=&icon;
    for(int i=0;i<6;i++)
    {
        LONG height=custom?-14-i:-12,weight=i<2?700:400;
        const WCHAR *face=custom?L"Tahoma":L"MS UI Gothic";
        if(set)
        {fonts[i]->lfHeight=height;fonts[i]->lfWeight=weight;fonts[i]->lfItalic=0;lstrcpyW(fonts[i]->lfFaceName,face);}
        else if(fonts[i]->lfHeight!=height||fonts[i]->lfWeight!=weight||fonts[i]->lfItalic||lstrcmpW(fonts[i]->lfFaceName,face))
        {fprintf(stderr,"FAIL Classic role %d: height=%ld weight=%ld italic=%u\n",i,fonts[i]->lfHeight,fonts[i]->lfWeight,fonts[i]->lfItalic);return 1;}
    }
    if(set&&(!SystemParametersInfoW(SPI_SETNONCLIENTMETRICS,sizeof(n),&n,SPIF_UPDATEINIFILE)
             ||!SystemParametersInfoW(SPI_SETICONTITLELOGFONT,sizeof(icon),&icon,SPIF_UPDATEINIFILE)))return 2;
    if(set)
    {
        HMODULE module=GetModuleHandleW(L"uxtheme.dll");HANDLE file=NULL;
        HRESULT (WINAPI *open_theme)(LPCWSTR,LPCWSTR,LPCWSTR,HANDLE *,DWORD)=(void *)GetProcAddress(module,MAKEINTRESOURCEA(2));
        HRESULT (WINAPI *close_theme)(HANDLE)=(void *)GetProcAddress(module,MAKEINTRESOURCEA(3));
        HRESULT (WINAPI *apply_theme)(HANDLE,char *,HWND)=(void *)GetProcAddress(module,MAKEINTRESOURCEA(4));
        if(!open_theme||!close_theme||!apply_theme
           ||FAILED(open_theme(L"C:\\windows\\Resources\\Themes\\Luna\\luna.msstyles",L"NormalColor",L"NormalSize",&file,0)))return 2;
        HRESULT hr=apply_theme(file,NULL,NULL);close_theme(file);
        if(FAILED(hr)||FAILED(apply_theme(NULL,NULL,NULL)))return 2;
    }
    printf("PASS: Classic six fonts %s; caption dimensions %dx%d\n",set?"custom SPI stored":custom?"custom preserved":"Japanese XP measured defaults",n.iCaptionWidth,n.iCaptionHeight);
    return 0;
}
