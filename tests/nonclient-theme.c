/* XP's hit padding and the theme's visual assets have separate bounds. */
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>

static int cleanup_failed;
static BOOL destroy_during_region;

static LRESULT CALLBACK nonclient_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    if(msg==WM_NCDESTROY&&GetPropW(hwnd,L"Win4WineCaptionRegion"))cleanup_failed=1;
    if(msg==WM_WINDOWPOSCHANGING&&destroy_during_region&&GetPropW(hwnd,L"Win4WineCaptionRegion"))
    {
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

static int check_region(HWND hwnd,HTHEME theme,int caption_bottom,BOOL maximized)
{
    RECT window,caption;
    HRGN actual=CreateRectRgn(0,0,0,0),expected=NULL,body;
    int failed;
    GetWindowRect(hwnd,&window);
    SetRect(&caption,0,0,window.right-window.left,caption_bottom);
    if(FAILED(GetThemeBackgroundRegion(theme,NULL,maximized?WP_MAXCAPTION:WP_CAPTION,
                                       CS_ACTIVE,&caption,&expected)))return 1;
    body=CreateRectRgn(0,caption.bottom,window.right-window.left,window.bottom-window.top);
    CombineRgn(expected,expected,body,RGN_OR);
    failed=GetWindowRgn(hwnd,actual)==ERROR||!EqualRgn(actual,expected);
    if(failed)fprintf(stderr,"window region differs from public theme, maximized=%d\n",maximized);
    DeleteObject(body);DeleteObject(actual);DeleteObject(expected);
    return failed;
}

static int check_region_lifecycle(HWND hwnd,HTHEME theme,const WNDCLASSW *wc,int caption_bottom)
{
    HWND custom,owned,doomed;
    HRGN actual=CreateRectRgn(0,0,0,0),app=CreateRectRgn(2,2,150,100),assigned;
    BOOL (WINAPI *draw)(HWND,HDC);
    HMODULE module=GetModuleHandleW(L"uxtheme.dll");
    HRESULT (WINAPI *open_theme)(LPCWSTR,LPCWSTR,LPCWSTR,HANDLE *,DWORD);
    HRESULT (WINAPI *close_theme)(HANDLE);
    HRESULT (WINAPI *apply_theme)(HANDLE,char *,HWND);
    HANDLE file=NULL;
    int failed=0;
    draw=(void *)GetProcAddress(module,"Win4WineDrawNonClient");
    open_theme=(void *)GetProcAddress(module,MAKEINTRESOURCEA(2));
    close_theme=(void *)GetProcAddress(module,MAKEINTRESOURCEA(3));
    apply_theme=(void *)GetProcAddress(module,MAKEINTRESOURCEA(4));
    if(!draw||!open_theme||!close_theme||!apply_theme)return 1;
    if(check_region(hwnd,theme,caption_bottom,FALSE))return 1;
    SetWindowPos(hwnd,NULL,0,0,450,240,SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
    if(check_region(hwnd,theme,caption_bottom,FALSE))failed=1;
    SetPropW(hwnd,L"Win4WineMaximized",(HANDLE)1);
    SendMessageW(hwnd,WM_NCPAINT,1,0);
    if(check_region(hwnd,theme,caption_bottom,TRUE))failed=1;
    RemovePropW(hwnd,L"Win4WineMaximized");
    SendMessageW(hwnd,WM_NCPAINT,1,0);
    if(check_region(hwnd,theme,caption_bottom,FALSE))failed=1;

    custom=CreateWindowW(wc->lpszClassName,L"Custom region",WS_OVERLAPPEDWINDOW,
                         80,80,400,200,NULL,NULL,wc->hInstance,NULL);
    assigned=CreateRectRgn(2,2,150,100);SetWindowRgn(custom,assigned,FALSE);
    draw(custom,NULL);
    if(GetWindowRgn(custom,actual)==ERROR||!EqualRgn(actual,app))failed=1;
    assigned=CreateRectRgn(2,2,150,100);SetWindowRgn(hwnd,assigned,FALSE);
    SetWindowPos(hwnd,NULL,0,0,420,210,SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
    draw(hwnd,NULL);
    if(GetWindowRgn(hwnd,actual)==ERROR||!EqualRgn(actual,app))failed=1;

    owned=CreateWindowW(wc->lpszClassName,L"Theme restoration",WS_OVERLAPPEDWINDOW,
                        80,80,400,200,NULL,NULL,wc->hInstance,NULL);
    draw(owned,NULL);
    if(check_region(owned,theme,caption_bottom,FALSE))failed=1;
    if(FAILED(apply_theme(NULL,NULL,NULL))||IsThemeActive())return 1;
    SendMessageW(owned,WM_THEMECHANGED,0,0);
    SendMessageW(hwnd,WM_THEMECHANGED,0,0);
    if(GetWindowRgn(owned,actual)!=ERROR)failed=1;
    if(GetWindowRgn(hwnd,actual)==ERROR||!EqualRgn(actual,app))failed=1;
    if(FAILED(open_theme(L"C:\\windows\\Resources\\Themes\\Luna\\luna.msstyles",L"NormalColor",L"NormalSize",&file,0)))return 1;
    if(FAILED(apply_theme(file,NULL,NULL)))return 1;
    close_theme(file);
    SendMessageW(owned,WM_THEMECHANGED,0,0);
    if(check_region(owned,theme,caption_bottom,FALSE))failed=1;
    DestroyWindow(owned);DestroyWindow(custom);

    doomed=CreateWindowW(wc->lpszClassName,L"Destroy on region callback",WS_OVERLAPPEDWINDOW,
                         80,80,400,200,NULL,NULL,wc->hInstance,NULL);
    destroy_during_region=TRUE;draw(doomed,NULL);destroy_during_region=FALSE;
    if(IsWindow(doomed)) { failed=1;DestroyWindow(doomed); }
    DeleteObject(actual);DeleteObject(app);
    return failed;
}

static int check_button_pixels(HWND hwnd, HTHEME theme, const RECT *window,
                               const int first[3], const int last[3], int top, int bottom,
                               BOOL active, unsigned int disabled, unsigned long *hash)
{
    BOOL (WINAPI *draw)(HWND,HDC);
    HDC screen = GetDC(NULL), actual = CreateCompatibleDC(screen), expected = CreateCompatibleDC(screen);
    HBITMAP a = CreateCompatibleBitmap(screen,400,200), e = CreateCompatibleBitmap(screen,400,200);
    HGDIOBJ old_a = SelectObject(actual,a), old_e = SelectObject(expected,e);
    RECT caption = {0,0,window->right-window->left,GetSystemMetrics(SM_CYFRAME)+GetSystemMetrics(SM_CYCAPTION)-1};
    int parts[3] = {WP_MINBUTTON,WP_MAXBUTTON,WP_CLOSEBUTTON}, i,x,y,failed=0,count;
    draw=(void *)GetProcAddress(GetModuleHandleW(L"uxtheme.dll"),"Win4WineDrawNonClient");
    *hash=2166136261u;
    FillRect(actual,&caption,GetStockObject(WHITE_BRUSH));
    FillRect(expected,&caption,GetStockObject(WHITE_BRUSH));
    if(!draw || !draw(hwnd,actual)) failed=1;
    DrawThemeBackground(theme,expected,WP_CAPTION,active?CS_ACTIVE:CS_INACTIVE,&caption,NULL);
    for(i=0;i<3;i++)
    {
        int margin=MulDiv(1,GetDpiForWindow(hwnd),96),gap=MulDiv(2,GetDpiForWindow(hwnd),96);
        RECT button={first[i]+(i!=1?margin:0),top+gap,
                     last[i]-(i==1?gap-margin:i==2?gap:0),0};
        SIZE image;
        GetThemePartSize(theme,NULL,WP_CLOSEBUTTON,CBS_NORMAL,NULL,TS_TRUE,&image);
        button.bottom=button.top+MulDiv(image.cy,GetDpiForWindow(hwnd),96);
        int state=disabled&(1u<<i)?CBS_DISABLED:CBS_NORMAL;
        if(!active&&SUCCEEDED(GetThemeInt(theme,parts[i],state,TMT_IMAGECOUNT,&count))&&count>=8) state+=4;
        DrawThemeBackground(theme,expected,parts[i],state,&button,NULL);
        for(y=top;y<bottom;y++)for(x=first[i];x<last[i];x++)
        {
            COLORREF pixel=GetPixel(actual,x,y);
            if(pixel!=GetPixel(expected,x,y)) failed=1;
            *hash=(*hash^pixel)*16777619u;
        }
    }
    if(failed)fprintf(stderr,"caption button pixels differ: active=%d disabled_mask=%u\n",active,disabled);
    SelectObject(actual,old_a);SelectObject(expected,old_e);DeleteObject(a);DeleteObject(e);
    DeleteDC(actual);DeleteDC(expected);ReleaseDC(NULL,screen);
    return failed;
}

int wmain(void)
{
    WNDCLASSW wc = {0};
    HWND hwnd, other;
    HTHEME theme;
    SIZE image;
    RECT window;
    int x, y, first[3] = {-1, -1, -1}, last[3] = {0}, top = -1, bottom = 0;
    int roles[3] = {HTMINBUTTON, HTMAXBUTTON, HTCLOSE};
    int width, height, i, gap, padding;
    UINT dpi;
    unsigned long active_hash,inactive_hash,disabled_hash;
    int image_count;
    if (!SetProcessDPIAware() || !IsThemeActive()) return 2;
    theme = OpenThemeData(NULL, L"Window");
    if (!theme || FAILED(GetThemePartSize(theme, NULL, WP_CLOSEBUTTON, CBS_NORMAL,
                                          NULL, TS_TRUE, &image))) return 3;
    wc.lpfnWndProc = nonclient_proc;
    wc.lpszClassName = L"NC geometry probe";
    wc.hInstance = GetModuleHandleW(NULL);
    if (!RegisterClassW(&wc)) return 4;
    hwnd = CreateWindowW(wc.lpszClassName, L"Caption geometry", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                          80, 80, 400, 200, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) return 5;
    dpi = GetDpiForWindow(hwnd);
    width = MulDiv(image.cx, dpi, 96); height = MulDiv(image.cy, dpi, 96);
    gap = MulDiv(2, dpi, 96);
    padding = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION) - 1;
    GetWindowRect(hwnd, &window);
    for (y = 0; y < padding; y++)
        for (x = 0; x < window.right - window.left; x++)
        {
            LRESULT hit = SendMessageW(hwnd, WM_NCHITTEST, 0,
                                      MAKELPARAM(window.left + x, window.top + y));
            for (i = 0; i < 3; i++) if (hit == roles[i])
            {
                if (first[i] < 0 || x < first[i]) first[i] = x;
                if (x >= last[i]) last[i] = x + 1;
                if (i == 2)
                {
                    if (top < 0 || y < top) top = y;
                    if (y >= bottom) bottom = y + 1;
                }
            }
        }
    printf("dpi=%u source=%ldx%ld asset=%dx%d min=%d..%d max=%d..%d close=%d..%d y=%d..%d\n",
           dpi, image.cx, image.cy, width, height, first[0], last[0], first[1], last[1], first[2], last[2], top, bottom);
    {
        int margin=MulDiv(1,dpi,96);
        if(last[0]-first[0]!=width+margin || last[1]-first[1]!=width+gap-margin
           || last[2]-first[2]!=width+gap+margin)return 6;
        if(top!=GetSystemMetrics(SM_CYFRAME)||bottom!=padding
           ||first[1]-last[0]!=gap||first[2]!=last[1])return 7;
        /* Anchor the strip to the actual window edge independently of the
         * observed hits. Otherwise moving both paint and hit boxes together
         * could pass the relative-size checks at non-reference DPIs. */
        if(last[2]!=(window.right-window.left)-GetSystemMetrics(SM_CXFRAME))return 15;
        /* Independent measured XP fixture: a 400px normal window at 96 DPI. */
        if(dpi==96&&(first[0]!=326||last[0]!=348||first[1]!=350||last[1]!=372
                     ||first[2]!=372||last[2]!=396||top!=4||bottom!=29))return 14;
    }
    SetCursorPos(window.left+20,window.top+100);
    SetForegroundWindow(hwnd);
    if(GetAncestor(GetForegroundWindow(),GA_ROOT)!=hwnd) return 9;
    if(check_button_pixels(hwnd,theme,&window,first,last,top,bottom,TRUE,0,&active_hash))return 10;
    other=CreateWindowW(wc.lpszClassName,L"Inactive caption test",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                        500,80,400,200,NULL,NULL,wc.hInstance,NULL);
    SetForegroundWindow(other);
    if(GetAncestor(GetForegroundWindow(),GA_ROOT)==hwnd) return 9;
    if(check_button_pixels(hwnd,theme,&window,first,last,top,bottom,FALSE,0,&inactive_hash))return 10;
    if(SUCCEEDED(GetThemeInt(theme,WP_CLOSEBUTTON,1,TMT_IMAGECOUNT,&image_count))&&image_count>=8&&
       active_hash==inactive_hash)return 11;
    SetWindowLongW(hwnd, GWL_STYLE, GetWindowLongW(hwnd, GWL_STYLE) & ~WS_MAXIMIZEBOX);
    if (SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(window.left + (first[1] + last[1]) / 2,
                    window.top + (top + bottom) / 2)) != HTCAPTION) return 8;
    if(check_button_pixels(hwnd,theme,&window,first,last,top,bottom,FALSE,2,&disabled_hash))return 10;
    if(disabled_hash==inactive_hash)return 11;
    SetWindowLongW(hwnd,GWL_STYLE,(GetWindowLongW(hwnd,GWL_STYLE)|WS_MAXIMIZEBOX)&~WS_MINIMIZEBOX);
    EnableMenuItem(GetSystemMenu(hwnd,FALSE),SC_CLOSE,MF_BYCOMMAND|MF_GRAYED);
    if(check_button_pixels(hwnd,theme,&window,first,last,top,bottom,FALSE,5,&disabled_hash))return 10;
    if(check_region_lifecycle(hwnd,theme,&wc,padding))return 12;
    DestroyWindow(other);
    DestroyWindow(hwnd);
    if(cleanup_failed)return 13;
    CloseThemeData(theme);
    puts("PASS caption bounds/hit pixels, active/inactive/disabled assets and region ownership/resize/maximize/theme/cleanup");
    return 0;
}
