/* Public shell-hook regression: excluded foregrounds must not create tasks. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>
enum { APP, OTHER, TOOL, UNOWNED, OWNED_APP, DIALOG, COUNT };
static UINT shell_message;
static HWND windows[COUNT];
static unsigned created[COUNT], activated[COUNT];
static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == shell_message)
        for (unsigned i=0; i<COUNT; i++)
            if ((HWND)lp == windows[i] && windows[i])
            {
                if (wp == HSHELL_WINDOWCREATED) created[i]++;
                if (wp == HSHELL_WINDOWACTIVATED) activated[i]++;
            }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static void pump(DWORD duration)
{
    DWORD started=GetTickCount(); MSG msg;
    while (GetTickCount()-started<duration)
    {
        while (PeekMessageW(&msg,NULL,0,0,PM_REMOVE))
        { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}
static HWND own(unsigned slot, DWORD ex, HWND owner)
{
    static const WCHAR *titles[COUNT]={L"App A",L"Other app",L"Owned tool",L"Unowned tool",L"Owned app B",L"Dialog D"};
    windows[slot]=CreateWindowExW(ex,L"Win4WineActivationFixture",titles[slot],
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,100+(int)slot*30,100+(int)slot*25,320,180,
        owner,NULL,GetModuleHandleW(NULL),NULL);
    return windows[slot];
}
int wmain(void)
{
    WNDCLASSW wc={0}; HWND observer; unsigned before; BOOL ok=TRUE, result;
    wc.lpfnWndProc=proc; wc.hInstance=GetModuleHandleW(NULL); wc.lpszClassName=L"Win4WineActivationFixture";
    if (!RegisterClassW(&wc)) return 2;
    shell_message=RegisterWindowMessageW(L"SHELLHOOK");
    observer=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Observer",WS_POPUP,0,0,1,1,NULL,NULL,wc.hInstance,NULL);
    if (!observer || !RegisterShellHookWindow(observer)) return 2;
    if (!own(APP,0,NULL) || !own(OTHER,0,NULL)) return 2;
    SetForegroundWindow(windows[OTHER]); pump(1200);
    before=activated[APP];
    if (!own(TOOL,WS_EX_TOOLWINDOW,windows[APP])) return 2;
    SetForegroundWindow(windows[TOOL]); pump(1200);
    result=created[APP]==1 && created[TOOL]==0 && activated[TOOL]==0 && activated[APP]>before;
    printf("%s owned tool activates its included app owner (A %u->%u tool=%u)\n",result?"PASS":"FAIL",before,activated[APP],activated[TOOL]); ok &= result;
    if (!own(UNOWNED,WS_EX_TOOLWINDOW,NULL)) return 2;
    SetForegroundWindow(windows[UNOWNED]); pump(1200);
    result=created[UNOWNED]==0 && activated[UNOWNED]==0;
    printf("%s unowned tool creates no task through activation\n",result?"PASS":"FAIL"); ok &= result;
    if (!own(OWNED_APP,WS_EX_APPWINDOW,windows[APP]) || !own(DIALOG,WS_EX_TOOLWINDOW,windows[OWNED_APP])) return 2;
    pump(800); /* Let initial X11 map/focus requests settle before the comparison. */
    SetForegroundWindow(windows[OTHER]); pump(1200);
    printf("ownership A=%p B=%p D=%p D-owner=%p B-owner=%p other=%p foreground=%p B-created=%u\n",
        windows[APP],windows[OWNED_APP],windows[DIALOG],GetWindow(windows[DIALOG],GW_OWNER),
        GetWindow(windows[OWNED_APP],GW_OWNER),windows[OTHER],GetForegroundWindow(),created[OWNED_APP]);
    result=GetForegroundWindow()==windows[OTHER];
    printf("%s other app is foreground before dialog activation\n",result?"PASS":"FAIL"); ok &= result;
    before=activated[OWNED_APP];
    printf("set foreground D result=%d\n",SetForegroundWindow(windows[DIALOG])); pump(1200);
    printf("foreground after D=%p\n",GetForegroundWindow());
    result=created[OWNED_APP]==1 && activated[OWNED_APP]>before && created[DIALOG]==0 && activated[DIALOG]==0;
    printf("%s dialog activates nearest included APPWINDOW owner B (B %u->%u dialog=%u)\n",result?"PASS":"FAIL",before,activated[OWNED_APP],activated[DIALOG]); ok &= result;
    for (int i=COUNT-1;i>=0;i--) DestroyWindow(windows[i]);
    pump(400); DeregisterShellHookWindow(observer); DestroyWindow(observer);
    return ok?0:1;
}
