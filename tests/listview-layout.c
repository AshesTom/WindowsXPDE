/* listview-layout: report view list view and header geometry for several fonts.
 *   listview-layout-test OUT */
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <stdio.h>
#include <string.h>
static void one(FILE *out, HWND parent, HFONT font, BOOL explorer, BOOL fullrow, BOOL images)
{
    HWND lv = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT, 10, 10, 350, 110, parent, NULL, NULL, NULL);
    LVCOLUMNW col;
    LVITEMW item;
    RECT rc, header;
    int i;
    memset(&col, 0, sizeof(col)); memset(&item, 0, sizeof(item));
    if (font) SendMessageW(lv, WM_SETFONT, (WPARAM)font, FALSE);
    if (fullrow) SendMessageW(lv, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
    if (explorer) SetWindowTheme(lv, L"Explorer", NULL);
    if (images) SendMessageW(lv, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)ImageList_Create(16, 16, ILC_COLOR32, 1, 1));
    col.mask = LVCF_TEXT | LVCF_WIDTH; col.cx = 90; col.pszText = (WCHAR *)L"名前";
    SendMessageW(lv, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);
    col.pszText = (WCHAR *)L"サイズ"; SendMessageW(lv, LVM_INSERTCOLUMNW, 1, (LPARAM)&col);
    item.mask = LVIF_TEXT;
    for (i = 0; i < 3; i++)
    {
        WCHAR text[32];
        swprintf(text, 32, L"ファイル %d.txt", i + 1);
        item.iItem = i; item.pszText = text;
        SendMessageW(lv, LVM_INSERTITEMW, 0, (LPARAM)&item);
    }
    GetWindowRect((HWND)SendMessageW(lv, LVM_GETHEADER, 0, 0), &header);
    MapWindowPoints(NULL, lv, (POINT *)&header, 2);
    fprintf(out, "explorer %d fullrow %d images %d header %ld,%ld,%ld,%ld spacing %lx\n", explorer, fullrow, images,
            header.left, header.top, header.right, header.bottom, (long)SendMessageW(lv, LVM_GETITEMSPACING, TRUE, 0));
    for (i = 0; i < 2; i++)
    {
        RECT label, icon, sub;
        rc.left = LVIR_BOUNDS; SendMessageW(lv, LVM_GETITEMRECT, i, (LPARAM)&rc);
        label.left = LVIR_LABEL; SendMessageW(lv, LVM_GETITEMRECT, i, (LPARAM)&label);
        icon.left = LVIR_ICON; SendMessageW(lv, LVM_GETITEMRECT, i, (LPARAM)&icon);
        sub.top = 1; sub.left = LVIR_LABEL; SendMessageW(lv, LVM_GETSUBITEMRECT, i, (LPARAM)&sub);
        fprintf(out, "  item %d bounds %ld,%ld,%ld,%ld label %ld,%ld,%ld,%ld icon %ld,%ld,%ld,%ld sub1 %ld,%ld,%ld,%ld\n", i,
                rc.left, rc.top, rc.right, rc.bottom, label.left, label.top, label.right, label.bottom,
                icon.left, icon.top, icon.right, icon.bottom, sub.left, sub.top, sub.right, sub.bottom);
    }
    DestroyWindow(lv);
}
int wmain(int argc, WCHAR **argv)
{
    FILE *out = argc > 1 ? _wfopen(argv[1], L"w") : stdout;
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
    NONCLIENTMETRICSW ncm;
    HWND parent;
    HFONT font;
    InitCommonControlsEx(&icc);
    memset(&ncm, 0, sizeof(ncm)); ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    font = CreateFontIndirectW(&ncm.lfMessageFont);
    parent = CreateWindowW(L"STATIC", L"p", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 300, NULL, NULL, NULL, NULL);
    one(out, parent, font, TRUE, TRUE, FALSE);
    one(out, parent, font, TRUE, TRUE, TRUE);
    one(out, parent, NULL, FALSE, FALSE, FALSE);
    {
        static const struct { const WCHAR *face; int height; } fonts[] = {
            { L"Meiryo UI", -12 }, { L"Meiryo", -12 }, { L"MS UI Gothic", -12 }, { L"Tahoma", -11 },
            { L"Segoe UI", -12 }, { L"Arial", -20 }, { L"Arial", -8 }, { L"Arial", -30 } };
        unsigned int k;
        for (k = 0; k < sizeof(fonts) / sizeof(fonts[0]); k++)
        {
            HFONT f = CreateFontW(fonts[k].height, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, fonts[k].face);
            HDC dc = GetDC(parent);
            TEXTMETRICW tm;
            SelectObject(dc, f);
            GetTextMetricsW(dc, &tm);
            ReleaseDC(parent, dc);
            fprintf(out, "font %S %d tmHeight %ld internal %ld\n", fonts[k].face, fonts[k].height, tm.tmHeight, tm.tmInternalLeading);
            one(out, parent, f, FALSE, FALSE, FALSE);
            one(out, parent, f, FALSE, FALSE, TRUE);
        }
    }
    fclose(out);
    return 0;
}
