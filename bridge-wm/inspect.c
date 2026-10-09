/* Local verification utility; no Microsoft implementation code or resources. */
#ifndef UNICODE
#define UNICODE
#endif
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdint.h>
#include <wchar.h>

static BOOL CALLBACK dump(HWND hwnd, LPARAM unused)
{
    WCHAR cls[256], text[512];
    char utf8[2048];
    (void)unused;
    GetClassNameW(hwnd, cls, 256);
    GetWindowTextW(hwnd, text, 512);
    WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8), NULL, NULL);
    printf("%08lx visible=%d iconic=%d class=%ls title=%s\n", (unsigned long)(uintptr_t)hwnd,
           IsWindowVisible(hwnd), IsIconic(hwnd), cls, utf8);
    return TRUE;
}

static HRESULT shortcut(const WCHAR *name, const WCHAR *target, const WCHAR *arguments)
{
    WCHAR path[MAX_PATH], dir[MAX_PATH];
    IShellLinkW *link;
    IPersistFile *persist;
    HRESULT hr = SHGetFolderPathW(NULL, CSIDL_PROGRAMS | CSIDL_FLAG_CREATE, NULL, 0, dir);
    if (FAILED(hr)) return hr;
    swprintf(path, MAX_PATH, L"%ls\\%ls.lnk", dir, name);
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link);
    if (FAILED(hr)) return hr;
    IShellLinkW_SetPath(link, target);
    IShellLinkW_SetArguments(link, arguments);
    hr = IShellLinkW_QueryInterface(link, &IID_IPersistFile, (void **)&persist);
    if (SUCCEEDED(hr))
    {
        hr = IPersistFile_Save(persist, path, TRUE);
        IPersistFile_Release(persist);
    }
    IShellLinkW_Release(link);
    return hr;
}

int wmain(int argc, WCHAR **argv)
{
    HWND hwnd;
    if (argc < 2) return 2;
    if (!wcscmp(argv[1], L"dump")) { EnumWindows(dump, 0); return 0; }
    if (!wcscmp(argv[1], L"shortcuts"))
    {
        HRESULT a, b;
        CoInitialize(NULL);
        a = shortcut(L"Windows Notepad", L"C:\\windows\\system32\\notepad.exe", L"");
        b = shortcut(L"Linux Terminal", L"C:\\windows\\system32\\start.exe", L"/unix /usr/bin/xterm");
        CoUninitialize();
        return FAILED(a) || FAILED(b);
    }
    if (argc != 3) return 2;
    hwnd = (HWND)(uintptr_t)wcstoul(argv[2], NULL, 16);
    if (!IsWindow(hwnd)) return 3;
    if (!wcscmp(argv[1], L"minimize")) ShowWindow(hwnd, SW_MINIMIZE);
    else if (!wcscmp(argv[1], L"restore")) { ShowWindow(hwnd, SW_RESTORE); SetForegroundWindow(hwnd); }
    else if (!wcscmp(argv[1], L"close")) PostMessageW(hwnd, WM_CLOSE, 0, 0);
    else return 2;
    return 0;
}
