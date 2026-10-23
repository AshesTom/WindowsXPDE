/* XP's Network Connections folder (netshell.dll) shows the connections that
 * XP's Network Connections service (netman) reports through COM; Wine has no
 * netman. This local server stands in for it with the Linux system's network
 * adapters: every wired and wireless adapter becomes a "Local Area
 * Connection" or "Wireless Network Connection", with its state (connected,
 * cable unplugged, disabled), its hardware name and the Linux firewall.
 *
 * The interfaces are netman's, whose proxies netshell.dll registers:
 * INetConnectionManager2 hands netshell the properties of every connection
 * at once, as a SAFEARRAY of VARIANT arrays (NETCON_PROPERTIES_EX, read by
 * netshell field by field); INetConnectionManager, INetConnection and
 * IPersistNetConnection give the connection objects its pages and its
 * status window use. Every object lives in this process (CLSCTX_LOCAL_SERVER,
 * as netman's do); it ends a while after its last client is gone. */
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <stdio.h>
#include <wchar.h>

static const GUID CLSID_ConnectionManager = {0xba126ad1, 0x2166, 0x11d1, {0xb1, 0xd0, 0x00, 0x80, 0x5f, 0xc1, 0x27, 0x0e}};
static const GUID CLSID_ConnectionManager2 = {0xba126ae5, 0x2166, 0x11d1, {0xb1, 0xd0, 0x00, 0x80, 0x5f, 0xc1, 0x27, 0x0e}};
static const GUID CLSID_LanConnection = {0xba126adb, 0x2166, 0x11d1, {0xb1, 0xd0, 0x00, 0x80, 0x5f, 0xc1, 0x27, 0x0e}};
static const GUID CLSID_LanConnectionUi = {0x7007acc5, 0x3202, 0x11d1, {0xaa, 0xd2, 0x00, 0x80, 0x5f, 0xc1, 0x27, 0x0e}};
static const GUID IID_IEnumNetConnection = {0xc08956a0, 0x1cd3, 0x11d1, {0xb1, 0xc5, 0x00, 0x80, 0x5f, 0xc1, 0x27, 0x0e}};
static const GUID IID_INetConnection = {0xc08956a1, 0x1cd3, 0x11d1, {0xb1, 0xc5, 0x00, 0x80, 0x5f, 0xc1, 0x27, 0x0e}};
static const GUID IID_INetConnectionManager = {0xc08956a2, 0x1cd3, 0x11d1, {0xb1, 0xc5, 0x00, 0x80, 0x5f, 0xc1, 0x27, 0x0e}};
static const GUID IID_INetLanConnection = {0xfaedcf54, 0x31fe, 0x11d1, {0xaa, 0xd2, 0x00, 0x80, 0x5f, 0xc1, 0x27, 0x0e}};
static const GUID IID_IPersistNetConnection = {0xfaedcf59, 0x31fe, 0x11d1, {0xaa, 0xd2, 0x00, 0x80, 0x5f, 0xc1, 0x27, 0x0e}};
static const GUID IID_INetConnectionManager2 = {0xfaedcf69, 0x31fe, 0x11d1, {0xaa, 0xd2, 0x00, 0x80, 0x5f, 0xc1, 0x27, 0x0e}};

/* netcon.h */
enum { NCS_DISCONNECTED = 0, NCS_CONNECTED = 2, NCS_MEDIA_DISCONNECTED = 7 };
enum { NCM_LAN = 3 };
enum { NCSM_LAN = 1, NCSM_WIRELESS = 2 };
enum { NCCF_ALL_USERS = 0x1, NCCF_FIREWALLED = 0x400 };

typedef struct
{
    GUID id;
    WCHAR *name;
    WCHAR *device_name;
    DWORD status;
    DWORD media_type;
    DWORD characteristics;
    CLSID this_object;
    CLSID ui_object;
} NETCON_PROPERTIES;

typedef struct
{
    WCHAR *name;
    BOOL show_icon;
    GUID id;
} LANCON_INFO;

/* ---- The Linux adapters */

#define LINK_LIMIT 16

struct link
{
    WCHAR ifname[32];
    WCHAR name[128];
    WCHAR device[256];
    GUID id;
    DWORD status;
    BOOL wireless;
};

static LONG objects, locks;
static DWORD last_release;

static void object_added(void) { InterlockedIncrement(&objects); }
static void object_gone(void)
{
    if (!InterlockedDecrement(&objects)) last_release = GetTickCount();
}

static BOOL read_file(const WCHAR *path, char *buffer, DWORD size)
{
    DWORD read = 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    BOOL ok = ReadFile(file, buffer, size - 1, &read, NULL);
    CloseHandle(file);
    buffer[ok ? read : 0] = 0;
    return ok;
}

static BOOL interface_file(const WCHAR *ifname, const WCHAR *name, char *buffer, DWORD size)
{
    WCHAR path[MAX_PATH];
    swprintf(path, MAX_PATH, L"Z:\\sys\\class\\net\\%ls\\%ls", ifname, name);
    return read_file(path, buffer, size);
}

static BOOL interface_has(const WCHAR *ifname, const WCHAR *name)
{
    WCHAR path[MAX_PATH];
    swprintf(path, MAX_PATH, L"Z:\\sys\\class\\net\\%ls\\%ls", ifname, name);
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

/* The value of KEY= in a uevent file. */
static BOOL uevent_value(const char *text, const char *key, char *out, size_t size)
{
    size_t length = strlen(key);
    for (const char *line = text; line && *line; line = strchr(line, '\n') ? strchr(line, '\n') + 1 : NULL)
    {
        if (strncmp(line, key, length) || line[length] != '=') continue;
        const char *value = line + length + 1;
        size_t n = strcspn(value, "\r\n");
        if (n >= size) n = size - 1;
        memcpy(out, value, n);
        out[n] = 0;
        return TRUE;
    }
    return FALSE;
}

/* A device's name from the system's pci.ids or usb.ids ("Vendor Device"). */
static BOOL hardware_name(const WCHAR *const *files, unsigned vendor, unsigned product, WCHAR *out, size_t size)
{
    for (; *files; files++)
    {
        HANDLE file = CreateFileW(*files, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (file == INVALID_HANDLE_VALUE) continue;
        DWORD length = GetFileSize(file, NULL), read = 0;
        char *text = length != INVALID_FILE_SIZE ? HeapAlloc(GetProcessHeap(), 0, length + 1) : NULL;
        if (text && ReadFile(file, text, length, &read, NULL))
        {
            char vendor_key[8], product_key[8], vendor_name[128] = "", product_name[192] = "";
            text[read] = 0;
            snprintf(vendor_key, sizeof(vendor_key), "%04x  ", vendor);
            snprintf(product_key, sizeof(product_key), "\t%04x ", product);
            for (char *line = text; line && *line; line = strchr(line, '\n') ? strchr(line, '\n') + 1 : NULL)
            {
                if (!vendor_name[0])
                {
                    if (!strncmp(line, vendor_key, 6))
                        snprintf(vendor_name, sizeof(vendor_name), "%.*s", (int)strcspn(line + 6, "\r\n"), line + 6);
                    continue;
                }
                if (line[0] != '\t' && line[0] != '#') break;
                if (!strncmp(line, product_key, 6))
                {
                    const char *name = line + 6;
                    while (*name == ' ') name++;
                    snprintf(product_name, sizeof(product_name), "%.*s", (int)strcspn(name, "\r\n"), name);
                    break;
                }
            }
            if (vendor_name[0])
            {
                /* "Intel Corporation" reads as XP's "Intel". */
                static const char *const suffixes[] = {" Corporation", " Corp.", ", Inc.", " Inc.", " Co., Ltd.",
                                                       " Co., Ltd", " Semiconductor", " Technology",
                                                       " Communications"};
                for (unsigned i = 0; i < ARRAYSIZE(suffixes); i++)
                {
                    char *found = strstr(vendor_name, suffixes[i]);
                    if (found) *found = 0;
                }
                char name[320];
                snprintf(name, sizeof(name), "%s%s%s", vendor_name, product_name[0] ? " " : "", product_name);
                MultiByteToWideChar(CP_UTF8, 0, name, -1, out, (int)size);
            }
        }
        if (text) HeapFree(GetProcessHeap(), 0, text);
        CloseHandle(file);
        if (out[0]) return TRUE;
    }
    return FALSE;
}

static void device_name(const WCHAR *ifname, WCHAR *out, size_t size)
{
    static const WCHAR *const pci_ids[] = {L"Z:\\usr\\share\\hwdata\\pci.ids", L"Z:\\usr\\share\\misc\\pci.ids",
                                           L"Z:\\usr\\share\\pci.ids", NULL};
    static const WCHAR *const usb_ids[] = {L"Z:\\usr\\share\\hwdata\\usb.ids", L"Z:\\usr\\share\\misc\\usb.ids",
                                           L"Z:\\usr\\share\\usb.ids", NULL};
    char uevent[2048], value[128];
    unsigned vendor, product;
    out[0] = 0;
    if (interface_file(ifname, L"device\\uevent", uevent, sizeof(uevent)))
    {
        if (uevent_value(uevent, "PCI_ID", value, sizeof(value)) && sscanf(value, "%x:%x", &vendor, &product) == 2 &&
            hardware_name(pci_ids, vendor, product, out, size))
            return;
        if (uevent_value(uevent, "PRODUCT", value, sizeof(value)) && sscanf(value, "%x/%x", &vendor, &product) == 2 &&
            hardware_name(usb_ids, vendor, product, out, size))
            return;
        if (uevent_value(uevent, "DRIVER", value, sizeof(value)))
        {
            MultiByteToWideChar(CP_UTF8, 0, value, -1, out, (int)size);
            return;
        }
    }
    lstrcpynW(out, ifname, (int)size);
}

/* XP's names for its connections, from its own DLLs. */
static void xp_string(const WCHAR *module, UINT id, const WCHAR *fallback, WCHAR *out, int size)
{
    HMODULE library = LoadLibraryExW(module, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!library || !LoadStringW(library, id, out, size)) lstrcpynW(out, fallback, size);
    if (library) FreeLibrary(library);
}

/* A GUID that stays the same for an adapter: from its interface name. */
static GUID link_guid(const WCHAR *ifname)
{
    GUID id = {0x4c696e75, 0x7820, 0x4e65, {0}};
    unsigned long long hash = 1469598103934665603ULL;
    for (const WCHAR *c = ifname; *c; c++) hash = (hash ^ *c) * 1099511628211ULL;
    memcpy(id.Data4, &hash, sizeof(id.Data4));
    return id;
}

/* systemd's units that start with the system (wscfirewall.c). */
static BOOL unit_enabled(const WCHAR *unit)
{
    WCHAR path[MAX_PATH] = L"Z:\\etc\\systemd\\system\\multi-user.target.wants\\";
    lstrcatW(path, unit);
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static BOOL linux_firewall(void)
{
    char text[4096];
    if (unit_enabled(L"firewalld.service") || unit_enabled(L"nftables.service") || unit_enabled(L"iptables.service"))
        return TRUE;
    if (!unit_enabled(L"ufw.service") || !read_file(L"Z:\\etc\\ufw\\ufw.conf", text, sizeof(text))) return FALSE;
    for (char *line = text; line && *line; line = strchr(line, '\n') ? strchr(line, '\n') + 1 : NULL)
        if (!strncmp(line, "ENABLED=yes", 11)) return TRUE;
    return FALSE;
}

static int compare_links(const void *a, const void *b)
{
    const struct link *x = a, *y = b;
    if (x->wireless != y->wireless) return x->wireless - y->wireless;
    return lstrcmpW(x->ifname, y->ifname);
}

/* The physical Ethernet and wireless adapters (not lo, bridges or tunnels). */
static unsigned scan_links(struct link *links)
{
    WIN32_FIND_DATAW found;
    HANDLE find = FindFirstFileW(L"Z:\\sys\\class\\net\\*", &found);
    unsigned count = 0, wired = 0, wireless = 0;
    WCHAR lan[96], wlan[96];
    if (find == INVALID_HANDLE_VALUE) return 0;
    do
    {
        char text[64];
        struct link *link = &links[count];
        if (found.cFileName[0] == '.' || lstrlenW(found.cFileName) >= (int)ARRAYSIZE(link->ifname)) continue;
        if (!interface_file(found.cFileName, L"type", text, sizeof(text)) || atoi(text) != 1) continue;
        if (!interface_has(found.cFileName, L"device")) continue;
        memset(link, 0, sizeof(*link));
        lstrcpyW(link->ifname, found.cFileName);
        link->wireless = interface_has(link->ifname, L"wireless") || interface_has(link->ifname, L"phy80211");
        if (++count == LINK_LIMIT) break;
    } while (FindNextFileW(find, &found));
    FindClose(find);
    qsort(links, count, sizeof(*links), compare_links);
    xp_string(L"netshell.dll", 1811, L"Local Area Connection", lan, ARRAYSIZE(lan));
    xp_string(L"xpsp2res.dll", 16022, L"Wireless Network Connection", wlan, ARRAYSIZE(wlan));
    for (unsigned i = 0; i < count; i++)
    {
        struct link *link = &links[i];
        char flags[32] = "", carrier[8] = "", state[32] = "";
        unsigned number = link->wireless ? ++wireless : ++wired;
        if (number == 1) lstrcpyW(link->name, link->wireless ? wlan : lan);
        else swprintf(link->name, ARRAYSIZE(link->name), L"%ls %u", link->wireless ? wlan : lan, number);
        device_name(link->ifname, link->device, ARRAYSIZE(link->device));
        link->id = link_guid(link->ifname);
        interface_file(link->ifname, L"flags", flags, sizeof(flags));
        interface_file(link->ifname, L"carrier", carrier, sizeof(carrier));
        interface_file(link->ifname, L"operstate", state, sizeof(state));
        if (!(strtoul(flags, NULL, 16) & 1))
            link->status = NCS_DISCONNECTED;
        else if (atoi(carrier) == 1 && (!strncmp(state, "up", 2) || !strncmp(state, "unknown", 7)))
            link->status = NCS_CONNECTED;
        else
            link->status = NCS_MEDIA_DISCONNECTED;
    }
    return count;
}

static DWORD characteristics(void)
{
    return NCCF_ALL_USERS | (linux_firewall() ? NCCF_FIREWALLED : 0);
}

static BOOL find_link(const WCHAR *ifname, struct link *out)
{
    struct link links[LINK_LIMIT];
    unsigned count = scan_links(links);
    for (unsigned i = 0; i < count; i++)
        if (!lstrcmpW(links[i].ifname, ifname))
        {
            *out = links[i];
            return TRUE;
        }
    return FALSE;
}

static WCHAR *task_string(const WCHAR *text)
{
    size_t size = (lstrlenW(text) + 1) * sizeof(WCHAR);
    WCHAR *copy = CoTaskMemAlloc(size);
    if (copy) memcpy(copy, text, size);
    return copy;
}

/* ---- INetConnection, IPersistNetConnection, INetLanConnection: one adapter */

typedef struct connection connection;

struct connection_vtbl
{
    HRESULT (WINAPI *QueryInterface)(connection *, REFIID, void **);
    ULONG (WINAPI *AddRef)(connection *);
    ULONG (WINAPI *Release)(connection *);
    HRESULT (WINAPI *Connect)(connection *);
    HRESULT (WINAPI *Disconnect)(connection *);
    HRESULT (WINAPI *Delete)(connection *);
    HRESULT (WINAPI *Duplicate)(connection *, const WCHAR *, void **);
    HRESULT (WINAPI *GetProperties)(connection *, NETCON_PROPERTIES **);
    HRESULT (WINAPI *GetUiObjectClassId)(connection *, CLSID *);
    HRESULT (WINAPI *Rename)(connection *, const WCHAR *);
};

struct persist_vtbl
{
    HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **);
    ULONG (WINAPI *AddRef)(void *);
    ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *GetClassID)(void *, CLSID *);
    HRESULT (WINAPI *GetSizeMax)(void *, ULONG *);
    HRESULT (WINAPI *Load)(void *, const BYTE *, ULONG);
    HRESULT (WINAPI *Save)(void *, BYTE *, ULONG);
};

struct lan_vtbl
{
    HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **);
    ULONG (WINAPI *AddRef)(void *);
    ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *GetInfo)(void *, DWORD, LANCON_INFO *);
    HRESULT (WINAPI *SetInfo)(void *, DWORD, const LANCON_INFO *);
    HRESULT (WINAPI *GetDeviceGuid)(void *, GUID *);
};

struct connection
{
    const struct connection_vtbl *vtbl;
    const struct persist_vtbl *persist;
    const struct lan_vtbl *lan;
    LONG refs;
    WCHAR ifname[32];
};

#define FROM_PERSIST(p) ((connection *)((char *)(p) - offsetof(connection, persist)))
#define FROM_LAN(p) ((connection *)((char *)(p) - offsetof(connection, lan)))

static HRESULT WINAPI connection_QueryInterface(connection *self, REFIID iid, void **out)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_INetConnection)) *out = &self->vtbl;
    else if (IsEqualIID(iid, &IID_IPersistNetConnection)) *out = &self->persist;
    else if (IsEqualIID(iid, &IID_INetLanConnection)) *out = &self->lan;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    InterlockedIncrement(&self->refs);
    return S_OK;
}

static ULONG WINAPI connection_AddRef(connection *self) { return InterlockedIncrement(&self->refs); }

static ULONG WINAPI connection_Release(connection *self)
{
    ULONG refs = InterlockedDecrement(&self->refs);
    if (!refs)
    {
        HeapFree(GetProcessHeap(), 0, self);
        object_gone();
    }
    return refs;
}

/* NetworkManager connects and disconnects the adapter (Enable and Disable).
 * Wine runs a Unix executable named in CreateProcess natively, and cannot
 * wait for it: give it a moment before netshell reads the state again. */
static HRESULT nmcli_device(const WCHAR *ifname, const WCHAR *action)
{
    WCHAR command[160];
    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION process;
    swprintf(command, ARRAYSIZE(command), L"/usr/bin/env -u WINEDEBUG -u WINEDLLOVERRIDES nmcli device %ls %ls",
             action, ifname);
    if (!CreateProcessW(L"Z:\\usr\\bin\\env", command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup,
                        &process))
        return HRESULT_FROM_WIN32(GetLastError());
    if (process.hThread) CloseHandle(process.hThread);
    if (process.hProcess) CloseHandle(process.hProcess);
    Sleep(1500);
    return S_OK;
}

static HRESULT WINAPI connection_Connect(connection *self) { return nmcli_device(self->ifname, L"connect"); }
static HRESULT WINAPI connection_Disconnect(connection *self) { return nmcli_device(self->ifname, L"disconnect"); }
static HRESULT WINAPI connection_Delete(connection *self) { (void)self; return E_UNEXPECTED; }

static HRESULT WINAPI connection_Duplicate(connection *self, const WCHAR *name, void **out)
{
    (void)self; (void)name;
    *out = NULL;
    return E_UNEXPECTED;
}

static HRESULT WINAPI connection_GetProperties(connection *self, NETCON_PROPERTIES **out)
{
    struct link link;
    NETCON_PROPERTIES *properties;
    *out = NULL;
    if (!find_link(self->ifname, &link)) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    if (!(properties = CoTaskMemAlloc(sizeof(*properties)))) return E_OUTOFMEMORY;
    properties->id = link.id;
    properties->name = task_string(link.name);
    properties->device_name = task_string(link.device);
    properties->status = link.status;
    properties->media_type = NCM_LAN;
    properties->characteristics = characteristics();
    properties->this_object = CLSID_LanConnection;
    properties->ui_object = CLSID_LanConnectionUi;
    *out = properties;
    return S_OK;
}

static HRESULT WINAPI connection_GetUiObjectClassId(connection *self, CLSID *out)
{
    (void)self;
    *out = CLSID_LanConnectionUi;
    return S_OK;
}

static HRESULT WINAPI connection_Rename(connection *self, const WCHAR *name)
{
    (void)self; (void)name;
    return E_ACCESSDENIED;
}

static HRESULT WINAPI persist_QueryInterface(void *self, REFIID iid, void **out)
{
    return connection_QueryInterface(FROM_PERSIST(self), iid, out);
}
static ULONG WINAPI persist_AddRef(void *self) { return connection_AddRef(FROM_PERSIST(self)); }
static ULONG WINAPI persist_Release(void *self) { return connection_Release(FROM_PERSIST(self)); }

static HRESULT WINAPI persist_GetClassID(void *self, CLSID *out)
{
    (void)self;
    *out = CLSID_LanConnection;
    return S_OK;
}

/* The persisted form is the Linux interface name, as netshell keeps it in
 * NETCON_PROPERTIES_EX's bstrPersistData. */
static HRESULT WINAPI persist_GetSizeMax(void *self, ULONG *size)
{
    *size = (lstrlenW(FROM_PERSIST(self)->ifname) + 1) * sizeof(WCHAR);
    return S_OK;
}

static HRESULT WINAPI persist_Load(void *self, const BYTE *data, ULONG size)
{
    connection *object = FROM_PERSIST(self);
    ULONG length = size / sizeof(WCHAR);
    if (!data || !length || length > ARRAYSIZE(object->ifname)) return E_INVALIDARG;
    memcpy(object->ifname, data, length * sizeof(WCHAR));
    object->ifname[length - 1] = 0;
    return S_OK;
}

static HRESULT WINAPI persist_Save(void *self, BYTE *data, ULONG size)
{
    connection *object = FROM_PERSIST(self);
    ULONG needed = (lstrlenW(object->ifname) + 1) * sizeof(WCHAR);
    if (size < needed) return E_INVALIDARG;
    memcpy(data, object->ifname, needed);
    return S_OK;
}

static HRESULT WINAPI lan_QueryInterface(void *self, REFIID iid, void **out)
{
    return connection_QueryInterface(FROM_LAN(self), iid, out);
}
static ULONG WINAPI lan_AddRef(void *self) { return connection_AddRef(FROM_LAN(self)); }
static ULONG WINAPI lan_Release(void *self) { return connection_Release(FROM_LAN(self)); }

/* LCIF_NAME 1, LCIF_ICON 2, LCIF_COMP 8: XP shows the notification area
 * icon of the connection on request; the Linux one is xptray.exe's. */
static HRESULT WINAPI lan_GetInfo(void *self, DWORD mask, LANCON_INFO *info)
{
    struct link link;
    if (!find_link(FROM_LAN(self)->ifname, &link)) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    memset(info, 0, sizeof(*info));
    if (mask & 1) info->name = task_string(link.name);
    if (mask & 2) info->show_icon = FALSE;
    info->id = link.id;
    return S_OK;
}

static HRESULT WINAPI lan_SetInfo(void *self, DWORD mask, const LANCON_INFO *info)
{
    (void)self; (void)mask; (void)info;
    return S_OK;
}

static HRESULT WINAPI lan_GetDeviceGuid(void *self, GUID *out)
{
    *out = link_guid(FROM_LAN(self)->ifname);
    return S_OK;
}

static const struct connection_vtbl connection_vtbl = {
    connection_QueryInterface, connection_AddRef, connection_Release, connection_Connect, connection_Disconnect,
    connection_Delete, connection_Duplicate, connection_GetProperties, connection_GetUiObjectClassId,
    connection_Rename};
static const struct persist_vtbl persist_vtbl = {
    persist_QueryInterface, persist_AddRef, persist_Release, persist_GetClassID, persist_GetSizeMax, persist_Load,
    persist_Save};
static const struct lan_vtbl lan_vtbl = {
    lan_QueryInterface, lan_AddRef, lan_Release, lan_GetInfo, lan_SetInfo, lan_GetDeviceGuid};

static connection *connection_create(const WCHAR *ifname)
{
    connection *object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object) return NULL;
    object->vtbl = &connection_vtbl;
    object->persist = &persist_vtbl;
    object->lan = &lan_vtbl;
    object->refs = 1;
    if (ifname) lstrcpynW(object->ifname, ifname, ARRAYSIZE(object->ifname));
    object_added();
    return object;
}

/* ---- IEnumNetConnection */

typedef struct enumerator enumerator;

struct enumerator_vtbl
{
    HRESULT (WINAPI *QueryInterface)(enumerator *, REFIID, void **);
    ULONG (WINAPI *AddRef)(enumerator *);
    ULONG (WINAPI *Release)(enumerator *);
    HRESULT (WINAPI *Next)(enumerator *, ULONG, connection **, ULONG *);
    HRESULT (WINAPI *Skip)(enumerator *, ULONG);
    HRESULT (WINAPI *Reset)(enumerator *);
    HRESULT (WINAPI *Clone)(enumerator *, enumerator **);
};

struct enumerator
{
    const struct enumerator_vtbl *vtbl;
    LONG refs;
    unsigned count, position;
    struct link links[LINK_LIMIT];
};

static HRESULT WINAPI enumerator_QueryInterface(enumerator *self, REFIID iid, void **out)
{
    if (!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_IEnumNetConnection))
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    *out = self;
    InterlockedIncrement(&self->refs);
    return S_OK;
}

static ULONG WINAPI enumerator_AddRef(enumerator *self) { return InterlockedIncrement(&self->refs); }

static ULONG WINAPI enumerator_Release(enumerator *self)
{
    ULONG refs = InterlockedDecrement(&self->refs);
    if (!refs)
    {
        HeapFree(GetProcessHeap(), 0, self);
        object_gone();
    }
    return refs;
}

static HRESULT WINAPI enumerator_Next(enumerator *self, ULONG wanted, connection **out, ULONG *fetched)
{
    ULONG given = 0;
    while (given < wanted && self->position < self->count)
    {
        if (!(out[given] = connection_create(self->links[self->position].ifname))) break;
        given++;
        self->position++;
    }
    if (fetched) *fetched = given;
    return given == wanted ? S_OK : S_FALSE;
}

static HRESULT WINAPI enumerator_Skip(enumerator *self, ULONG count)
{
    self->position = min(self->count, self->position + count);
    return self->position < self->count ? S_OK : S_FALSE;
}

static HRESULT WINAPI enumerator_Reset(enumerator *self)
{
    self->position = 0;
    return S_OK;
}

static HRESULT WINAPI enumerator_Clone(enumerator *self, enumerator **out);

static const struct enumerator_vtbl enumerator_vtbl = {
    enumerator_QueryInterface, enumerator_AddRef, enumerator_Release, enumerator_Next, enumerator_Skip,
    enumerator_Reset, enumerator_Clone};

static enumerator *enumerator_create(void)
{
    enumerator *object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object) return NULL;
    object->vtbl = &enumerator_vtbl;
    object->refs = 1;
    object->count = scan_links(object->links);
    object_added();
    return object;
}

static HRESULT WINAPI enumerator_Clone(enumerator *self, enumerator **out)
{
    enumerator *copy = HeapAlloc(GetProcessHeap(), 0, sizeof(*copy));
    if (!(*out = copy)) return E_OUTOFMEMORY;
    *copy = *self;
    copy->refs = 1;
    object_added();
    return S_OK;
}

/* ---- INetConnectionManager and INetConnectionManager2 */

typedef struct manager manager;

struct manager_vtbl
{
    HRESULT (WINAPI *QueryInterface)(manager *, REFIID, void **);
    ULONG (WINAPI *AddRef)(manager *);
    ULONG (WINAPI *Release)(manager *);
    HRESULT (WINAPI *EnumConnections)(manager *, DWORD, enumerator **);
};

struct manager2_vtbl
{
    HRESULT (WINAPI *QueryInterface)(manager *, REFIID, void **);
    ULONG (WINAPI *AddRef)(manager *);
    ULONG (WINAPI *Release)(manager *);
    HRESULT (WINAPI *EnumConnectionProperties)(manager *, SAFEARRAY **);
};

struct manager
{
    const void *vtbl;
    LONG refs;
    BOOL second;
};

static HRESULT WINAPI manager_QueryInterface(manager *self, REFIID iid, void **out)
{
    if (!IsEqualIID(iid, &IID_IUnknown) &&
        !IsEqualIID(iid, self->second ? &IID_INetConnectionManager2 : &IID_INetConnectionManager))
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    *out = self;
    InterlockedIncrement(&self->refs);
    return S_OK;
}

static ULONG WINAPI manager_AddRef(manager *self) { return InterlockedIncrement(&self->refs); }

static ULONG WINAPI manager_Release(manager *self)
{
    ULONG refs = InterlockedDecrement(&self->refs);
    if (!refs)
    {
        HeapFree(GetProcessHeap(), 0, self);
        object_gone();
    }
    return refs;
}

/* INetConnectionManager::EnumConnections(NETCONMGR_ENUM_FLAGS, IEnumNetConnection **) */
static HRESULT WINAPI manager_EnumConnections(manager *self, DWORD flags, enumerator **out)
{
    (void)self; (void)flags;
    return (*out = enumerator_create()) ? S_OK : E_OUTOFMEMORY;
}

static void put(SAFEARRAY *array, LONG index, VARIANT *value)
{
    SafeArrayPutElement(array, &index, value);
    VariantClear(value);
}

static void put_long(SAFEARRAY *array, LONG index, LONG number)
{
    VARIANT value;
    V_VT(&value) = VT_I4;
    V_I4(&value) = number;
    put(array, index, &value);
}

static void put_string(SAFEARRAY *array, LONG index, BSTR string)
{
    VARIANT value;
    V_VT(&value) = VT_BSTR;
    V_BSTR(&value) = string;
    put(array, index, &value);
}

static void put_guid(SAFEARRAY *array, LONG index, const GUID *id)
{
    WCHAR text[40];
    StringFromGUID2(id, text, ARRAYSIZE(text));
    put_string(array, index, SysAllocString(text));
}

/* INetConnectionManager2::EnumConnectionProperties(SAFEARRAY(VARIANT) *): one
 * VARIANT array per connection, in NETCON_PROPERTIES_EX's order. */
static HRESULT WINAPI manager_EnumConnectionProperties(manager *self, SAFEARRAY **out)
{
    struct link links[LINK_LIMIT];
    unsigned count = scan_links(links);
    DWORD flags = characteristics();
    SAFEARRAY *all = SafeArrayCreateVector(VT_VARIANT, 0, count);
    (void)self;
    if (!all) return E_OUTOFMEMORY;
    for (unsigned i = 0; i < count; i++)
    {
        SAFEARRAY *fields = SafeArrayCreateVector(VT_VARIANT, 0, 12);
        VARIANT value;
        if (!fields) continue;
        put_long(fields, 0, 0x54);  /* sizeof(NETCON_PROPERTIES_EX) */
        put_guid(fields, 1, &links[i].id);
        put_string(fields, 2, SysAllocString(links[i].name));
        put_string(fields, 3, SysAllocString(links[i].device));
        put_long(fields, 4, links[i].status);
        put_long(fields, 5, NCM_LAN);
        put_long(fields, 6, links[i].wireless ? NCSM_WIRELESS : NCSM_LAN);
        put_long(fields, 7, flags);
        put_guid(fields, 8, &CLSID_LanConnection);
        put_guid(fields, 9, &CLSID_LanConnectionUi);
        put_string(fields, 10, SysAllocString(L""));
        put_string(fields, 11, SysAllocStringByteLen((const char *)links[i].ifname,
                                                     (lstrlenW(links[i].ifname) + 1) * sizeof(WCHAR)));
        V_VT(&value) = VT_ARRAY | VT_VARIANT;
        V_ARRAY(&value) = fields;
        put(all, i, &value);
    }
    *out = all;
    return S_OK;
}

static const struct manager_vtbl manager_vtbl = {
    manager_QueryInterface, manager_AddRef, manager_Release, manager_EnumConnections};
static const struct manager2_vtbl manager2_vtbl = {
    manager_QueryInterface, manager_AddRef, manager_Release, manager_EnumConnectionProperties};

/* ---- Class factories */

typedef struct factory factory;

struct factory_vtbl
{
    HRESULT (WINAPI *QueryInterface)(factory *, REFIID, void **);
    ULONG (WINAPI *AddRef)(factory *);
    ULONG (WINAPI *Release)(factory *);
    HRESULT (WINAPI *CreateInstance)(factory *, IUnknown *, REFIID, void **);
    HRESULT (WINAPI *LockServer)(factory *, BOOL);
};

struct factory
{
    const struct factory_vtbl *vtbl;
    const GUID *clsid;
};

static HRESULT WINAPI factory_QueryInterface(factory *self, REFIID iid, void **out)
{
    if (!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_IClassFactory))
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    *out = self;
    return S_OK;
}

static ULONG WINAPI factory_AddRef(factory *self) { (void)self; return 2; }
static ULONG WINAPI factory_Release(factory *self) { (void)self; return 1; }

static HRESULT WINAPI factory_CreateInstance(factory *self, IUnknown *outer, REFIID iid, void **out)
{
    IUnknown *object;
    HRESULT hr;
    *out = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (IsEqualCLSID(self->clsid, &CLSID_LanConnection))
        object = (IUnknown *)connection_create(NULL);
    else
    {
        manager *created = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*created));
        if (created)
        {
            created->second = IsEqualCLSID(self->clsid, &CLSID_ConnectionManager2);
            created->vtbl = created->second ? (const void *)&manager2_vtbl : (const void *)&manager_vtbl;
            created->refs = 1;
            object_added();
        }
        object = (IUnknown *)created;
    }
    if (!object) return E_OUTOFMEMORY;
    hr = object->lpVtbl->QueryInterface(object, iid, out);
    object->lpVtbl->Release(object);
    return hr;
}

static HRESULT WINAPI factory_LockServer(factory *self, BOOL lock)
{
    (void)self;
    if (lock) InterlockedIncrement(&locks);
    else if (!InterlockedDecrement(&locks)) last_release = GetTickCount();
    return S_OK;
}

static const struct factory_vtbl factory_vtbl = {
    factory_QueryInterface, factory_AddRef, factory_Release, factory_CreateInstance, factory_LockServer};

static factory factories[] = {
    {&factory_vtbl, &CLSID_ConnectionManager},
    {&factory_vtbl, &CLSID_ConnectionManager2},
    {&factory_vtbl, &CLSID_LanConnection},
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, WCHAR *command_line, int show)
{
    DWORD cookies[ARRAYSIZE(factories)];
    (void)instance; (void)previous; (void)command_line; (void)show;
    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) return 1;
    for (unsigned i = 0; i < ARRAYSIZE(factories); i++)
        CoRegisterClassObject(factories[i].clsid, (IUnknown *)&factories[i], CLSCTX_LOCAL_SERVER,
                              REGCLS_MULTIPLEUSE, &cookies[i]);
    /* Ends two minutes after its last object and lock are gone. */
    last_release = GetTickCount();
    for (;;)
    {
        Sleep(5000);
        if (!objects && !locks && GetTickCount() - last_release > 120000) break;
    }
    for (unsigned i = 0; i < ARRAYSIZE(factories); i++) CoRevokeClassObject(cookies[i]);
    CoUninitialize();
    return 0;
}
