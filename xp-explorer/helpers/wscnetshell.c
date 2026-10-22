/* netshell.dll beside XP's Security Center service (wscsvc.dll), in
 * C:\xp\wsc: the one call it imports from XP's netshell, whose own imports
 * (dot3api, dot3dlg...) Wine does not have. */
#include <windows.h>
#include <objbase.h>

/* The start of NETCON_PROPERTIES (netcon.h): the two strings it owns. */
struct netcon_properties
{
    GUID id;
    WCHAR *name;
    WCHAR *device_name;
};

void WINAPI NcFreeNetconProperties(struct netcon_properties *properties)
{
    if (!properties) return;
    CoTaskMemFree(properties->name);
    CoTaskMemFree(properties->device_name);
    CoTaskMemFree(properties);
}
