/* dm-reposition: where DM_REPOSITION moves a top-level dialog, relative to the work area.
 *   dm-reposition-test OUT
 * Windows 7 SP1: the right edge at most at the work area's right, the bottom at most 4 pixels
 * above its bottom, then the left and top at least at the work area's (the top wins when the
 * dialog is taller than the work area); a dialog already inside does not move. */
#include <windows.h>
#include <stdio.h>

int wmain( int argc, WCHAR **argv )
{
    static const struct { int dx, dy, cy; } cases[] =
    {
        { 0, 100, 150 },    /* bottom 100 below the work area's */
        { 0, -2, 150 },     /* bottom 2 above it: inside the 4 pixels */
        { 0, -5, 150 },     /* bottom 5 above it: stays */
        { 100, 50, 150 },   /* right and bottom out */
        { -50, -50, 150 },  /* left and top out (offsets from the top left) */
        { 0, 0, 2000 },     /* taller than the work area */
    };
    static struct { DLGTEMPLATE t; WORD menu, cls, title; } tmpl;
    MONITORINFO info = { .cbSize = sizeof(info) };
    POINT origin = { 0, 0 };
    unsigned int i;
    FILE *out;

    if (argc < 2 || !(out = _wfopen( argv[1], L"w" ))) return 1;
    GetMonitorInfoW( MonitorFromPoint( origin, MONITOR_DEFAULTTOPRIMARY ), &info );
    for (i = 0; i < ARRAYSIZE(cases); i++)
    {
        RECT before, after;
        HWND dlg;
        int x, y;
        tmpl.t.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
        tmpl.t.cx = 200;
        tmpl.t.cy = cases[i].cy;
        if (!(dlg = CreateDialogIndirectParamW( 0, &tmpl.t, 0, NULL, 0 ))) return 2;
        GetWindowRect( dlg, &before );
        /* cases 0 to 3 place the bottom right corner (positive = right of and below the work
         * area's), 4 and 5 the top left corner */
        if (i < 4)
        {
            x = info.rcWork.right - (before.right - before.left) + cases[i].dx;
            y = info.rcWork.bottom - (before.bottom - before.top) + cases[i].dy;
        }
        else
        {
            x = info.rcWork.left + cases[i].dx;
            y = info.rcWork.top + cases[i].dy;
        }
        SetWindowPos( dlg, 0, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
        GetWindowRect( dlg, &before );
        SendMessageW( dlg, DM_REPOSITION, 0, 0 );
        GetWindowRect( dlg, &after );
        fprintf( out, "case %u: before %ld,%ld,%ld,%ld after %ld,%ld,%ld,%ld (from the work area)\n", i,
                 before.left - info.rcWork.left, before.top - info.rcWork.top,
                 before.right - info.rcWork.right, before.bottom - info.rcWork.bottom,
                 after.left - info.rcWork.left, after.top - info.rcWork.top,
                 after.right - info.rcWork.right, after.bottom - info.rcWork.bottom );
        DestroyWindow( dlg );
    }
    fclose( out );
    return 0;
}
