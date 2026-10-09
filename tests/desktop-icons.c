/* Show the standard desktop icons of the Start panel desktop on a reference
 * machine, for comparing icon labels. Usage: desktop-icons [hide]. Writes only
 * the documented HideDesktopIcons values of the current user. */
#include <windows.h>
#include <shlobj.h>

int wmain(int argc, WCHAR **argv)
{
    static const WCHAR *icons[] = {
        L"{20D04FE0-3AEA-1069-A2D8-08002B30309D}", L"{450D8FBA-AD25-11D0-98A8-0800361B1103}",
        L"{208D2C60-3AEA-1069-A2D7-08002B30309D}", L"{871C5380-42A0-1069-A2EA-08002B30309D}",
    };
    DWORD hide = argc > 1 && !wcscmp(argv[1], L"hide");
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel",
                        0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL))
        return 2;
    for (unsigned i = 0; i < ARRAYSIZE(icons); i++)
        RegSetValueExW(key, icons[i], 0, REG_DWORD, (const BYTE *)&hide, sizeof(hide));
    RegCloseKey(key);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return 0;
}
