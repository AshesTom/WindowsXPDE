/* Public band geometry and theme tiling regression; no embedded theme data. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
static DWORD band_style;static UINT header_size=10;static int band_count=1;
static LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
 RECT all;GetClientRect(hwnd,&all);
 switch(msg){case RB_GETBANDCOUNT:return band_count;
 case RB_GETBANDINFOW:((REBARBANDINFOW*)lp)->fStyle=band_style;((REBARBANDINFOW*)lp)->cxHeader=header_size;return TRUE;
 case RB_GETRECT:*(RECT*)lp=all;return TRUE;
 case WM_PRINTCLIENT:FillRect((HDC)wp,&all,GetSysColorBrush(COLOR_BTNFACE));return 23;}
 return DefWindowProcW(hwnd,msg,wp,lp);
}
int wmain(void)
{
 WNDCLASSW wc={0};HWND tray;HDC actual,expected;HBITMAP a,b;HGDIOBJ oa,ob;DWORD *ap,*bp;BITMAPINFO bi={0};int failures=0;
 wc.hInstance=GetModuleHandleW(NULL);wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"Shell_TrayWnd";if(!RegisterClassW(&wc))return 2;
 wc.lpfnWndProc=procedure;wc.lpszClassName=L"ReBarWindow32";if(!RegisterClassW(&wc))return 2;
 tray=CreateWindowW(L"Shell_TrayWnd",L"",WS_POPUP,0,0,240,240,NULL,NULL,wc.hInstance,NULL);
 actual=CreateCompatibleDC(NULL);expected=CreateCompatibleDC(NULL);bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=210;bi.bmiHeader.biHeight=-210;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 a=CreateDIBSection(actual,&bi,DIB_RGB_COLORS,(void**)&ap,NULL,0);b=CreateDIBSection(expected,&bi,DIB_RGB_COLORS,(void**)&bp,NULL,0);oa=SelectObject(actual,a);ob=SelectObject(expected,b);
 for(int vertical=0;vertical<2;vertical++)for(int mode=0;mode<5;mode++){
  int width=vertical?30:200,height=vertical?200:30,delta=0;RECT all={0,0,width,height},header=all,grip;SIZE size;POINT origin;RECT clip;HTHEME theme;
  HWND rebar=CreateWindowW(L"ReBarWindow32",L"",WS_CHILD|(vertical?CCS_VERT:0),0,0,width,height,tray,NULL,wc.hInstance,NULL);
  band_style=RBBS_HIDETITLE|RBBS_GRIPPERALWAYS;header_size=10;band_count=1;
  if(mode==1)band_style|=RBBS_NOGRIPPER;
  if(mode==2)band_style|=RBBS_FIXEDSIZE;
  if(mode==3)header_size=0;
  if(mode==4)band_style&=~RBBS_GRIPPERALWAYS;
  if(FAILED(SetWindowTheme(rebar,L"Taskbar",NULL)))return 2;
  theme=OpenThemeData(rebar,L"Rebar");if(!theme)return 2;
  for(int i=0;i<210*210;i++)ap[i]=bp[i]=0x123456;
  SetViewportOrgEx(actual,5,3,NULL);SetViewportOrgEx(expected,5,3,NULL);IntersectClipRect(actual,1,1,width-1,height-1);IntersectClipRect(expected,1,1,width-1,height-1);
  FillRect(expected,&all,GetSysColorBrush(COLOR_BTNFACE));
  if(!mode){int part=vertical?RP_GRIPPERVERT:RP_GRIPPER;if(FAILED(GetThemePartSize(theme,expected,part,0,NULL,TS_TRUE,&size)))return 2;
   if(vertical)header.bottom=10;else header.right=10;DrawThemeParentBackground(rebar,expected,&header);
   if(vertical)grip=(RECT){header.left,header.top,header.right,header.top+size.cy};
   else grip=(RECT){header.left,header.top,header.left+size.cx,header.bottom};
   DrawThemeBackground(theme,expected,part,0,&grip,&header);
  }
  if(SendMessageW(rebar,WM_PRINTCLIENT,(WPARAM)actual,PRF_CLIENT)!=23)failures++;
  GetViewportOrgEx(actual,&origin);GetClipBox(actual,&clip);if(origin.x!=5||origin.y!=3||clip.left!=1||clip.top!=1||clip.right!=width-1||clip.bottom!=height-1)failures++;
  GdiFlush();for(int i=0;i<210*210;i++)if((ap[i]&0xffffff)!=(bp[i]&0xffffff))delta++;
  printf("axis=%d mode=%d pixel-delta=%d\n",vertical,mode,delta);if(delta)failures++;
  SelectClipRgn(actual,NULL);SelectClipRgn(expected,NULL);SetViewportOrgEx(actual,0,0,NULL);SetViewportOrgEx(expected,0,0,NULL);CloseThemeData(theme);DestroyWindow(rebar);
 }
 SelectObject(actual,oa);SelectObject(expected,ob);DeleteObject(a);DeleteObject(b);DeleteDC(actual);DeleteDC(expected);DestroyWindow(tray);
 puts(failures?"FAIL: gripper public full-axis geometry":"PASS: horizontal/vertical themed tiling, locked/fixed/headerless exclusions, translated clipped DC and sentinels");return !!failures;
}
