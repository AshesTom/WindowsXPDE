/* Discover COM class providers through their public factory entry point. */
#ifndef UNICODE
#define UNICODE
#endif
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

typedef HRESULT (WINAPI *factory_fn)(REFCLSID, REFIID, void **);
int wmain(int argc, WCHAR **argv)
{
    CLSID clsid;
    HMODULE module;
    factory_fn factory;
    IClassFactory *object = NULL;
    HRESULT hr;
    if (argc != 3 || FAILED(CLSIDFromString(argv[2], &clsid))) return 2;
    CoInitialize(NULL);
    module = LoadLibraryW(argv[1]);
    if (!module) { printf("load-error=%lu\n", GetLastError()); return 3; }
    factory = (factory_fn)(void *)GetProcAddress(module, "DllGetClassObject");
    if (!factory) return 4;
    hr = factory(&clsid, &IID_IClassFactory, (void **)&object);
    printf("HRESULT=%08lx\n", (unsigned long)hr);
    if (SUCCEEDED(hr) && object) IClassFactory_Release(object);
    FreeLibrary(module);
    CoUninitialize();
    return FAILED(hr) ? 1 : 0;
}
