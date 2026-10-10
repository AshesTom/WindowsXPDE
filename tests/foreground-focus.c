/*
 * Report the foreground window, its thread's focus, the focused item of a
 * focused list view and the keyboard-cue setting. Used to compare which
 * window XP and Wine activate after the last folder window closes.
 * Usage: foreground-focus [OUTPUT] (XP: A:\FF.EXE COM1). Waits two seconds
 * so that the Run dialog that started it has gone.
 */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

static void show(const char *what, HWND hwnd)
{
    char class_name[64] = "", text[64] = "";

    if (hwnd)
    {
        GetClassNameA(hwnd, class_name, sizeof(class_name));
        GetWindowTextA(hwnd, text, sizeof(text));
    }
    printf("%s %p class=%s text=%s", what, (void *)hwnd, class_name, text);
    if (hwnd) printf(" uistate=%#lx", (unsigned long)SendMessageA(hwnd, WM_QUERYUISTATE, 0, 0));
    printf("\n");
}

int main(int argc, char **argv)
{
    GUITHREADINFO info;
    char class_name[64] = "";
    BOOL cues = FALSE;
    HWND foreground;

    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    if (argc > 1 && !freopen(argv[1], "w", stdout)) return 1;
    Sleep(2000);
    foreground = GetForegroundWindow();
    show("foreground", foreground);
    if (foreground && GetGUIThreadInfo(GetWindowThreadProcessId(foreground, NULL), &info))
    {
        show("active", info.hwndActive);
        show("focus", info.hwndFocus);
    }
    if (info.hwndFocus && GetClassNameA(info.hwndFocus, class_name, sizeof(class_name)) &&
        !lstrcmpA(class_name, "SysListView32"))
        printf("listview focused=%ld selected=%ld count=%ld\n",
               (long)SendMessageA(info.hwndFocus, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_FOCUSED),
               (long)SendMessageA(info.hwndFocus, LVM_GETSELECTEDCOUNT, 0, 0),
               (long)SendMessageA(info.hwndFocus, LVM_GETITEMCOUNT, 0, 0));
    SystemParametersInfoA(SPI_GETKEYBOARDCUES, 0, &cues, 0);
    printf("keyboardcues=%d\n", cues);
    fflush(stdout);
    return 0;
}
