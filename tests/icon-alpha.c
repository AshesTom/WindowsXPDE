/* Print what DrawIconEx makes of a 32-bit icon with alpha over plain
 * backgrounds. Usage: icon-alpha-test [COM1|FILE]. The icon's 16x16 pixels
 * cover 16 alpha values by 16 colour values (the same in red, green and
 * blue; green is printed); each background prints the 256 results in hex.
 * Self-authored bitmaps and public user32/gdi32 calls only. */
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

static const BYTE alphas[16] = {1, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 254};
static const BYTE colours[16] = {0, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255};

int main(int argc, char **argv)
{
    BITMAPINFO info = {{sizeof(info.bmiHeader), 16, -16, 1, 32, BI_RGB, 0, 0, 0, 0, 0}, {{0, 0, 0, 0}}};
    static const BYTE backgrounds[] = {0x00, 0x3a, 0x80, 0xc4, 0xff};
    ICONINFO icon_info = {TRUE, 0, 0, NULL, NULL};
    DWORD *colour_bits, *target;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP colour, mask, canvas;
    HICON icon;
    char line[1200];

    if (argc > 1)
    {
        int serial = !lstrcmpiA(argv[1], "COM1");
        out = CreateFileA(argv[1], GENERIC_WRITE, 0, NULL, serial ? OPEN_EXISTING : CREATE_ALWAYS, 0, NULL);
        if (serial && out != INVALID_HANDLE_VALUE)
        {
            DCB dcb = {.DCBlength = sizeof(dcb)};
            GetCommState(out, &dcb);
            dcb.BaudRate = CBR_115200; dcb.ByteSize = 8; dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;
            SetCommState(out, &dcb);
        }
    }
    emit("ICONALPHA\r\n");
    colour = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void **)&colour_bits, NULL, 0);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
        {
            BYTE c = colours[x], a = alphas[y];
            colour_bits[y * 16 + x] = (DWORD)a << 24 | c << 16 | c << 8 | c;  /* not premultiplied, as in icons */
        }
    mask = CreateBitmap(16, 16, 1, 1, NULL);  /* all 0: the alpha decides */
    icon_info.hbmColor = colour;
    icon_info.hbmMask = mask;
    icon = CreateIconIndirect(&icon_info);
    canvas = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void **)&target, NULL, 0);
    SelectObject(dc, canvas);
    for (size_t b = 0; b < sizeof(backgrounds); b++)
    {
        int n;
        for (int i = 0; i < 256; i++)
            target[i] = backgrounds[b] << 16 | backgrounds[b] << 8 | backgrounds[b];
        DrawIconEx(dc, 0, 0, icon, 16, 16, 0, NULL, DI_NORMAL);
        GdiFlush();
        n = snprintf(line, sizeof(line), "bg %02x:", backgrounds[b]);
        for (int i = 0; i < 256; i++)
            n += snprintf(line + n, sizeof(line) - n, "%s%02x", i % 16 ? "" : " ", (unsigned)(target[i] >> 8 & 0xff));
        snprintf(line + n, sizeof(line) - n, "\r\n");
        emit(line);
    }
    emit("ICONALPHA complete\r\n");
    DestroyIcon(icon);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
