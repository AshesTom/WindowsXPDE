/* Compare public font and theme drawing APIs. No resource inspection/export. */
#define _WIN32_WINNT 0x0501
#define WINVER 0x0501
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>

static FILE *out;
static const WCHAR title[]=L"Agjp WIN4WINE title";
static void face(const char *label,const LOGFONTW *f)
{
    char name[128];WideCharToMultiByte(CP_UTF8,0,f->lfFaceName,-1,name,sizeof(name),NULL,NULL);
    fprintf(out,"%s face=%s height=%ld weight=%ld italic=%u charset=%u quality=%u\n",label,name,
            f->lfHeight,f->lfWeight,f->lfItalic,f->lfCharSet,f->lfQuality);
}
static void ink(const char *label,HDC dc,DWORD *bits,const RECT *r)
{
    int x,y,n=0,left=420,top=60,right=-1,bottom=-1,white=0;
    GdiFlush();
    for(y=0;y<60;y++)for(x=0;x<420;x++)if((bits[y*420+x]&0xffffff)!=0x2050b4)
    {
        DWORD pixel=bits[y*420+x]&0xffffff;
        n++;left=min(left,x);top=min(top,y);right=max(right,x+1);bottom=max(bottom,y+1);
        if(pixel==0xffffff)white++;
    }
    fprintf(out,"%s rect=%ld,%ld..%ld,%ld ink=%d,%d..%d,%d count=%d white=%d\n",
            label,r->left,r->top,r->right,r->bottom,left,top,right,bottom,n,white);
    (void)dc;
}
static void bitmap(HDC dc,DWORD *bits,HTHEME theme,int state,const char *label,int method,int bottom)
{
    RECT r={26,4,310,bottom};int i;HRESULT hr=S_OK;
    for(i=0;i<420*60;i++)bits[i]=0x2050b4;
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    if(method==0)DrawTextW(dc,title,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS);
    else hr=DrawThemeText(theme,dc,WP_CAPTION,state,title,-1,
                         DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS,0,&r);
    fprintf(out,"%s result=%08lx ",label,(unsigned long)hr);ink(label,dc,bits,&r);
}
int wmain(int argc,WCHAR **argv)
{
    WNDCLASSW cls={0};HWND owner;
    HDC screen,dc;HTHEME theme;HFONT f,old_font;LOGFONTW lf={0};TEXTMETRICW tm;
    HBITMAP bmp,old_bitmap;BITMAPINFO info={0};DWORD *bits;WCHAR realized[LF_FACESIZE];SIZE extent;
    COLORREF color;POINT point;int value,state,i;char label[80],name[128];HRESULT hr;
    if(argc>2)return 2;
    if(argc>1&&!lstrcmpiW(argv[1],L"COM1"))
    {
        HANDLE port=CreateFileW(L"\\\\.\\COM1",GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
        DCB settings={0};COMMTIMEOUTS timeouts={0};int descriptor;
        if(port==INVALID_HANDLE_VALUE)return 2;
        settings.DCBlength=sizeof(settings);if(!GetCommState(port,&settings)){CloseHandle(port);return 2;}
        settings.BaudRate=115200;settings.ByteSize=8;settings.Parity=NOPARITY;settings.StopBits=ONESTOPBIT;
        settings.fOutxCtsFlow=FALSE;settings.fOutxDsrFlow=FALSE;settings.fOutX=FALSE;settings.fInX=FALSE;
        settings.fDtrControl=DTR_CONTROL_DISABLE;settings.fRtsControl=RTS_CONTROL_DISABLE;timeouts.WriteTotalTimeoutConstant=2000;
        if(!SetCommState(port,&settings)||!SetCommTimeouts(port,&timeouts)){CloseHandle(port);return 2;}
        descriptor=_open_osfhandle((intptr_t)port,_O_WRONLY|_O_BINARY);if(descriptor<0){CloseHandle(port);return 2;}
        out=_fdopen(descriptor,"wb");if(!out){_close(descriptor);return 2;}
    }
    else out=argc>1?_wfopen(argv[1],L"wb"):stdout;
    if(!out)return 2;
    { BOOL (WINAPI *aware)(void)=(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDPIAware");
      if(aware)aware(); }
    /* XP initializes its theme context after an ordinary application window
     * exists. Use only our own class and window, never another app's handle. */
    cls.lpfnWndProc=DefWindowProcW;cls.hInstance=GetModuleHandleW(NULL);
    cls.lpszClassName=L"Win4WineCaptionTextProbe";cls.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
    if(!RegisterClassW(&cls))return 1;
    owner=CreateWindowW(cls.lpszClassName,L"Caption text probe",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                        40,40,420,160,NULL,NULL,cls.hInstance,NULL);
    if(!owner)return 1;
    UpdateWindow(owner);
    screen=GetDC(NULL);theme=OpenThemeData(owner,L"Window");
    {
        BOOL smoothing=FALSE;
        BOOL result=SystemParametersInfoW(SPI_GETFONTSMOOTHING,0,&smoothing,0);
        fprintf(out,"font-smoothing result=%d enabled=%d\n",result,smoothing);
    }
    fprintf(out,"dpi=%d theme=%p frame=%dx%d caption=%d\n",GetDeviceCaps(screen,LOGPIXELSY),theme,
            GetSystemMetrics(SM_CXFRAME),GetSystemMetrics(SM_CYFRAME),GetSystemMetrics(SM_CYCAPTION));
    if(!theme||FAILED(GetThemeSysFont(theme,TMT_CAPTIONFONT,&lf)))return 1;
    face("theme-caption",&lf);dc=CreateCompatibleDC(screen);f=CreateFontIndirectW(&lf);old_font=SelectObject(dc,f);
    GetTextFaceW(dc,LF_FACESIZE,realized);WideCharToMultiByte(CP_UTF8,0,realized,-1,name,sizeof(name),NULL,NULL);
    GetTextMetricsW(dc,&tm);GetTextExtentPoint32W(dc,title,lstrlenW(title),&extent);
    fprintf(out,"realized=%s text-extent=%ldx%ld metrics height=%ld ascent=%ld descent=%ld internal=%ld external=%ld ave=%ld max=%ld overhang=%ld weight=%ld italic=%u charset=%u pitch=%u\n",
            name,extent.cx,extent.cy,tm.tmHeight,tm.tmAscent,tm.tmDescent,tm.tmInternalLeading,tm.tmExternalLeading,
            tm.tmAveCharWidth,tm.tmMaxCharWidth,tm.tmOverhang,tm.tmWeight,tm.tmItalic,tm.tmCharSet,tm.tmPitchAndFamily);
    for(i=0;title[i];i++)
    {
        ABC abc={0};int width=0;GLYPHMETRICS gm={0};MAT2 matrix={{0,1},{0,0},{0,0},{0,1}};
        BOOL a=GetCharABCWidthsW(dc,title[i],title[i],&abc),w=GetCharWidth32W(dc,title[i],title[i],&width);
        DWORD g=GetGlyphOutlineW(dc,title[i],GGO_METRICS,&gm,0,NULL,&matrix);
        fprintf(out,"char=%04x abc-result=%d abc=%d,%u,%d width-result=%d width=%d glyph-result=%08lx bbox=%ux%u origin=%ld,%ld advance=%d,%d\n",
                title[i],a,abc.abcA,abc.abcB,abc.abcC,w,width,g,gm.gmBlackBoxX,gm.gmBlackBoxY,
                gm.gmptGlyphOrigin.x,gm.gmptGlyphOrigin.y,gm.gmCellIncX,gm.gmCellIncY);
    }
    for(state=CS_ACTIVE;state<=CS_INACTIVE;state++)
    {
        const int colors[]={TMT_TEXTCOLOR,TMT_TEXTSHADOWCOLOR,TMT_GLOWCOLOR};
        for(i=0;i<3;i++){color=0;hr=GetThemeColor(theme,WP_CAPTION,state,colors[i],&color);
            fprintf(out,"state=%d color-id=%d hr=%08lx colorref=%08lx\n",state,colors[i],(unsigned long)hr,(unsigned long)color);}
        value=0;hr=GetThemeEnumValue(theme,WP_CAPTION,state,TMT_TEXTSHADOWTYPE,&value);
        fprintf(out,"state=%d shadow-type hr=%08lx value=%d\n",state,(unsigned long)hr,value);
        point.x=point.y=0;hr=GetThemePosition(theme,WP_CAPTION,state,TMT_TEXTSHADOWOFFSET,&point);
        fprintf(out,"state=%d shadow-offset hr=%08lx value=%ld,%ld\n",state,(unsigned long)hr,point.x,point.y);
    }
    info.bmiHeader.biSize=sizeof(info.bmiHeader);info.bmiHeader.biWidth=420;info.bmiHeader.biHeight=-60;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void **)&bits,NULL,0);old_bitmap=SelectObject(dc,bmp);
    for(i=29;i<=30;i++)
    {
        snprintf(label,sizeof(label),"DrawText-bottom%d",i);bitmap(dc,bits,theme,CS_ACTIVE,label,0,i);
        snprintf(label,sizeof(label),"DrawThemeText-active-bottom%d",i);bitmap(dc,bits,theme,CS_ACTIVE,label,1,i);
        snprintf(label,sizeof(label),"DrawThemeText-inactive-bottom%d",i);bitmap(dc,bits,theme,CS_INACTIVE,label,1,i);
    }
    SelectObject(dc,old_bitmap);SelectObject(dc,old_font);DeleteObject(bmp);DeleteObject(f);DeleteDC(dc);
    ReleaseDC(NULL,screen);CloseThemeData(theme);DestroyWindow(owner);fclose(out);return 0;
}
