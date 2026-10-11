/* Print the state and type of separators in menus that come from MENU and
 * MENUEX resources and from the menu functions. Usage:
 * menu-separator-test [COM1|FILE]. Stage 3 saw XP report MFS_GRAYED for
 * the separators of shell32's MENUEX menus and Wine 0. Public user32 calls
 * only; the resources are built in memory, as an RC compiler writes them. */
#include <windows.h>
#include <stdio.h>

static HANDLE out = INVALID_HANDLE_VALUE;

static void emit(const char *text)
{
    DWORD written;
    if (out == INVALID_HANDLE_VALUE) fputs(text, stdout);
    else WriteFile(out, text, lstrlenA(text), &written, NULL);
}

static void dump(const char *name, HMENU menu)
{
    char line[160];
    int count = GetMenuItemCount(menu);
    snprintf(line, sizeof(line), "%s: %d items\r\n", name, count);
    emit(line);
    for (int i = 0; i < count; i++)
    {
        MENUITEMINFOW info = {.cbSize = sizeof(info), .fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID};
        UINT flags = GetMenuState(menu, i, MF_BYPOSITION);
        GetMenuItemInfoW(menu, i, TRUE, &info);
        snprintf(line, sizeof(line), "  %d type=%#x state=%#x id=%u GetMenuState=%#x\r\n", i,
                 info.fType, info.fState, info.wID, flags);
        emit(line);
    }
}

/* MENUEX template: header (version 1, offset 4, help id), then items:
 * DWORD type, DWORD state, DWORD id, WORD flags (0x80 last, 0x01 popup),
 * NUL-terminated text, DWORD-aligned. */
static WORD *item_ex(WORD *p, DWORD type, DWORD state, DWORD id, WORD flags, const WCHAR *text)
{
    DWORD *d = (DWORD *)p;
    *d++ = type; *d++ = state; *d++ = id;
    p = (WORD *)d;
    *p++ = flags;
    while ((*p++ = *text++));
    if ((ULONG_PTR)p & 2) *p++ = 0;
    return p;
}

int main(int argc, char **argv)
{
    static WORD ex[256], old[128];
    WORD *p;
    HMENU menu;
    if (argc > 1)
    {
        int serial = !lstrcmpiA(argv[1], "COM1");
        out = CreateFileA(argv[1], GENERIC_WRITE, 0, NULL, serial ? OPEN_EXISTING : CREATE_ALWAYS, 0, NULL);
        if (serial && out != INVALID_HANDLE_VALUE)
        {
            DCB dcb = {.DCBlength = sizeof(dcb)};
            GetCommState(out, &dcb);
            dcb.BaudRate = CBR_115200; dcb.ByteSize = 8; dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;
            SetCommState(out, &dcb);
        }
    }
    emit("MENUSEPARATOR\r\n");

    /* MENUEX: item, separator (state 0), separator with id, grayed item, separator stated disabled, last item. */
    p = ex;
    *p++ = 1; *p++ = 4; *(DWORD *)p = 0; p += 2;
    p = item_ex(p, MFT_STRING, 0, 100, 0, L"One");
    p = item_ex(p, MFT_SEPARATOR, 0, 0, 0, L"");
    p = item_ex(p, MFT_SEPARATOR, 0, 13, 0, L"");
    p = item_ex(p, MFT_STRING, MFS_GRAYED, 101, 0, L"Two");
    p = item_ex(p, MFT_SEPARATOR, MFS_DISABLED, 0, 0, L"");
    p = item_ex(p, MFT_STRING, 0, 102, 0x80, L"Three");
    menu = LoadMenuIndirectW(ex);
    dump("MENUEX", menu);
    DestroyMenu(menu);

    /* MENU: header (version 0, offset 0), items: WORD flags, WORD id (not for popups), text. */
    p = old;
    *p++ = 0; *p++ = 0;
    *p++ = MF_STRING; *p++ = 200; p[0] = 'A'; p[1] = 0; p += 2;
    *p++ = MF_SEPARATOR; *p++ = 0; *p++ = 0;   /* RC's SEPARATOR: flags 0, id 0, empty text */
    *p++ = MF_STRING | MF_END; *p++ = 201; p[0] = 'B'; p[1] = 0; p += 2;
    menu = LoadMenuIndirectW(old);
    dump("MENU", menu);
    DestroyMenu(menu);

    menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 300, L"A");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    InsertMenuW(menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    {
        MENUITEMINFOW info = {.cbSize = sizeof(info), .fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID,
                              .fType = MFT_SEPARATOR, .wID = 0};
        InsertMenuItemW(menu, -1, TRUE, &info);
        info.fMask = MIIM_FTYPE;
        InsertMenuItemW(menu, -1, TRUE, &info);
    }
    dump("functions", menu);
    DestroyMenu(menu);
    emit("MENUSEPARATOR complete\r\n");
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
