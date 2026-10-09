/* Public XP/Wine comparison probe. Output contains metrics, never resources. */
#define _WIN32_WINNT 0x0501
#define WINVER 0x0501
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <wincrypt.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>

/* XP SP3's RSA/AES provider supports SHA-256; older-target SDK headers hide it. */
#ifndef CALG_SHA_256
#define CALG_SHA_256 0x0000800c
#endif

static FILE *output;
static BOOL property_origin;
static void origin(const char *label, HTHEME theme, int part, int state, int property)
{
    PROPERTYORIGIN value=PO_NOTFOUND;
    HRESULT hr=GetThemePropertyOrigin(theme,part,state,property,&value);
    fprintf(output,"%s origin hr=%08lx value=%d part=%d state=%d property=%d\n",
            label,(unsigned long)hr,value,part,state,property);
}
static void theme_hash(void)
{
    WCHAR path[MAX_PATH],color[128],theme_size[128];
    HCRYPTPROV provider=0;
    HCRYPTHASH hash=0;
    HANDLE file=INVALID_HANDLE_VALUE;
    BYTE buffer[16384],digest[32];
    DWORD count,size=sizeof(digest),failure=0;
    HRESULT result;
    unsigned i;
    result=GetCurrentThemeName(path,MAX_PATH,color,128,theme_size,128);
    if(FAILED(result))
    {fprintf(output,"theme-sha256 unavailable hr=%08lx\n",(unsigned long)result);return;}
    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE ||
       !CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT) ||
       !CryptCreateHash(provider,CALG_SHA_256,0,0,&hash))goto failed;
    for(;;)
    {
        if(!ReadFile(file,buffer,sizeof(buffer),&count,NULL))goto failed;
        if(!count)break;
        if(!CryptHashData(hash,buffer,count,0))goto failed;
    }
    if(!CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0) || size!=sizeof(digest))goto failed;
    fprintf(output,"theme-sha256=");
    for(i=0;i<sizeof(digest);i++)fprintf(output,"%02x",digest[i]);
    fprintf(output,"\n");
    goto done;
failed:
    failure=GetLastError();
    fprintf(output,"theme-sha256 error=%lu\n",failure);
done:
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
}
static void font(const char *label, const LOGFONTW *f)
{
    char face[128];
    WideCharToMultiByte(CP_UTF8,0,f->lfFaceName,-1,face,sizeof(face),NULL,NULL);
    fprintf(output,"%s face=%s height=%ld weight=%ld italic=%u charset=%u\n",
            label,face,f->lfHeight,f->lfWeight,f->lfItalic,f->lfCharSet);
}
static void text_value(const char *label, LPCWSTR value)
{
    char text[1024];
    WideCharToMultiByte(CP_UTF8,0,value,-1,text,sizeof(text),NULL,NULL);
    fprintf(output,"%s=%s\n",label,text);
}
static void system_font(const char *label, HTHEME theme)
{
    LOGFONTW f={0};
    HRESULT hr=GetThemeSysFont(theme,TMT_CAPTIONFONT,&f);
    fprintf(output,"%s handle=%p hr=%08lx\n",label,theme,(unsigned long)hr);
    if(SUCCEEDED(hr))font(label,&f);
}
static void probe_spi_font(void)
{
    NONCLIENTMETRICSW saved={0},changed,restored={0};
    HTHEME existing,fresh;
    BOOL result;
    saved.cbSize=sizeof(saved);
    if(!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(saved),&saved,0))
    {
        fprintf(output,"spi-font-save result=0 error=%lu\n",GetLastError());
        return;
    }
    existing=OpenThemeData(NULL,L"Window");
    font("spi-font-before",&saved.lfCaptionFont);
    system_font("spi-font-existing-before",existing);
    changed=saved;
    lstrcpyW(changed.lfCaptionFont.lfFaceName,L"Win4Wine SPI Probe");
    changed.lfCaptionFont.lfHeight=-17;
    changed.lfCaptionFont.lfWeight=FW_NORMAL;
    /* Temporary user setting only: no profile persistence or broadcast. */
    SetLastError(0);
    result=SystemParametersInfoW(SPI_SETNONCLIENTMETRICS,sizeof(changed),&changed,0);
    fprintf(output,"spi-font-change result=%d error=%lu\n",result,GetLastError());
    if(result)
    {
        restored.cbSize=sizeof(restored);
        if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(restored),&restored,0))
            font("spi-font-current",&restored.lfCaptionFont);
        system_font("spi-font-existing-after",existing);
        fresh=OpenThemeData(NULL,L"Window");
        system_font("spi-font-fresh-after",fresh);
        system_font("spi-font-null-after",NULL);
        if(fresh)CloseThemeData(fresh);
    }
    /* Always attempt restoration, including when the temporary setter failed. */
    SetLastError(0);
    result=SystemParametersInfoW(SPI_SETNONCLIENTMETRICS,sizeof(saved),&saved,0);
    fprintf(output,"spi-font-restore result=%d error=%lu\n",result,GetLastError());
    restored.cbSize=sizeof(restored);
    if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(restored),&restored,0))
    {
        font("spi-font-restored",&restored.lfCaptionFont);
        fprintf(output,"spi-font-restored-exact=%d\n",
                !memcmp(&saved.lfCaptionFont,&restored.lfCaptionFont,sizeof(LOGFONTW)));
    }
    system_font("spi-font-existing-restored",existing);
    if(existing)CloseThemeData(existing);
    fflush(output);
}
static BOOL CALLBACK child(HWND hwnd, LPARAM unused)
{
    WCHAR cls[128];
    LOGFONTW f;
    HFONT handle;
    (void)unused;
    if(GetClassNameW(hwnd,cls,128) && !lstrcmpiW(cls,L"Button"))
    {
        handle=(HFONT)SendMessageW(hwnd,WM_GETFONT,0,0);
        if(handle && GetObjectW(handle,sizeof(f),&f))font("taskbar-button",&f);
    }
    return TRUE;
}
static void theme_font(const char *label, HWND owner, HDC dc, LPCWSTR class_name)
{
    HTHEME theme;
    LOGFONTW f={0};
    HRESULT hr;
    char name[160];
    SetLastError(0);
    theme=OpenThemeData(owner,class_name);
    fprintf(output,"%s open owner=%p handle=%p error=%lu\n",label,owner,theme,GetLastError());
    if(!theme)return;
    hr=GetThemeFont(theme,NULL,BP_PUSHBUTTON,PBS_NORMAL,TMT_FONT,&f);
    fprintf(output,"%s font-null-dc hr=%08lx\n",label,(unsigned long)hr);
    if(SUCCEEDED(hr))
    {
        snprintf(name,sizeof(name),"%s-null-dc",label);font(name,&f);
    }
    ZeroMemory(&f,sizeof(f));
    hr=GetThemeFont(theme,dc,BP_PUSHBUTTON,PBS_NORMAL,TMT_FONT,&f);
    fprintf(output,"%s font-real-dc hr=%08lx dc=%p\n",label,(unsigned long)hr,dc);
    if(SUCCEEDED(hr))
    {
        snprintf(name,sizeof(name),"%s-real-dc",label);font(name,&f);
    }
    if(property_origin)origin(label,theme,BP_PUSHBUTTON,PBS_NORMAL,TMT_FONT);
    CloseThemeData(theme);
}
static void documentation(void)
{
    WCHAR file[MAX_PATH]={0},color[128]={0},size[128]={0},value[1024];
    const WCHAR *properties[]={L"DisplayName",L"Tooltip",L"Author",L"Version"};
    const char *labels[]={"theme-doc-displayname","theme-doc-tooltip","theme-doc-author","theme-doc-version"};
    HRESULT hr=GetCurrentThemeName(file,MAX_PATH,color,128,size,128);
    int i;
    fprintf(output,"theme-doc-name hr=%08lx\n",(unsigned long)hr);
    if(FAILED(hr))return;
    for(i=0;i<4;i++)
    {
        value[0]=0;
        hr=GetThemeDocumentationProperty(file,properties[i],value,1024);
        fprintf(output,"%s hr=%08lx\n",labels[i],(unsigned long)hr);
        if(SUCCEEDED(hr))text_value(labels[i],value);
    }
}
static void measure(HWND hwnd)
{
    NONCLIENTMETRICSW n={0};
    LOGFONTW f={0};
    HDC dc=GetDC(NULL);
    HTHEME window=OpenThemeData(hwnd,L"Window");
    RECT rect;int x,y,code,ids[]={HTMINBUTTON,HTMAXBUTTON,HTCLOSE},i;
    n.cbSize=sizeof(n);
    if(property_origin)
    {
        origin("window-caption-sysfont",window,0,0,TMT_CAPTIONFONT);
        origin("window-caption-partfont",window,WP_CAPTION,CS_ACTIVE,TMT_FONT);
        documentation();
    }
    {
        WCHAR file[MAX_PATH]={0},color[128]={0},size[128]={0};
        HTHEME task_band;
        HRESULT hr=GetCurrentThemeName(file,MAX_PATH,color,128,size,128);
        fprintf(output,"ui-language user=%04x system=%04x theme-name hr=%08lx\n",
                GetUserDefaultUILanguage(),GetSystemDefaultUILanguage(),(unsigned long)hr);
        if(SUCCEEDED(hr))
        {text_value("theme-file",file);text_value("theme-color",color);text_value("theme-size",size);}
        system_font("caption-null-theme",NULL);
        system_font("caption-window-theme",window);
        SetLastError(0);
        task_band=OpenThemeData(NULL,L"TaskBand");
        fprintf(output,"taskband-theme open handle=%p error=%lu\n",task_band,GetLastError());
        if(task_band)
        {system_font("caption-taskband-theme",task_band);CloseThemeData(task_band);}
    }
    fprintf(output,"dpi=%d theme=%d caption=%dx%d small-caption=%dx%d frame=%dx%d\n",
        GetDeviceCaps(dc,LOGPIXELSY),IsThemeActive(),GetSystemMetrics(SM_CXSIZE),GetSystemMetrics(SM_CYSIZE),
        GetSystemMetrics(SM_CXSMSIZE),GetSystemMetrics(SM_CYSMSIZE),GetSystemMetrics(SM_CXSIZEFRAME),GetSystemMetrics(SM_CYSIZEFRAME));
    if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(n),&n,0))
    {font("caption",&n.lfCaptionFont);font("small-caption",&n.lfSmCaptionFont);
     font("menu",&n.lfMenuFont);font("status",&n.lfStatusFont);font("message",&n.lfMessageFont);}
    if(SystemParametersInfoW(SPI_GETICONTITLELOGFONT,sizeof(f),&f,0))font("icon",&f);
    {
        /* Use our own child: OpenThemeData associates its handle with owner,
         * so opening Button on the measured top-level would replace Window. */
        HWND button=CreateWindowW(L"Button",L"Start font probe",WS_CHILD,
                                 0,0,100,30,hwnd,NULL,GetModuleHandleW(NULL),NULL);
        HDC button_dc=button?GetDC(button):NULL;
        fprintf(output,"font-probe button=%p dc=%p\n",button,button_dc);
        theme_font("start-null-owner",NULL,dc,L"Start::Button");
        theme_font("button-null-owner",NULL,dc,L"Button");
        if(button&&button_dc)
        {
            HRESULT hr;
            theme_font("start-button-owner",button,button_dc,L"Start::Button");
            theme_font("button-button-owner",button,button_dc,L"Button");
            hr=SetWindowTheme(button,L"Start",NULL);
            fprintf(output,"start-subapp set-window-theme hr=%08lx\n",(unsigned long)hr);
            theme_font("start-subapp-button-owner",button,button_dc,L"Button");
        }
        if(button_dc)ReleaseDC(button,button_dc);
        if(button)DestroyWindow(button);
    }
    if(window)
    {
        int count=0;SIZE size={0};
        GetThemeInt(window,WP_CLOSEBUTTON,CBS_NORMAL,TMT_IMAGECOUNT,&count);
        GetThemePartSize(window,NULL,WP_CLOSEBUTTON,CBS_NORMAL,NULL,TS_TRUE,&size);
        fprintf(output,"close-theme=%ldx%ld states=%d\n",size.cx,size.cy,count);
        if(SUCCEEDED(GetThemeSysFont(window,TMT_CAPTIONFONT,&f)))font("theme-caption",&f);
    }
    GetWindowRect(hwnd,&rect);
    {
        RECT bounds[3];
        int left=max(rect.left,rect.right-GetSystemMetrics(SM_CXSIZE)*8);
        for(i=0;i<3;i++)SetRect(&bounds[i],rect.right,rect.bottom,rect.left,rect.top);
        for(y=rect.top;y<min(rect.bottom,rect.top+40);y++)for(x=left;x<rect.right;x++)
        {
            code=SendMessageW(hwnd,WM_NCHITTEST,0,MAKELPARAM(x,y));
            for(i=0;i<3;i++)if(code==ids[i])
            {
                if(x<bounds[i].left)bounds[i].left=x;
                if(y<bounds[i].top)bounds[i].top=y;
                if(x>=bounds[i].right)bounds[i].right=x+1;
                if(y>=bounds[i].bottom)bounds[i].bottom=y+1;
            }
        }
        for(i=0;i<3;i++)fprintf(output,"hit-%d=%ld,%ld..%ld,%ld\n",ids[i],bounds[i].left-rect.left,
            bounds[i].top-rect.top,bounds[i].right-rect.left,bounds[i].bottom-rect.top);
    }
    {
        HRGN region=CreateRectRgn(0,0,0,0);RECT box;int kind=GetWindowRgn(hwnd,region);
        GetRgnBox(region,&box);fprintf(output,"region=%d box=%ld,%ld..%ld,%ld corner=%d\n",kind,box.left,box.top,box.right,box.bottom,PtInRegion(region,0,0));DeleteObject(region);
    }
    EnumChildWindows(FindWindowW(L"Shell_TrayWnd",NULL),child,0);
    if(window)CloseThemeData(window);
    ReleaseDC(NULL,dc);fflush(output);
}
static LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
int wmain(int argc,WCHAR **argv)
{
    WNDCLASSW cls={0};HWND a,b;MSG msg;int i;
    BOOL display=FALSE,spi_font=FALSE,hash_theme=FALSE,thread_english=FALSE;
    for(i=2;i<argc;i++)
    {
        if(!lstrcmpW(argv[i],L"--display-1024x768"))display=TRUE;
        else if(!lstrcmpW(argv[i],L"--thread-english"))thread_english=TRUE;
        else if(!lstrcmpW(argv[i],L"--property-origin"))property_origin=TRUE;
        else if(!lstrcmpW(argv[i],L"--probe-spi-font"))spi_font=TRUE;
        else if(!lstrcmpW(argv[i],L"--theme-hash"))hash_theme=TRUE;
        else return 2;
    }
    if (argc>1 && !lstrcmpiW(argv[1],L"COM1"))
    {
        HANDLE port=CreateFileW(L"\\\\.\\COM1",GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
        DCB state={0};
        COMMTIMEOUTS timeouts={0};
        int descriptor;
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
        output=_fdopen(descriptor,"wb");
        if(!output){_close(descriptor);return 2;}
    }
    else output=argc>1?_wfopen(argv[1],L"wb"):fopen("C:\\win4wine-reference.txt","wb");
    if(!output)return 2;
    if(thread_english)
    {
        LCID before=GetThreadLocale();
        BOOL changed=SetThreadLocale(0x0409);
        fprintf(output,"thread-english result=%d before=%04lx after=%04lx user=%04x system=%04x\n",
                changed,before,GetThreadLocale(),GetUserDefaultLangID(),GetSystemDefaultLangID());
    }
    if(display)
    {
        DEVMODEW mode={0};
        LONG result;
        mode.dmSize=sizeof(mode);mode.dmPelsWidth=1024;mode.dmPelsHeight=768;mode.dmBitsPerPel=32;
        mode.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
        /* Explicit comparison option only; do not persist display settings. */
        result=ChangeDisplaySettingsW(&mode,0);
        fprintf(output,"display-change result=%ld actual=%dx%d\n",result,
                GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN));
    }
    cls.lpfnWndProc=procedure;cls.hInstance=GetModuleHandleW(NULL);cls.lpszClassName=L"Win4WineReference";
    cls.hbrBackground=GetSysColorBrush(COLOR_WINDOW);cls.hIcon=LoadIconW(NULL,IDI_APPLICATION);cls.hCursor=LoadCursorW(NULL,IDC_ARROW);
    if(!RegisterClassW(&cls))return 2;
    a=CreateWindowW(cls.lpszClassName,L"Win4Wine Reference",WS_OVERLAPPEDWINDOW,20,40,500,320,NULL,NULL,cls.hInstance,NULL);
    b=CreateWindowW(cls.lpszClassName,L"Inactive Reference",WS_OVERLAPPEDWINDOW,max(40,GetSystemMetrics(SM_CXSCREEN)-520),max(80,GetSystemMetrics(SM_CYSCREEN)-360),500,320,NULL,NULL,cls.hInstance,NULL);
    if(!a||!b)return 2;
    ShowWindow(b,SW_SHOWNOACTIVATE);ShowWindow(a,SW_SHOW);SetForegroundWindow(a);UpdateWindow(a);UpdateWindow(b);
    measure(a);
    if(hash_theme)theme_hash();
    if(spi_font)probe_spi_font();
    fclose(output);
    while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    return 0;
}
