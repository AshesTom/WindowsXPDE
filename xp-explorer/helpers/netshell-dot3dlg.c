/* dot3dlg.dll for XP's netshell.dll: the wired 802.1X authentication
 * balloons of XP SP3, which netshell imports. XP's own needs OneX.dll;
 * Linux authenticates wired connections itself, so there is never one to
 * show. The shapes are XP SP3 dot3dlg.dll's, stdcall. */
#include <windows.h>

HRESULT WINAPI Dot3ACCanShowBalloon(const GUID *connection, void *status, void *text, void *icon, void *cookie,
                                    void *reserved)
{
    (void)connection; (void)status; (void)text; (void)icon; (void)cookie; (void)reserved;
    return S_FALSE;
}

HRESULT WINAPI Dot3ACOnBalloonClick(const GUID *connection, void *text, void *cookie)
{
    (void)connection; (void)text; (void)cookie;
    return E_NOTIMPL;
}
