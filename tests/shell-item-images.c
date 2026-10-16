/* shell-item-images: IShellItemImageFactory::GetImage of an item at a few sizes, saved as 32-bit BMPs
 * (16, 32, 48 and 256 are the icon's own sizes; 40 and 96 are scaled, with WIC's Fant mode).
 *   shell-item-images-test OUTDIR PATH */
#define COBJMACROS
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <stdio.h>

static void save(const WCHAR *path, HBITMAP bmp)
{
    BITMAP bm;
    BITMAPINFOHEADER bi;
    BITMAPFILEHEADER bf;
    HDC dc = GetDC(NULL);
    HANDLE file;
    DWORD size, written;
    void *bits;
    memset(&bi, 0, sizeof(bi));
    memset(&bf, 0, sizeof(bf));
    bi.biSize = sizeof(bi);
    bf.bfType = 0x4d42;
    GetObjectW(bmp, sizeof(bm), &bm);
    bi.biWidth = bm.bmWidth; bi.biHeight = bm.bmHeight; bi.biPlanes = 1; bi.biBitCount = 32;
    size = bm.bmWidth * bm.bmHeight * 4;
    bits = malloc(size);
    GetDIBits(dc, bmp, 0, bm.bmHeight, bits, (BITMAPINFO *)&bi, DIB_RGB_COLORS);
    ReleaseDC(NULL, dc);
    bf.bfOffBits = sizeof(bf) + sizeof(bi); bf.bfSize = bf.bfOffBits + size;
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, &bf, sizeof(bf), &written, NULL);
    WriteFile(file, &bi, sizeof(bi), &written, NULL);
    WriteFile(file, bits, size, &written, NULL);
    CloseHandle(file);
    free(bits);
}

int wmain(int argc, WCHAR **argv)
{
    static const int sizes[] = { 16, 32, 40, 48, 96, 256 };
    IShellItemImageFactory *factory = NULL;
    unsigned int i;
    if (argc < 3) return 1;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(SHCreateItemFromParsingName(argv[2], NULL, &IID_IShellItemImageFactory, (void **)&factory))) return 2;
    for (i = 0; i < ARRAYSIZE(sizes); i++)
    {
        SIZE size = { sizes[i], sizes[i] };
        HBITMAP bmp = NULL;
        WCHAR path[MAX_PATH];
        HRESULT hr = IShellItemImageFactory_GetImage(factory, size, SIIGBF_RESIZETOFIT, &bmp);
        swprintf(path, MAX_PATH, L"%ls\\img-%d-%08lx.bmp", argv[1], sizes[i], hr);
        if (bmp) save(path, bmp), DeleteObject(bmp);
        else { FILE *f = _wfopen(path, L"w"); if (f) fclose(f); }
    }
    return 0;
}
