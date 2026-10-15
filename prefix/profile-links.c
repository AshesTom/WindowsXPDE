/* Make Windows 7's Favorites links for the user (Windows 7's shell prefix).
 * Usage: profile-links
 *
 * Windows 7 gives a new user three links in the Links folder, which its
 * folder windows show under Favorites: the desktop, Downloads and Recent
 * Places. This makes them through IShellLink, pointing at the user's own
 * known folders, as Windows 7's links do (prefix/win7.py writes the
 * folder's desktop.ini with their Japanese names). An existing link is
 * left alone. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <stdio.h>

static int make_link(const WCHAR *folder, const WCHAR *name, PIDLIST_ABSOLUTE target)
{
    IShellLinkW *link = NULL;
    IPersistFile *file = NULL;
    WCHAR path[MAX_PATH];
    HRESULT hr;

    swprintf(path, MAX_PATH, L"%ls\\%ls", folder, name);
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) return 0;
    if (!target) return 1;
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link);
    if (SUCCEEDED(hr)) hr = IShellLinkW_SetIDList(link, target);
    if (SUCCEEDED(hr)) hr = IShellLinkW_QueryInterface(link, &IID_IPersistFile, (void **)&file);
    if (SUCCEEDED(hr)) hr = IPersistFile_Save(file, path, TRUE);
    if (file) IPersistFile_Release(file);
    if (link) IShellLinkW_Release(link);
    if (FAILED(hr)) fwprintf(stderr, L"profile-links: %ls: %08lx\n", path, hr);
    return FAILED(hr);
}

int wmain(void)
{
    PIDLIST_ABSOLUTE desktop = NULL, downloads = NULL, recent = NULL;
    WCHAR *links = NULL;
    int failed = 0;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(SHGetKnownFolderPath(&FOLDERID_Links, KF_FLAG_CREATE, NULL, &links))) return 1;
    SHGetKnownFolderIDList(&FOLDERID_Desktop, 0, NULL, &desktop);
    SHGetKnownFolderIDList(&FOLDERID_Downloads, 0, NULL, &downloads);
    /* Recent Places: a desktop item of its class, as in Windows 7's link
     * (size, type 0x1f, sort order 0x80, the class, the terminating 0) */
    if ((recent = CoTaskMemAlloc(22)))
    {
        static const GUID recent_places = {0x22877a6d, 0x37a1, 0x461a, {0x91, 0xb0, 0xdb, 0xda, 0x5a, 0xae, 0xbc, 0x99}};
        BYTE *item = (BYTE *)recent;
        item[0] = 20; item[1] = 0; item[2] = 0x1f; item[3] = 0x80;
        memcpy(item + 4, &recent_places, sizeof(recent_places));
        item[20] = item[21] = 0;
    }
    failed |= make_link(links, L"Desktop.lnk", desktop);
    failed |= make_link(links, L"Downloads.lnk", downloads);
    failed |= make_link(links, L"RecentPlaces.lnk", recent);
    CoTaskMemFree(desktop);
    CoTaskMemFree(downloads);
    CoTaskMemFree(recent);
    CoTaskMemFree(links);
    CoUninitialize();
    return failed;
}
