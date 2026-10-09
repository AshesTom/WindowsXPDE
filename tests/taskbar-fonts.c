/* Read-only public GDI/WM_GETFONT measurements; no native implementation inspection. */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>
static FILE *out;
static void font(const char *label,HFONT handle)
{
    LOGFONTW f={0}; HDC dc=GetDC(NULL); HFONT old=NULL; WCHAR realized[LF_FACESIZE]={0};
    if(handle){old=SelectObject(dc,handle);GetTextFaceW(dc,LF_FACESIZE,realized);}
    fprintf(out,"%s handle=%p get=%d",label,handle,GetObjectW(handle,sizeof(f),&f));
    fprintf(out," face=%ls height=%ld weight=%ld italic=%u charset=%u quality=%u\n",f.lfFaceName,f.lfHeight,f.lfWeight,f.lfItalic,f.lfCharSet,f.lfQuality);
    fprintf(out,"realized=%ls\n",realized);
    if(old)SelectObject(dc,old);
    ReleaseDC(NULL,dc);
}
static BOOL CALLBACK child(HWND hwnd,LPARAM unused)
{
    WCHAR cls[128];HFONT handle;
    (void)unused;
    GetClassNameW(hwnd,cls,128);
    handle=(HFONT)SendMessageW(hwnd,WM_GETFONT,0,0);
    fprintf(out,"window=%p parent=%p class=%ls visible=%d ",hwnd,GetParent(hwnd),cls,IsWindowVisible(hwnd));
    font("WM_GETFONT",handle);
    return TRUE;
}
int wmain(int argc,WCHAR **argv)
{
    HWND tray;
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
    fprintf(out,"acp=%u user=%04x system=%04x ui=%04x\n",GetACP(),GetUserDefaultLangID(),GetSystemDefaultLangID(),GetUserDefaultUILanguage());
    font("DEFAULT_GUI_FONT",GetStockObject(DEFAULT_GUI_FONT));
    font("SYSTEM_FONT",GetStockObject(SYSTEM_FONT));
    tray=FindWindowW(L"Shell_TrayWnd",NULL);
    if(tray){child(tray,0);EnumChildWindows(tray,child,0);}else fprintf(out,"tray absent\n");
    fflush(out);if(out!=stdout)fclose(out);
    return 0;
}
