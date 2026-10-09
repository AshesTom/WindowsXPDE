/* Select XP's Start panel or folder task pane through the public shell
 * settings API. Usage: menu-settings classic|xp|webview-on|webview-off */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <wchar.h>
#ifndef SSF_STARTPANELON
#define SSF_STARTPANELON 0x00200000
#endif

int wmain(int argc, WCHAR **argv)
{
    SHELLSTATE state = {0};
    BOOL panel;
    if (argc == 2 && (!wcscmp(argv[1], L"webview-on") || !wcscmp(argv[1], L"webview-off")))
    {
        BOOL webview = !wcscmp(argv[1], L"webview-on");
        state.fWebView = webview;
        SHGetSetSettings(&state, SSF_WEBVIEW, TRUE);
        state.fWebView = !webview;
        SHGetSetSettings(&state, SSF_WEBVIEW, FALSE);
        if (!!state.fWebView != webview)
        {
            fprintf(stderr, "Folder task setting was not saved\n");
            return 1;
        }
        return 0;
    }
    if (argc != 2 || (wcscmp(argv[1], L"classic") && wcscmp(argv[1], L"xp"))) return 2;
    panel = !wcscmp(argv[1], L"xp");
    state.fStartPanelOn = panel;
    SHGetSetSettings(&state, SSF_STARTPANELON, TRUE);
    state.fStartPanelOn = !panel;
    SHGetSetSettings(&state, SSF_STARTPANELON, FALSE);
    if (!!state.fStartPanelOn != panel)
    {
        fprintf(stderr, "Start menu setting was not saved\n");
        return 1;
    }
    return 0;
}
