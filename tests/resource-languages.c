/* List resource names and languages of a module's resource types.
 * Usage: resource-languages FILE TYPE [TYPE...]. Only names and language IDs
 * are printed; resource contents are not read or exported. */
#include <windows.h>
#include <stdio.h>

static BOOL CALLBACK language(HMODULE module, LPCWSTR type, LPCWSTR name, WORD lang, LONG_PTR param)
{
    (void)module; (void)type; (void)param;
    if (IS_INTRESOURCE(name)) printf("  #%u lang=%04x\n", (unsigned)(UINT_PTR)name, lang);
    else wprintf(L"  %ls lang=%04x\n", name, lang);
    return TRUE;
}

static BOOL CALLBACK resource(HMODULE module, LPCWSTR type, LPWSTR name, LONG_PTR param)
{
    (void)param;
    return EnumResourceLanguagesW(module, type, name, language, 0);
}

int wmain(int argc, WCHAR **argv)
{
    HMODULE module;
    if (argc < 3) return 1;
    module = LoadLibraryExW(argv[1], NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!module) return 2;
    for (int i = 2; i < argc; i++)
    {
        WCHAR *end;
        unsigned long id = wcstoul(argv[i], &end, 10);
        LPCWSTR type = *end ? argv[i] : MAKEINTRESOURCEW(id);
        wprintf(L"type %ls\n", argv[i]);
        EnumResourceNamesW(module, type, resource, 0);
    }
    FreeLibrary(module);
    return 0;
}
