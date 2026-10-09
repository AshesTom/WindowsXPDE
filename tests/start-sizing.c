/* Derive Start ideal size from public theme/text/image metadata, never artwork. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
static int failures;
static void check(BOOL ok,const char *label){printf("%s %s\n",ok?"PASS":"FAIL",label);if(!ok)failures++;}
int wmain(void)
{
    WNDCLASSW wc={0};HWND parent,button,ordinary;HMODULE native;HIMAGELIST list;
    HIMAGELIST (WINAPI *create)(int,int,UINT,int,int);BOOL (WINAPI *destroy)(HIMAGELIST);
    int (WINAPI *add)(HIMAGELIST,HBITMAP,HBITMAP);BUTTON_IMAGELIST images={0};
    HDC screen,dc;HBITMAP bitmap,old_bitmap;HBRUSH red;RECT fill={0,0,25,20},extent,content={0},margin;
    HTHEME theme;SIZE actual={0},expected;WCHAR cls[64];HFONT huge,old_font;LOGFONTW lf={0};int i;
    HMODULE ux=GetModuleHandleW(L"uxtheme.dll");HANDLE saved_theme=NULL;
    HRESULT (WINAPI *open_theme)(LPCWSTR,LPCWSTR,LPCWSTR,HANDLE *,DWORD)=(void*)GetProcAddress(ux,MAKEINTRESOURCEA(2));
    HRESULT (WINAPI *close_theme)(HANDLE)=(void*)GetProcAddress(ux,MAKEINTRESOURCEA(3));
    HRESULT (WINAPI *apply_theme)(HANDLE,char *,HWND)=(void*)GetProcAddress(ux,MAKEINTRESOURCEA(4));
    WCHAR file[MAX_PATH],color[128],size_name[128];
    const WCHAR text[]=L"\x30b9\x30bf\x30fc\x30c8";const LONG widths[]={0,99,112,200};
    if(!IsThemeActive()||!open_theme||!close_theme||!apply_theme||
       FAILED(GetCurrentThemeName(file,MAX_PATH,color,128,size_name,128))||
       FAILED(open_theme(file,color,size_name,&saved_theme,0)))return 2;
    wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"Shell_TrayWnd";
    if(!RegisterClassW(&wc))return 2;
    parent=CreateWindowW(wc.lpszClassName,L"",WS_POPUP,0,0,300,60,NULL,NULL,wc.hInstance,NULL);
    button=CreateWindowW(L"Button",text,WS_CHILD|BS_LEFT|BS_VCENTER,0,0,112,30,parent,NULL,wc.hInstance,NULL);
    if(!parent||!button)return 2;
    GetClassNameW(button,cls,64);check(!lstrcmpW(cls,L"Win4WineStartButton"),"isolated Start class");
    native=LoadLibraryW(L"C:\\xp\\comctl32.dll");if(!native)return 2;
    create=(void*)GetProcAddress(native,"ImageList_Create");destroy=(void*)GetProcAddress(native,"ImageList_Destroy");add=(void*)GetProcAddress(native,"ImageList_Add");
    if(!create||!destroy||!add)return 2;
    list=create(25,20,ILC_COLOR32,1,1);screen=GetDC(NULL);dc=CreateCompatibleDC(screen);
    bitmap=CreateCompatibleBitmap(screen,25,20);old_bitmap=SelectObject(dc,bitmap);red=CreateSolidBrush(RGB(255,0,0));FillRect(dc,&fill,red);SelectObject(dc,old_bitmap);
    if(!list||add(list,bitmap,NULL)!=0)return 2;
    images.himl=list;images.uAlign=BUTTON_IMAGELIST_ALIGN_LEFT;
    if(!SendMessageW(button,BCM_SETIMAGELIST,0,(LPARAM)&images)||FAILED(SetWindowTheme(button,L"Start",NULL)))return 2;
    theme=GetWindowTheme(button);if(!theme)return 2;
    /* A deliberately unrelated caller font must not replace theme label metrics. */
    lf.lfHeight=-40;lstrcpyW(lf.lfFaceName,L"Courier New");huge=CreateFontIndirectW(&lf);
    SendMessageW(button,WM_SETFONT,(WPARAM)huge,FALSE);old_font=SelectObject(dc,huge);
    if(FAILED(GetThemeTextExtent(theme,dc,BP_PUSHBUTTON,PBS_NORMAL,text,-1,DT_SINGLELINE|DT_NOPREFIX,NULL,&extent)))return 2;
    content.right=extent.right-extent.left+2+25;content.bottom=max(extent.bottom-extent.top+2,20);
    if(FAILED(GetThemeBackgroundExtent(theme,dc,BP_PUSHBUTTON,PBS_NORMAL,&content,&extent)))return 2;
    expected.cx=extent.right-extent.left;expected.cy=extent.bottom-extent.top;
    for(i=0;i<4;i++)
    {
        actual.cx=widths[i];actual.cy=0;
        check(SendMessageW(button,BCM_GETIDEALSIZE,0,(LPARAM)&actual)&&actual.cx==expected.cx&&actual.cy==expected.cy,"theme text + native opaque logo + margins + theme extent");
        printf("input-width=%ld actual=%ld,%ld expected=%ld,%ld\n",widths[i],actual.cx,actual.cy,expected.cx,expected.cy);
    }
    for(i=0;i<2;i++)
    {
        images.margin.left=3;images.margin.right=5;images.margin.top=7;images.margin.bottom=9;
        images.uAlign=i?BUTTON_IMAGELIST_ALIGN_TOP:BUTTON_IMAGELIST_ALIGN_LEFT;
        check(SendMessageW(button,BCM_SETIMAGELIST,0,(LPARAM)&images),"set synthetic image margins/alignment");
        if(FAILED(GetThemeTextExtent(theme,dc,BP_PUSHBUTTON,PBS_NORMAL,text,-1,DT_SINGLELINE|DT_NOPREFIX,NULL,&extent)))return 2;
        content.right=i?max(extent.right-extent.left+2,25+8):extent.right-extent.left+2+25+8;
        content.bottom=i?extent.bottom-extent.top+2+20+16:max(extent.bottom-extent.top+2,20+16);
        if(FAILED(GetThemeBackgroundExtent(theme,dc,BP_PUSHBUTTON,PBS_NORMAL,&content,&extent)))return 2;
        actual.cx=actual.cy=0;
        check(SendMessageW(button,BCM_GETIDEALSIZE,0,(LPARAM)&actual)&&actual.cx==extent.right-extent.left&&actual.cy==extent.bottom-extent.top,"preserve image margins and horizontal/vertical alignment");
    }
    check(SendMessageW(button,BCM_GETTEXTMARGIN,0,(LPARAM)&margin)&&margin.left==1&&margin.top==1&&margin.right==1&&margin.bottom==1,"XP default Start text margin");
    check(SUCCEEDED(apply_theme(NULL,NULL,NULL))&&!IsThemeActive(),"enter classic through Wine theme API");actual.cx=actual.cy=0;
    check(SendMessageW(button,BCM_GETIDEALSIZE,0,(LPARAM)&actual)&&actual.cy>=40,"theme-disabled Start uses explicit font");
    ordinary=CreateWindowW(L"Button",text,WS_POPUP,0,0,112,30,NULL,NULL,wc.hInstance,NULL);
    SendMessageW(ordinary,WM_SETFONT,(WPARAM)huge,FALSE);actual.cx=actual.cy=0;
    check(SendMessageW(ordinary,BCM_GETIDEALSIZE,0,(LPARAM)&actual)&&actual.cy>=40,"ordinary Button retains explicit font");
    check(SUCCEEDED(apply_theme(saved_theme,NULL,NULL)),"restore selected theme");close_theme(saved_theme);
    DestroyWindow(ordinary);DestroyWindow(parent);destroy(list);SelectObject(dc,old_font);
    DeleteObject(huge);DeleteObject(red);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);FreeLibrary(native);
    return failures?1:0;
}
