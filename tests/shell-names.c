/* Report locale code pages and the registry display-name values of shell
 * namespace classes. Usage: shell-names [COM1|FILE]. Reads public registry
 * values only; resolved strings come from SHLoadIndirectString when present. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HANDLE out = INVALID_HANDLE_VALUE;

static void emit(const WCHAR *text)
{
    char utf8[1024];
    DWORD written;
    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8), NULL, NULL);
    if (length <= 1) return;
    if (out == INVALID_HANDLE_VALUE) fputs(utf8, stdout);
    else WriteFile(out, utf8, length - 1, &written, NULL);
}

int wmain(int argc, WCHAR **argv)
{
    static const WCHAR *classes[] = {
        L"{20D04FE0-3AEA-1069-A2D8-08002B30309D}", L"{208D2C60-3AEA-1069-A2D7-08002B30309D}",
        L"{645FF040-5081-101B-9F08-00AA002F954E}", L"{450D8FBA-AD25-11D0-98A8-0800361B1103}",
        L"{2227A280-3AEA-1069-A2DE-08002B30309D}", L"{871C5380-42A0-1069-A2EA-08002B30309D}",
        L"{21EC2020-3AEA-1069-A2DD-08002B30309D}",
    };
    static const WCHAR *names[] = {L"", L"LocalizedString", L"InfoTip"};
    WCHAR line[1024], key[128], value[512], resolved[512];
    HRESULT (WINAPI *load_indirect)(const WCHAR *, WCHAR *, UINT, void **) =
        (void *)GetProcAddress(LoadLibraryW(L"shlwapi.dll"), "SHLoadIndirectString");

    if (argc > 1)
    {
        out = CreateFileW(argv[1], GENERIC_WRITE, 0, NULL,
                          wcsncmp(argv[1], L"COM", 3) ? CREATE_ALWAYS : OPEN_EXISTING, 0, NULL);
        if (out != INVALID_HANDLE_VALUE && !wcsncmp(argv[1], L"COM", 3))
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
    swprintf(line, ARRAYSIZE(line), L"SHELLNAMES acp=%u oem=%u user=%04lx system=%04lx ui=%04x\r\n",
             GetACP(), GetOEMCP(), (unsigned long)GetUserDefaultLCID(), (unsigned long)GetSystemDefaultLCID(),
             GetUserDefaultUILanguage());
    emit(line);
    for (unsigned c = 0; c < ARRAYSIZE(classes); c++)
        for (unsigned n = 0; n < ARRAYSIZE(names); n++)
        {
            DWORD type, size = sizeof(value) - sizeof(WCHAR);
            HKEY hkey;
            LONG error;
            swprintf(key, ARRAYSIZE(key), L"CLSID\\%ls", classes[c]);
            memset(value, 0, sizeof(value));
            resolved[0] = 0;
            if (RegOpenKeyExW(HKEY_CLASSES_ROOT, key, 0, KEY_QUERY_VALUE, &hkey)) continue;
            error = RegQueryValueExW(hkey, names[n][0] ? names[n] : NULL, NULL, &type, (BYTE *)value, &size);
            RegCloseKey(hkey);
            if (error || (type != REG_SZ && type != REG_EXPAND_SZ)) continue;
            if (value[0] == '@' && load_indirect) load_indirect(value, resolved, ARRAYSIZE(resolved), NULL);
            swprintf(line, ARRAYSIZE(line), L"%ls %ls=%ls -> %ls\r\n", classes[c],
                     names[n][0] ? names[n] : L"(default)", value, resolved);
            emit(line);
        }
    emit(L"SHELLNAMES complete\r\n");
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
