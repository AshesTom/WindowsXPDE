/* Public Classic Button ideal-size measurements with self-authored images. */
#define _WIN32_WINNT 0x0501
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <stdio.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>
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

int wmain(int argc,WCHAR**argv)
{
    FILE*out=output(argc,argv);WNDCLASSW wc={0};HWND parent,button;HFONT font,old;
    NONCLIENTMETRICSW ncm={0};INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_STANDARD_CLASSES};HDC dc;SIZE text={0};
    BOOL verify=argc>2&&!lstrcmpW(argv[2],L"--verify-xp96");int failures=0;
    const SIZE xp96[]={{46,21},{62,21},{65,21},{71,21},{62,40},{70,29},{36,17}};
    const WCHAR label[]=L"\x30b9\x30bf\x30fc\x30c8";int i;
    if(!out)return 2;
    InitCommonControlsEx(&ic);
    ncm.cbSize=sizeof(ncm);if(!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(ncm),&ncm,0))return 2;
    if(verify&&(IsThemeActive()||GetSystemMetrics(SM_CXSMICON)!=16||
        ncm.lfCaptionFont.lfHeight!=-12||ncm.lfCaptionFont.lfWeight!=700||
        lstrcmpW(ncm.lfCaptionFont.lfFaceName,L"MS UI Gothic")))return 2;
    font=CreateFontIndirectW(&ncm.lfCaptionFont);
    wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"Shell_TrayWnd";wc.hInstance=GetModuleHandleW(NULL);RegisterClassW(&wc);
    parent=CreateWindowW(wc.lpszClassName,L"Classic sizing public fixture",WS_OVERLAPPEDWINDOW|WS_VISIBLE,80,80,400,160,NULL,NULL,wc.hInstance,NULL);
    button=CreateWindowW(L"Button",label,WS_VISIBLE|WS_CHILD|BS_LEFT|BS_VCENTER,10,10,200,40,parent,NULL,wc.hInstance,NULL);
    if(!button)return 2;
    SendMessageW(button,WM_SETFONT,(WPARAM)font,FALSE);
    dc=GetDC(button);old=SelectObject(dc,font);GetTextExtentPoint32W(dc,label,lstrlenW(label),&text);SelectObject(dc,old);ReleaseDC(button,dc);
    fprintf(out,"CLASSIC-SIZING theme=%d font=%ls height=%ld weight=%ld text=%ld,%ld edge=%dx%d\n",IsThemeActive(),ncm.lfCaptionFont.lfFaceName,ncm.lfCaptionFont.lfHeight,ncm.lfCaptionFont.lfWeight,text.cx,text.cy,GetSystemMetrics(SM_CXEDGE),GetSystemMetrics(SM_CYEDGE));
    for(i=0;i<7;i++)
    {
        HIMAGELIST list=NULL;BUTTON_IMAGELIST images={0};SIZE ideal={0};BOOL ok;int width=i==3?25:16,height=i==4?40:i==3?20:16;RECT margin={0};
        if(i>=5)
        {
            LOGFONTW custom=ncm.lfCaptionFont;HFONT other;
            custom.lfHeight=i==5?-20:-8;other=CreateFontIndirectW(&custom);
            SendMessageW(button,WM_SETFONT,(WPARAM)other,FALSE);DeleteObject(font);font=other;
        }
        if(i>0&&i<5)
        {
            HDC screen=GetDC(NULL),memory=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,width,height),old_bitmap=SelectObject(memory,bitmap);
            RECT fill={0,0,width,height};FillRect(memory,&fill,GetSysColorBrush(COLOR_BTNFACE));SelectObject(memory,old_bitmap);
            list=ImageList_Create(width,height,ILC_COLOR32,1,1);ImageList_Add(list,bitmap,NULL);
            DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(NULL,screen);
            images.himl=list;images.uAlign=BUTTON_IMAGELIST_ALIGN_LEFT;images.margin.right=i==2?3:0;
            fprintf(out,"case=%d set-image=%ld ",i,(long)SendMessageW(button,BCM_SETIMAGELIST,0,(LPARAM)&images));
        }
        else fprintf(out,"case=%d ",i);
        ok=SendMessageW(button,BCM_GETIDEALSIZE,0,(LPARAM)&ideal);
        fprintf(out,"ideal-result=%d ideal=%ld,%ld ",ok,ideal.cx,ideal.cy);
        fprintf(out,"margin-result=%ld ",(long)SendMessageW(button,BCM_GETTEXTMARGIN,0,(LPARAM)&margin));
        fprintf(out,"margin=%ld,%ld..%ld,%ld\n",margin.left,margin.top,margin.right,margin.bottom);
        if(verify&&(!ok||ideal.cx!=xp96[i].cx||ideal.cy!=xp96[i].cy||
                    margin.left!=1||margin.top!=1||margin.right!=1||margin.bottom!=1))failures++;
        images.himl=NULL;SendMessageW(button,BCM_SETIMAGELIST,0,(LPARAM)&images);if(list)ImageList_Destroy(list);
    }
    if(verify)fprintf(out,"%s: seven independent XP96 Classic sizing fixtures\n",failures?"FAIL":"PASS");
    fprintf(out,"END-CLASSIC-SIZING\n");fflush(out);DestroyWindow(parent);DeleteObject(font);if(out!=stdout)fclose(out);return failures?1:0;
}
