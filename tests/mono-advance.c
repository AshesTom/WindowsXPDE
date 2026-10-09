/* Public GDI bitmap/outline advance and smoothing regression probe. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <wchar.h>
static const WCHAR title[]=L"Agjp WIN4WINE title";
#define WIDTH 420
#define HEIGHT 60
static uint64_t render(HDC target,HDC capture,DWORD *bits,int method,int extra)
{
    RECT r={0,0,WIDTH,HEIGHT};INT dx[64],pdy[128];int len=lstrlenW(title);
    FillRect(target,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(target,TRANSPARENT);SetTextColor(target,RGB(255,255,255));
    SetTextCharacterExtra(target,0);
    for(int i=0;i<len;i++){GetCharWidth32W(target,title[i],title[i],&dx[i]);dx[i]+=extra;pdy[i*2]=dx[i];pdy[i*2+1]=method==4?(i%3)-1:0;}
    SetTextCharacterExtra(target,method==3?extra:0);
    ExtTextOutW(target,26,10,method==2||method==4?ETO_PDY:0,NULL,title,len,method==0||method==3?NULL:method==2||method==4?pdy:dx);
    SetTextCharacterExtra(target,0);GdiFlush();
    if(target!=capture)BitBlt(capture,0,0,WIDTH,HEIGHT,target,0,0,SRCCOPY);
    GdiFlush();uint64_t hash=1469598103934665603ULL;
    for(int i=0;i<WIDTH*HEIGHT;i++){hash^=bits[i]&0xffffff;hash*=1099511628211ULL;}
    return hash;
}
int wmain(int argc,WCHAR **argv)
{
    BOOL smoothing,enabled=GetEnvironmentVariableW(L"WIN4WINE_EXTERNAL_SHELL",NULL,0)>0;
    WNDCLASSW cls={0};HWND hwnd;HDC screen,memory;HBITMAP bitmap,oldbitmap;DWORD *bits;
    BITMAPINFO info={0};int fail=0;const int qualities[]={DEFAULT_QUALITY,NONANTIALIASED_QUALITY,ANTIALIASED_QUALITY,CLEARTYPE_QUALITY};
    if(argc==2&&(!wcscmp(argv[1],L"--smoothing-off")||!wcscmp(argv[1],L"--smoothing-on")))
    {smoothing=!wcscmp(argv[1],L"--smoothing-on");return !SystemParametersInfoW(SPI_SETFONTSMOOTHING,smoothing,NULL,SPIF_UPDATEINIFILE);}
    if(argc!=1||!SystemParametersInfoW(SPI_GETFONTSMOOTHING,0,&smoothing,0))return 2;
    cls.lpfnWndProc=DefWindowProcW;cls.hInstance=GetModuleHandleW(NULL);cls.lpszClassName=L"Win4WineMonoAdvance";
    if(!RegisterClassW(&cls))return 2;
    hwnd=CreateWindowW(cls.lpszClassName,L"GDI mono advance regression",WS_POPUP|WS_VISIBLE,20,20,WIDTH,HEIGHT,NULL,NULL,cls.hInstance,NULL);
    if(!hwnd)return 2;
    UpdateWindow(hwnd);screen=GetDC(hwnd);memory=CreateCompatibleDC(screen);
    info.bmiHeader.biSize=sizeof(info.bmiHeader);info.bmiHeader.biWidth=WIDTH;info.bmiHeader.biHeight=-HEIGHT;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    bitmap=CreateDIBSection(memory,&info,DIB_RGB_COLORS,(void **)&bits,NULL,0);oldbitmap=SelectObject(memory,bitmap);
    printf("optin=%d smoothing=%d\n",enabled,smoothing);
    uint64_t dib_hashes[4][6];
    for(unsigned q=0;q<sizeof(qualities)/sizeof(qualities[0]);q++)for(int surface=0;surface<2;surface++)
    {
        HDC dc=surface?screen:memory;LOGFONTW lf={0};HFONT font,oldfont;
        uint64_t natural,explicit_dx,pdy,extra,explicit_extra,vertical;
        lf.lfHeight=-13;lf.lfWeight=700;lf.lfQuality=qualities[q];lf.lfCharSet=DEFAULT_CHARSET;lstrcpyW(lf.lfFaceName,L"MS UI Gothic");
        font=CreateFontIndirectW(&lf);oldfont=SelectObject(dc,font);
        natural=render(dc,memory,bits,0,0);explicit_dx=render(dc,memory,bits,1,0);pdy=render(dc,memory,bits,2,0);
        vertical=render(dc,memory,bits,4,0);
        extra=render(dc,memory,bits,3,2);explicit_extra=render(dc,memory,bits,1,2);
        printf("quality=%d surface=%s natural=%016llx explicit=%016llx pdy=%016llx extra=%016llx explicit-extra=%016llx vertical-pdy=%016llx\n",
                qualities[q],surface?"screen":"DIB",(unsigned long long)natural,(unsigned long long)explicit_dx,
                (unsigned long long)pdy,(unsigned long long)extra,(unsigned long long)explicit_extra,(unsigned long long)vertical);
        uint64_t results[]={natural,explicit_dx,pdy,extra,explicit_extra,vertical};
        for(int i=0;i<6;i++)
        {
            if(!surface)dib_hashes[q][i]=results[i];
            else if(dib_hashes[q][i]!=results[i])
            {fprintf(stderr,"FAIL DIB/screen q=%d method=%d\n",qualities[q],i);fail=1;}
        }
        if(explicit_dx!=pdy||extra!=explicit_extra){fprintf(stderr,"FAIL explicit/PDY/char-extra q=%d surface=%d\n",qualities[q],surface);fail=1;}
        if(enabled&&(qualities[q]==NONANTIALIASED_QUALITY||(qualities[q]==DEFAULT_QUALITY&&!smoothing))&&natural!=explicit_dx)
        {fprintf(stderr,"FAIL mono natural/metric advance q=%d surface=%d\n",qualities[q],surface);fail=1;}
        SelectObject(dc,oldfont);DeleteObject(font);
    }
    SelectObject(memory,oldbitmap);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(hwnd,screen);DestroyWindow(hwnd);
    if(!fail)puts("PASS: GDI advance/explicit dx/PDY/char-extra");
    return fail;
}
