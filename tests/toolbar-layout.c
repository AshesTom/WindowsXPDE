/* Print the child layout of an open folder window with public messages.
 * Usage: toolbar-layout [COM1|FILE]. Only window classes, rectangles relative to
 * the folder window and values that messages return directly are printed. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

static HANDLE out = INVALID_HANDLE_VALUE;
static HWND top;

static void emit(const char *text)
{
    DWORD written;
    if (out == INVALID_HANDLE_VALUE) fputs(text, stdout);
    else WriteFile(out, text, (DWORD)strlen(text), &written, NULL);
}

static BOOL CALLBACK child(HWND hwnd, LPARAM param)
{
    char cls[64], line[512];
    RECT rect, frame;
    HFONT font;
    LOGFONTA lf = {0};
    int n;
    (void)param;
    GetClassNameA(hwnd, cls, sizeof(cls));
    GetWindowRect(hwnd, &rect);
    GetWindowRect(top, &frame);
    OffsetRect(&rect, -frame.left, -frame.top);
    n = snprintf(line, sizeof(line), "%s (%ld,%ld)-(%ld,%ld) style=%08lx", cls, rect.left, rect.top, rect.right,
                 rect.bottom, (unsigned long)GetWindowLongA(hwnd, GWL_STYLE));
    if ((font = (HFONT)SendMessageA(hwnd, WM_GETFONT, 0, 0)) && GetObjectA(font, sizeof(lf), &lf))
        n += snprintf(line + n, sizeof(line) - n, " font=%ld/%ld", lf.lfHeight, lf.lfWeight);
    if (!strcmp(cls, "ToolbarWindow32"))
    {
        DWORD size = (DWORD)SendMessageA(hwnd, TB_GETBUTTONSIZE, 0, 0);
        DWORD padding = (DWORD)SendMessageA(hwnd, TB_GETPADDING, 0, 0);
        n += snprintf(line + n, sizeof(line) - n, " buttons=%ld size=%ux%u padding=%ux%u ex=%08lx rows=%ld",
                      (long)SendMessageA(hwnd, TB_BUTTONCOUNT, 0, 0), LOWORD(size), HIWORD(size), LOWORD(padding),
                      HIWORD(padding), (unsigned long)SendMessageA(hwnd, TB_GETEXTENDEDSTYLE, 0, 0),
                      (long)SendMessageA(hwnd, TB_GETROWS, 0, 0));
    }
    else if (strstr(cls, "ComboBox") && !strstr(cls, "Ex"))
        n += snprintf(line + n, sizeof(line) - n, " selection=%ld item=%ld current=%ld count=%ld",
                      (long)SendMessageA(hwnd, CB_GETITEMHEIGHT, (WPARAM)-1, 0),
                      (long)SendMessageA(hwnd, CB_GETITEMHEIGHT, 0, 0),
                      (long)SendMessageA(hwnd, CB_GETCURSEL, 0, 0),
                      (long)SendMessageA(hwnd, CB_GETCOUNT, 0, 0));
    else if (!strcmp(cls, "ReBarWindow32"))
    {
        long count = (long)SendMessageA(hwnd, RB_GETBANDCOUNT, 0, 0);
        n += snprintf(line + n, sizeof(line) - n, " bands=%ld rows=%ld height=%ld", count,
                      (long)SendMessageA(hwnd, RB_GETROWCOUNT, 0, 0), (long)SendMessageA(hwnd, RB_GETBARHEIGHT, 0, 0));
        for (long i = 0; i < count; i++)
            n += snprintf(line + n, sizeof(line) - n, " row%ld=%ld", i,
                          (long)SendMessageA(hwnd, RB_GETROWHEIGHT, i, 0));
    }
    snprintf(line + n, sizeof(line) - n, "\r\n");
    emit(line);
    return TRUE;
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        out = CreateFileA(argv[1], GENERIC_WRITE, 0, NULL, _strnicmp(argv[1], "COM", 3) ? CREATE_ALWAYS : OPEN_EXISTING,
                          0, NULL);
        if (out != INVALID_HANDLE_VALUE && !_strnicmp(argv[1], "COM", 3))
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
    emit("TOOLBARLAYOUT\r\n");
    if ((top = FindWindowA("CabinetWClass", NULL)))
    {
        char line[128];
        RECT rect;
        GetWindowRect(top, &rect);
        snprintf(line, sizeof(line), "CabinetWClass %ldx%ld\r\n", rect.right - rect.left, rect.bottom - rect.top);
        emit(line);
        EnumChildWindows(top, child, 0);
    }
    emit("TOOLBARLAYOUT complete\r\n");
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
