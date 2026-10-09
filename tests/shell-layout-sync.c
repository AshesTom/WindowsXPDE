/* Public taskbar readiness/lifetime fixture for the bridge proxy. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>
static unsigned sizes,failures;
static BOOL watching;
static LRESULT CALLBACK tray_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    if(msg==WM_SIZE&&watching)sizes++;
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static void pump(DWORD duration)
{
    DWORD started=GetTickCount();MSG msg;
    while(GetTickCount()-started<duration)
    {
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        Sleep(10);
    }
}
static void check(BOOL value,const char *label)
{printf("%s %s (notifications=%u)\n",value?"PASS":"FAIL",label,sizes);if(!value)failures++;}
int wmain(void)
{
    WNDCLASSW wc={0};HWND tray,start,rebar;WCHAR path[MAX_PATH],folder[MAX_PATH],tsv[MAX_PATH],command[MAX_PATH*3];
    HANDLE file;STARTUPINFOW si={0};PROCESS_INFORMATION process={0};BOOL launched;
    si.cb=sizeof(si);
    wc.lpfnWndProc=tray_proc;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"Shell_TrayWnd";
    if(!RegisterClassW(&wc))return 2;
    wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"ReBarWindow32";
    if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 2;
    GetTempPathW(MAX_PATH,path);GetTempFileNameW(path,L"WPL",0,folder);DeleteFileW(folder);
    if(!CreateDirectoryW(folder,NULL))return 2;
    swprintf(tsv,MAX_PATH,L"%ls\\windows.tsv",folder);file=CreateFileW(tsv,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
    if(file==INVALID_HANDLE_VALUE)return 2;
    CloseHandle(file);
    tray=CreateWindowW(L"Shell_TrayWnd",L"Layout fixture",WS_POPUP|WS_VISIBLE,0,0,300,40,NULL,NULL,wc.hInstance,NULL);
    if(!tray)return 2;
    GetModuleFileNameW(NULL,path,MAX_PATH);{WCHAR *slash=wcsrchr(path,L'\\');if(!slash)return 2;lstrcpyW(slash+1,L"proxy.exe");}
    swprintf(command,MAX_PATH*3,L"\"%ls\" \"%ls\"",path,folder);
    launched=CreateProcessW(path,command,NULL,NULL,FALSE,0,NULL,NULL,&si,&process);if(!launched)return 2;
    watching=TRUE;sizes=0;pump(900);check(sizes==0,"unready taskbar receives no notification");
    start=CreateWindowW(L"Button",L"Start",WS_CHILD|WS_VISIBLE,0,0,80,30,tray,NULL,wc.hInstance,NULL);
    rebar=CreateWindowW(L"ReBarWindow32",L"",WS_CHILD|WS_VISIBLE,80,0,200,30,tray,NULL,wc.hInstance,NULL);
    if(!start||!rebar){TerminateProcess(process.hProcess,2);return 2;}
    for(unsigned i=0;i<8;i++)
    {SetWindowPos(rebar,NULL,81+i,0,200,30,SWP_NOZORDER|SWP_NOACTIVATE);pump(100);}
    check(sizes==0,"moving child layout receives no premature notification");
    pump(1200);check(sizes==1,"ready taskbar receives one deferred notification");
    pump(900);check(sizes==1,"stable taskbar receives no repeated notification");
    ShowWindow(tray,SW_HIDE);pump(500);ShowWindow(tray,SW_SHOWNOACTIVATE);pump(900);
    check(sizes==1,"temporary hide preserves completed lifetime");
    watching=FALSE;DestroyWindow(tray);pump(500);
    tray=CreateWindowW(L"Shell_TrayWnd",L"Replacement",WS_POPUP|WS_VISIBLE,0,0,300,40,NULL,NULL,wc.hInstance,NULL);
    start=CreateWindowW(L"Button",L"Start",WS_CHILD|WS_VISIBLE,0,0,80,30,tray,NULL,wc.hInstance,NULL);
    rebar=CreateWindowW(L"ReBarWindow32",L"",WS_CHILD|WS_VISIBLE,80,0,200,30,tray,NULL,wc.hInstance,NULL);
    if(!tray||!start||!rebar){TerminateProcess(process.hProcess,2);return 2;}
    sizes=0;watching=TRUE;pump(1200);check(sizes==1,"replacement taskbar receives its own notification");
    watching=FALSE;DestroyWindow(tray);PostThreadMessageW(process.dwThreadId,WM_QUIT,0,0);
    if(WaitForSingleObject(process.hProcess,2000)!=WAIT_OBJECT_0)TerminateProcess(process.hProcess,2);
    CloseHandle(process.hProcess);CloseHandle(process.hThread);
    DeleteFileW(tsv);swprintf(tsv,MAX_PATH,L"%ls\\frames.tsv",folder);DeleteFileW(tsv);RemoveDirectoryW(folder);
    return failures?1:0;
}
