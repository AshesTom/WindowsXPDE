/* Public XP/Wine Start control sizing comparison. No resource export. */
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
static void box(const char *label,const RECT *r)
{fprintf(out,"%s=%ld,%ld..%ld,%ld ",label,r->left,r->top,r->right,r->bottom);}
static void font(const char *label,const LOGFONTW *f)
{
    char face[256];WideCharToMultiByte(CP_UTF8,0,f->lfFaceName,-1,face,sizeof(face),NULL,NULL);
    fprintf(out,"%s face=%s height=%ld weight=%ld italic=%u charset=%u\n",label,face,f->lfHeight,f->lfWeight,f->lfItalic,f->lfCharSet);
}
static BOOL remote_query(HWND hwnd,UINT message,void *data,SIZE_T bytes)
{
    DWORD pid;HANDLE process;void *remote;BOOL result=FALSE;SIZE_T copied;
    GetWindowThreadProcessId(hwnd,&pid);
    process=OpenProcess(PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
    if(!process)return FALSE;
    remote=VirtualAllocEx(process,NULL,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(remote)
    {
        if(WriteProcessMemory(process,remote,data,bytes,&copied) && copied==bytes &&
           SendMessageW(hwnd,message,0,(LPARAM)remote) &&
           ReadProcessMemory(process,remote,data,bytes,&copied) && copied==bytes)result=TRUE;
        VirtualFreeEx(process,remote,0,MEM_RELEASE);
    }
    CloseHandle(process);return result;
}
static void theme(const char *label,HWND owner,const WCHAR *class_name,const WCHAR *text)
{
    HTHEME handle=OpenThemeData(owner,class_name);HDC dc=GetDC(owner);LOGFONTW f={0};SIZE s={0};
    RECT r={0,0,112,30},content={0},extent={0};MARGINS m={0};HRESULT hr;int type=0,i;
    fprintf(out,"theme=%s open=%d\n",label,handle!=NULL);
    if(!handle){ReleaseDC(owner,dc);return;}
    for(i=TS_MIN;i<=TS_DRAW;i++)
    {
        hr=GetThemePartSize(handle,dc,BP_PUSHBUTTON,PBS_NORMAL,&r,(enum THEMESIZE)i,&s);
        fprintf(out,"%s part-size kind=%d hr=%08lx size=%ld,%ld\n",label,i,(unsigned long)hr,s.cx,s.cy);
    }
    hr=GetThemeMargins(handle,dc,BP_PUSHBUTTON,PBS_NORMAL,TMT_CONTENTMARGINS,&r,&m);
    fprintf(out,"%s content-margins hr=%08lx lrtb=%d,%d,%d,%d\n",label,(unsigned long)hr,m.cxLeftWidth,m.cxRightWidth,m.cyTopHeight,m.cyBottomHeight);
    hr=GetThemeMargins(handle,dc,BP_PUSHBUTTON,PBS_NORMAL,TMT_SIZINGMARGINS,&r,&m);
    fprintf(out,"%s sizing-margins hr=%08lx lrtb=%d,%d,%d,%d\n",label,(unsigned long)hr,m.cxLeftWidth,m.cxRightWidth,m.cyTopHeight,m.cyBottomHeight);
    hr=GetThemeEnumValue(handle,BP_PUSHBUTTON,PBS_NORMAL,TMT_SIZINGTYPE,&type);
    fprintf(out,"%s sizing-type hr=%08lx value=%d\n",label,(unsigned long)hr,type);
    hr=GetThemeBackgroundContentRect(handle,dc,BP_PUSHBUTTON,PBS_NORMAL,&r,&content);
    fprintf(out,"%s content hr=%08lx ",label,(unsigned long)hr);box("rect",&content);fputc('\n',out);
    hr=GetThemeBackgroundExtent(handle,dc,BP_PUSHBUTTON,PBS_NORMAL,&r,&extent);
    fprintf(out,"%s extent hr=%08lx ",label,(unsigned long)hr);box("rect",&extent);fputc('\n',out);
    hr=GetThemeFont(handle,dc,BP_PUSHBUTTON,PBS_NORMAL,TMT_FONT,&f);
    fprintf(out,"%s font-real-dc hr=%08lx\n",label,(unsigned long)hr);
    if(SUCCEEDED(hr))
    {
        HFONT local=CreateFontIndirectW(&f),old=local?SelectObject(dc,local):NULL;
        font(label,&f);
        hr=GetThemeTextExtent(handle,dc,BP_PUSHBUTTON,PBS_NORMAL,text,-1,DT_SINGLELINE|DT_NOPREFIX,NULL,&extent);
        fprintf(out,"%s text-extent hr=%08lx ",label,(unsigned long)hr);box("rect",&extent);fputc('\n',out);
        if(old)SelectObject(dc,old);
        if(local)DeleteObject(local);
    }
    CloseThemeData(handle);ReleaseDC(owner,dc);
}
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
int wmain(int argc,WCHAR **argv)
{
    HWND tray,button,owner,fixture;RECT r;LOGFONTW f={0};HFONT hfont;WCHAR text[128]={0},cls[128];
    BUTTON_IMAGELIST images={0};RECT margin={0};const LONG widths[]={0,99,112,200};int i;HDC dc;
    out=output(argc,argv);if(!out)return 2;
    tray=FindWindowW(L"Shell_TrayWnd",NULL);button=FindWindowExW(tray,NULL,L"Button",NULL);
    if(!button)button=FindWindowExW(tray,NULL,L"Win4WineStartButton",NULL);
    fprintf(out,"START-METRICS theme=%d app-themed=%d screen=%dx%d\n",IsThemeActive(),IsAppThemed(),GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN));
    if(button)
    {
        GetClassNameW(button,cls,128);GetWindowTextW(button,text,128);
        fprintf(out,"actual hwnd=%p class=%ls style=%08lx ex=%08lx\n",button,cls,GetWindowLongW(button,GWL_STYLE),GetWindowLongW(button,GWL_EXSTYLE));
        GetWindowRect(button,&r);box("window",&r);GetClientRect(button,&r);box("client",&r);fputc('\n',out);
        for(i=0;i<4;i++)
        {
            SIZE ideal={widths[i],0};BOOL result=remote_query(button,BCM_GETIDEALSIZE,&ideal,sizeof(ideal));
            fprintf(out,"actual ideal input-width=%ld result=%d size=%ld,%ld\n",widths[i],result,ideal.cx,ideal.cy);
        }
        i=remote_query(button,BCM_GETTEXTMARGIN,&margin,sizeof(margin));fprintf(out,"actual text-margin result=%d ",i);box("margin",&margin);fputc('\n',out);
        i=remote_query(button,BCM_GETIMAGELIST,&images,sizeof(images));
        fprintf(out,"actual imagelist result=%d opaque-handle=%p align=%u ",i,images.himl,images.uAlign);box("margin",&images.margin);fputc('\n',out);
        hfont=(HFONT)SendMessageW(button,WM_GETFONT,0,0);
        fprintf(out,"actual WM_GETFONT handle=%p GetObject=%d\n",hfont,GetObjectW(hfont,sizeof(f),&f));
        if(f.lfFaceName[0])
        {
            SIZE extent={0};HFONT local=CreateFontIndirectW(&f),old;
            font("actual-font",&f);dc=GetDC(NULL);old=local?SelectObject(dc,local):NULL;
            GetTextExtentPoint32W(dc,text,lstrlenW(text),&extent);fprintf(out,"actual-font text-extent=%ld,%ld\n",extent.cx,extent.cy);
            if(old)SelectObject(dc,old);
            if(local)DeleteObject(local);
            ReleaseDC(NULL,dc);
        }
    }
    else {fprintf(out,"actual Start button absent\n");lstrcpyW(text,L"\x30b9\x30bf\x30fc\x30c8");}
    owner=CreateWindowW(L"STATIC",L"",WS_POPUP,0,0,200,30,NULL,NULL,GetModuleHandleW(NULL),NULL);
    fixture=owner?CreateWindowW(L"Button",text,WS_CHILD,0,0,112,30,owner,NULL,GetModuleHandleW(NULL),NULL):NULL;
    if(fixture)
    {
        SetWindowTheme(fixture,L"Start",NULL);
        theme("owned-Start-Button",fixture,L"Button",text);
        theme("null-Start::Button",NULL,L"Start::Button",text);
    }
    if(owner)DestroyWindow(owner);
    fprintf(out,"END-START-METRICS\n");
    if(fflush(out)||ferror(out)){if(out!=stdout)fclose(out);return 2;}
    if(out!=stdout&&fclose(out))return 2;
    return 0;
}
