/* Print the values under registry keys with public calls, recursively.
 * Usage: reg-dump COM1|FILE KEY... where KEY starts with HKCR, HKCU or HKLM; a
 * path component of 36 characters is taken as a GUID and gets its braces, for
 * keyboards that cannot type them. Only the named keys are read. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HANDLE out = INVALID_HANDLE_VALUE;

static void emit(const WCHAR *text)
{
    char utf8[4096];
    DWORD written;
    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8), NULL, NULL);
    if (length <= 1) return;
    if (out == INVALID_HANDLE_VALUE) fputs(utf8, stdout);
    else WriteFile(out, utf8, length - 1, &written, NULL);
}

static void dump(HKEY root, const WCHAR *path, int depth)
{
    WCHAR line[2048], name[256], child[1024];
    BYTE data[2048];
    HKEY key;

    if (RegOpenKeyExW(root, path, 0, KEY_READ, &key)) return;
    swprintf(line, ARRAYSIZE(line), L"[%ls]\r\n", path);
    emit(line);
    for (DWORD i = 0;; i++)
    {
        DWORD name_size = ARRAYSIZE(name), size = sizeof(data) - sizeof(WCHAR), type;
        if (RegEnumValueW(key, i, name, &name_size, NULL, &type, data, &size)) break;
        if (type == REG_SZ || type == REG_EXPAND_SZ)
        {
            data[size] = data[size + 1] = 0;
            swprintf(line, ARRAYSIZE(line), L"  %ls = %ls'%ls'\r\n", name[0] ? name : L"@",
                     type == REG_EXPAND_SZ ? L"expand:" : L"", (WCHAR *)data);
        }
        else if (type == REG_DWORD)
            swprintf(line, ARRAYSIZE(line), L"  %ls = dword:%08lx\r\n", name[0] ? name : L"@", *(DWORD *)data);
        else
        {
            int n = swprintf(line, ARRAYSIZE(line), L"  %ls = type%lu:", name[0] ? name : L"@", type);
            for (DWORD b = 0; b < size && b < 64; b++) n += swprintf(line + n, ARRAYSIZE(line) - n, L" %02x", data[b]);
            swprintf(line + n, ARRAYSIZE(line) - n, L"\r\n");
        }
        emit(line);
    }
    for (DWORD i = 0; depth < 6; i++)
    {
        DWORD name_size = ARRAYSIZE(name);
        if (RegEnumKeyExW(key, i, name, &name_size, NULL, NULL, NULL, NULL)) break;
        swprintf(child, ARRAYSIZE(child), L"%ls\\%ls", path, name);
        dump(root, child, depth + 1);
    }
    RegCloseKey(key);
}

/* Copy PATH, adding braces around components that are bare GUIDs. */
static void add_braces(const WCHAR *path, WCHAR *result, size_t size)
{
    size_t n = 0;
    while (*path && n + 40 < size)
    {
        const WCHAR *end = wcschr(path, '\\');
        size_t length = end ? (size_t)(end - path) : wcslen(path);
        BOOL guid = length == 36 && path[8] == '-' && path[13] == '-' && path[18] == '-' && path[23] == '-';
        if (guid) result[n++] = '{';
        memcpy(result + n, path, length * sizeof(WCHAR));
        n += length;
        if (guid) result[n++] = '}';
        path += length;
        if (*path) result[n++] = *path++;
    }
    result[n] = 0;
}

int wmain(int argc, WCHAR **argv)
{
    if (argc < 3) return 1;
    out = CreateFileW(argv[1], GENERIC_WRITE, 0, NULL, wcsncmp(argv[1], L"COM", 3) ? CREATE_ALWAYS : OPEN_EXISTING,
                      0, NULL);
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
    emit(L"REGDUMP\r\n");
    for (int i = 2; i < argc; i++)
    {
        HKEY root = !wcsncmp(argv[i], L"HKCR\\", 5) ? HKEY_CLASSES_ROOT
                  : !wcsncmp(argv[i], L"HKCU\\", 5) ? HKEY_CURRENT_USER
                  : !wcsncmp(argv[i], L"HKLM\\", 5) ? HKEY_LOCAL_MACHINE : NULL;
        WCHAR path[1024];
        add_braces(argv[i] + 5, path, ARRAYSIZE(path));
        if (root) dump(root, path, 0);
    }
    emit(L"REGDUMP complete\r\n");
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
