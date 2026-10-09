/* Report where CW_USEDEFAULT places successive top-level windows.
 * Usage: default-position [COM1|FILE] [--sizes|--metrics]. Self-authored windows only. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HANDLE out = INVALID_HANDLE_VALUE;

static void emit(const char *text)
{
    DWORD written;
    if (out == INVALID_HANDLE_VALUE) fputs(text, stdout);
    else WriteFile(out, text, (DWORD)strlen(text), &written, NULL);
}

int wmain(int argc, WCHAR **argv)
{
    WNDCLASSW wc = {0};
    HWND windows[12];
    RECT work, rect;
    char line[256];
    int count = 0;

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
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"DefaultPosition";
    if (!RegisterClassW(&wc)) return 2;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    snprintf(line, sizeof(line), "DEFAULTPOS screen=%dx%d work=%ld,%ld..%ld,%ld caption=%d frame=%d,%d\r\n",
             GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), work.left, work.top,
             work.right, work.bottom, GetSystemMetrics(SM_CYCAPTION),
             GetSystemMetrics(SM_CXSIZEFRAME), GetSystemMetrics(SM_CYSIZEFRAME));
    emit(line);
    for (int i = 0; i < 12; i++)
    {
        BOOL size_default = i % 2 == 0;
        windows[i] = CreateWindowW(wc.lpszClassName, L"Default position probe", WS_OVERLAPPEDWINDOW,
                                   CW_USEDEFAULT, CW_USEDEFAULT, size_default ? CW_USEDEFAULT : 300,
                                   size_default ? CW_USEDEFAULT : 200, NULL, NULL, wc.hInstance, NULL);
        if (!windows[i]) break;
        count++;
        ShowWindow(windows[i], SW_SHOWNORMAL);
        GetWindowRect(windows[i], &rect);
        snprintf(line, sizeof(line), "window %d sizedefault=%d rect=%ld,%ld..%ld,%ld\r\n", i, size_default,
                 rect.left, rect.top, rect.right, rect.bottom);
        emit(line);
    }
    for (int i = 0; i < count; i++) DestroyWindow(windows[i]);
    if (argc > 2 && !wcscmp(argv[2], L"--sizes"))
    {
        /* Temporarily change only the in-memory work area; restore it below. */
        static const RECT areas[] = {{0, 0, 1024, 768}, {0, 0, 1024, 600}, {0, 0, 800, 734},
                                     {0, 100, 1024, 734}, {100, 0, 1024, 734}, {0, 0, 640, 480}};
        for (unsigned a = 0; a < ARRAYSIZE(areas); a++)
        {
            RECT area = areas[a], got;
            HWND hwnd;
            SystemParametersInfoW(SPI_SETWORKAREA, 0, &area, 0);
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &got, 0);
            hwnd = CreateWindowW(wc.lpszClassName, L"Default size probe", WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                 NULL, NULL, wc.hInstance, NULL);
            GetWindowRect(hwnd, &rect);
            snprintf(line, sizeof(line), "size work=%ld,%ld..%ld,%ld rect=%ld,%ld..%ld,%ld\r\n",
                     got.left, got.top, got.right, got.bottom, rect.left, rect.top, rect.right, rect.bottom);
            emit(line);
            DestroyWindow(hwnd);
        }
        SystemParametersInfoW(SPI_SETWORKAREA, 0, &work, 0);
    }
    if (argc > 2 && !wcscmp(argv[2], L"--metrics"))
    {
        /* Temporarily change the in-memory caption metrics; restore them below. */
        static const int sizes[][2] = {{18, 16}, {30, 24}, {40, 36}};
        NONCLIENTMETRICSW saved = {.cbSize = sizeof(saved)};
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(saved), &saved, 0);
        for (unsigned m = 0; m < ARRAYSIZE(sizes); m++)
        {
            NONCLIENTMETRICSW changed = saved;
            changed.iCaptionHeight = sizes[m][0];
            changed.iCaptionWidth = sizes[m][1];
            SystemParametersInfoW(SPI_SETNONCLIENTMETRICS, sizeof(changed), &changed, 0);
            for (int i = 0; i < 3; i++)
            {
                HWND hwnd = CreateWindowW(wc.lpszClassName, L"Default metric probe", WS_OVERLAPPEDWINDOW,
                                          CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                          NULL, NULL, wc.hInstance, NULL);
                GetWindowRect(hwnd, &rect);
                snprintf(line, sizeof(line), "metric caption=%d size=%d,%d frame=%d,%d rect=%ld,%ld..%ld,%ld\r\n",
                         GetSystemMetrics(SM_CYCAPTION), GetSystemMetrics(SM_CXSIZE), GetSystemMetrics(SM_CYSIZE),
                         GetSystemMetrics(SM_CXFRAME), GetSystemMetrics(SM_CYFRAME),
                         rect.left, rect.top, rect.right, rect.bottom);
                emit(line);
                DestroyWindow(hwnd);
            }
        }
        SystemParametersInfoW(SPI_SETNONCLIENTMETRICS, sizeof(saved), &saved, 0);
    }
    emit("DEFAULTPOS complete\r\n");
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
