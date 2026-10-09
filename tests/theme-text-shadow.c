/* Compare public theme single-shadow drawing with public property queries. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define WIDTH 420
#define HEIGHT 60
static DWORD *bits;
static int fail;
static const WCHAR title[]=L"Agjp WIN4WINE title";
static uint64_t paint(HDC dc,HTHEME theme,int state,RECT *rect,DWORD flags,int method,WCHAR *text)
{
    RECT before=*rect,clip_before,clip_after;POINT vp_before,vp_after;DTTOPTS opts={0};
    HGDIOBJ font=GetCurrentObject(dc,OBJ_FONT);COLORREF color=GetTextColor(dc);int bk=GetBkMode(dc);
    HRGN region_before=CreateRectRgn(0,0,0,0),region_after=CreateRectRgn(0,0,0,0);
    GetClipBox(dc,&clip_before);GetViewportOrgEx(dc,&vp_before);int has_clip=GetClipRgn(dc,region_before);
    for(int i=0;i<WIDTH*HEIGHT;i++)bits[i]=0x2050b4;
    if(method==0)DrawTextW(dc,text,-1,rect,flags);
    else if(method==1)
    {opts.dwSize=sizeof(opts);if(FAILED(DrawThemeTextEx(theme,dc,WP_CAPTION,state,text,-1,flags,rect,&opts)))fail=1;}
    else
    {
        int type;POINT offset;COLORREF shadow;
        if(!(flags&(DT_CALCRECT|DT_MODIFYSTRING))&&SUCCEEDED(GetThemeEnumValue(theme,WP_CAPTION,state,TMT_TEXTSHADOWTYPE,&type))
           &&type==TST_SINGLE&&SUCCEEDED(GetThemePosition(theme,WP_CAPTION,state,TMT_TEXTSHADOWOFFSET,&offset))
           &&SUCCEEDED(GetThemeColor(theme,WP_CAPTION,state,TMT_TEXTSHADOWCOLOR,&shadow)))
        {RECT r=*rect;r.left+=offset.x;r.top+=offset.y;SetTextColor(dc,shadow);DrawTextW(dc,text,-1,&r,flags);SetTextColor(dc,color);}
        DrawTextW(dc,text,-1,rect,flags);
    }
    GetClipBox(dc,&clip_after);GetViewportOrgEx(dc,&vp_after);int after_clip=GetClipRgn(dc,region_after);
    if(font!=GetCurrentObject(dc,OBJ_FONT)||color!=GetTextColor(dc)||bk!=GetBkMode(dc)
       ||memcmp(&vp_before,&vp_after,sizeof(vp_before))||memcmp(&clip_before,&clip_after,sizeof(clip_before))
       ||has_clip!=after_clip||(has_clip==1&&!EqualRgn(region_before,region_after))
       ||(!(flags&DT_CALCRECT)&&memcmp(rect,&before,sizeof(before))))
    {fprintf(stderr,"FAIL DC/RECT preservation method=%d flags=%lx\n",method,flags);fail=1;}
    DeleteObject(region_before);DeleteObject(region_after);GdiFlush();
    uint64_t hash=1469598103934665603ULL;
    for(int i=0;i<WIDTH*HEIGHT;i++){hash^=bits[i]&0xffffff;hash*=1099511628211ULL;}
    return hash;
}
int wmain(void)
{
    BOOL enabled=GetEnvironmentVariableW(L"WIN4WINE_EXTERNAL_SHELL",NULL,0)>0;
    WNDCLASSW cls={0};HWND hwnd;HDC screen,dc;HBITMAP bitmap,old_bitmap;HFONT font,old_font;
    BITMAPINFO info={0};LOGFONTW lf={0};HTHEME theme;
    cls.lpfnWndProc=DefWindowProcW;cls.hInstance=GetModuleHandleW(NULL);cls.lpszClassName=L"Win4WineThemeShadow";
    if(!RegisterClassW(&cls))return 2;
    hwnd=CreateWindowW(cls.lpszClassName,L"Theme shadow",WS_OVERLAPPEDWINDOW,0,0,420,160,NULL,NULL,cls.hInstance,NULL);
    screen=GetDC(hwnd);dc=CreateCompatibleDC(screen);theme=OpenThemeData(hwnd,L"Window");if(!theme)return 2;
    info.bmiHeader.biSize=sizeof(info.bmiHeader);info.bmiHeader.biWidth=WIDTH;info.bmiHeader.biHeight=-HEIGHT;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void **)&bits,NULL,0);old_bitmap=SelectObject(dc,bitmap);
    lf.lfHeight=-13;lf.lfWeight=700;lf.lfQuality=NONANTIALIASED_QUALITY;lf.lfCharSet=DEFAULT_CHARSET;lstrcpyW(lf.lfFaceName,L"MS UI Gothic");
    font=CreateFontIndirectW(&lf);old_font=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    for(int clipped=0;clipped<2;clipped++)for(int bottom=29;bottom<=30;bottom++)for(int state=CS_ACTIVE;state<=CS_INACTIVE;state++)
    {
        HRGN region=NULL;
        if(clipped){SetViewportOrgEx(dc,7,3,NULL);region=CreateRectRgn(25,6,145,25);SelectClipRgn(dc,region);}
        RECT r={26,4,310,bottom};DWORD flags=DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS;
        WCHAR text[64];lstrcpyW(text,title);uint64_t expected=paint(dc,theme,state,&r,flags,enabled?2:0,text);
        lstrcpyW(text,title);uint64_t actual=paint(dc,theme,state,&r,flags,1,text);
        if(actual!=expected){fprintf(stderr,"FAIL shadow pixels clip=%d bottom=%d state=%d\n",clipped,bottom,state);fail=1;}
        printf("optin=%d clipped=%d bottom=%d state=%d hash=%016llx\n",enabled,clipped,bottom,state,(unsigned long long)actual);
        if(clipped){SelectClipRgn(dc,NULL);SetViewportOrgEx(dc,0,0,NULL);DeleteObject(region);}
    }
    for(int modify=0;modify<2;modify++)
    {
        RECT a={26,4,66,29},b=a;WCHAR plain[64],themed[64];lstrcpyW(plain,title);lstrcpyW(themed,title);
        DWORD flags=DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS|(modify?DT_MODIFYSTRING:DT_CALCRECT);
        uint64_t expected=paint(dc,theme,CS_ACTIVE,&a,flags,0,plain),actual=paint(dc,theme,CS_ACTIVE,&b,flags,1,themed);
        if(expected!=actual||memcmp(&a,&b,sizeof(a))||lstrcmpW(plain,themed))
        {fprintf(stderr,"FAIL CALCRECT/MODIFYSTRING exclusion modify=%d\n",modify);fail=1;}
    }
    SelectObject(dc,old_font);SelectObject(dc,old_bitmap);DeleteObject(font);DeleteObject(bitmap);DeleteDC(dc);
    ReleaseDC(hwnd,screen);CloseThemeData(theme);DestroyWindow(hwnd);
    if(!fail)puts("PASS: public single shadow/DC/RECT/clip/viewport/CALCRECT/MODIFYSTRING");
    return fail;
}
