/* HNetCfg.dll beside XP's Security Center service (wscsvc.dll), in
 * C:\xp\wsc: the private Internet Connection Firewall calls it imports, and
 * the SharedAccess service (Windows Firewall) whose state it checks first.
 * Wine's hnetcfg has neither. The firewall is the Linux system's: on when
 * firewalld, nftables or iptables starts with the system, or ufw does and is enabled;
 * XP's EnableFirewall policy value overrides it.
 *
 * The calls' shapes are wscsvc.dll's (XP SP3), all stdcall returning a
 * Win32 error: IcfConnect(&h), IcfGetOperationalMode(h, &enabled,
 * &no_exceptions), IcfGetAdapters(h, &list) with list {count, entries of
 * 0x54 bytes}, the matching frees, and the change notifications. */
#include <windows.h>

#define POLICY L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile"

struct adapters
{
    DWORD count;
    void *entries;
};

/* A unit the Linux system starts at boot. (systemd's /run/systemd/units
 * names the running ones with a colon, which Wine cannot show.) */
static BOOL unit_active(const WCHAR *unit)
{
    WCHAR path[MAX_PATH] = L"Z:\\etc\\systemd\\system\\multi-user.target.wants\\";
    lstrcatW(path, unit);
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static BOOL ufw_enabled(void)
{
    char text[4096];
    DWORD size = 0;
    HANDLE file = CreateFileW(L"Z:\\etc\\ufw\\ufw.conf", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    ReadFile(file, text, sizeof(text) - 1, &size, NULL);
    CloseHandle(file);
    text[size] = 0;
    for (char *line = text; line && *line; line = strchr(line, '\n') ? strchr(line, '\n') + 1 : NULL)
        if (!strncmp(line, "ENABLED=yes", 11)) return TRUE;
    return FALSE;
}

static DWORD policy(const WCHAR *name, DWORD fallback)
{
    DWORD value, size = sizeof(value);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, POLICY, name, RRF_RT_REG_DWORD, NULL, &value, &size)) return fallback;
    return value;
}

static BOOL linux_firewall(void)
{
    return unit_active(L"firewalld.service") || unit_active(L"nftables.service") ||
           unit_active(L"iptables.service") || (unit_active(L"ufw.service") && ufw_enabled());
}

DWORD WINAPI IcfConnect(HANDLE *handle)
{
    *handle = (HANDLE)1;
    return ERROR_SUCCESS;
}

DWORD WINAPI IcfDisconnect(HANDLE *handle)
{
    *handle = NULL;
    return ERROR_SUCCESS;
}

DWORD WINAPI IcfGetOperationalMode(HANDLE handle, DWORD *enabled, DWORD *no_exceptions)
{
    (void)handle;
    *enabled = policy(L"EnableFirewall", linux_firewall());
    *no_exceptions = policy(L"DoNotAllowExceptions", 0);
    return ERROR_SUCCESS;
}

/* No adapter of its own: the firewall covers every connection. */
DWORD WINAPI IcfGetAdapters(HANDLE handle, struct adapters **list)
{
    (void)handle;
    if (!(*list = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(**list)))) return ERROR_OUTOFMEMORY;
    return ERROR_SUCCESS;
}

DWORD WINAPI IcfFreeAdapters(struct adapters **list)
{
    HeapFree(GetProcessHeap(), 0, *list);
    *list = NULL;
    return ERROR_SUCCESS;
}

/* The Linux firewall's changes are seen at the next check. */
DWORD WINAPI IcfChangeNotificationCreate(HANDLE event, HANDLE *notification)
{
    (void)event;
    *notification = (HANDLE)1;
    return ERROR_SUCCESS;
}

DWORD WINAPI IcfChangeNotificationDestroy(HANDLE *notification)
{
    *notification = NULL;
    return ERROR_SUCCESS;
}

/* The SharedAccess service, in svchost: running while the session is. */
static SERVICE_STATUS_HANDLE status_handle;
static SERVICE_STATUS status = {SERVICE_WIN32_SHARE_PROCESS, SERVICE_START_PENDING, SERVICE_ACCEPT_STOP, 0, 0, 0, 0};

static DWORD WINAPI control(DWORD code, DWORD type, void *data, void *context)
{
    (void)type; (void)data; (void)context;
    if (code == SERVICE_CONTROL_STOP || code == SERVICE_CONTROL_SHUTDOWN)
    {
        status.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(status_handle, &status);
        return NO_ERROR;
    }
    SetServiceStatus(status_handle, &status);
    return code == SERVICE_CONTROL_INTERROGATE ? NO_ERROR : ERROR_CALL_NOT_IMPLEMENTED;
}

void WINAPI ServiceMain(DWORD argc, WCHAR **argv)
{
    (void)argc; (void)argv;
    if (!(status_handle = RegisterServiceCtrlHandlerExW(L"SharedAccess", control, NULL))) return;
    status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(status_handle, &status);
}
