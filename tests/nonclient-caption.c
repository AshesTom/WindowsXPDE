/* End-to-end public NC/WM_PRINT caption tests, with independent API rendering. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { HDC dc;HBITMAP bitmap;HGDIOBJ old;DWORD *pixels;int width,height; } SURFACE;
static int failed;
static SURFACE surface(int width,int height)
{
    SURFACE s={0};BITMAPINFO info={0};s.width=width;s.height=height;s.dc=CreateCompatibleDC(NULL);
    info.bmiHeader.biSize=sizeof(info.bmiHeader);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    s.bitmap=CreateDIBSection(s.dc,&info,DIB_RGB_COLORS,(void **)&s.pixels,NULL,0);s.old=SelectObject(s.dc,s.bitmap);
    if(!s.dc||!s.bitmap||!s.pixels){fputs("NC surface allocation failed\n",stderr);ExitProcess(2);}
    return s;
}
static void destroy(SURFACE *s){SelectObject(s->dc,s->old);DeleteObject(s->bitmap);DeleteDC(s->dc);}
static void clear(SURFACE *s){for(int i=0;i<s->width*s->height;i++)s->pixels[i]=0x2050b4;}
static int equal_pixels(DWORD a,DWORD b){return (a&0xffffff)==(b&0xffffff);}
static void print(HWND hwnd,SURFACE *s)
{
    clear(s);SendMessageW(hwnd,WM_PRINT,(WPARAM)s->dc,PRF_NONCLIENT);GdiFlush();
}
static int first_button(HWND hwnd,const RECT *window,int border)
{
    int first=window->right-window->left;
    for(int x=0;x<first;x++)
    {
        LRESULT hit=SendMessageW(hwnd,WM_NCHITTEST,0,MAKELPARAM(window->left+x,window->top+border+3));
        if(hit==HTMINBUTTON||hit==HTMAXBUTTON||hit==HTCLOSE){first=x;break;}
    }
    return first;
}
static void check(HWND hwnd,HTHEME theme,const WCHAR *title,BOOL active,BOOL maximized)
{
    RECT window,text;LOGFONTW lf;HFONT font,oldfont;SIZE extent;
    GetWindowRect(hwnd,&window);int width=window.right-window.left,border=GetSystemMetrics(SM_CYFRAME);
    int height=border+GetSystemMetrics(SM_CYCAPTION),button=first_button(hwnd,&window,border);
    /* Public XP96 fixture: text begins four pixels after the 16px icon,
     * and its right edge is one pixel before the public min-button hit area. */
    text=(RECT){26,border,button-1,height};
    SURFACE actual=surface(width,height+4),expected=surface(width,height+4);
    SIZE_T bytes=(SIZE_T)width*(height+4)*sizeof(DWORD);DWORD *blank=malloc(bytes);
    if(!blank){fputs("NC reference allocation failed\n",stderr);ExitProcess(2);}
    SetWindowTextW(hwnd,L"");print(hwnd,&actual);memcpy(blank,actual.pixels,bytes);memcpy(expected.pixels,blank,bytes);
    if(FAILED(GetThemeSysFont(theme,TMT_CAPTIONFONT,&lf))){failed=1;goto done;}
    font=CreateFontIndirectW(&lf);oldfont=SelectObject(expected.dc,font);GetTextExtentPoint32W(expected.dc,title,lstrlenW(title),&extent);
    if(extent.cx<=text.right-text.left){fprintf(stderr,"FAIL title fixture is not long enough\n");failed=1;}
    SetTextColor(expected.dc,GetThemeSysColor(theme,active?COLOR_CAPTIONTEXT:COLOR_INACTIVECAPTIONTEXT));SetBkMode(expected.dc,TRANSPARENT);
    DrawThemeText(theme,expected.dc,maximized?WP_MAXCAPTION:WP_CAPTION,active?CS_ACTIVE:CS_INACTIVE,title,-1,
                  DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS,0,&text);
    GdiFlush();
    SetWindowTextW(hwnd,title);print(hwnd,&actual);
    int different=0,changes=0,overlap=0;
    for(int y=0;y<height;y++)for(int x=0;x<width;x++)
    {
        int i=y*width+x;
        if(!equal_pixels(actual.pixels[i],expected.pixels[i]))different++;
        if(!equal_pixels(actual.pixels[i],blank[i])){changes++;if(x>=button||x<text.left)overlap++;}
    }
    if(different||!changes||overlap){fprintf(stderr,"FAIL NC title pixels width=%d active=%d max=%d different=%d changes=%d overlap=%d\n",width,active,maximized,different,changes,overlap);failed=1;}
    /* Prove the fixture detects omission of ellipsis and literal '&' handling. */
    memcpy(actual.pixels,blank,bytes);HFONT oldactual=SelectObject(actual.dc,font);
    SetTextColor(actual.dc,GetTextColor(expected.dc));SetBkMode(actual.dc,TRANSPARENT);
    DrawThemeText(theme,actual.dc,maximized?WP_MAXCAPTION:WP_CAPTION,active?CS_ACTIVE:CS_INACTIVE,title,-1,
                  DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX,0,&text);GdiFlush();
    int detects_ellipsis=0;for(int i=0;i<width*height;i++)if(!equal_pixels(actual.pixels[i],expected.pixels[i]))detects_ellipsis++;
    if(!detects_ellipsis){fprintf(stderr,"FAIL fixture cannot distinguish ellipsis\n");failed=1;}
    memcpy(actual.pixels,blank,bytes);
    DrawThemeText(theme,actual.dc,maximized?WP_MAXCAPTION:WP_CAPTION,active?CS_ACTIVE:CS_INACTIVE,title,-1,
                  DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS,0,&text);GdiFlush();
    int detects_prefix=0;for(int i=0;i<width*height;i++)if(!equal_pixels(actual.pixels[i],expected.pixels[i]))detects_prefix++;
    if(!detects_prefix){fprintf(stderr,"FAIL fixture cannot distinguish literal ampersand\n");failed=1;}
    SelectObject(actual.dc,oldactual);SelectObject(expected.dc,oldfont);DeleteObject(font);
    /* Translate and clip a real WM_PRINT NC call. Everything outside the
     * original caller clip must retain its sentinel; all state must survive. */
    SURFACE translated=surface(width+24,height+24);clear(&translated);
    HRGN clip=CreateRectRgn(31,10,min(width,150),height+8),before=CreateRectRgn(0,0,0,0),after=CreateRectRgn(0,0,0,0);
    SetViewportOrgEx(translated.dc,7,5,NULL);SelectClipRgn(translated.dc,clip);GetClipRgn(translated.dc,before);
    SetTextColor(translated.dc,RGB(1,2,3));SetBkColor(translated.dc,RGB(4,5,6));SetBkMode(translated.dc,OPAQUE);
    /* Use the same clipped NC background for the expected title, since
     * StretchBlt's clipped gradient sampling is a separate GDI behavior. */
    SURFACE translated_expected=surface(width+24,height+24);clear(&translated_expected);
    SetViewportOrgEx(translated_expected.dc,7,5,NULL);SelectClipRgn(translated_expected.dc,clip);
    SetWindowTextW(hwnd,L"");SendMessageW(hwnd,WM_PRINT,(WPARAM)translated_expected.dc,PRF_NONCLIENT);
    font=CreateFontIndirectW(&lf);oldfont=SelectObject(translated_expected.dc,font);
    SetTextColor(translated_expected.dc,GetThemeSysColor(theme,active?COLOR_CAPTIONTEXT:COLOR_INACTIVECAPTIONTEXT));
    SetBkMode(translated_expected.dc,TRANSPARENT);
    DrawThemeText(theme,translated_expected.dc,maximized?WP_MAXCAPTION:WP_CAPTION,active?CS_ACTIVE:CS_INACTIVE,title,-1,
                  DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS,0,&text);GdiFlush();
    SelectObject(translated_expected.dc,oldfont);DeleteObject(font);SetWindowTextW(hwnd,title);
    HGDIOBJ caller_font=GetCurrentObject(translated.dc,OBJ_FONT);POINT origin;
    SendMessageW(hwnd,WM_PRINT,(WPARAM)translated.dc,PRF_NONCLIENT);GdiFlush();GetViewportOrgEx(translated.dc,&origin);GetClipRgn(translated.dc,after);
    if(origin.x!=7||origin.y!=5||!EqualRgn(before,after)||caller_font!=GetCurrentObject(translated.dc,OBJ_FONT)
       ||GetTextColor(translated.dc)!=RGB(1,2,3)||GetBkColor(translated.dc)!=RGB(4,5,6)||GetBkMode(translated.dc)!=OPAQUE)
    {fprintf(stderr,"FAIL WM_PRINT caller DC preservation\n");failed=1;}
    int clip_differences=0;
    for(int y=0;y<translated.height;y++)for(int x=0;x<translated.width;x++)
    {
        DWORD wanted=PtInRegion(before,x,y)?translated_expected.pixels[y*translated_expected.width+x]:0x2050b4;
        if(!equal_pixels(translated.pixels[y*translated.width+x],wanted))clip_differences++;
    }
    if(clip_differences){fprintf(stderr,"FAIL translated/clipped NC pixels=%d\n",clip_differences);failed=1;}
    DeleteObject(clip);DeleteObject(before);DeleteObject(after);destroy(&translated);destroy(&translated_expected);
    printf("checked caption width=%d active=%d maximized=%d title-changes=%d button-start=%d\n",width,active,maximized,changes,button);
done:
    free(blank);destroy(&actual);destroy(&expected);
}
int wmain(void)
{
    WNDCLASSW wc={0};HWND hwnd,other;HTHEME theme;WCHAR title[400];
    if(!GetEnvironmentVariableW(L"WIN4WINE_EXTERNAL_SHELL",NULL,0)||!SetProcessDPIAware()||!IsThemeActive())return 2;
    wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"Win4WineNcCaption";wc.hInstance=GetModuleHandleW(NULL);wc.hIcon=LoadIconW(NULL,IDI_APPLICATION);
    if(!RegisterClassW(&wc))return 2;
    hwnd=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW|WS_VISIBLE,80,80,400,200,NULL,NULL,wc.hInstance,NULL);
    other=CreateWindowW(wc.lpszClassName,L"Foreground control",WS_OVERLAPPEDWINDOW|WS_VISIBLE,550,100,200,160,NULL,NULL,wc.hInstance,NULL);
    if(!hwnd||!other||GetDpiForWindow(hwnd)!=96)return 2;
    theme=OpenThemeData(hwnd,L"Window");if(!theme)return 2;
    lstrcpyW(title,L"日本語&Ampersand & literal — ");for(int i=0;i<12;i++)lstrcatW(title,L"長い日本語タイトルと省略表示 ");
    SetCursorPos(10,70);
    for(int mode=0;mode<3;mode++)
    {
        if(mode==2)ShowWindow(hwnd,SW_MAXIMIZE);
        else SetWindowPos(hwnd,NULL,0,0,mode?240:400,200,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        for(int active=0;active<2;active++)
        {
            SetForegroundWindow(active?hwnd:other);
            if((GetAncestor(GetForegroundWindow(),GA_ROOT)==hwnd)!=active){fprintf(stderr,"FAIL foreground fixture\n");return 2;}
            check(hwnd,theme,title,active,mode==2);
        }
    }
    CloseThemeData(theme);DestroyWindow(hwnd);DestroyWindow(other);
    if(!failed)puts("PASS: NC Japanese/literal ampersand ellipsis/buttons/maximize/translated clipped WM_PRINT DC");
    return failed;
}
