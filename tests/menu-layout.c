/* Show self-built popup menus and print the menu window and item rectangles.
 * Usage: menu-layout [COM1]. Public user32 calls only. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HANDLE out = INVALID_HANDLE_VALUE;
static HWND owner;
static HMENU current;

static void emit(const char *text)
{
    DWORD written;
    if (out == INVALID_HANDLE_VALUE) fputs(text, stdout);
    else WriteFile(out, text, (DWORD)strlen(text), &written, NULL);
}

static void CALLBACK report(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
    HWND popup = FindWindowA("#32768", NULL);
    RECT rect;
    char line[256];
    int count = GetMenuItemCount(current);
    (void)hwnd; (void)msg; (void)time;
    KillTimer(owner, id);
    if (popup && GetWindowRect(popup, &rect))
    {
        sprintf(line, " window %ld,%ld %ldx%ld\r\n", rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
        emit(line);
    }
    for (int i = 0; i < count; i++)
    {
        GetMenuItemRect(owner, current, i, &rect);
        sprintf(line, " item%d %ld,%ld %ldx%ld\r\n", i, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
        emit(line);
    }
    EndMenu();
}

static void show(const char *name, HMENU menu)
{
    char line[128];
    sprintf(line, "menu %s\r\n", name);
    emit(line);
    current = menu;
    SetTimer(owner, 1, 300, report);
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN, 100, 100, 0, owner, NULL);
    DestroyMenu(menu);
}

int main(int argc, char **argv)
{
    HMENU menu, sub;
    if (argc > 1)
    {
        out = CreateFileA(argv[1], GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (out != INVALID_HANDLE_VALUE)
        {
            DCB dcb = {.DCBlength = sizeof(dcb)};
            GetCommState(out, &dcb);
            dcb.BaudRate = CBR_115200;
            dcb.ByteSize = 8;
            dcb.Parity = NOPARITY;
            dcb.StopBits = ONESTOPBIT;
            SetCommState(out, &dcb);
        }
    }
    owner = CreateWindowA("STATIC", "menu-layout", WS_POPUP | WS_VISIBLE, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    SetForegroundWindow(owner);
    emit("MENULAYOUT\r\n");

    menu = CreatePopupMenu();
    AppendMenuA(menu, MF_STRING, 1, "a");
    show("one", menu);

    menu = CreatePopupMenu();
    AppendMenuA(menu, MF_STRING, 1, "abcdefghij");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, 2, "x");
    show("separator", menu);

    menu = CreatePopupMenu();
    sub = CreatePopupMenu();
    AppendMenuA(sub, MF_STRING, 3, "s");
    AppendMenuA(menu, MF_STRING, 1, "abcdefghij");
    AppendMenuA(menu, MF_POPUP, (UINT_PTR)sub, "sub");
    show("submenu", menu);

    menu = CreatePopupMenu();
    AppendMenuA(menu, MF_STRING, 1, "abc\tCtrl+X");
    AppendMenuA(menu, MF_STRING | MF_CHECKED, 2, "checked");
    show("accelerator", menu);

    menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"\x65b0\x898f\x4f5c\x6210(&W)");
    AppendMenuW(menu, MF_STRING, 2, L"\x30d7\x30ed\x30d1\x30c6\x30a3(&R)");
    show("japanese", menu);

    emit("MENULAYOUT complete\r\n");
    return 0;
}
