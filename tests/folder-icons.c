/* Print the path, display name and icon location of the special folders the
 * Start panel lists, with public shell calls, and the entries of their
 * desktop.ini. Usage: folder-icons [COM1|FILE]. Only names, paths and
 * icon indexes are printed. */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

static HANDLE out = INVALID_HANDLE_VALUE;

/* mingw's C declaration of IExtractIconW leaves out the IUnknown methods. */
typedef struct extract_icon { const struct extract_icon_vtbl *lpVtbl; } extract_icon;
struct extract_icon_vtbl
{
    HRESULT (WINAPI *QueryInterface)(extract_icon *, REFIID, void **);
    ULONG (WINAPI *AddRef)(extract_icon *);
    ULONG (WINAPI *Release)(extract_icon *);
    HRESULT (WINAPI *GetIconLocation)(extract_icon *, UINT, WCHAR *, UINT, int *, UINT *);
};

static void emit(const WCHAR *text)
{
    char utf8[2048];
    DWORD written;
    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8), NULL, NULL);
    if (length <= 1) return;
    if (out == INVALID_HANDLE_VALUE) fputs(utf8, stdout);
    else WriteFile(out, utf8, length - 1, &written, NULL);
}

static void icon_location(IShellFolder *desktop, LPITEMIDLIST pidl, WCHAR *text, int size)
{
    IShellFolder *parent;
    LPCITEMIDLIST child;
    extract_icon *extract;
    WCHAR file[MAX_PATH] = L"";
    int index = 0;
    UINT flags = 0;

    text[0] = 0;
    (void)desktop;
    if (FAILED(SHBindToParent(pidl, &IID_IShellFolder, (void **)&parent, &child))) return;
    if (SUCCEEDED(parent->lpVtbl->GetUIObjectOf(parent, NULL, 1, &child, &IID_IExtractIconW, NULL,
                                                 (void **)&extract)))
    {
        if (SUCCEEDED(extract->lpVtbl->GetIconLocation(extract, GIL_FORSHELL, file, MAX_PATH, &index, &flags)))
            swprintf(text, size, L"%ls,%d flags=%x", file, index, flags);
        extract->lpVtbl->Release(extract);
    }
    parent->lpVtbl->Release(parent);
}

int wmain(int argc, WCHAR **argv)
{
    static const struct { const WCHAR *name; int csidl; } folders[] = {
        {L"personal", CSIDL_PERSONAL}, {L"recent", CSIDL_RECENT}, {L"pictures", CSIDL_MYPICTURES},
        {L"music", CSIDL_MYMUSIC}, {L"drives", CSIDL_DRIVES}, {L"controls", CSIDL_CONTROLS},
        {L"printers", CSIDL_PRINTERS}, {L"favorites", CSIDL_FAVORITES}, {L"network", CSIDL_NETHOOD},
        {L"startmenu", CSIDL_STARTMENU}, {L"programs", CSIDL_PROGRAMS}, {L"startup", CSIDL_STARTUP},
        {L"desktop", CSIDL_DESKTOPDIRECTORY}, {L"templates", CSIDL_TEMPLATES}, {L"sendto", CSIDL_SENDTO},
        {L"commonprograms", CSIDL_COMMON_PROGRAMS}, {L"commonstartup", CSIDL_COMMON_STARTUP},
    };
    WCHAR line[2048], path[MAX_PATH], location[MAX_PATH + 64];
    IShellFolder *desktop;

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
    CoInitialize(NULL);
    emit(L"FOLDERICONS\r\n");
    if (FAILED(SHGetDesktopFolder(&desktop))) return 1;
    for (unsigned i = 0; i < ARRAYSIZE(folders); i++)
    {
        LPITEMIDLIST pidl;
        SHFILEINFOW info = {0};
        DWORD attributes;

        path[0] = 0;
        SHGetFolderPathW(NULL, folders[i].csidl, NULL, 0, path);
        if (FAILED(SHGetSpecialFolderLocation(NULL, folders[i].csidl, &pidl)))
        {
            swprintf(line, ARRAYSIZE(line), L"%ls missing path='%ls'\r\n", folders[i].name, path);
            emit(line);
            continue;
        }
        SHGetFileInfoW((LPCWSTR)pidl, 0, &info, sizeof(info), SHGFI_PIDL | SHGFI_DISPLAYNAME | SHGFI_SYSICONINDEX);
        icon_location(desktop, pidl, location, ARRAYSIZE(location));
        attributes = path[0] ? GetFileAttributesW(path) : 0;
        swprintf(line, ARRAYSIZE(line), L"%ls path='%ls' attr=%lx name='%ls' icon='%ls'\r\n", folders[i].name, path,
                 (unsigned long)attributes, info.szDisplayName, location);
        emit(line);
        if (path[0])
        {
            WCHAR ini[MAX_PATH + 16], sections[2048], entries[4096];
            swprintf(ini, ARRAYSIZE(ini), L"%ls\\desktop.ini", path);
            if (GetPrivateProfileSectionNamesW(sections, ARRAYSIZE(sections), ini))
                for (WCHAR *section = sections; *section; section += wcslen(section) + 1)
                {
                    swprintf(line, ARRAYSIZE(line), L"  [%ls]\r\n", section);
                    emit(line);
                    if (!GetPrivateProfileSectionW(section, entries, ARRAYSIZE(entries), ini)) continue;
                    for (WCHAR *entry = entries; *entry; entry += wcslen(entry) + 1)
                    {
                        swprintf(line, ARRAYSIZE(line), L"    %ls\r\n", entry);
                        emit(line);
                    }
                }
        }
        CoTaskMemFree(pidl);
    }
    desktop->lpVtbl->Release(desktop);
    emit(L"FOLDERICONS complete\r\n");
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
