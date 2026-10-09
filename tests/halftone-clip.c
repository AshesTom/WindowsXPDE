/* Self-authored pixels: HALFTONE clipping must preserve the stretch mapping. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef struct { HDC dc;HBITMAP bitmap;HGDIOBJ old;int width,height;BITMAPINFO *info;void *bits; } SURFACE;
static SURFACE surface(int width,int height,int format,BOOL bottomup)
{
    struct {BITMAPINFOHEADER header;RGBQUAD colors[256];} info={0};void *bits;
    const int depths[]={32,24,16,16,8,4,1,32};SURFACE s={0};
    s.width=width;s.height=height;s.dc=CreateCompatibleDC(NULL);
    info.header.biSize=sizeof(info.header);info.header.biWidth=width;info.header.biHeight=bottomup?height:-height;
    info.header.biPlanes=1;info.header.biBitCount=depths[format];
    if(format==3||format==7)
    {DWORD *masks=(DWORD*)info.colors;info.header.biCompression=BI_BITFIELDS;masks[0]=format==3?0xf800:0xff;masks[1]=format==3?0x7e0:0xff00;masks[2]=format==3?0x1f:0xff0000;}
    if(depths[format]<=8)
    {
        int count=1<<depths[format];info.header.biClrUsed=count;
        for(int i=0;i<count;i++)info.colors[i]=(RGBQUAD){i*255/(count-1),i*255/(count-1),i*255/(count-1),0};
    }
    s.bitmap=CreateDIBSection(s.dc,(BITMAPINFO*)&info,DIB_RGB_COLORS,&bits,NULL,0);s.old=SelectObject(s.dc,s.bitmap);
    if(!s.dc||!s.bitmap){fputs("DIB allocation failed\n",stderr);ExitProcess(2);}
    s.info=malloc(sizeof(info));if(!s.info)ExitProcess(2);memcpy(s.info,&info,sizeof(info));s.bits=bits;
    return s;
}
static void destroy(SURFACE *s){SelectObject(s->dc,s->old);DeleteObject(s->bitmap);DeleteDC(s->dc);free(s->info);}
static int run(int format,int scale,int mirror,BOOL bottomup,int mode,BOOL dibits,uint32_t *hash)
{
    const int sizes[][4]={{11,21,1288,29},{137,99,61,37},{37,19,143,61},{31,23,31,23}};
    int sw=sizes[scale][0],sh=sizes[scale][1],dw=sizes[scale][2],dh=sizes[scale][3];
    SURFACE source=surface(sw+16,sh+16,format,bottomup),full=surface(dw+40,dh+40,format,bottomup),clipped=surface(dw+40,dh+40,format,bottomup);
    HBRUSH brush=CreateSolidBrush(RGB(32,80,180));RECT body={0,0,dw+40,dh+40};
    FillRect(full.dc,&body,brush);FillRect(clipped.dc,&body,brush);DeleteObject(brush);
    for(int y=0;y<source.height;y++)for(int x=0;x<source.width;x++)
    {int shade=(x*19+y*23+(x*y)%31)&255;SetPixelV(source.dc,x,y,RGB(shade,(shade*3)&255,255-shade));}
    COLORREF sentinel=GetPixel(clipped.dc,0,0);
    int left=16+dw/5,top=11+dh/4,right=min(16+dw,left+119),bottom=min(11+dh,top+24);
    HRGN region=CreateRectRgn(left,top,right,bottom),hole=CreateRectRgn(left+7,top+3,min(right,left+12),min(bottom,top+8));
    CombineRgn(region,region,hole,RGN_DIFF);DeleteObject(hole);
    SetViewportOrgEx(full.dc,7,5,NULL);SetViewportOrgEx(clipped.dc,7,5,NULL);SelectClipRgn(clipped.dc,region);
    SetStretchBltMode(full.dc,mode);SetStretchBltMode(clipped.dc,mode);
    int sx=3+(mirror&1?sw-1:0),sy=3+(mirror&2?sh-1:0),width=mirror&1?-sw:sw,height=mirror&2?-sh:sh;
    int dx=9+(mirror&4?dw-1:0),dy=6+(mirror&8?dh-1:0),dwidth=mirror&4?-dw:dw,dheight=mirror&8?-dh:dh;
    BOOL result;
    if(dibits)
    {
        int a=StretchDIBits(full.dc,dx,dy,dwidth,dheight,sx,sy,width,height,source.bits,source.info,DIB_RGB_COLORS,SRCCOPY);
        int b=StretchDIBits(clipped.dc,dx,dy,dwidth,dheight,sx,sy,width,height,source.bits,source.info,DIB_RGB_COLORS,SRCCOPY);
        result=a!=0&&a!=(int)GDI_ERROR&&b!=0&&b!=(int)GDI_ERROR;
    }
    else result=StretchBlt(full.dc,dx,dy,dwidth,dheight,source.dc,sx,sy,width,height,SRCCOPY)
                  &&StretchBlt(clipped.dc,dx,dy,dwidth,dheight,source.dc,sx,sy,width,height,SRCCOPY);
    if(!result)return 2;
    GdiFlush();SelectClipRgn(clipped.dc,NULL);SetViewportOrgEx(full.dc,0,0,NULL);SetViewportOrgEx(clipped.dc,0,0,NULL);
    int differences=0,outside=0;
    for(int y=max(0,top-3);y<min(clipped.height,bottom+3);y++)for(int x=max(0,left-3);x<min(clipped.width,right+3);x++)
    {
        COLORREF actual=GetPixel(clipped.dc,x,y),expected=PtInRegion(region,x,y)?GetPixel(full.dc,x,y):sentinel;
        *hash=(*hash^actual)*16777619u;
        if(actual!=expected){differences++;if(!PtInRegion(region,x,y))outside++;}
    }
    DeleteObject(region);destroy(&source);destroy(&full);destroy(&clipped);
    if(differences)fprintf(stderr,"difference api=%d format=%d scale=%d mirror=%d bottomup=%d mode=%d pixels=%d outside=%d\n",dibits,format,scale,mirror,bottomup,mode,differences,outside);
    return differences?1:0;
}
int wmain(void)
{
    int failed=0,cases=0;BOOL enabled=GetEnvironmentVariableW(L"WIN4WINE_EXTERNAL_SHELL",NULL,0)>0;
    for(int dibits=0;dibits<2;dibits++)for(int mode=COLORONCOLOR;mode<=HALFTONE;mode++)
    {
        uint32_t hash=2166136261u;int mismatches=0,count=0;
        for(int format=0;format<8;format++)for(int scale=0;scale<4;scale++)for(int mirror=0;mirror<(format?4:16);mirror++)for(int bottomup=0;bottomup<2;bottomup++)
        {int result=run(format,scale,mirror,bottomup,mode,dibits,&hash);if(result==2)return 2;mismatches+=result;cases++;count++;}
        printf("optin=%d api=%s mode=%d cases=%d mismatches=%d hash=%08lx\n",enabled,dibits?"StretchDIBits":"StretchBlt",mode,count,mismatches,(unsigned long)hash);
        if(mismatches&&(mode!=HALFTONE||enabled))failed=1;
    }
    if(!failed)printf("PASS: %d self-authored stretch cases; %s/8 formats/mirror/up-down DIB/clip hole/viewport\n",cases,
                       enabled?"HALFTONE and COLORONCOLOR":"COLORONCOLOR; opt-out HALFTONE measurements recorded");
    return failed;
}
