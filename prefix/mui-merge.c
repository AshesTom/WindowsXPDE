/* Copy a DLL with the resources of its MUI file added.
 * Usage: mui-merge SOURCE.dll SOURCE.mui DESTINATION.dll
 *
 * Windows 7 keeps a DLL's strings, menus and dialogs of each language in a
 * .mui file next to it (ja-JP\shell32.dll.mui); Wine's loader does not look
 * for MUI files. prefix/win7.py makes copies of Windows 7's DLLs that hold
 * the resources of their language themselves. The version resource stays the
 * DLL's own. This only reads and writes resources (public resource APIs). */
#include <windows.h>
#include <stdio.h>

static HANDLE update;
static int count, failed;

static BOOL CALLBACK language_proc(HMODULE module, LPCWSTR type, LPCWSTR name, WORD language, LONG_PTR param)
{
    HRSRC resource = FindResourceExW(module, type, name, language);
    DWORD size = SizeofResource(module, resource);
    void *data = LockResource(LoadResource(module, resource));

    (void)param;
    if (UpdateResourceW(update, type, name, language, data, size)) count++;
    else failed++;
    return TRUE;
}

static BOOL CALLBACK name_proc(HMODULE module, LPCWSTR type, LPWSTR name, LONG_PTR param)
{
    (void)param;
    return EnumResourceLanguagesW(module, type, name, language_proc, 0);
}

static BOOL CALLBACK type_proc(HMODULE module, LPWSTR type, LONG_PTR param)
{
    (void)param;
    if (!IS_INTRESOURCE(type) && !lstrcmpiW(type, L"MUI")) return TRUE;
    if (type == RT_VERSION) return TRUE;
    return EnumResourceNamesW(module, type, name_proc, 0);
}

int wmain(int argc, WCHAR **argv)
{
    HMODULE mui;

    if (argc != 4)
    {
        fwprintf(stderr, L"usage: mui-merge SOURCE.dll SOURCE.mui DESTINATION.dll\n");
        return 2;
    }
    if (!CopyFileW(argv[1], argv[3], FALSE))
    {
        fwprintf(stderr, L"cannot copy %ls: %lu\n", argv[1], GetLastError());
        return 1;
    }
    if (!(mui = LoadLibraryExW(argv[2], NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)))
    {
        fwprintf(stderr, L"cannot read %ls: %lu\n", argv[2], GetLastError());
        return 1;
    }
    if (!(update = BeginUpdateResourceW(argv[3], FALSE)))
    {
        fwprintf(stderr, L"cannot update %ls: %lu\n", argv[3], GetLastError());
        return 1;
    }
    EnumResourceTypesW(mui, type_proc, 0);
    FreeLibrary(mui);
    if (failed || !EndUpdateResourceW(update, failed != 0))
    {
        fwprintf(stderr, L"cannot write %ls (%d resources failed): %lu\n", argv[3], failed, GetLastError());
        return 1;
    }
    wprintf(L"%d resources\n", count);
    return 0;
}
