/* Public Win32 layout measurements; no theme resource inspection or export. */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>

static FILE *out;
static void rect(const char *label, const RECT *r)
{
    fprintf(out,"%s=%ld,%ld..%ld,%ld ",label,r->left,r->top,r->right,r->bottom);
}
static void bands(HWND hwnd)
{
    DWORD pid; HANDLE process; void *remote; LRESULT count,i;
    GetWindowThreadProcessId(hwnd,&pid);
    process=OpenProcess(PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
    remote=process?VirtualAllocEx(process,NULL,sizeof(REBARBANDINFOW),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE):NULL;
    if(!remote){fprintf(out,"band-query unavailable error=%lu\n",GetLastError());if(process)CloseHandle(process);return;}
    count=SendMessageW(hwnd,RB_GETBANDCOUNT,0,0);
    fprintf(out,"bands=%ld\n",(long)count);
    /* These control messages are >= WM_USER: their pointers require explicit
     * target-process data storage. No code is written or executed there. */
    for(i=0;i<count && i<64;i++)
    {
        REBARBANDINFOW info={0}; RECT box={0},border={0}; SIZE_T copied;
        info.cbSize=offsetof(REBARBANDINFOW,rcChevronLocation); /* XP size */
        info.fMask=RBBIM_STYLE|RBBIM_SIZE|RBBIM_CHILD|RBBIM_HEADERSIZE;
        if(!WriteProcessMemory(process,remote,&info,sizeof(info),&copied) ||
           !SendMessageW(hwnd,RB_GETBANDINFOW,i,(LPARAM)remote) ||
           !ReadProcessMemory(process,remote,&info,sizeof(info),&copied))continue;
        if(!SendMessageW(hwnd,RB_GETRECT,i,(LPARAM)remote) ||
           !ReadProcessMemory(process,remote,&box,sizeof(box),&copied))continue;
        SendMessageW(hwnd,RB_GETBANDBORDERS,i,(LPARAM)remote);
        if(!ReadProcessMemory(process,remote,&border,sizeof(border),&copied))continue;
        fprintf(out,"band=%ld style=%08x width=%u header=%u child=%p ",(long)i,info.fStyle,info.cx,info.cxHeader,info.hwndChild);
        rect("rect",&box);rect("borders",&border);fputc('\n',out);
    }
    VirtualFreeEx(process,remote,0,MEM_RELEASE);CloseHandle(process);
}
static BOOL CALLBACK window(HWND hwnd, LPARAM unused)
{
    WCHAR cls[128]; RECT wr,cr; POINT p={0}; HDC screen; int x,y,k;
    (void)unused;
    if(!GetClassNameW(hwnd,cls,128) || !GetWindowRect(hwnd,&wr) || !GetClientRect(hwnd,&cr))return TRUE;
    ClientToScreen(hwnd,&p);OffsetRect(&cr,p.x,p.y);
    fprintf(out,"window=%p parent=%p class=%ls visible=%d style=%08lx ex=%08lx ",hwnd,GetParent(hwnd),cls,IsWindowVisible(hwnd),GetWindowLongW(hwnd,GWL_STYLE),GetWindowLongW(hwnd,GWL_EXSTYLE));
    rect("window",&wr);rect("client-screen",&cr);fputc('\n',out);
    if(!lstrcmpW(cls,L"ReBarWindow32"))bands(hwnd);
    if(!IsWindowVisible(hwnd))return TRUE;
    /* Sample actual displayed pixels along both vertical edges. Numeric COLORREF (00bbggrr, ffffffff = invalid)
     * strips distinguish child borders, parent gaps and theme gradients. */
    screen=GetDC(NULL);
    for(k=0;k<3;k++)
    {
        y=wr.top+(wr.bottom-wr.top-1)*k/2;
        fprintf(out,"edge-colorref hwnd=%p y=%d left:",hwnd,y);
        for(x=wr.left-3;x<=wr.left+3;x++)fprintf(out," %08lx",GetPixel(screen,x,y));
        fprintf(out," right:");
        for(x=wr.right-3;x<=wr.right+3;x++)fprintf(out," %08lx",GetPixel(screen,x,y));
        fputc('\n',out);
    }
    ReleaseDC(NULL,screen);return TRUE;
}
static void capture(HWND hwnd,COLORREF *pixels,int width,int height)
{
    HDC dc;int x,y;
    RedrawWindow(hwnd,NULL,NULL,RDW_INVALIDATE|RDW_FRAME|RDW_UPDATENOW);
    GdiFlush();dc=GetWindowDC(hwnd);
    for(y=0;y<height;y++)for(x=0;x<width;x++)pixels[y*width+x]=GetPixel(dc,x,y);
    ReleaseDC(hwnd,dc);
}
static void caption(void)
{
    const int width=420,height=48; HWND hwnd; HICON icon; HBITMAP color,mask;
    BITMAPINFO bi={0}; DWORD *bits; BYTE maskbits[32]={0}; COLORREF before[420*48],after[420*48];
    RECT wr,iconbox={width,height,-1,-1},text={width,height,-1,-1},hit={width,height,-1,-1};
    TITLEBARINFO title={0}; WNDCLASSW wc={0}; int x,y,nicon=0,ntext=0;
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=16;bi.bmiHeader.biHeight=-16;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    color=CreateDIBSection(NULL,&bi,DIB_RGB_COLORS,(void**)&bits,NULL,0);
    mask=CreateBitmap(16,16,1,1,maskbits);
    if(!color || !mask){fprintf(out,"caption-icon allocation failed\n");goto done;}
    for(x=0;x<256;x++)bits[x]=0xffdb19a7;
    {ICONINFO ii={TRUE,0,0,mask,color};icon=CreateIconIndirect(&ii);}
    if(!icon)goto done;
    wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"Win4WineLayoutProbe";
    RegisterClassW(&wc);
    hwnd=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,width,160,NULL,NULL,GetModuleHandleW(NULL),NULL);
    if(!hwnd){DestroyIcon(icon);goto done;}
    SendMessageW(hwnd,WM_SETICON,ICON_SMALL,(LPARAM)icon);
    SetForegroundWindow(hwnd);GetWindowRect(hwnd,&wr);title.cbSize=sizeof(title);
    if(GetTitleBarInfo(hwnd,&title)){rect("titlebar-screen",&title.rcTitleBar);fputc('\n',out);}
    capture(hwnd,before,width,height);
    SetWindowTextW(hwnd,L"Agjp WIN4WINE title");capture(hwnd,after,width,height);
    for(y=0;y<height;y++)for(x=0;x<100;x++)
    {
        int code=(int)SendMessageW(hwnd,WM_NCHITTEST,0,MAKELPARAM(wr.left+x,wr.top+y));
        if(code==HTSYSMENU){hit.left=min(hit.left,x);hit.top=min(hit.top,y);hit.right=max(hit.right,x+1);hit.bottom=max(hit.bottom,y+1);}
        if(after[y*width+x]==RGB(0xdb,0x19,0xa7)){nicon++;iconbox.left=min(iconbox.left,x);iconbox.top=min(iconbox.top,y);iconbox.right=max(iconbox.right,x+1);iconbox.bottom=max(iconbox.bottom,y+1);}
    }
    for(y=0;y<height;y++)for(x=0;x<width-110;x++)if(before[y*width+x]!=after[y*width+x])
    {ntext++;text.left=min(text.left,x);text.top=min(text.top,y);text.right=max(text.right,x+1);text.bottom=max(text.bottom,y+1);}
    fprintf(out,"caption window-relative ");rect("icon-pixels",&iconbox);fprintf(out,"count=%d ",nicon);rect("title-change-pixels",&text);fprintf(out,"count=%d ",ntext);rect("HTSYSMENU",&hit);fputc('\n',out);
    DestroyWindow(hwnd);DestroyIcon(icon);
done:
    if(color)DeleteObject(color);
    if(mask)DeleteObject(mask);
}
int wmain(int argc,wchar_t **argv)
{
    HWND tray;
    if(argc>1 && !lstrcmpiW(argv[1],L"COM1"))
    {
        HANDLE port=CreateFileW(L"\\\\.\\COM1",GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
        DCB state={0};COMMTIMEOUTS timeouts={0};int descriptor;
        if(port==INVALID_HANDLE_VALUE)return 2;
        state.DCBlength=sizeof(state);
        if(!GetCommState(port,&state)){CloseHandle(port);return 2;}
        state.BaudRate=115200;state.ByteSize=8;state.Parity=NOPARITY;state.StopBits=ONESTOPBIT;
        state.fOutxCtsFlow=FALSE;state.fOutxDsrFlow=FALSE;state.fOutX=FALSE;state.fInX=FALSE;
        state.fDtrControl=DTR_CONTROL_DISABLE;state.fRtsControl=RTS_CONTROL_DISABLE;
        timeouts.WriteTotalTimeoutConstant=2000;
        if(!SetCommState(port,&state)||!SetCommTimeouts(port,&timeouts)){CloseHandle(port);return 2;}
        descriptor=_open_osfhandle((intptr_t)port,_O_WRONLY|_O_BINARY);
        if(descriptor<0){CloseHandle(port);return 2;}
        out=_fdopen(descriptor,"wb");
        if(!out){_close(descriptor);return 2;}
    }
    else out=argc>1?_wfopen(argv[1],L"wb"):stdout;
    if(!out)return 1;
    fprintf(out,"screen=%dx%d frame=%dx%d caption=%d icon=%dx%d\n",GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),GetSystemMetrics(SM_CXFRAME),GetSystemMetrics(SM_CYFRAME),GetSystemMetrics(SM_CYCAPTION),GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON));
    tray=FindWindowW(L"Shell_TrayWnd",NULL);
    if(tray){window(tray,0);EnumChildWindows(tray,window,0);}else fprintf(out,"Shell_TrayWnd absent\n");
    caption();
    if(fflush(out) || ferror(out)){if(out!=stdout)fclose(out);return 2;}
    if(out!=stdout && fclose(out))return 2;
    return 0;
}
