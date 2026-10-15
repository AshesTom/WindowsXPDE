/* rebar-bands: the bands of the first ReBarWindow32 in a top-level window, read
 * from the owning process (RB_GETBANDINFO into memory allocated there).
 *   rebar-bands-test OUT TOPCLASS
 * Build it for the target's bitness (MINGW_CC=x86_64-w64-mingw32-gcc for
 * Windows 7's x64 explorer): the structure is read from its memory. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    FILE *out;
    HWND top, rebar;
    DWORD pid; HANDLE process; REBARBANDINFOW *remote;
    UINT i, count;
    if (argc < 3 || !(out = _wfopen(argv[1], L"w"))) return 1;
    top = FindWindowW(argv[2], NULL);
    if (!top) { fprintf(out, "no window\n"); return 1; }
    rebar = FindWindowExW(FindWindowExW(top, NULL, L"WorkerW", NULL), NULL, L"ReBarWindow32", NULL);
    GetWindowThreadProcessId(rebar, &pid);
    process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    remote = VirtualAllocEx(process, NULL, 4096, MEM_COMMIT, PAGE_READWRITE);
    count = SendMessageW(rebar, RB_GETBANDCOUNT, 0, 0);
    fprintf(out, "rebar %p bands %u rows %ld height %ld\n", rebar, count, (long)SendMessageW(rebar, RB_GETROWCOUNT, 0, 0),
            (long)SendMessageW(rebar, RB_GETBARHEIGHT, 0, 0));
    for (i = 0; i < count; i++)
    {
        REBARBANDINFOW info;
        RECT rc, borders, *rrc = (RECT *)((char *)remote + 1024);
        WCHAR cls[64] = L"";
        SIZE_T done;
        memset(&info, 0, sizeof(info));
        info.cbSize = sizeof(info);
        info.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_IDEALSIZE | RBBIM_HEADERSIZE | RBBIM_ID;
        WriteProcessMemory(process, remote, &info, sizeof(info), &done);
        SendMessageW(rebar, RB_GETBANDINFOW, i, (LPARAM)remote);
        ReadProcessMemory(process, remote, &info, sizeof(info), &done);
        SendMessageW(rebar, RB_GETRECT, i, (LPARAM)rrc);
        ReadProcessMemory(process, rrc, &rc, sizeof(rc), &done);
        SendMessageW(rebar, RB_GETBANDBORDERS, i, (LPARAM)rrc);
        ReadProcessMemory(process, rrc, &borders, sizeof(borders), &done);
        if (info.hwndChild) GetClassNameW(info.hwndChild, cls, 64);
        fprintf(out, "band %u id %u style %x child %ls min %ux%u cy %u max %u integral %u cx %u ideal %u header %u rect %ld,%ld,%ld,%ld borders %ld,%ld,%ld,%ld\n",
                i, info.wID, info.fStyle, cls, info.cxMinChild, info.cyMinChild, info.cyChild, info.cyMaxChild, info.cyIntegral,
                info.cx, info.cxIdeal, info.cxHeader, rc.left, rc.top, rc.right, rc.bottom,
                borders.left, borders.top, borders.right, borders.bottom);
    }
    fclose(out);
    return 0;
}
