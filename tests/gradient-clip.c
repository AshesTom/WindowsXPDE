/* Portable public theme clip/viewport measurement; no asset inspection. */
#define _WIN32_WINNT 0x0501
#define WINVER 0x0501
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>
static FILE *out;
typedef struct { HDC dc;HBITMAP bitmap;HGDIOBJ old;DWORD *pixels;int width,height; } SURFACE;
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
int wmain(int argc,WCHAR **argv)
{
    WNDCLASSW cls={0};HWND owner;HTHEME theme;HDC screen;
    WCHAR path[MAX_PATH],color[128],size[128];RECT r={0,0,1288,29};
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

    cls.lpfnWndProc=DefWindowProcW;cls.hInstance=GetModuleHandleW(NULL);cls.lpszClassName=L"Win4WineGradientClipProbe";
    owner=RegisterClassW(&cls)?CreateWindowW(cls.lpszClassName,L"Gradient clip public API probe",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                          40,40,420,160,NULL,NULL,cls.hInstance,NULL):NULL;
    if(!owner)return 2;
    UpdateWindow(owner);theme=OpenThemeData(owner,L"Window");screen=GetDC(owner);
    fprintf(out,"gradient-clip dpi=%d theme=%p rect=0,0..1288,29 viewport=7,5 clip=31,10..150,37\n",GetDeviceCaps(screen,LOGPIXELSY),theme);
    ReleaseDC(owner,screen);
    if(!theme){fprintf(out,"OpenThemeData failed error=%lu\n",GetLastError());fclose(out);DestroyWindow(owner);return 2;}
    if(SUCCEEDED(GetCurrentThemeName(path,MAX_PATH,color,128,size,128)))
    {
        char p[1024],c[256],z[256];
        WideCharToMultiByte(CP_UTF8,0,path,-1,p,sizeof(p),NULL,NULL);WideCharToMultiByte(CP_UTF8,0,color,-1,c,sizeof(c),NULL,NULL);
        WideCharToMultiByte(CP_UTF8,0,size,-1,z,sizeof(z),NULL,NULL);fprintf(out,"theme path=%s color=%s size=%s\n",p,c,z);
    }
    for(int part=WP_CAPTION;part<=WP_MAXCAPTION;part+=WP_MAXCAPTION-WP_CAPTION)
    for(int state=CS_ACTIVE;state<=CS_INACTIVE;state++)for(int mode=0;mode<3;mode++)
    {
        int ox=mode?7:0,oy=mode?5:0,differences=0,left=1312,top=64,right=0,bottom=0;
        uint32_t hash_a=2166136261u,hash_b=2166136261u;
        SURFACE a=surface(1288,40),b=surface(1312,64);clear(&a);clear(&b);
        HRESULT h1=DrawThemeBackground(theme,a.dc,part,state,&r,NULL),h2;
        SetViewportOrgEx(b.dc,ox,oy,NULL);HRGN clip=NULL;
        if(mode!=1){clip=CreateRectRgn(31,10,150,37);SelectClipRgn(b.dc,clip);}
        h2=DrawThemeBackground(theme,b.dc,part,state,&r,NULL);GdiFlush();
        for(int y=10;y<34;y++)for(int x=31;x<150;x++)
        {
            DWORD p=a.pixels[(y-oy)*a.width+x-ox]&0xffffff,q=b.pixels[y*b.width+x]&0xffffff;
            hash_a=(hash_a^p)*16777619u;hash_b=(hash_b^q)*16777619u;
            if(!equal_pixels(p,q))
            {
                if(differences<3)fprintf(out,"sample part=%d state=%d mode=%d xy=%d,%d baseline=%06lx clipped=%06lx\n",part,state,mode,x,y,p,q);
                differences++;left=min(left,x);top=min(top,y);right=max(right,x+1);bottom=max(bottom,y+1);
            }
        }
        fprintf(out,"part=%d state=%d mode=%s hr=%08lx,%08lx differences=%d bbox=%d,%d..%d,%d hash=%08lx,%08lx\n",part,state,
                mode==0?"clip-only":mode==1?"viewport-only":"viewport-clip",(unsigned long)h1,(unsigned long)h2,differences,left,top,right,bottom,
                (unsigned long)hash_a,(unsigned long)hash_b);
        if(clip)DeleteObject(clip);
        destroy(&a);destroy(&b);
    }
    fputs("gradient-clip measurement complete\n",out);CloseThemeData(theme);DestroyWindow(owner);fclose(out);return 0;
}
