/* Public actual tray-clock mask comparison, Japanese Luna profile at 96 DPI. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
static HWND clock_window;
static int failures;
static BOOL CALLBACK child(HWND hwnd,LPARAM unused)
{
    WCHAR name[80];(void)unused;GetClassNameW(hwnd,name,80);
    if(!lstrcmpW(name,L"TrayClockWClass"))clock_window=hwnd;
    return TRUE;
}
static void measure(HDC actual,HDC drawn,int width,int height,const char *label)
{
    int x,y,count=0,actual_count=0,delta=0,l=9999,t=9999,r=-1,b=-1;
    for(y=0;y<height;y++)for(x=0;x<width;x++)
    {
        BOOL a=GetPixel(actual,x,y)==RGB(255,255,255);
        BOOL d=GetPixel(drawn,x,y)==RGB(255,255,255);
        actual_count+=a;delta+=a!=d;
        if(d){count++;if(x<l)l=x;if(y<t)t=y;if(x>r)r=x;if(y>b)b=y;}
    }
    if(!count || delta)failures++;
    printf("%s actual-white=%d drawn-white=%d mask-delta=%d drawn-ink=%d,%d..%d,%d\n",label,actual_count,count,delta,l,t,r+1,b+1);
}
int wmain(int argc,WCHAR **argv)
{
    HWND owner,tray;HTHEME theme;HDC dc,drawn;HBITMAP bitmap;HGDIOBJ old;
    WCHAR text[128],after[128];RECT rect;LOGFONTW font={0};HFONT explicit_font,previous;
    if(argc!=2||lstrcmpW(argv[1],L"--isolated-session"))return 2;
    HDC screen=GetDC(NULL);int dpi=GetDeviceCaps(screen,LOGPIXELSY);ReleaseDC(NULL,screen);
    if(dpi!=96||!IsThemeActive())return 2;
    tray=FindWindowW(L"Shell_TrayWnd",NULL);if(!tray)return 2;
    EnumChildWindows(tray,child,0);if(!clock_window)return 2;
    owner=CreateWindowW(L"STATIC",L"Clock pixels",WS_OVERLAPPEDWINDOW|WS_VISIBLE,40,40,260,160,NULL,NULL,GetModuleHandleW(NULL),NULL);
    if(!owner)return 2;
    UpdateWindow(owner);
    theme=OpenThemeData(owner,L"TrayNotify::Clock");if(!theme)return 2;
    RedrawWindow(clock_window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_UPDATENOW);
    GetWindowTextW(clock_window,text,128);GetClientRect(clock_window,&rect);
    dc=GetDC(clock_window);drawn=CreateCompatibleDC(dc);bitmap=CreateCompatibleBitmap(dc,rect.right,rect.bottom);old=SelectObject(drawn,bitmap);
    FillRect(drawn,&rect,GetStockObject(BLACK_BRUSH));
    printf("CLOCK-PIXELS text=%ls rect=%ld,%ld theme=%p hr=%lx\n",text,rect.right,rect.bottom,theme,(long)GetThemeFont(theme,dc,CLP_TIME,CLS_NORMAL,TMT_FONT,&font));
    printf("font=%ls height=%ld weight=%ld charset=%u\n",font.lfFaceName,font.lfHeight,font.lfWeight,font.lfCharSet);
    DrawThemeText(theme,drawn,CLP_TIME,CLS_NORMAL,text,-1,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX,0,&rect);
    measure(dc,drawn,rect.right,rect.bottom,"public-theme");
    FillRect(drawn,&rect,GetStockObject(BLACK_BRUSH));
    ZeroMemory(&font,sizeof(font));font.lfHeight=-12;font.lfWeight=400;font.lfCharSet=1;lstrcpyW(font.lfFaceName,L"MS UI Gothic");
    explicit_font=CreateFontIndirectW(&font);previous=SelectObject(drawn,explicit_font);SetBkMode(drawn,TRANSPARENT);SetTextColor(drawn,RGB(255,255,255));
    DrawTextW(drawn,text,-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    measure(dc,drawn,rect.right,rect.bottom,"explicit-MSUI12");
    GetWindowTextW(clock_window,after,128);printf("text-stable=%d\n",!lstrcmpW(text,after));
    SelectObject(drawn,previous);DeleteObject(explicit_font);SelectObject(drawn,old);DeleteObject(bitmap);DeleteDC(drawn);ReleaseDC(clock_window,dc);CloseThemeData(theme);DestroyWindow(owner);return failures||lstrcmpW(text,after)?1:0;
}
