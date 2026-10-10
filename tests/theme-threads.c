/* Draw the same theme part from several threads at once and compare every
 * result with a single-threaded reference. Theme images are bitmaps shared by
 * the process, and one that is selected into two DCs at once draws nothing in
 * the second; Wine patch 0008 serializes those blits. Usage: theme-threads
 * [THREADS [ROUNDS]]; exits 1 if any draw differs. Needs an active visual
 * style. Public uxtheme calls only. */
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 200
#define HEIGHT 30

static HTHEME theme;
static DWORD reference[WIDTH * HEIGHT];
static LONG mismatches, draws;
static int rounds = 300;

static HDC create_dc(DWORD **bits)
{
    BITMAPINFO info = {0};
    HDC dc = CreateCompatibleDC(NULL);
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = WIDTH;
    info.bmiHeader.biHeight = -HEIGHT;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    SelectObject(dc, CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)bits, NULL, 0));
    return dc;
}

static void draw(HDC dc, DWORD *bits)
{
    RECT rect = {0, 0, WIDTH, HEIGHT};
    memset(bits, 0, sizeof(reference));
    /* WP_CAPTION / CS_ACTIVE and WP_CLOSEBUTTON / CBS_NORMAL: a stretched
     * background and a true-size image from the same theme file. */
    DrawThemeBackground(theme, dc, 1, 1, &rect, NULL);
    rect.left = WIDTH - 25;
    rect.top = 5;
    rect.right = WIDTH - 4;
    rect.bottom = 26;
    DrawThemeBackground(theme, dc, 18, 1, &rect, NULL);
    GdiFlush();
}

static DWORD WINAPI worker(void *arg)
{
    DWORD *bits;
    HDC dc = create_dc(&bits);
    (void)arg;
    for (int i = 0; i < rounds; i++)
    {
        draw(dc, bits);
        InterlockedIncrement(&draws);
        if (memcmp(bits, reference, sizeof(reference))) InterlockedIncrement(&mismatches);
    }
    DeleteObject(GetCurrentObject(dc, OBJ_BITMAP));
    DeleteDC(dc);
    return 0;
}

int main(int argc, char **argv)
{
    HANDLE threads[64];
    int count = argc > 1 ? atoi(argv[1]) : 4;
    DWORD *bits;
    HDC dc;
    HWND hwnd;

    if (argc > 2) rounds = atoi(argv[2]);
    if (count < 1 || count > 64 || rounds < 1) return 2;
    hwnd = CreateWindowA("STATIC", "", WS_POPUP, 0, 0, 1, 1, NULL, NULL, NULL, NULL);
    if (!IsThemeActive() || !(theme = OpenThemeData(hwnd, L"Window")))
    {
        puts("no active visual style");
        return 2;
    }
    dc = create_dc(&bits);
    draw(dc, bits);
    memcpy(reference, bits, sizeof(reference));
    for (int i = 0; i < count; i++) threads[i] = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    WaitForMultipleObjects(count, threads, TRUE, INFINITE);
    printf("threads=%d draws=%ld mismatched=%ld opt-in=%s\n", count, draws, mismatches,
           GetEnvironmentVariableA("WIN4WINE_EXTERNAL_SHELL", NULL, 0) ? "yes" : "no");
    return mismatches ? 1 : 0;
}
