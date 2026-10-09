/* Public API regression check for the XP classic Start sizing shim. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
int wmain(void)
{
    HWND button = CreateWindowW(L"Button", L"Start", WS_POPUP | BS_PUSHBUTTON,
                                0, 0, 0, 0, NULL, NULL, GetModuleHandleW(NULL), NULL);
    SIZE size = {0};
    HFONT font;
    LRESULT result;
    if (!button) return 1;
    if (SendMessageW(button, BCM_GETIDEALSIZE, 0, 0)) return 2;
    font = CreateFontW(-24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       0, 0, 0, 0, L"Tahoma");
    SendMessageW(button, WM_SETFONT, (WPARAM)font, FALSE);
    result = SendMessageW(button, BCM_GETIDEALSIZE, 0, (LPARAM)&size);
    printf("result=%ld width=%ld height=%ld\n", (long)result, size.cx, size.cy);
    DestroyWindow(button);
    DeleteObject(font);
    return !(result && size.cx > 24 && size.cy >= 24);
}
