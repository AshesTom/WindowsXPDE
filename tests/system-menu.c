/* Cross-process default-menu checks used by the XP taskbar. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <wchar.h>

struct shared { HWND hwnd; };
static LRESULT CALLBACK owner_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_APP)
    {
        LONG style = GetWindowLongW(hwnd, GWL_STYLE);
        SetWindowLongW(hwnd, GWL_STYLE, wp ? style | WS_MINIMIZEBOX : style & ~WS_MINIMIZEBOX);
        return 0;
    }
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(hwnd, msg, wp, lp);
}
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #test); goto done; } } while (0)
int wmain(int argc, WCHAR **argv)
{
    HANDLE map, ready;
    struct shared *state;
    if (argc == 4 && !wcscmp(argv[1], L"owner"))
    {
        WNDCLASSW cls = {0};
        MSG msg;
        map = (HANDLE)(uintptr_t)wcstoul(argv[2], NULL, 16);
        ready = (HANDLE)(uintptr_t)wcstoul(argv[3], NULL, 16);
        state = MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*state));
        if (!state) return 2;
        cls.hInstance = GetModuleHandleW(NULL);
        cls.lpfnWndProc = owner_proc;
        cls.lpszClassName = L"MenuTestOwner";
        RegisterClassW(&cls);
        state->hwnd = CreateWindowW(cls.lpszClassName, L"menu owner", WS_OVERLAPPEDWINDOW,
                                    40, 40, 240, 160, NULL, NULL, cls.hInstance, NULL);
        SetEvent(ready);
        while (GetMessageW(&msg, NULL, 0, 0) > 0) DispatchMessageW(&msg);
        return 0;
    }
    else
    {
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
        PROCESS_INFORMATION pi = {0};
        STARTUPINFOW si = {0};
        WCHAR exe[MAX_PATH], command[MAX_PATH + 80];
        HMENU menu, replacement;
        int result = 1;
        map = CreateFileMappingW(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, sizeof(*state), NULL);
        ready = CreateEventW(&sa, TRUE, FALSE, NULL);
        state = MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*state));
        CHECK(map && ready && state);
        GetModuleFileNameW(NULL, exe, MAX_PATH);
        swprintf(command, MAX_PATH + 80, L"\"%ls\" owner %lx %lx", exe,
                 (unsigned long)(uintptr_t)map, (unsigned long)(uintptr_t)ready);
        si.cb = sizeof(si);
        CHECK(CreateProcessW(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi));
        CHECK(WaitForSingleObject(ready, 10000) == WAIT_OBJECT_0 && IsWindow(state->hwnd));
        if (argc == 2 && !wcscmp(argv[1], L"disabled"))
        {
            CHECK(!GetSystemMenu(state->hwnd, FALSE));
            puts("PASS: foreign-menu shim disabled");
            result = 0;
            goto done;
        }
        menu = GetSystemMenu(state->hwnd, FALSE);
        CHECK(menu && GetMenuState(menu, SC_MINIMIZE, MF_BYCOMMAND) == MF_ENABLED);
        CHECK(menu == GetSystemMenu(state->hwnd, FALSE));
        SendMessageW(state->hwnd, WM_APP, 0, 0);
        CHECK(GetMenuState(GetSystemMenu(state->hwnd, FALSE), SC_MINIMIZE, MF_BYCOMMAND) & MF_GRAYED);
        SendMessageW(state->hwnd, WM_APP, 1, 0);
        CHECK(GetMenuState(GetSystemMenu(state->hwnd, FALSE), SC_MINIMIZE, MF_BYCOMMAND) == MF_ENABLED);
        CHECK(!GetSystemMenu(state->hwnd, TRUE) && !IsMenu(menu));
        replacement = GetSystemMenu(state->hwnd, FALSE);
        CHECK(replacement && IsMenu(replacement));
        SendMessageW(state->hwnd, WM_CLOSE, 0, 0);
        CHECK(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
        CHECK(!GetSystemMenu(state->hwnd, FALSE));
        puts("PASS: opt-in isolation, foreign menu, style changes, cache, revert, destruction");
        result = 0;
    done:
        if (pi.hProcess)
        {
            if (WaitForSingleObject(pi.hProcess, 0) == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        if (state) UnmapViewOfFile(state);
        if (map) CloseHandle(map);
        if (ready) CloseHandle(ready);
        return result;
    }
}
