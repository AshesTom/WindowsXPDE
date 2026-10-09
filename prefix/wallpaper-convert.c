/* Convert a wallpaper image to the bitmap XP's desktop paints.
 * Usage: wallpaper-convert SOURCE DESTINATION
 *
 * XP shows a JPEG wallpaper by converting it to "Wallpaper1.bmp" in the
 * user's local application data; without that, it switches to the HTML
 * Active Desktop. This does the same conversion with Wine's WIC codecs. The
 * source file is only read. */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    IWICImagingFactory *factory = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    BITMAPFILEHEADER file = {0};
    BITMAPINFOHEADER info = {0};
    UINT width = 0, height = 0, stride;
    BYTE *bits = NULL;
    WCHAR temporary[MAX_PATH];
    HANDLE out;
    DWORD written;
    int result = 1;

    if (argc != 3) return 2;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IWICImagingFactory, (void **)&factory)) ||
        FAILED(IWICImagingFactory_CreateDecoderFromFilename(factory, argv[1], NULL, GENERIC_READ,
                                                            WICDecodeMetadataCacheOnDemand, &decoder)) ||
        FAILED(IWICBitmapDecoder_GetFrame(decoder, 0, &frame)) ||
        FAILED(IWICImagingFactory_CreateFormatConverter(factory, &converter)) ||
        FAILED(IWICFormatConverter_Initialize(converter, (IWICBitmapSource *)frame, &GUID_WICPixelFormat24bppBGR,
                                              WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom)) ||
        FAILED(IWICFormatConverter_GetSize(converter, &width, &height)) ||
        !width || !height || width > 16384 || height > 16384)
        goto done;
    stride = (width * 3 + 3) & ~3u;
    if (!(bits = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)stride * height))) goto done;
    /* Bottom-up DIB rows, as BMP files store them. */
    for (UINT y = 0; y < height; y++)
    {
        WICRect row = {0, (INT)(height - 1 - y), (INT)width, 1};
        if (FAILED(IWICFormatConverter_CopyPixels(converter, &row, stride, stride, bits + (SIZE_T)y * stride)))
            goto done;
    }
    info.biSize = sizeof(info);
    info.biWidth = width;
    info.biHeight = height;
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biSizeImage = stride * height;
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + info.biSizeImage;
    if (swprintf(temporary, MAX_PATH, L"%ls.tmp", argv[2]) < 0) goto done;
    out = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (out == INVALID_HANDLE_VALUE) goto done;
    result = !(WriteFile(out, &file, sizeof(file), &written, NULL) &&
               WriteFile(out, &info, sizeof(info), &written, NULL) &&
               WriteFile(out, bits, info.biSizeImage, &written, NULL));
    CloseHandle(out);
    if (!result && !MoveFileExW(temporary, argv[2], MOVEFILE_REPLACE_EXISTING)) result = 1;
    if (result) DeleteFileW(temporary);

done:
    if (bits) HeapFree(GetProcessHeap(), 0, bits);
    if (converter) IWICFormatConverter_Release(converter);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    if (factory) IWICImagingFactory_Release(factory);
    CoUninitialize();
    if (result) fprintf(stderr, "cannot convert wallpaper\n");
    return result;
}
