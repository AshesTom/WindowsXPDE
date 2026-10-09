/* Public DrawText vertical centering: round the remaining space once. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>
#include <string.h>
int wmain(void)
{
    HDC dc=CreateCompatibleDC(NULL);BITMAPINFO info={0};DWORD *bits,snapshot[200*80];
    HBITMAP bmp,oldbmp;HFONT font=NULL,oldfont=NULL;TEXTMETRICW tm={0};int fail=0;
    if(!GetEnvironmentVariableW(L"WIN4WINE_EXTERNAL_SHELL",NULL,0))return 2;
    info.bmiHeader.biSize=sizeof(info.bmiHeader);info.bmiHeader.biWidth=200;info.bmiHeader.biHeight=-80;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void **)&bits,NULL,0);oldbmp=SelectObject(dc,bmp);
    for(int height=11;height<21;height++)
    {
        font=CreateFontW(-height,0,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,NONANTIALIASED_QUALITY,0,L"Arial");
        oldfont=SelectObject(dc,font);GetTextMetricsW(dc,&tm);
        if(tm.tmHeight%2)break;
        SelectObject(dc,oldfont);DeleteObject(font);font=NULL;
    }
    if(!font)return 2;
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    for(int extra=12;extra<=13;extra++)
    {
        RECT r={10,4,180,4+tm.tmHeight+extra},top=r;
        memset(bits,0,sizeof(snapshot));DrawTextW(dc,L"Agjp",-1,&r,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);GdiFlush();memcpy(snapshot,bits,sizeof(snapshot));
        top.top+=(top.bottom-top.top-tm.tmHeight)/2;
        memset(bits,0,sizeof(snapshot));DrawTextW(dc,L"Agjp",-1,&top,DT_SINGLELINE|DT_TOP|DT_NOPREFIX);GdiFlush();
        if(memcmp(snapshot,bits,sizeof(snapshot))){fprintf(stderr,"FAIL DT_VCENTER remaining-space rounding tmHeight=%ld extra=%d\n",tm.tmHeight,extra);fail=1;}
    }
    SelectObject(dc,oldfont);SelectObject(dc,oldbmp);DeleteObject(font);DeleteObject(bmp);DeleteDC(dc);
    if(!fail)printf("PASS: DT_VCENTER rounds remaining space once, text height %ld\n",tm.tmHeight);
    return fail;
}
