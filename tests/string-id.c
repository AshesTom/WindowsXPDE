/* Print the string resource IDs of a module whose text equals one of the
 * names the shell shows, to reference them from the registry.
 * Usage: string-id MODULE. The module is loaded as data; only IDs are printed. */
#include <windows.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    static const WCHAR *names[] = {
        L"コントロール パネル", /* Control Panel */
    };
    WCHAR text[256];
    HMODULE module;

    if (argc < 2 || !(module = LoadLibraryExW(argv[1], NULL, LOAD_LIBRARY_AS_DATAFILE))) return 1;
    for (unsigned n = 0; n < ARRAYSIZE(names); n++)
        for (UINT id = 1; id < 65536; id++)
            if (LoadStringW(module, id, text, ARRAYSIZE(text)) && !wcscmp(text, names[n]))
                printf("name %u id %u\n", n, id);
    return 0;
}
