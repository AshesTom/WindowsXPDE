/* Read-only public theme/clock geometry comparison; no resource inspection. */
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
static WCHAR clock_text[128]=L"12:34";
static void font(const char *label,const LOGFONTW*f)
{
    fprintf(out,"%s face=%ls height=%ld weight=%ld italic=%u charset=%u quality=%u\n",label,f->lfFaceName,f->lfHeight,f->lfWeight,f->lfItalic,f->lfCharSet,f->lfQuality);
}
static BOOL CALLBACK child(HWND hwnd,LPARAM unused)
{
    WCHAR cls[128];RECT r,c;WCHAR text[128];(void)unused;
    GetClassNameW(hwnd,cls,128);
    if(lstrcmpW(cls,L"TrayClockWClass"))return TRUE;
    GetWindowRect(hwnd,&r);GetClientRect(hwnd,&c);GetWindowTextW(hwnd,text,128);
    if(*text)lstrcpynW(clock_text,text,128);
    fprintf(out,"actual-clock hwnd=%p window=%ld,%ld..%ld,%ld client=%ld,%ld..%ld,%ld text=%ls style=%08lx ex=%08lx\n",hwnd,r.left,r.top,r.right,r.bottom,c.left,c.top,c.right,c.bottom,text,GetWindowLongW(hwnd,GWL_STYLE),GetWindowLongW(hwnd,GWL_EXSTYLE));
    return TRUE;
}
static void theme(const char*label,HWND owner,const WCHAR*name)
{
    HTHEME t;HDC dc=GetDC(owner);LOGFONTW lf={0};COLORREF color=0;RECT box={0,0,200,30},extent={0};HRESULT hr;int i;
    t=OpenThemeData(owner,name);fprintf(out,"%s theme=%p error=%lu\n",label,t,GetLastError());
    if(!t){ReleaseDC(owner,dc);return;}
    for(i=0;i<2;i++)
    {
        hr=GetThemeFont(t,i?dc:NULL,CLP_TIME,CLS_NORMAL,TMT_FONT,&lf);
        fprintf(out,"%s font-%s hr=%08lx ",label,i?"dc":"null",(DWORD)hr);
        if(SUCCEEDED(hr))font("value",&lf);else fputc('\n',out);
    }
    hr=GetThemeColor(t,CLP_TIME,CLS_NORMAL,TMT_TEXTCOLOR,&color);
    fprintf(out,"%s textcolor hr=%08lx color=%08lx\n",label,(DWORD)hr,color);
    hr=GetThemeTextExtent(t,dc,CLP_TIME,CLS_NORMAL,clock_text,-1,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX,&box,&extent);
    fprintf(out,"%s extent-current-dc hr=%08lx text=%ls rect=%ld,%ld..%ld,%ld\n",label,(DWORD)hr,clock_text,extent.left,extent.top,extent.right,extent.bottom);
    if(SUCCEEDED(GetThemeFont(t,dc,CLP_TIME,CLS_NORMAL,TMT_FONT,&lf)))
    {
        HFONT f=CreateFontIndirectW(&lf),old=SelectObject(dc,f);TEXTMETRICW tm={0};SIZE size={0};
        GetTextMetricsW(dc,&tm);GetTextExtentPoint32W(dc,clock_text,lstrlenW(clock_text),&size);
        fprintf(out,"%s gdi-selected height=%ld ascent=%ld descent=%ld extent=%ld,%ld\n",label,tm.tmHeight,tm.tmAscent,tm.tmDescent,size.cx,size.cy);
        hr=GetThemeTextExtent(t,dc,CLP_TIME,CLS_NORMAL,clock_text,-1,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX,&box,&extent);
        fprintf(out,"%s extent-theme-font-dc hr=%08lx rect=%ld,%ld..%ld,%ld\n",label,(DWORD)hr,extent.left,extent.top,extent.right,extent.bottom);
        SelectObject(dc,old);DeleteObject(f);
    }
    CloseThemeData(t);ReleaseDC(owner,dc);
}
int wmain(int argc,WCHAR**argv)
{
    HDC screen;HWND owner,tray;NONCLIENTMETRICSW ncm={0};LOGFONTW icon={0};
    if(argc>1&&!lstrcmpiW(argv[1],L"COM1"))
    {
        HANDLE port=CreateFileW(L"\\\\.\\COM1",GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
        DCB state={0};COMMTIMEOUTS timeouts={0};int descriptor;
        if(port==INVALID_HANDLE_VALUE)return 2;
        state.DCBlength=sizeof(state);
        if(!GetCommState(port,&state))return 2;
        state.BaudRate=115200;state.ByteSize=8;state.Parity=NOPARITY;state.StopBits=ONESTOPBIT;
        state.fOutxCtsFlow=FALSE;state.fOutxDsrFlow=FALSE;state.fOutX=FALSE;state.fInX=FALSE;
        state.fDtrControl=DTR_CONTROL_DISABLE;state.fRtsControl=RTS_CONTROL_DISABLE;
        timeouts.WriteTotalTimeoutConstant=2000;
        if(!SetCommState(port,&state)||!SetCommTimeouts(port,&timeouts))return 2;
        descriptor=_open_osfhandle((intptr_t)port,_O_WRONLY|_O_BINARY);
        if(descriptor<0)return 2;
        out=_fdopen(descriptor,"wb");
    }
    else out=argc>1?_wfopen(argv[1],L"wb"):stdout;
    if(!out)return 2;

    owner=CreateWindowW(L"STATIC",L"Clock public API probe",WS_OVERLAPPEDWINDOW|WS_VISIBLE,40,40,360,180,NULL,NULL,GetModuleHandleW(NULL),NULL);
    if(!owner)return 2;
    UpdateWindow(owner);
    screen=GetDC(owner);
    fprintf(out,"CLOCK-METRICS acp=%u dpi=%d theme-active=%d\n",GetACP(),GetDeviceCaps(screen,LOGPIXELSY),IsThemeActive());
    ReleaseDC(owner,screen);
    tray=FindWindowW(L"Shell_TrayWnd",NULL);if(tray)EnumChildWindows(tray,child,0);else fprintf(out,"actual-clock absent\n");
    ncm.cbSize=sizeof(ncm);
    if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(ncm),&ncm,0))
    {
        font("spi-caption",&ncm.lfCaptionFont);font("spi-small-caption",&ncm.lfSmCaptionFont);
        font("spi-menu",&ncm.lfMenuFont);font("spi-status",&ncm.lfStatusFont);font("spi-message",&ncm.lfMessageFont);
    }
    if(SystemParametersInfoW(SPI_GETICONTITLELOGFONT,sizeof(icon),&icon,0))font("spi-icon",&icon);
    theme("Clock",owner,L"Clock");theme("TrayNotify::Clock",owner,L"TrayNotify::Clock");
    fprintf(out,"SetWindowTheme TrayNotify hr=%08lx\n",(DWORD)SetWindowTheme(owner,L"TrayNotify",NULL));
    theme("subapp-Clock",owner,L"Clock");
    fprintf(out,"END-CLOCK-METRICS\n");fflush(out);DestroyWindow(owner);
    if(out!=stdout)fclose(out);
    return 0;
}
