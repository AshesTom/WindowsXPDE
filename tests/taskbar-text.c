/* Public taskbar text comparison. Run only in a disposable reference session.
 * --isolated-session closes old self-authored Win4WineReference fixtures only.
 * Toolbar storage is accessed only through documented messages and remote buffers.
 */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>
static FILE *out;
static FILE *output(int argc,WCHAR **argv)
{
    if(argc>1&&!lstrcmpiW(argv[1],L"COM1"))
    {
        HANDLE port=CreateFileW(L"\\\\.\\COM1",GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
        DCB state={0};COMMTIMEOUTS timeouts={0};int descriptor;FILE *stream;
        if(port==INVALID_HANDLE_VALUE)return NULL;
        state.DCBlength=sizeof(state);
        if(!GetCommState(port,&state)){CloseHandle(port);return NULL;}
        state.BaudRate=115200;state.ByteSize=8;state.Parity=NOPARITY;state.StopBits=ONESTOPBIT;
        state.fOutxCtsFlow=FALSE;state.fOutxDsrFlow=FALSE;state.fOutX=FALSE;state.fInX=FALSE;
        state.fDtrControl=DTR_CONTROL_DISABLE;state.fRtsControl=RTS_CONTROL_DISABLE;
        timeouts.WriteTotalTimeoutConstant=2000;
        if(!SetCommState(port,&state)||!SetCommTimeouts(port,&timeouts)){CloseHandle(port);return NULL;}
        descriptor=_open_osfhandle((intptr_t)port,_O_WRONLY|_O_BINARY);
        if(descriptor<0){CloseHandle(port);return NULL;}
        stream=_fdopen(descriptor,"wb");if(!stream)_close(descriptor);return stream;
    }
    return argc>1?_wfopen(argv[1],L"wb"):stdout;
}
static HWND toolbar;
static BOOL CALLBACK find(HWND h,LPARAM p){WCHAR c[80];(void)p;GetClassNameW(h,c,80);if(!lstrcmpW(c,L"ToolbarWindow32")){HWND q=GetParent(h);GetClassNameW(q,c,80);if(!lstrcmpW(c,L"MSTaskSwWClass"))toolbar=h;}return TRUE;}
static BOOL CALLBACK cleanup(HWND h,LPARAM p){WCHAR c[100];(void)p;GetClassNameW(h,c,100);if(!lstrcmpW(c,L"Win4WineReference"))PostMessageW(h,WM_CLOSE,0,0);return TRUE;}
static LRESULT remote(UINT msg,WPARAM wp,void *buf,SIZE_T bytes){DWORD pid;SIZE_T n;LRESULT result=-1;GetWindowThreadProcessId(toolbar,&pid);HANDLE p=OpenProcess(PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_VM_WRITE,FALSE,pid);if(!p)return -1;void *r=VirtualAllocEx(p,NULL,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(r){WriteProcessMemory(p,r,buf,bytes,&n);result=SendMessageW(toolbar,msg,wp,(LPARAM)r);ReadProcessMemory(p,r,buf,bytes,&n);VirtualFreeEx(p,r,0,MEM_RELEASE);}CloseHandle(p);return result;}
static void pump(DWORD ms){DWORD end=GetTickCount()+ms;MSG m;while((LONG)(end-GetTickCount())>0){while(PeekMessageW(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(10);}}
static void measure(const char *label){int n=(int)SendMessageW(toolbar,TB_BUTTONCOUNT,0,0),i;fprintf(out,"%s buttons=%d\n",label,n);for(i=0;i<n;i++){TBBUTTON b={0};WCHAR text[512]={0};RECT r={0};remote(TB_GETBUTTON,i,&b,sizeof(b));LRESULT len=remote(TB_GETBUTTONTEXTW,b.idCommand,text,sizeof(text));fprintf(out,"button=%d id=%d state=%u text-result=%ld text=%ls\n",i,b.idCommand,b.fsState,(long)len,text);if(lstrcmpW(text,L"W4W Task 1234"))continue;remote(TB_GETITEMRECT,i,&r,sizeof(r));MapWindowPoints(toolbar,NULL,(POINT*)&r,2);HDC dc=GetDC(NULL);int x,y,count=0,left=9999,top=9999,right=-1,bottom=-1;for(y=r.top+3;y<r.bottom-3;y++)for(x=r.left+28;x<r.right-4;x++)if(GetPixel(dc,x,y)==RGB(255,255,255)){count++;if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;}ReleaseDC(NULL,dc);fprintf(out,"%s rect=%ld,%ld..%ld,%ld ink=%ld,%ld..%ld,%ld count=%d\n",label,r.left,r.top,r.right,r.bottom,left-r.left,top-r.top,right+1-r.left,bottom+1-r.top,count);}fflush(out);}
static void ownfont(int height){LOGFONTW lf={0};lf.lfHeight=height;lf.lfWeight=400;lf.lfCharSet=1;lstrcpyW(lf.lfFaceName,L"MS UI Gothic");HFONT f=CreateFontIndirectW(&lf);HDC dc=CreateCompatibleDC(NULL);HBITMAP b=CreateBitmap(300,40,1,32,NULL);HGDIOBJ ob=SelectObject(dc,b),of=SelectObject(dc,f);RECT r={0,0,300,40};FillRect(dc,&r,GetStockObject(BLACK_BRUSH));SetTextColor(dc,RGB(255,255,255));SetBkMode(dc,TRANSPARENT);SIZE sz;TEXTMETRICW tm;GetTextExtentPoint32W(dc,L"W4W Task 1234",13,&sz);GetTextMetricsW(dc,&tm);TextOutW(dc,0,0,L"W4W Task 1234",13);int x,y,l=999,t=999,rr=-1,bb=-1,n=0;for(y=0;y<40;y++)for(x=0;x<300;x++)if(GetPixel(dc,x,y)==RGB(255,255,255)){n++;if(x<l)l=x;if(x>rr)rr=x;if(y<t)t=y;if(y>bb)bb=y;}fprintf(out,"OWNFONT extent=%ld,%ld metrics=%ld,%ld,%ld ink=%d,%d..%d,%d count=%d\n",sz.cx,sz.cy,tm.tmHeight,tm.tmAscent,tm.tmDescent,l,t,rr+1,bb+1,n);SelectObject(dc,of);SelectObject(dc,ob);DeleteObject(f);DeleteObject(b);DeleteDC(dc);}
static void themedfont(HTHEME theme){LOGFONTW lf={0};lf.lfHeight=-13;lf.lfWeight=400;lf.lfCharSet=1;lstrcpyW(lf.lfFaceName,L"MS UI Gothic");HFONT f=CreateFontIndirectW(&lf);HDC dc=CreateCompatibleDC(NULL);HBITMAP b=CreateBitmap(300,40,1,32,NULL);HGDIOBJ ob=SelectObject(dc,b),of=SelectObject(dc,f);RECT r={0,0,300,40};FillRect(dc,&r,GetStockObject(BLACK_BRUSH));SetTextColor(dc,RGB(255,255,255));SetBkMode(dc,TRANSPARENT);SIZE sz;TEXTMETRICW tm;GetTextExtentPoint32W(dc,L"W4W Task 1234",13,&sz);GetTextMetricsW(dc,&tm);DrawThemeText(theme,dc,TP_BUTTON,TS_NORMAL,L"W4W Task 1234",13,DT_SINGLELINE|DT_LEFT|DT_TOP,0,&r);int x,y,l=999,t=999,rr=-1,bb=-1,n=0;for(y=0;y<40;y++)for(x=0;x<300;x++)if(GetPixel(dc,x,y)==RGB(255,255,255)){n++;if(x<l)l=x;if(x>rr)rr=x;if(y<t)t=y;if(y>bb)bb=y;}fprintf(out,"THEMED-SELECTED13 extent=%ld,%ld metrics=%ld,%ld,%ld ink=%d,%d..%d,%d count=%d\n",sz.cx,sz.cy,tm.tmHeight,tm.tmAscent,tm.tmDescent,l,t,rr+1,bb+1,n);SelectObject(dc,of);SelectObject(dc,ob);DeleteObject(f);DeleteObject(b);DeleteDC(dc);}
int wmain(int argc,WCHAR **argv){if(argc!=3||lstrcmpW(argv[2],L"--isolated-session"))return 2;out=output(argc,argv);if(!out)return 2;ownfont(-13);ownfont(-12);EnumWindows(cleanup,0);pump(700);HWND target=CreateWindowW(L"STATIC",L"W4W Task 1234",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,400,250,NULL,NULL,GetModuleHandleW(NULL),NULL);HWND other=CreateWindowW(L"STATIC",L"Control",WS_OVERLAPPEDWINDOW|WS_VISIBLE,520,100,300,250,NULL,NULL,GetModuleHandleW(NULL),NULL);pump(1000);SetWindowTheme(target,L"TaskBand",NULL);HTHEME th=OpenThemeData(target,L"Toolbar");LOGFONTW tf={0};HRESULT hr=GetThemeFont(th,NULL,TP_BUTTON,TS_NORMAL,TMT_FONT,&tf);fprintf(out,"TASKTHEME handle=%p hr=%lx font=%ls height=%ld weight=%ld charset=%u\n",th,(long)hr,tf.lfFaceName,tf.lfHeight,tf.lfWeight,tf.lfCharSet);hr=GetThemeFont(th,NULL,0,0,TMT_FONT,&tf);fprintf(out,"TASKTHEME-DEFAULT hr=%lx font=%ls height=%ld weight=%ld charset=%u\n",(long)hr,tf.lfFaceName,tf.lfHeight,tf.lfWeight,tf.lfCharSet);for(int st=0;st<=6;st++){hr=GetThemeFont(th,NULL,0,st,TMT_FONT,&tf);fprintf(out,"TASKTHEME-STATE state=%d hr=%lx font=%ls height=%ld weight=%ld charset=%u\n",st,(long)hr,tf.lfFaceName,tf.lfHeight,tf.lfWeight,tf.lfCharSet);}if(th){themedfont(th);CloseThemeData(th);}EnumChildWindows(FindWindowW(L"Shell_TrayWnd",NULL),find,0);fprintf(out,"TASKTEXT screen=%d,%d toolbar=%p\n",GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),toolbar);SetForegroundWindow(other);pump(700);measure("inactive");SetForegroundWindow(target);pump(700);measure("active");DestroyWindow(other);DestroyWindow(target);if(out!=stdout)fclose(out);return 0;}
