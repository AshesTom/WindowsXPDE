/* rasdlg.dll for XP's netshell.dll (the Network Connections folder) in the
 * XP shell's processes. netshell asks XP's Dial-Up Networking dialogs whether
 * Windows dials manually, and offers its wizards; Wine's rasdlg leaves these
 * as stubs, whose first call ends the shell. Without dial-up connections,
 * each answers as XP does when Remote Access has nothing to show. The
 * shapes are XP SP3 rasdlg.dll's, all stdcall. */
#include <windows.h>

#define ERROR_UNSUPPORTED ERROR_CALL_NOT_IMPLEMENTED

/* RasUserGetManualDial(HWND, BOOL, BOOL *): "operator-assisted dialing" off. */
DWORD WINAPI RasUserGetManualDial(HWND window, BOOL unused, BOOL *manual)
{
    (void)window; (void)unused;
    if (manual) *manual = FALSE;
    return ERROR_SUCCESS;
}

DWORD WINAPI RasUserEnableManualDial(HWND window, BOOL unused, BOOL manual)
{
    (void)window; (void)unused; (void)manual;
    return ERROR_SUCCESS;
}

BOOL WINAPI RasUserPrefsDlg(HWND window)
{
    (void)window;
    return FALSE;
}

BOOL WINAPI RasDialDlgW(WCHAR *phonebook, WCHAR *entry, WCHAR *number, void *info)
{
    (void)phonebook; (void)entry; (void)number; (void)info;
    SetLastError(ERROR_UNSUPPORTED);
    return FALSE;
}

BOOL WINAPI RasEntryDlgW(WCHAR *phonebook, WCHAR *entry, void *info)
{
    (void)phonebook; (void)entry; (void)info;
    SetLastError(ERROR_UNSUPPORTED);
    return FALSE;
}

BOOL WINAPI RasPhonebookDlgW(WCHAR *phonebook, WCHAR *entry, void *info)
{
    (void)phonebook; (void)entry; (void)info;
    SetLastError(ERROR_UNSUPPORTED);
    return FALSE;
}

/* The New Connection Wizard's dial-up and incoming pages: none. */
DWORD WINAPI RasWizQueryMaxPageCount(DWORD type)
{
    (void)type;
    return 0;
}

DWORD WINAPI RasWizCreateNewEntry(DWORD type, void *data, WCHAR *name, WCHAR *device, DWORD *flags)
{
    (void)type; (void)data; (void)name; (void)device; (void)flags;
    return ERROR_UNSUPPORTED;
}

DWORD WINAPI RasWizGetNCCFlags(DWORD type, void *data, DWORD *flags)
{
    (void)type; (void)data;
    if (flags) *flags = 0;
    return ERROR_UNSUPPORTED;
}

DWORD WINAPI RasWizGetSuggestedEntryName(DWORD type, void *data, WCHAR *name)
{
    (void)type; (void)data;
    if (name) *name = 0;
    return ERROR_UNSUPPORTED;
}

DWORD WINAPI RasWizGetUserInputConnectionName(void *data, WCHAR *name)
{
    (void)data;
    if (name) *name = 0;
    return ERROR_UNSUPPORTED;
}

DWORD WINAPI RasWizIsEntryRenamable(DWORD type, void *data, BOOL *renamable)
{
    (void)type; (void)data;
    if (renamable) *renamable = FALSE;
    return ERROR_UNSUPPORTED;
}

DWORD WINAPI RasWizSetEntryName(DWORD type, void *data, const WCHAR *name)
{
    (void)type; (void)data; (void)name;
    return ERROR_UNSUPPORTED;
}

DWORD WINAPI RasSrvAddPropPages(HANDLE connection, HWND window, void *add, LPARAM lparam, void **context)
{
    (void)connection; (void)window; (void)add; (void)lparam; (void)context;
    return ERROR_UNSUPPORTED;
}

DWORD WINAPI RasSrvAddWizPages(void *add, LPARAM lparam, void **context)
{
    (void)add; (void)lparam; (void)context;
    return ERROR_UNSUPPORTED;
}
