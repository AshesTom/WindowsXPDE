/* Public work-area/cache regression, driven by an owned X11 test display. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
static int settings,display;
static LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
 if(msg==WM_SETTINGCHANGE && wp==SPI_SETWORKAREA)settings++;
 if(msg==WM_DISPLAYCHANGE)display++;
 return DefWindowProcW(hwnd,msg,wp,lp);
}
static void pump(DWORD duration)
{
 DWORD end=GetTickCount()+duration;MSG msg;
 do{while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(10);}while((LONG)(end-GetTickCount())>0);
}
int wmain(int argc,WCHAR **argv)
{
 WNDCLASSW wc={0};HWND hwnd,follower;RECT initial_spi,initial_monitor,initial_max,custom={11,13,311,213};MONITORINFO info={0};int last=0,stale=argc>2,previous_settings;DWORD deadline;
 if(argc<2)return 2;
 info.cbSize=sizeof(info);
 wc.lpfnWndProc=procedure;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"Win4WineWorkAreaProbe";if(!RegisterClassW(&wc))return 2;
 hwnd=CreateWindowW(wc.lpszClassName,L"Public work area regression",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,320,160,NULL,NULL,wc.hInstance,NULL);if(!hwnd)return 2;
 /* stays maximized: it must follow each new work area by itself, as on Windows */
 follower=CreateWindowW(wc.lpszClassName,L"Maximized follower",WS_OVERLAPPEDWINDOW|WS_VISIBLE,120,120,320,160,NULL,NULL,wc.hInstance,NULL);if(!follower)return 2;
 ShowWindow(follower,SW_MAXIMIZE);
 pump(500);SystemParametersInfoW(SPI_GETWORKAREA,0,&initial_spi,0);GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTOPRIMARY),&info);initial_monitor=info.rcWork;
 ShowWindow(hwnd,SW_MAXIMIZE);pump(100);GetWindowRect(hwnd,&initial_max);ShowWindow(hwnd,SW_RESTORE);pump(100);previous_settings=settings;
 printf("READY spi=%ld,%ld,%ld,%ld monitor=%ld,%ld,%ld,%ld max=%ld,%ld settings=%d display=%d\n",initial_spi.left,initial_spi.top,initial_spi.right,initial_spi.bottom,initial_monitor.left,initial_monitor.top,initial_monitor.right,initial_monitor.bottom,initial_max.right-initial_max.left,initial_max.bottom-initial_max.top,settings,display);fflush(stdout);
 deadline=GetTickCount()+60000;
 while((LONG)(deadline-GetTickCount())>0){int phase=0;long l,t,r,b;FILE *control=_wfopen(argv[1],L"rb");pump(20);
  if(control){if(fscanf(control,"%d %ld %ld %ld %ld",&phase,&l,&t,&r,&b)!=5)phase=0;fclose(control);}
  if(phase<=last)continue;
  if(phase==10){DestroyWindow(follower);DestroyWindow(hwnd);puts("PASS: dynamic work area, explicit override, property dedup, maximized windows follow and unchanged display mode");return 0;}
  RECT requested={l,t,r,b},expected_spi=stale?initial_spi:requested,expected_monitor=stale?initial_monitor:requested,got={0},maximum={0};BOOL match=FALSE;DWORD until=GetTickCount()+7000;
  if(phase==8&&!SystemParametersInfoW(SPI_SETWORKAREA,0,&custom,0))return 3;
  if(phase>=8)expected_spi=custom;
  do{pump(20);SystemParametersInfoW(SPI_GETWORKAREA,0,&got,0);GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTOPRIMARY),&info);match=EqualRect(&got,&expected_spi)&&EqualRect(&info.rcWork,&expected_monitor);if(match)break;}while((LONG)(until-GetTickCount())>0);
  if(!match){printf("FAIL phase=%d spi=%ld,%ld,%ld,%ld monitor=%ld,%ld,%ld,%ld\n",phase,got.left,got.top,got.right,got.bottom,info.rcWork.left,info.rcWork.top,info.rcWork.right,info.rcWork.bottom);fflush(stdout);return 4;}
  pump(500);
  if((phase==2||phase==4)&&settings!=previous_settings){printf("FAIL same-property storm settings=%d previous=%d\n",settings,previous_settings);fflush(stdout);return 5;}
  if(display){printf("FAIL synthetic display-change count=%d\n",display);fflush(stdout);return 6;}
  ShowWindow(hwnd,SW_MAXIMIZE);pump(100);GetWindowRect(hwnd,&maximum);ShowWindow(hwnd,SW_RESTORE);pump(100);
  LONG expected_width=initial_max.right-initial_max.left+(expected_monitor.right-expected_monitor.left)-(initial_monitor.right-initial_monitor.left);
  LONG expected_height=initial_max.bottom-initial_max.top+(expected_monitor.bottom-expected_monitor.top)-(initial_monitor.bottom-initial_monitor.top);
  if(maximum.right-maximum.left!=expected_width||maximum.bottom-maximum.top!=expected_height){printf("FAIL maximize phase=%d got=%ld,%ld expected=%ld,%ld\n",phase,maximum.right-maximum.left,maximum.bottom-maximum.top,expected_width,expected_height);fflush(stdout);return 7;}
  {RECT follow={0};until=GetTickCount()+3000;do{pump(20);GetWindowRect(follower,&follow);}while(!EqualRect(&follow,&maximum)&&(LONG)(until-GetTickCount())>0);
   if(!EqualRect(&follow,&maximum)){printf("FAIL maximized follower phase=%d got=%ld,%ld,%ld,%ld expected=%ld,%ld,%ld,%ld\n",phase,follow.left,follow.top,follow.right,follow.bottom,maximum.left,maximum.top,maximum.right,maximum.bottom);fflush(stdout);return 9;}}
  printf("STEP %d spi=%ld,%ld,%ld,%ld monitor=%ld,%ld,%ld,%ld max=%ld,%ld settings=%d display=%d\n",phase,got.left,got.top,got.right,got.bottom,info.rcWork.left,info.rcWork.top,info.rcWork.right,info.rcWork.bottom,maximum.right-maximum.left,maximum.bottom-maximum.top,settings,display);fflush(stdout);previous_settings=settings;last=phase;
 }
 puts("FAIL control timeout");return 8;
}
