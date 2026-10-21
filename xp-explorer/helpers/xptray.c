/* Network, power and volume icons for XP's notification area.
 *
 * On XP these came from systray.exe/stobject.dll and netshell.dll; under Wine
 * there are none, so this program reads the Linux state instead:
 *   power    /sys/class/power_supply (battery level, AC, time remaining)
 *   network  /sys/class/net, /proc/net/route and nmcli (SSID, signal, speed)
 *   volume   wpctl (PipeWire) or pactl (PulseAudio)
 * and shows XP's own icons, flyouts and the Network Connections panel, from
 * the user's ISO extracted into C:\xp\tray.
 *
 * The icons are sent to XP's Shell_TrayWnd in XP's own WM_COPYDATA format,
 * since Wine's builtin Shell_NotifyIcon speaks Wine's format, not XP's. */
#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define WM_TRAY      (WM_APP + 1)
#define WM_OPEN_CONNECTIONS (WM_APP + 2)
#define WM_OPEN_WIRELESS (WM_APP + 3)
#define ID_POWER     1
#define ID_NETWORK   2
#define ID_VOLUME    3
#define ID_COUNT     4
#define TIMER_POLL   1
#define TIMER_FIND   2
#define TIMER_APPLY  3
#define TIMER_CONNECTIONS 5
#define TIMER_WIRELESS 6
#define TIMER_WIRELESS_CONNECT 7
#define CMD_MIXER    100
#define CMD_SOUND    101
#define CMD_NETWORK  102
#define CMD_POWER    103
#define CMD_EXIT     104
#define CMD_STATUS   105
#define CMD_METER    106
#define CMD_CONNECTIONS 108
#define CMD_NETWORK_SETTINGS 109

#define CONNECTION_LIMIT 64
#define CONN_CONNECT 500
#define CONN_STATUS  501
#define CONN_SETTINGS 502
#define CONN_REFRESH 503
#define CONN_CLOSE   504
#define WIFI_TASKS 600
#define WIFI_LIST 601
#define WIFI_REFRESH 602
#define WIFI_SETTINGS 603
#define WIFI_CONNECT 604
#define WIFI_HELP 605

/* XP's NOTIFYICONDATAW (version 3, 956 bytes) in a 32-bit process, wrapped as
 * its tray expects it in WM_COPYDATA. */
typedef struct
{
    DWORD cbSize;
    DWORD hWnd;
    UINT uID;
    UINT uFlags;
    UINT uCallbackMessage;
    DWORD hIcon;
    WCHAR szTip[128];
    DWORD dwState;
    DWORD dwStateMask;
    WCHAR szInfo[256];
    UINT uTimeout;
    WCHAR szInfoTitle[64];
    DWORD dwInfoFlags;
    GUID guidItem;
} XPNOTIFYICONDATA;

typedef struct
{
    DWORD dwSignature;
    DWORD dwMessage;
    XPNOTIFYICONDATA nid;
} XPTRAYDATA;

typedef struct
{
    BOOL shown;
    HICON icon;
    WCHAR tip[128];
} TRAYICON;

static HWND window, popup;
static HWND connections_window, connections_list, connections_hint, connections_connect;
static HWND wireless_window, wireless_list, wireless_hint, wireless_connect_button;
static HFONT wireless_title_font;
static HFONT wireless_text_font;
static TRAYICON icons[ID_COUNT];
static UINT taskbar_created;
static int tick;

typedef struct
{
    WCHAR name[128];
    WCHAR uuid[40];
    WCHAR type[48];
    WCHAR device[48];
    BOOL active;
} NMCONNECTION;

typedef struct
{
    WCHAR ssid[128];
    WCHAR bssid[32];
    WCHAR security[64];
    int signal;
    BOOL secured;
    BOOL active;
} NMWIRELESS;

static NMCONNECTION connections[CONNECTION_LIMIT];
static unsigned connection_count;
static int nmcli_checked, nmcli_available;
static NMWIRELESS wireless_networks[CONNECTION_LIMIT];
static unsigned wireless_count;
static ULONGLONG wireless_connect_started;
static WCHAR wireless_connect_ssid[128];

/* Linux state */
static int volume = -1, muted;
static int wanted_volume = -1;
static int battery = -1, charging, on_ac, minutes_left = -1, warned_low;
static WCHAR net_name[32];
static int net_up = -1, net_wireless, net_limited, net_signal = -1, net_speed;
static WCHAR net_ssid[64];
static unsigned long long rx_last, tx_last;

/* XP icons, by resource ID in the files extracted from the user's ISO. */
static struct { const WCHAR *file; int id; HICON icon; } xp_icons[] = {
    { L"stobject.dll", 230, NULL },  /* 0 volume */
    { L"stobject.dll", 231, NULL },  /* 1 volume muted */
    { L"stobject.dll", 200, NULL },  /* 2 plugged in, with a battery */
    { L"powercfg.cpl", 202, NULL },  /* 3 running on battery */
    { L"netshell.dll", 190, NULL },  /* 4 LAN idle */
    { L"netshell.dll", 191, NULL },  /* 5 LAN sending */
    { L"netshell.dll", 192, NULL },  /* 6 LAN receiving */
    { L"netshell.dll", 193, NULL },  /* 7 LAN sending and receiving */
    { L"netshell.dll", 195, NULL },  /* 8 LAN cable unplugged */
    { L"netshell.dll", 196, NULL },  /* 9 LAN limited connectivity */
    { L"xpsp2res.dll", 1003, NULL }, /* 10 wireless connected */
    { L"xpsp2res.dll", 1004, NULL }, /* 11 wireless not connected */
    { L"xpsp2res.dll", 1018, NULL }, /* 12 wireless limited connectivity */
};

static HICON xp_icon(int index)
{
    if (!xp_icons[index].icon)
    {
        WCHAR path[MAX_PATH];
        HMODULE module;
        swprintf(path, MAX_PATH, L"C:\\xp\\tray\\%ls", xp_icons[index].file);
        if ((module = LoadLibraryExW(path, NULL, LOAD_LIBRARY_AS_DATAFILE)))
            xp_icons[index].icon = LoadImageW(module, MAKEINTRESOURCEW(xp_icons[index].id), IMAGE_ICON, 16, 16, 0);
        if (!xp_icons[index].icon) xp_icons[index].icon = LoadIconW(NULL, IDI_APPLICATION);
    }
    return xp_icons[index].icon;
}

/* ---- XP's notification area ------------------------------------------- */

static HWND find_tray(void)
{
    HWND tray = NULL;
    /* Wine's desktop keeps a hidden Shell_TrayWnd of its own; XP's is the visible one. */
    while ((tray = FindWindowExW(NULL, tray, L"Shell_TrayWnd", NULL)))
        if (IsWindowVisible(tray)) return tray;
    return NULL;
}

static BOOL tray_send(DWORD message, UINT id, const WCHAR *info_title, const WCHAR *info, DWORD info_flags)
{
    XPTRAYDATA data = {0};
    COPYDATASTRUCT copy;
    HWND tray = find_tray();
    if (!tray) return FALSE;
    data.dwSignature = 0x34753423;
    data.dwMessage = message;
    data.nid.cbSize = sizeof(data.nid);
    data.nid.hWnd = (DWORD)(ULONG_PTR)window;
    data.nid.uID = id;
    if (message != NIM_DELETE)
    {
        data.nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        data.nid.uCallbackMessage = WM_TRAY;
        data.nid.hIcon = (DWORD)(ULONG_PTR)icons[id].icon;
        wcsncpy(data.nid.szTip, icons[id].tip, 127);
    }
    if (info)
    {
        data.nid.uFlags |= NIF_INFO;
        wcsncpy(data.nid.szInfo, info, 255);
        wcsncpy(data.nid.szInfoTitle, info_title, 63);
        data.nid.uTimeout = 10000;
        data.nid.dwInfoFlags = info_flags;
    }
    copy.dwData = 1;
    copy.cbData = sizeof(data);
    copy.lpData = &data;
    return SendMessageTimeoutW(tray, WM_COPYDATA, (WPARAM)window, (LPARAM)&copy,
                               SMTO_ABORTIFHUNG, 3000, NULL) != 0;
}

/* Shows, updates or hides icon ID; a balloon goes with it when INFO is given. */
static void set_icon(UINT id, BOOL show, HICON icon, const WCHAR *tip, const WCHAR *info_title, const WCHAR *info,
                     DWORD info_flags)
{
    TRAYICON *t = &icons[id];
    if (!show)
    {
        if (t->shown) tray_send(NIM_DELETE, id, NULL, NULL, 0);
        t->shown = FALSE;
        return;
    }
    if (t->shown && t->icon == icon && !wcscmp(t->tip, tip) && !info) return;
    t->icon = icon;
    wcsncpy(t->tip, tip, 127);
    t->tip[127] = 0;
    if (!t->shown) t->shown = tray_send(NIM_ADD, id, info_title, info, info_flags);
    else tray_send(NIM_MODIFY, id, info_title, info, info_flags);
}

/* ---- Linux side --------------------------------------------------------- */

/* Reads a small file such as one of sysfs; returns FALSE if it is missing. */
static BOOL read_text(const WCHAR *path, char *buffer, DWORD size)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD done = 0;
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    ReadFile(file, buffer, size - 1, &done, NULL);
    CloseHandle(file);
    buffer[done] = 0;
    buffer[strcspn(buffer, "\r\n")] = 0;
    return TRUE;
}

static long long read_number(const WCHAR *directory, const WCHAR *name, long long fallback)
{
    WCHAR path[MAX_PATH];
    char text[64];
    swprintf(path, MAX_PATH, L"%ls\\%ls", directory, name);
    return read_text(path, text, sizeof(text)) && *text ? atoll(text) : fallback;
}

static BOOL exists(const WCHAR *path)
{
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static void shell_quote_w(const WCHAR *source, WCHAR *target, size_t capacity);

/* Wine runs a Unix executable named in CreateProcess natively (as
 * linux-launch.c does); env drops the Wine debug channels. */
static BOOL spawn(const WCHAR *script, HANDLE unused)
{
    static const WCHAR prefix[] = L"/usr/bin/env -u WINEDEBUG -u WINEDLLOVERRIDES /bin/sh -c \"";
    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION process;
    size_t script_length = wcslen(script), capacity = wcslen(prefix) + script_length * 2 + 3;
    WCHAR *line = malloc(capacity * sizeof(WCHAR));
    WCHAR *write;
    BOOL ok;
    (void)unused;
    if (!line) return FALSE;
    wcscpy(line, prefix);
    write = line + wcslen(line);
    while (*script)
    {
        size_t slashes = 0;
        while (*script == L'\\') { slashes++; script++; }
        if (*script == L'\"')
        {
            for (size_t i = 0; i < slashes * 2 + 1; i++) *write++ = L'\\';
            *write++ = *script++;
        }
        else if (!*script)
        {
            for (size_t i = 0; i < slashes * 2; i++) *write++ = L'\\';
            break;
        }
        else
        {
            for (size_t i = 0; i < slashes; i++) *write++ = L'\\';
            *write++ = *script++;
        }
    }
    *write++ = L'\"';
    *write = 0;
    ok = CreateProcessW(L"Z:\\usr\\bin\\env", line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                        &startup, &process);
    free(line);
    if (!ok) return FALSE;
    if (process.hThread) CloseHandle(process.hThread);
    if (process.hProcess) CloseHandle(process.hProcess);
    return ok;
}

/* Runs SCRIPT and returns what it printed (at most SIZE - 1 bytes). */
static BOOL capture(const WCHAR *script, char *buffer, DWORD size)
{
    WCHAR path[MAX_PATH], done_path[MAX_PATH + 8], unix_path[MAX_PATH], *command;
    HANDLE file;
    DWORD done = 0;
    BOOL ok = FALSE;
    *buffer = 0;
    /* Wine's Unix subprocesses do not inherit a Win32 anonymous pipe as stdout.
     * Let the Unix shell redirect to a temporary file visible through Z: instead. */
    if (!GetTempFileNameW(L"Z:\\tmp", L"xpt", 0, path)) return FALSE;
    swprintf(done_path, ARRAYSIZE(done_path), L"%ls.done", path);
    wcscpy(unix_path, path + 2);
    for (WCHAR *p = unix_path; *p; p++) if (*p == L'\\') *p = L'/';
    size_t capacity = wcslen(script) + wcslen(unix_path) * 2 + 48;
    command = malloc(capacity * sizeof(WCHAR));
    if (command)
    {
        swprintf(command, capacity, L"(%ls) > '%ls' 2>&1; : > '%ls.done'", script, unix_path, unix_path);
        if (spawn(command, NULL))
        {
            ULONGLONG started = GetTickCount64();
            /* Wine cannot wait on the process handle of a native Unix child. */
            while (GetFileAttributesW(done_path) == INVALID_FILE_ATTRIBUTES &&
                   GetTickCount64() - started < 10000) Sleep(20);
            if (GetFileAttributesW(done_path) != INVALID_FILE_ATTRIBUTES)
            {
                file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                if (file != INVALID_HANDLE_VALUE)
                {
                    ok = ReadFile(file, buffer, size - 1, &done, NULL);
                    CloseHandle(file);
                }
            }
        }
        free(command);
    }
    DeleteFileW(path);
    DeleteFileW(done_path);
    buffer[done] = 0;
    return ok;
}

static void poll_volume(void)
{
    char out[256], *p;
    if (!capture(L"if command -v wpctl >/dev/null; then wpctl get-volume @DEFAULT_AUDIO_SINK@; "
                 L"elif command -v pactl >/dev/null; then "
                 L"pactl get-sink-volume @DEFAULT_SINK@ | awk '/Volume/{gsub(\"%\",\"\",$5); "
                 L"printf \"Volume: %.2f\", $5/100; exit}'; "
                 L"pactl get-sink-mute @DEFAULT_SINK@ | grep -q yes && echo ' [MUTED]'; fi",
                 out, sizeof(out)) || !(p = strstr(out, "Volume:")))
    {
        volume = -1;
        return;
    }
    volume = (int)(atof(p + 7) * 100 + 0.5);
    muted = strstr(out, "MUTED") != NULL;
}

static void poll_power(void)
{
    WIN32_FIND_DATAW found;
    HANDLE search = FindFirstFileW(L"Z:\\sys\\class\\power_supply\\*", &found);
    long long energy = 0, full = 0, power = 0;
    int batteries = 0;
    battery = -1;
    on_ac = 0;
    charging = 0;
    minutes_left = -1;
    if (search == INVALID_HANDLE_VALUE) return;
    do
    {
        WCHAR directory[MAX_PATH], path[MAX_PATH];
        char text[64];
        if (found.cFileName[0] == '.') continue;
        swprintf(directory, MAX_PATH, L"Z:\\sys\\class\\power_supply\\%ls", found.cFileName);
        swprintf(path, MAX_PATH, L"%ls\\type", directory);
        if (!read_text(path, text, sizeof(text))) continue;
        if (!strcmp(text, "Mains"))
        {
            if (read_number(directory, L"online", 0)) on_ac = 1;
        }
        else if (!strcmp(text, "Battery") && read_number(directory, L"present", 1))
        {
            /* Peripherals (mice, headsets) report a scope of Device. */
            swprintf(path, MAX_PATH, L"%ls\\scope", directory);
            if (read_text(path, text, sizeof(text)) && !strcmp(text, "Device")) continue;
            batteries++;
            energy += read_number(directory, L"energy_now", read_number(directory, L"charge_now", 0));
            full += read_number(directory, L"energy_full", read_number(directory, L"charge_full", 0));
            power += llabs(read_number(directory, L"power_now", read_number(directory, L"current_now", 0)));
            swprintf(path, MAX_PATH, L"%ls\\status", directory);
            if (read_text(path, text, sizeof(text)) && !strcmp(text, "Charging")) charging = 1;
            if (battery < 0) battery = (int)read_number(directory, L"capacity", -1);
        }
    } while (FindNextFileW(search, &found));
    FindClose(search);
    if (!batteries) return;
    if (full > 0) battery = (int)(energy * 100 / full);
    if (battery > 100) battery = 100;
    if (!on_ac && !charging && power > 0) minutes_left = (int)(energy * 60 / power);
}

/* The interface that carries the default route, else the first wireless or
 * wired adapter; virtual interfaces (no device link) are left out. */
static void poll_network(BOOL detail)
{
    WIN32_FIND_DATAW found;
    HANDLE search;
    char route[8192], text[64], *line;
    WCHAR best[32] = L"", path[MAX_PATH], directory[MAX_PATH];
    int best_rank = 0;

    /* The whole routing table: read_text keeps only a first line. */
    {
        HANDLE file = CreateFileW(L"Z:\\proc\\net\\route", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        DWORD done = 0;
        route[0] = 0;
        if (file != INVALID_HANDLE_VALUE)
        {
            ReadFile(file, route, sizeof(route) - 1, &done, NULL);
            CloseHandle(file);
            route[done] = 0;
        }
    }
    search = FindFirstFileW(L"Z:\\sys\\class\\net\\*", &found);
    if (search != INVALID_HANDLE_VALUE)
    {
        do
        {
            char name[32], needle[48];
            int rank;
            if (found.cFileName[0] == '.') continue;
            swprintf(directory, MAX_PATH, L"Z:\\sys\\class\\net\\%ls", found.cFileName);
            swprintf(path, MAX_PATH, L"%ls\\device", directory);
            if (!exists(path)) continue;
            WideCharToMultiByte(CP_UTF8, 0, found.cFileName, -1, name, sizeof(name), NULL, NULL);
            snprintf(needle, sizeof(needle), "\n%s\t00000000\t", name);
            swprintf(path, MAX_PATH, L"%ls\\wireless", directory);
            rank = strstr(route, needle) ? 3 : exists(path) ? 2 : 1;
            if (rank > best_rank)
            {
                best_rank = rank;
                wcsncpy(best, found.cFileName, 31);
            }
        } while (FindNextFileW(search, &found));
        FindClose(search);
    }
    if (!best_rank)
    {
        net_up = -1;
        return;
    }
    if (wcscmp(best, net_name))
    {
        wcscpy(net_name, best);
        rx_last = tx_last = 0;
    }
    swprintf(directory, MAX_PATH, L"Z:\\sys\\class\\net\\%ls", best);
    swprintf(path, MAX_PATH, L"%ls\\wireless", directory);
    net_wireless = exists(path);
    swprintf(path, MAX_PATH, L"%ls\\operstate", directory);
    net_up = read_text(path, text, sizeof(text)) && !strcmp(text, "up");
    net_limited = net_up && best_rank < 3;
    /* Wired links report their speed in sysfs; nmcli gives a wireless one below. */
    if (!net_wireless || !net_up) net_speed = (int)read_number(directory, L"speed", 0);
    if (!net_wireless || !net_up)
    {
        net_ssid[0] = 0;
        net_signal = -1;
        return;
    }
    if (!detail) return;
    /* nmcli: "yes:SSID:SIGNAL:RATE" for the active network; no rescan. */
    if (capture(L"nmcli -t -f ACTIVE,SSID,SIGNAL,RATE dev wifi list --rescan no 2>/dev/null | grep '^yes:'",
                route, sizeof(route)) && !strncmp(route, "yes:", 4))
    {
        char ssid[128], *out = ssid, *p = route + 4;
        while (*p && *p != ':' && out < ssid + sizeof(ssid) - 1)
        {
            if (*p == '\\' && p[1]) p++;
            *out++ = *p++;
        }
        *out = 0;
        MultiByteToWideChar(CP_UTF8, 0, ssid, -1, net_ssid, 64);
        if (*p == ':') net_signal = atoi(++p);
        if ((p = strchr(p, ':'))) net_speed = atoi(p + 1);
        return;
    }
    /* Without NetworkManager: the link quality of /proc/net/wireless (out of 70). */
    {
        HANDLE file = CreateFileW(L"Z:\\proc\\net\\wireless", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        DWORD done = 0;
        char name[32];
        net_signal = -1;
        net_ssid[0] = 0;
        if (file == INVALID_HANDLE_VALUE) return;
        ReadFile(file, route, sizeof(route) - 1, &done, NULL);
        CloseHandle(file);
        route[done] = 0;
        WideCharToMultiByte(CP_UTF8, 0, best, -1, name, sizeof(name), NULL, NULL);
        for (line = strtok(route, "\n"); line; line = strtok(NULL, "\n"))
        {
            while (*line == ' ') line++;
            if (!strncmp(line, name, strlen(name)) && line[strlen(name)] == ':')
            {
                float quality;
                if (sscanf(line + strlen(name) + 1, "%*s %f", &quality) == 1) net_signal = (int)(quality * 100 / 70);
            }
        }
    }
}

/* ---- Icons -------------------------------------------------------------- */

static HMODULE xp_module(const WCHAR *file);

/* A string of XP's FILE in the user's language, formatted as XP formats it
 * (FormatMessage inserts: %1!s!, %1!u!), or FALLBACK in the same form. */
static void xp_text(const WCHAR *file, UINT id, const WCHAR *fallback, WCHAR *out, int size, ...)
{
    WCHAR format[256];
    HMODULE module = xp_module(file);
    va_list args;
    if (!module || !LoadStringW(module, id, format, ARRAYSIZE(format))) lstrcpynW(format, fallback, ARRAYSIZE(format));
    va_start(args, size);
    if (!FormatMessageW(FORMAT_MESSAGE_FROM_STRING, format, 0, 0, out, size, &args)) lstrcpynW(out, format, size);
    va_end(args);
}

static void signal_words(int signal, WCHAR *out, int size)
{
    /* netshell's Very Low (2601) to Excellent (2605). */
    static const WCHAR *words[] = { L"Very Low", L"Low", L"Good", L"Very Good", L"Excellent" };
    int level = signal >= 80 ? 4 : signal >= 60 ? 3 : signal >= 40 ? 2 : signal >= 20 ? 1 : 0;
    xp_text(L"netshell.dll", 2601 + level, words[level], out, size);
}

static void speed_text(WCHAR *out, size_t size)
{
    if (net_speed >= 1000) swprintf(out, size, L"%.1f Gbps", net_speed / 1000.0);
    else if (net_speed > 0) swprintf(out, size, L"%d.0 Mbps", net_speed);
    else xp_text(L"netshell.dll", 1026, L"Unknown", out, size);
}

static void show_volume(void)
{
    WCHAR tip[64];
    if (volume < 0)
    {
        set_icon(ID_VOLUME, FALSE, NULL, L"", NULL, NULL, 0);
        return;
    }
    /* XP's tip names the control only (stobject's "Volume", "Volume (muted)"). */
    if (muted) xp_text(L"stobject.dll", 258, L"Volume (muted)", tip, ARRAYSIZE(tip));
    else xp_text(L"stobject.dll", 252, L"Volume", tip, ARRAYSIZE(tip));
    set_icon(ID_VOLUME, TRUE, xp_icon(muted || !volume ? 1 : 0), tip, NULL, NULL, 0);
    if (popup && IsWindowVisible(popup) && wanted_volume < 0)
    {
        SendDlgItemMessageW(popup, 1001, TBM_SETPOS, TRUE, 100 - min(volume, 100));
        CheckDlgButton(popup, 1000, muted ? BST_CHECKED : BST_UNCHECKED);
    }
}

static void show_power(void)
{
    WCHAR tip[128];
    if (battery < 0)
    {
        set_icon(ID_POWER, FALSE, NULL, L"", NULL, NULL, 0);
        return;
    }
    /* stobject's tips, as XP's power meter shows them. */
    if (!on_ac && minutes_left > 0 && minutes_left < 60 * 24)
    {
        if (minutes_left >= 60)
            xp_text(L"stobject.dll", 162, L"%1!u!:%2!02u! hours (%3!u!%%) remaining", tip, ARRAYSIZE(tip),
                    minutes_left / 60, minutes_left % 60, battery);
        else xp_text(L"stobject.dll", 163, L"%1!u! min (%2!u!%%) remaining", tip, ARRAYSIZE(tip),
                     minutes_left, battery);
    }
    else xp_text(L"stobject.dll", 158, L"%1!u!%% remaining", tip, ARRAYSIZE(tip), battery);
    if (charging)
    {
        WCHAR suffix[32];
        xp_text(L"stobject.dll", 159, L" (charging)", suffix, ARRAYSIZE(suffix));
        wcsncat(tip, suffix, ARRAYSIZE(tip) - wcslen(tip) - 1);
    }
    if (!on_ac && !charging && battery <= 10 && !warned_low)
    {
        warned_low = 1;
        set_icon(ID_POWER, TRUE, xp_icon(3), tip, L"Your battery is running low",
                 L"You might want to plug in your computer now.", NIIF_WARNING);
        return;
    }
    if (on_ac || battery > 15) warned_low = 0;
    set_icon(ID_POWER, TRUE, xp_icon(on_ac || charging ? 2 : 3), tip, NULL, NULL, 0);
}

static void show_network(void)
{
    static int last_up = -2;
    WCHAR tip[128], speed[32], speed_line[64], signal[32], signal_line[64], info[256], title[64], name[64];
    WCHAR line[128];
    WCHAR directory[MAX_PATH];
    unsigned long long rx, tx;
    int index;
    BOOL balloon = FALSE;

    if (net_up < 0)
    {
        set_icon(ID_NETWORK, FALSE, NULL, L"", NULL, NULL, 0);
        last_up = -2;
        return;
    }
    /* The texts are XP's own (netshell.dll, and xpsp2res.dll for wireless). */
    if (net_wireless) xp_text(L"xpsp2res.dll", 16022, L"Wireless Network Connection", name, ARRAYSIZE(name));
    else xp_text(L"netshell.dll", 1811, L"Local Area Connection", name, ARRAYSIZE(name));
    speed_text(speed, 32);
    xp_text(L"netshell.dll", 2202, L"Speed: %1!s!", speed_line, ARRAYSIZE(speed_line), speed);
    if (net_signal >= 0) signal_words(net_signal, signal, ARRAYSIZE(signal));
    else xp_text(L"netshell.dll", 1026, L"Unknown", signal, ARRAYSIZE(signal));
    xp_text(L"xpsp2res.dll", 16008, L"Signal Strength:", line, ARRAYSIZE(line));
    swprintf(signal_line, ARRAYSIZE(signal_line), L"%ls %ls", line, signal);
    if (!net_up)
    {
        index = net_wireless ? 11 : 8;
        if (net_wireless) xp_text(L"xpsp2res.dll", 144, L"Not connected", line, ARRAYSIZE(line));
        else xp_text(L"netshell.dll", 2201, L"A network cable is unplugged.", line, ARRAYSIZE(line));
        swprintf(tip, 128, L"%ls\n%ls", name, line);
        if (last_up == 1)
        {
            balloon = TRUE;
            wcscpy(title, name);
            if (net_wireless) xp_text(L"netshell.dll", 2208, L"Wireless connection unavailable", info, ARRAYSIZE(info));
            else wcscpy(info, line);
        }
    }
    else
    {
        if (net_wireless)
        {
            index = net_limited ? 12 : 10;
            if (net_ssid[0]) swprintf(tip, 128, L"%ls (%ls)\n%ls\n%ls", name, net_ssid, speed_line, signal_line);
            else swprintf(tip, 128, L"%ls\n%ls", name, signal_line);
        }
        else
        {
            /* XP lights the two screens for sending and receiving. */
            swprintf(directory, MAX_PATH, L"Z:\\sys\\class\\net\\%ls\\statistics", net_name);
            rx = read_number(directory, L"rx_bytes", 0);
            tx = read_number(directory, L"tx_bytes", 0);
            index = net_limited ? 9 : 4 + (rx_last && tx > tx_last ? 1 : 0) + (rx_last && rx > rx_last ? 2 : 0);
            rx_last = rx;
            tx_last = tx;
            swprintf(tip, 128, L"%ls\n%ls", name, speed_line);
        }
        if (net_limited)
        {
            xp_text(L"xpsp2res.dll", 142, L"Limited or no connectivity", line, ARRAYSIZE(line));
            wcsncat(tip, L"\n", 127 - wcslen(tip));
            wcsncat(tip, line, 127 - wcslen(tip));
        }
        if (last_up == 0)
        {
            balloon = TRUE;
            xp_text(L"netshell.dll", 2223, L"%1 is now connected", title, ARRAYSIZE(title), name);
            if (net_wireless && net_ssid[0])
            {
                xp_text(L"netshell.dll", 2234, L"Connected to: %1!s!\r\nSignal Strength: %2!s!", line,
                        ARRAYSIZE(line), net_ssid, signal);
                swprintf(info, 256, L"%ls\n%ls", line, speed_line);
            }
            else wcscpy(info, speed_line);
        }
    }
    last_up = net_up;
    set_icon(ID_NETWORK, TRUE, xp_icon(index), tip, balloon ? title : NULL, balloon ? info : NULL, NIIF_INFO);
}

static void poll(void)
{
    static int last_up = -2;
    poll_power();
    show_power();
    /* nmcli for the SSID only every 10 seconds, or when the link changes. */
    poll_network(FALSE);
    if (net_up != last_up || tick % 10 == 0) poll_network(TRUE);
    last_up = net_up;
    show_network();
    if (tick % 2 == 0 || volume < 0)
    {
        poll_volume();
        show_volume();
    }
    tick++;
}

static void add_all(void)
{
    for (int i = 0; i < ID_COUNT; i++) icons[i].shown = FALSE;
    tick = 0;
    poll();
}

static void run_settings(UINT command);

/* ---- XP's own flyouts --------------------------------------------------
 * The windows XP opened from these icons, built from the dialog templates in
 * the user's own XP files (C:\xp\tray, extracted from their ISO) and filled
 * with the Linux state:
 *   volume   sndvol32.exe dialog 106, the single-click Volume window
 *   power    stobject.dll dialog 100 "Power Meter", around batmeter.dll 100
 *   network  netshell.dll pages 1801 (General) and 1806 (Support) of the
 *            "<connection> Status" property sheet */

static HMODULE xp_module(const WCHAR *file)
{
    /* Loaded once each: the tips read their strings every second. */
    static struct { const WCHAR *file; HMODULE module; } loaded[8];
    static unsigned count;
    WCHAR path[MAX_PATH];
    HMODULE module;
    for (unsigned i = 0; i < count; i++)
        if (!wcscmp(loaded[i].file, file)) return loaded[i].module;
    swprintf(path, MAX_PATH, L"C:\\xp\\tray\\%ls", file);
    module = LoadLibraryExW(path, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (module && count < ARRAYSIZE(loaded))
    {
        loaded[count].file = file;
        loaded[count++].module = module;
    }
    return module;
}

static void set_volume(int value)
{
    WCHAR script[256];
    swprintf(script, 256, L"if command -v wpctl >/dev/null; then wpctl set-volume @DEFAULT_AUDIO_SINK@ %d%%; "
             L"else pactl set-sink-volume @DEFAULT_SINK@ %d%%; fi", value, value);
    spawn(script, NULL);
    volume = value;
}

static void set_mute(int value)
{
    WCHAR script[256];
    swprintf(script, 256, L"if command -v wpctl >/dev/null; then wpctl set-mute @DEFAULT_AUDIO_SINK@ %d; "
             L"else pactl set-sink-mute @DEFAULT_SINK@ %d; fi", value, value);
    spawn(script, NULL);
    muted = value;
    show_volume();
}

/* Places a flyout above the notification area, by the cursor, as XP did. */
static void place_flyout(HWND dialog)
{
    POINT cursor;
    RECT work, rect;
    int width, height;
    GetWindowRect(dialog, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    GetCursorPos(&cursor);
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    cursor.x = max(work.left, min(cursor.x - width / 2, work.right - width));
    cursor.y = max(work.top, work.bottom - height);
    SetWindowPos(dialog, HWND_TOPMOST, cursor.x, cursor.y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
}

#define IDC_VOLUME_SLIDER 1001
#define IDC_VOLUME_MUTE   1000

static INT_PTR CALLBACK volume_proc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    switch (message)
    {
    case WM_INITDIALOG:
    {
        HWND slider_control = GetDlgItem(dialog, IDC_VOLUME_SLIDER);
        SendMessageW(slider_control, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(slider_control, TBM_SETTICFREQ, 10, 0);
        SendMessageW(slider_control, TBM_SETPAGESIZE, 0, 10);
        SendMessageW(slider_control, TBM_SETPOS, TRUE, 100 - max(0, min(volume, 100)));
        CheckDlgButton(dialog, IDC_VOLUME_MUTE, muted ? BST_CHECKED : BST_UNCHECKED);
        place_flyout(dialog);
        SetForegroundWindow(dialog);
        SetFocus(slider_control);
        return FALSE;
    }
    case WM_VSCROLL:
        wanted_volume = 100 - (int)SendDlgItemMessageW(dialog, IDC_VOLUME_SLIDER, TBM_GETPOS, 0, 0);
        SetTimer(window, TIMER_APPLY, 80, NULL);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDC_VOLUME_MUTE)
            set_mute(IsDlgButtonChecked(dialog, IDC_VOLUME_MUTE) == BST_CHECKED);
        else if (LOWORD(wparam) == IDCANCEL) DestroyWindow(dialog);
        return TRUE;
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) DestroyWindow(dialog);
        return TRUE;
    case WM_DESTROY:
        popup = NULL;
        return TRUE;
    }
    return FALSE;
}

static void show_popup(void)
{
    static HMODULE sndvol;
    if (popup) return;
    if (!sndvol) sndvol = xp_module(L"sndvol32.exe");
    if (sndvol) popup = CreateDialogParamW(sndvol, MAKEINTRESOURCEW(106), NULL, volume_proc, 0);
}

/* XP's Volume Control, the mixer window "Open Volume Control" and a double
 * click opened: sndvol32.exe itself, from a copy in C:\xp\bin whose import
 * of WINMM.dll names xpmix.dll (helpers/xpmix.c), which sets the volume of
 * the Linux sound server where Wine's winmm sets the process's own. */
static BOOL open_volume_control(void)
{
    static const WCHAR source[] = L"C:\\xp\\tray\\sndvol32.exe", target[] = L"C:\\xp\\bin\\sndvol32.exe";
    static const char name[] = "WINMM.dll";
    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION process;
    HANDLE file;
    DWORD size, done = 0, current_size = 0;
    char *image, *current;
    BOOL same, ok;
    if (!exists(L"C:\\xp\\bin\\xpmix.dll")) return FALSE;
    file = CreateFileW(source, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    size = GetFileSize(file, NULL);
    image = size != INVALID_FILE_SIZE ? malloc(size) : NULL;
    ok = image && ReadFile(file, image, size, &done, NULL) && done == size;
    CloseHandle(file);
    if (!ok)
    {
        free(image);
        return FALSE;
    }
    /* Its import table and its bound imports both name the DLL. */
    for (DWORD i = 0; i + sizeof(name) <= size; i++)
        if (!_strnicmp(image + i, name, sizeof(name))) memcpy(image + i, "xpmix.dll", sizeof(name));
    /* Written again only when the user's sndvol32 changed. */
    file = CreateFileW(target, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    current = malloc(size);
    same = FALSE;
    if (file != INVALID_HANDLE_VALUE)
    {
        same = current && GetFileSize(file, NULL) == size && ReadFile(file, current, size, &current_size, NULL) &&
               current_size == size && !memcmp(current, image, size);
        CloseHandle(file);
    }
    free(current);
    ok = same;
    if (!same && (file = CreateFileW(target, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL)) != INVALID_HANDLE_VALUE)
    {
        ok = WriteFile(file, image, size, &done, NULL) && done == size;
        CloseHandle(file);
    }
    free(image);
    if (!ok) return FALSE;
    if (!CreateProcessW(target, NULL, NULL, NULL, FALSE, 0, NULL, L"C:\\xp\\bin", &startup, &process)) return FALSE;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return TRUE;
}

/* Power Meter: batmeter's page inside stobject's frame. */
static HWND power_meter, power_page;

static void power_meter_update(void)
{
    WCHAR text[64];
    if (!power_page) return;
    SetDlgItemTextW(power_page, 1001, on_ac ? L"AC Power" : L"Batteries");
    ShowWindow(GetDlgItem(power_page, 1001), SW_SHOW);
    if (battery >= 0) swprintf(text, 64, L"%d%%", battery);
    else wcscpy(text, L"Unknown");
    SetDlgItemTextW(power_page, 1002, text);
    ShowWindow(GetDlgItem(power_page, 1002), SW_SHOW);
    ShowWindow(GetDlgItem(power_page, 1006), charging ? SW_SHOW : SW_HIDE);
    SendDlgItemMessageW(power_page, 1004, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendDlgItemMessageW(power_page, 1004, PBM_SETPOS, max(battery, 0), 0);
    if (minutes_left > 0 && minutes_left < 60 * 24) swprintf(text, 64, L"%d hr %d min", minutes_left / 60, minutes_left % 60);
    else wcscpy(text, L"Unknown");
    SetDlgItemTextW(power_page, 1068, text);
    ShowWindow(GetDlgItem(power_page, 1067), on_ac ? SW_HIDE : SW_SHOW);
    ShowWindow(GetDlgItem(power_page, 1068), on_ac ? SW_HIDE : SW_SHOW);
    SendDlgItemMessageW(power_page, 1003, STM_SETICON,
                        (WPARAM)LoadImageW(xp_module(xp_icons[on_ac || charging ? 2 : 3].file),
                                           MAKEINTRESOURCEW(xp_icons[on_ac || charging ? 2 : 3].id),
                                           IMAGE_ICON, 32, 32, 0), 0);
}

static INT_PTR CALLBACK power_page_proc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)dialog; (void)message; (void)wparam; (void)lparam;
    return message == WM_INITDIALOG;
}

static INT_PTR CALLBACK power_meter_proc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    switch (message)
    {
    case WM_INITDIALOG:
    {
        static HMODULE batmeter;
        RECT area;
        if (!batmeter) batmeter = xp_module(L"batmeter.dll");
        CheckDlgButton(dialog, 1002, BST_CHECKED);
        EnableWindow(GetDlgItem(dialog, 1002), FALSE);
        EnableWindow(GetDlgItem(dialog, 1003), FALSE);
        GetWindowRect(GetDlgItem(dialog, 1000), &area);
        MapWindowPoints(NULL, dialog, (POINT *)&area, 2);
        if (batmeter && (power_page = CreateDialogParamW(batmeter, MAKEINTRESOURCEW(100), dialog, power_page_proc, 0)))
            SetWindowPos(power_page, HWND_TOP, area.left, area.top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
        power_meter_update();
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL) DestroyWindow(dialog);
        return TRUE;
    case WM_CLOSE:
        DestroyWindow(dialog);
        return TRUE;
    case WM_DESTROY:
        power_meter = power_page = NULL;
        return TRUE;
    }
    return FALSE;
}

static void show_power_meter(void)
{
    static HMODULE stobject;
    if (power_meter)
    {
        SetForegroundWindow(power_meter);
        return;
    }
    if (!stobject) stobject = xp_module(L"stobject.dll");
    if (stobject && (power_meter = CreateDialogParamW(stobject, MAKEINTRESOURCEW(100), NULL, power_meter_proc, 0)))
        ShowWindow(power_meter, SW_SHOW);
}

/* "<connection> Status": netshell's General and Support pages. */
static HWND net_sheet, net_general, net_support;
static ULONGLONG net_since;

static void thousands(unsigned long long value, WCHAR *out, size_t size)
{
    WCHAR digits[32], *p = out;
    int length = swprintf(digits, 32, L"%llu", value);
    for (int i = 0; i < length && (size_t)(p - out) < size - 2; i++)
    {
        if (i && (length - i) % 3 == 0) *p++ = ',';
        *p++ = digits[i];
    }
    *p = 0;
}

/* XP's signal strength bars, drawn for the General page's picture. */
static HBITMAP signal_bitmap(int signal)
{
    HDC screen = GetDC(NULL), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 26, 13), old = SelectObject(dc, bitmap);
    RECT all = {0, 0, 26, 13};
    int bars = signal >= 80 ? 5 : signal >= 60 ? 4 : signal >= 40 ? 3 : signal >= 20 ? 2 : signal > 0 ? 1 : 0;
    FillRect(dc, &all, GetSysColorBrush(COLOR_BTNFACE));
    for (int i = 0; i < 5; i++)
    {
        RECT bar = {i * 5, 10 - (i + 1) * 2, i * 5 + 4, 13};
        HBRUSH brush = CreateSolidBrush(i < bars ? RGB(0, 170, 0) : RGB(200, 200, 200));
        FillRect(dc, &bar, brush);
        FrameRect(dc, &bar, GetSysColorBrush(COLOR_3DSHADOW));
        DeleteObject(brush);
    }
    SelectObject(dc, old);
    DeleteDC(dc);
    ReleaseDC(NULL, screen);
    return bitmap;
}

static void net_status_update(void)
{
    WCHAR text[128], directory[MAX_PATH];
    ULONGLONG seconds;
    HBITMAP old;
    if (!net_general) return;
    SetDlgItemTextW(net_general, 1007, net_up > 0 ? (net_limited ? L"Limited or no connectivity" : L"Connected")
                                                  : L"Disconnected");
    seconds = net_up > 0 && net_since ? (GetTickCount64() - net_since) / 1000 : 0;
    if (seconds >= 86400)
        swprintf(text, 128, L"%u day(s) %02u:%02u:%02u", (unsigned)(seconds / 86400), (unsigned)(seconds / 3600 % 24),
                 (unsigned)(seconds / 60 % 60), (unsigned)(seconds % 60));
    else
        swprintf(text, 128, L"%02u:%02u:%02u", (unsigned)(seconds / 3600), (unsigned)(seconds / 60 % 60),
                 (unsigned)(seconds % 60));
    SetDlgItemTextW(net_general, 1008, text);
    if (net_up > 0) speed_text(text, 128);
    else wcscpy(text, L"0 bps");
    SetDlgItemTextW(net_general, 1009, text);
    ShowWindow(GetDlgItem(net_general, 1043), net_wireless ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(net_general, 1044), net_wireless ? SW_SHOW : SW_HIDE);
    if (net_wireless && (old = (HBITMAP)SendDlgItemMessageW(net_general, 1044, STM_SETIMAGE, IMAGE_BITMAP,
                                                          (LPARAM)signal_bitmap(net_up > 0 ? net_signal : 0))))
        DeleteObject(old);
    swprintf(directory, MAX_PATH, L"Z:\\sys\\class\\net\\%ls\\statistics", net_name);
    thousands(read_number(directory, L"tx_bytes", 0), text, 128);
    SetDlgItemTextW(net_general, 1003, text);
    thousands(read_number(directory, L"rx_bytes", 0), text, 128);
    SetDlgItemTextW(net_general, 1005, text);
    thousands(read_number(directory, L"tx_errors", 0), text, 128);
    SetDlgItemTextW(net_general, 1012, text);
    thousands(read_number(directory, L"rx_errors", 0), text, 128);
    SetDlgItemTextW(net_general, 1013, text);
}

/* The Support page: address type, address, mask and gateway from nmcli. */
static void net_support_update(void)
{
    char out[1024], *line, *address = "", *gateway = "", *method = "";
    WCHAR script[512], wide[128], name[80];
    unsigned prefix_length = 0;
    if (!net_support) return;
    /* Every name goes into the shell quoted. */
    shell_quote_w(net_name, name, ARRAYSIZE(name));
    swprintf(script, 512, L"nmcli -t -g IP4.ADDRESS,IP4.GATEWAY device show %ls 2>/dev/null | head -2; "
             L"c=$(nmcli -g GENERAL.CONNECTION device show %ls 2>/dev/null); "
             L"[ -n \"$c\" ] && nmcli -g ipv4.method connection show \"$c\" 2>/dev/null", name, name);
    capture(script, out, sizeof(out));
    line = strtok(out, "\n");
    if (line) { address = line; line = strtok(NULL, "\n"); }
    if (line) { gateway = line; line = strtok(NULL, "\n"); }
    if (line) method = line;
    if (strchr(address, '|')) *strchr(address, '|') = 0;
    if (strchr(address, '/')) { prefix_length = atoi(strchr(address, '/') + 1); *strchr(address, '/') = 0; }
    SetDlgItemTextW(net_support, 1053, !strcmp(method, "manual") ? L"Manually Configured"
                                       : *method ? L"Assigned by DHCP" : L"Unknown");
    MultiByteToWideChar(CP_UTF8, 0, *address ? address : "0.0.0.0", -1, wide, 128);
    SetDlgItemTextW(net_support, 1050, wide);
    if (prefix_length)
    {
        unsigned mask = prefix_length >= 32 ? 0xffffffff : ~(0xffffffffu >> prefix_length);
        swprintf(wide, 128, L"%u.%u.%u.%u", mask >> 24, mask >> 16 & 255, mask >> 8 & 255, mask & 255);
    }
    else wcscpy(wide, L"0.0.0.0");
    SetDlgItemTextW(net_support, 1051, wide);
    MultiByteToWideChar(CP_UTF8, 0, *gateway ? gateway : "", -1, wide, 128);
    SetDlgItemTextW(net_support, 1052, wide);
}

static INT_PTR CALLBACK net_general_proc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    WCHAR script[256];
    (void)lparam;
    switch (message)
    {
    case WM_INITDIALOG:
    {
        RECT label, value;
        net_general = dialog;
        if (net_up <= 0) SetDlgItemTextW(dialog, 1025, L"&Enable");
        /* The signal bars go on the Signal Strength row, in the value column;
         * the Errors row is XP's for dial-up connections only. */
        GetWindowRect(GetDlgItem(dialog, 1043), &label);
        GetWindowRect(GetDlgItem(dialog, 1007), &value);
        MapWindowPoints(NULL, dialog, (POINT *)&label, 2);
        MapWindowPoints(NULL, dialog, (POINT *)&value, 2);
        SetWindowPos(GetDlgItem(dialog, 1044), NULL, value.left, label.top, 26, 13, SWP_NOZORDER);
        ShowWindow(GetDlgItem(dialog, 1027), SW_HIDE);
        ShowWindow(GetDlgItem(dialog, 1012), SW_HIDE);
        ShowWindow(GetDlgItem(dialog, 1013), SW_HIDE);
        net_status_update();
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == 1026) run_settings(CMD_NETWORK_SETTINGS);
        else if (LOWORD(wparam) == 1025)
        {
            /* Disable or enable, as XP's button does, through NetworkManager. */
            WCHAR name[80];
            shell_quote_w(net_name, name, ARRAYSIZE(name));
            swprintf(script, 256, net_up > 0 ? L"nmcli device disconnect %ls" : L"nmcli device connect %ls", name);
            spawn(script, NULL);
            PropSheet_PressButton(net_sheet, PSBTN_CANCEL);
        }
        return TRUE;
    case WM_DESTROY:
        net_general = NULL;
        return TRUE;
    }
    return FALSE;
}

static INT_PTR CALLBACK net_support_proc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    WCHAR script[256], text[512], value[4][64];
    (void)lparam;
    switch (message)
    {
    case WM_INITDIALOG:
        net_support = dialog;
        net_support_update();
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == 1056)
        {
            /* Repair: NetworkManager reapplies the connection. */
            WCHAR name[80];
            shell_quote_w(net_name, name, ARRAYSIZE(name));
            swprintf(script, 256, L"nmcli device reapply %ls || nmcli device connect %ls", name, name);
            spawn(script, NULL);
        }
        else if (LOWORD(wparam) == 1062)
        {
            for (int i = 0; i < 4; i++) GetDlgItemTextW(dialog, 1050 + i, value[i], 64);
            swprintf(text, 512, L"Physical Address:\t%ls\nIP Address:\t\t%ls\nSubnet Mask:\t%ls\nDefault Gateway:\t%ls",
                     net_name, value[0], value[1], value[2]);
            MessageBoxW(dialog, text, L"Network Connection Details", MB_OK);
        }
        return TRUE;
    case WM_DESTROY:
        net_support = NULL;
        return TRUE;
    }
    return FALSE;
}

static void show_network_status(void)
{
    static HMODULE netshell;
    PROPSHEETPAGEW pages[2] = {{0}};
    PROPSHEETHEADERW header = {0};
    WCHAR title[96];
    if (net_sheet)
    {
        SetForegroundWindow(net_sheet);
        return;
    }
    if (!netshell) netshell = xp_module(L"netshell.dll");
    if (!netshell || net_up < 0) return;
    for (int i = 0; i < 2; i++)
    {
        pages[i].dwSize = sizeof(pages[i]);
        pages[i].hInstance = netshell;
    }
    pages[0].pszTemplate = MAKEINTRESOURCEW(1801);
    pages[0].pfnDlgProc = net_general_proc;
    pages[1].pszTemplate = MAKEINTRESOURCEW(1806);
    pages[1].pfnDlgProc = net_support_proc;
    swprintf(title, 96, L"%ls Status", net_wireless ? L"Wireless Network Connection" : L"Local Area Connection");
    header.dwSize = sizeof(header);
    header.dwFlags = PSH_PROPSHEETPAGE | PSH_MODELESS | PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
    header.hInstance = netshell;
    header.pszCaption = title;
    header.nPages = 2;
    header.ppsp = pages;
    net_sheet = (HWND)PropertySheetW(&header);
    if (net_sheet)
    {
        /* XP's status sheet has a single Close button. */
        SetDlgItemTextW(net_sheet, IDCANCEL, L"&Close");
        ShowWindow(GetDlgItem(net_sheet, IDOK), SW_HIDE);
    }
}

static void flyouts_update(void)
{
    static int last_up = -2;
    if (net_up > 0 && last_up <= 0) net_since = GetTickCount64();
    last_up = net_up;
    power_meter_update();
    net_status_update();
}

/* ---- Network Connections: saved profiles and live actions from NetworkManager */

#define CONNECTIONS_CLASS L"XPNetworkConnections"

static HIMAGELIST connection_images;

/* nmcli -t escapes separators and backslashes. Decode a field up to ':' without
 * splitting a profile name that itself contains a colon. */
static void nmcli_field(char **cursor, char *out, size_t size)
{
    char *p = *cursor;
    size_t used = 0;
    while (*p && *p != '\n' && *p != '\r' && *p != ':')
    {
        char c = *p++;
        if (c == '\\' && *p)
        {
            c = *p++;
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
        }
        if (used + 1 < size) out[used++] = c;
    }
    out[used] = 0;
    if (*p == ':') p++;
    *cursor = p;
}

static void nmcli_wide(const char *source, WCHAR *target, size_t size)
{
    if (!MultiByteToWideChar(CP_UTF8, 0, source, -1, target, (int)size)) target[0] = 0;
}

static BOOL active_uuid(const char *uuid, const char active[][40], unsigned count)
{
    for (unsigned i = 0; i < count; i++)
        if (!strcmp(uuid, active[i])) return TRUE;
    return FALSE;
}

static const WCHAR *connection_type(const WCHAR *type)
{
    if (!wcscmp(type, L"802-11-wireless")) return L"Wireless";
    if (!wcscmp(type, L"802-3-ethernet")) return L"Ethernet";
    if (!wcscmp(type, L"vpn")) return L"VPN";
    if (!wcscmp(type, L"bridge")) return L"Bridge";
    if (!wcscmp(type, L"bond")) return L"Bond";
    if (!wcscmp(type, L"team")) return L"Team";
    if (!wcscmp(type, L"wireguard")) return L"WireGuard";
    return L"Network";
}

static int selected_connection(void)
{
    int row = ListView_GetNextItem(connections_list, -1, LVNI_SELECTED);
    if (row < 0) return -1;
    LVITEMW item = {.mask = LVIF_PARAM, .iItem = row};
    if (!ListView_GetItem(connections_list, &item) || item.lParam < 0 || (unsigned)item.lParam >= connection_count)
        return -1;
    return (int)item.lParam;
}

static void connections_update_buttons(void)
{
    int index = selected_connection();
    BOOL selected = index >= 0;
    if (connections_connect)
    {
        SetWindowTextW(connections_connect, selected && connections[index].active ? L"&Disconnect" : L"&Connect");
        EnableWindow(connections_connect, selected);
    }
    EnableWindow(GetDlgItem(GetParent(connections_list), CONN_STATUS), selected && connections[index].active);
}

static void connections_refresh(void)
{
    char output[16384], active_output[4096], active[CONNECTION_LIMIT][40];
    unsigned active_count = 0;
    char *context, *line;
    if (!connections_list) return;
    connection_count = 0;
    if (!nmcli_checked)
    {
        char path[256];
        nmcli_available = capture(L"command -v nmcli 2>/dev/null", path, sizeof(path)) && *path;
        nmcli_checked = 1;
    }
    if (!nmcli_available)
    {
        ListView_DeleteAllItems(connections_list);
        SetWindowTextW(connections_hint, L"NetworkManager's nmcli tool is not available. Use Settings to manage connections.");
        connections_update_buttons();
        return;
    }
    active_output[0] = 0;
    capture(L"nmcli -t -g UUID connection show --active 2>/dev/null", active_output, sizeof(active_output));
    for (line = strtok_s(active_output, "\r\n", &context); line && active_count < CONNECTION_LIMIT;
         line = strtok_s(NULL, "\r\n", &context))
    {
        strncpy(active[active_count], line, sizeof(active[active_count]) - 1);
        active[active_count][sizeof(active[active_count]) - 1] = 0;
        active_count++;
    }
    if (!capture(L"nmcli -t --escape yes -f NAME,UUID,TYPE,DEVICE connection show 2>/dev/null", output, sizeof(output)))
    {
        ListView_DeleteAllItems(connections_list);
        SetWindowTextW(connections_hint, L"NetworkManager is not available. Open Settings to manage network access.");
        connections_update_buttons();
        return;
    }
    for (line = strtok_s(output, "\r\n", &context); line && connection_count < CONNECTION_LIMIT;
         line = strtok_s(NULL, "\r\n", &context))
    {
        char *cursor = line, name[512], uuid[128], type[128], device[128];
        NMCONNECTION *connection;
        nmcli_field(&cursor, name, sizeof(name));
        nmcli_field(&cursor, uuid, sizeof(uuid));
        nmcli_field(&cursor, type, sizeof(type));
        nmcli_field(&cursor, device, sizeof(device));
        if (!*uuid || !*name) continue;
        connection = &connections[connection_count];
        nmcli_wide(name, connection->name, ARRAYSIZE(connection->name));
        nmcli_wide(uuid, connection->uuid, ARRAYSIZE(connection->uuid));
        nmcli_wide(type, connection->type, ARRAYSIZE(connection->type));
        nmcli_wide(device, connection->device, ARRAYSIZE(connection->device));
        connection->active = active_uuid(uuid, active, active_count);
        connection_count++;
    }
    ListView_DeleteAllItems(connections_list);
    for (unsigned i = 0; i < connection_count; i++)
    {
        NMCONNECTION *connection = &connections[i];
        BOOL wireless = !wcscmp(connection->type, L"802-11-wireless");
        int image = wireless ? (connection->active ? 0 : 1) : (connection->active ? 2 : 3);
        WCHAR state[32];
        LVITEMW item = {.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM, .iItem = (int)i,
                        .iImage = image, .lParam = i, .pszText = connection->name};
        int row = ListView_InsertItem(connections_list, &item);
        if (row < 0) continue;
        ListView_SetItemText(connections_list, row, 1, (WCHAR *)connection_type(connection->type));
        ListView_SetItemText(connections_list, row, 2, connection->device[0] ? connection->device : L"—");
        wcscpy(state, connection->active ? L"Connected" : L"Disconnected");
        ListView_SetItemText(connections_list, row, 3, state);
    }
    if (!connection_count)
        SetWindowTextW(connections_hint, L"No saved connections. Use Settings to add a Wi-Fi or wired connection.");
    else
        SetWindowTextW(connections_hint, L"Select a connection to connect or disconnect it through NetworkManager.");
    connections_update_buttons();
}

static void connection_action(void)
{
    int index = selected_connection();
    WCHAR script[256], uuid[96];
    if (index < 0) return;
    shell_quote_w(connections[index].uuid, uuid, ARRAYSIZE(uuid));
    swprintf(script, ARRAYSIZE(script), L"nmcli --wait 15 connection %ls uuid %ls >/dev/null 2>&1",
             connections[index].active ? L"down" : L"up", uuid);
    if (spawn(script, NULL))
    {
        SetWindowTextW(connections_hint, connections[index].active ? L"Disconnecting..." : L"Connecting...");
        SetTimer(connections_window, TIMER_CONNECTIONS, 1500, NULL);
    }
    else SetWindowTextW(connections_hint, L"NetworkManager could not start the requested connection action.");
}

static void connections_layout(HWND hwnd, int width, int height)
{
    int button_y = height - 42;
    MoveWindow(connections_list, 12, 12, width - 24, height - 104, TRUE);
    MoveWindow(connections_hint, 12, height - 84, width - 24, 22, TRUE);
    MoveWindow(connections_connect, width - 396, button_y, 90, 26, TRUE);
    MoveWindow(GetDlgItem(hwnd, CONN_STATUS), width - 300, button_y, 90, 26, TRUE);
    MoveWindow(GetDlgItem(hwnd, CONN_SETTINGS), width - 204, button_y, 90, 26, TRUE);
    MoveWindow(GetDlgItem(hwnd, CONN_REFRESH), 12, button_y, 90, 26, TRUE);
    MoveWindow(GetDlgItem(hwnd, CONN_CLOSE), width - 108, button_y, 90, 26, TRUE);
}

static LRESULT CALLBACK connections_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        LVCOLUMNW column = {.mask = LVCF_TEXT | LVCF_WIDTH};
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&controls);
        connections_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            12, 12, 540, 240, hwnd, (HMENU)(INT_PTR)1000, GetModuleHandleW(NULL), NULL);
        connections_hint = CreateWindowW(L"STATIC", L"Reading NetworkManager connections...",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 12, 260, 540, 22, hwnd, NULL, GetModuleHandleW(NULL), NULL);
        connections_connect = CreateWindowW(L"BUTTON", L"&Connect", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 90, 26, hwnd, (HMENU)(INT_PTR)CONN_CONNECT, GetModuleHandleW(NULL), NULL);
        CreateWindowW(L"BUTTON", L"&Status", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 90, 26, hwnd, (HMENU)(INT_PTR)CONN_STATUS, GetModuleHandleW(NULL), NULL);
        CreateWindowW(L"BUTTON", L"&Settings", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 90, 26, hwnd, (HMENU)(INT_PTR)CONN_SETTINGS, GetModuleHandleW(NULL), NULL);
        CreateWindowW(L"BUTTON", L"&Refresh", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 90, 26, hwnd, (HMENU)(INT_PTR)CONN_REFRESH, GetModuleHandleW(NULL), NULL);
        CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 90, 26, hwnd, (HMENU)(INT_PTR)CONN_CLOSE, GetModuleHandleW(NULL), NULL);
        ListView_SetExtendedListViewStyle(connections_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);
        column.pszText = L"Name"; column.cx = 220; ListView_InsertColumn(connections_list, 0, &column);
        column.pszText = L"Type"; column.cx = 95; ListView_InsertColumn(connections_list, 1, &column);
        column.pszText = L"Device"; column.cx = 90; ListView_InsertColumn(connections_list, 2, &column);
        column.pszText = L"Status"; column.cx = 100; ListView_InsertColumn(connections_list, 3, &column);
        connection_images = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 0);
        if (connection_images)
        {
            ImageList_AddIcon(connection_images, xp_icon(10)); /* connected wireless */
            ImageList_AddIcon(connection_images, xp_icon(11)); /* disconnected wireless */
            ImageList_AddIcon(connection_images, xp_icon(4));  /* connected wired */
            ImageList_AddIcon(connection_images, xp_icon(8));  /* disconnected wired */
            ListView_SetImageList(connections_list, connection_images, LVSIL_SMALL);
        }
        connections_layout(hwnd, 580, 340);
        connections_refresh();
        SetTimer(hwnd, TIMER_CONNECTIONS, 4000, NULL);
        return 0;
    }
    case WM_SIZE:
        connections_layout(hwnd, LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_NOTIFY:
    {
        NMHDR *header = (NMHDR *)lparam;
        if (header->hwndFrom == connections_list && header->code == LVN_ITEMCHANGED)
            connections_update_buttons();
        else if (header->hwndFrom == connections_list && header->code == NM_DBLCLK)
        {
            int index = selected_connection();
            if (index >= 0 && connections[index].active)
            {
                poll_network(TRUE);
                show_network_status();
            }
            else connection_action();
        }
        return 0;
    }
    case WM_TIMER:
        if (wparam == TIMER_CONNECTIONS)
        {
            connections_refresh();
            SetTimer(hwnd, TIMER_CONNECTIONS, 4000, NULL);
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case CONN_CONNECT: connection_action(); return 0;
        case CONN_STATUS:
            poll_network(TRUE);
            show_network_status();
            return 0;
        case CONN_SETTINGS: run_settings(CMD_NETWORK_SETTINGS); return 0;
        case CONN_REFRESH: connections_refresh(); return 0;
        case CONN_CLOSE: DestroyWindow(hwnd); return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_CONNECTIONS);
        if (connection_images) ImageList_Destroy(connection_images);
        connection_images = NULL;
        connections_window = connections_list = connections_hint = connections_connect = NULL;
        connection_count = 0;
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static void show_network_connections(void)
{
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW cls = {.style = CS_DBLCLKS, .lpfnWndProc = connections_proc, .hInstance = instance,
                     .hCursor = LoadCursorW(NULL, IDC_ARROW), .hIcon = xp_icon(4),
                     .hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1), .lpszClassName = CONNECTIONS_CLASS};
    if (connections_window)
    {
        ShowWindow(connections_window, SW_RESTORE);
        SetForegroundWindow(connections_window);
        return;
    }
    if (!GetClassInfoW(instance, CONNECTIONS_CLASS, &cls) && !RegisterClassW(&cls)) return;
    connections_window = CreateWindowExW(WS_EX_DLGMODALFRAME, CONNECTIONS_CLASS, L"Network Connections",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 600, 410,
        NULL, NULL, instance, NULL);
    if (connections_window)
    {
        ShowWindow(connections_window, SW_SHOW);
        SetForegroundWindow(connections_window);
    }
}

/* ---- XP-style available wireless networks, backed by NetworkManager ----- */

#define WIRELESS_CLASS L"XPWirelessNetworks"

static void shell_quote_w(const WCHAR *source, WCHAR *target, size_t capacity)
{
    size_t used = 0;
    if (!capacity) return;
    target[used++] = L'\'';
    while (*source && used + 5 < capacity)
    {
        if (*source == L'\'')
        {
            target[used++] = L'\'';
            target[used++] = L'\\';
            target[used++] = L'\'';
            target[used++] = L'\'';
            source++;
        }
        else target[used++] = *source++;
    }
    if (used + 2 <= capacity) target[used++] = L'\'';
    target[used] = 0;
}

static void wireless_layout(HWND hwnd, int width, int height)
{
    MoveWindow(GetDlgItem(hwnd, WIFI_TASKS), 0, 0, 155, height, TRUE);
    MoveWindow(GetDlgItem(hwnd, 610), 164, 2, width - 174, 23, TRUE);
    MoveWindow(GetDlgItem(hwnd, 611), 164, 36, width - 174, 27, TRUE);
    MoveWindow(wireless_list, 164, 64, width - 174, height - 100, TRUE);
    MoveWindow(wireless_hint, 164, height - 28, width - 250, 24, TRUE);
    MoveWindow(wireless_connect_button, width - 72, height - 29, 64, 22, TRUE);
}

static void wireless_update_button(void)
{
    int selected = (int)SendMessageW(wireless_list, LB_GETCURSEL, 0, 0);
    EnableWindow(wireless_connect_button, selected != LB_ERR && selected < (int)wireless_count);
    int count = (int)SendMessageW(wireless_list, LB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; i++)
        SendMessageW(wireless_list, LB_SETITEMHEIGHT, i, i == selected ? 114 : 48);
    InvalidateRect(wireless_list, NULL, TRUE);
}

static BOOL same_access_point(const NMWIRELESS *network, const WCHAR *ssid, const WCHAR *security)
{
    return !wcscmp(network->ssid, ssid) && !wcscmp(network->security, security);
}

static void wireless_refresh(void)
{
    char output[16384], *context, *line;
    WCHAR previous[128] = L"";
    int previous_row = (int)SendMessageW(wireless_list, LB_GETCURSEL, 0, 0), preferred = -1;
    if (!wireless_list) return;
    if (previous_row != LB_ERR)
    {
        LRESULT item = SendMessageW(wireless_list, LB_GETITEMDATA, previous_row, 0);
        if (item >= 0 && (unsigned)item < wireless_count)
            wcscpy(previous, wireless_networks[item].ssid);
    }
    wireless_count = 0;
    if (!nmcli_checked)
    {
        char path[256];
        nmcli_available = capture(L"command -v nmcli 2>/dev/null", path, sizeof(path)) && *path;
        nmcli_checked = 1;
    }
    if (!nmcli_available ||
        !capture(L"nmcli -t --escape yes -f SSID,BSSID,SIGNAL,SECURITY,IN-USE dev wifi list --rescan no 2>/dev/null",
                 output, sizeof(output)))
    {
        SendMessageW(wireless_list, LB_RESETCONTENT, 0, 0);
        SetWindowTextW(wireless_hint, L"NetworkManager is unavailable. Open Settings to manage network access.");
        wireless_update_button();
        return;
    }
    for (line = strtok_s(output, "\r\n", &context); line && wireless_count < CONNECTION_LIMIT;
         line = strtok_s(NULL, "\r\n", &context))
    {
        char *cursor = line, ssid[512], bssid[128], signal[32], security[128], in_use[16];
        WCHAR wide_ssid[128], wide_bssid[32], wide_security[64];
        int strength, duplicate = -1;
        NMWIRELESS *network;
        nmcli_field(&cursor, ssid, sizeof(ssid));
        nmcli_field(&cursor, bssid, sizeof(bssid));
        nmcli_field(&cursor, signal, sizeof(signal));
        nmcli_field(&cursor, security, sizeof(security));
        nmcli_field(&cursor, in_use, sizeof(in_use));
        if (!*bssid || !*ssid) continue;
        nmcli_wide(ssid, wide_ssid, ARRAYSIZE(wide_ssid));
        nmcli_wide(bssid, wide_bssid, ARRAYSIZE(wide_bssid));
        nmcli_wide(security, wide_security, ARRAYSIZE(wide_security));
        strength = atoi(signal);
        for (unsigned i = 0; i < wireless_count; i++)
            if (same_access_point(&wireless_networks[i], wide_ssid, wide_security))
            {
                duplicate = (int)i;
                break;
            }
        if (duplicate >= 0)
        {
            network = &wireless_networks[duplicate];
            if (strength > network->signal)
            {
                network->signal = strength;
                wcscpy(network->bssid, wide_bssid);
            }
            if (*in_use == '*') network->active = TRUE;
            continue;
        }
        network = &wireless_networks[wireless_count++];
        wcscpy(network->ssid, wide_ssid);
        wcscpy(network->bssid, wide_bssid);
        wcscpy(network->security, wide_security);
        network->signal = strength;
        network->secured = *security && strcmp(security, "--") != 0;
        network->active = *in_use == '*';
    }
    /* Keep the strongest AP first, while putting the currently connected SSID at the top. */
    for (unsigned i = 0; i < wireless_count; i++)
        for (unsigned j = i + 1; j < wireless_count; j++)
            if ((wireless_networks[j].active && !wireless_networks[i].active) ||
                (wireless_networks[j].active == wireless_networks[i].active &&
                 wireless_networks[j].signal > wireless_networks[i].signal))
            {
                NMWIRELESS swap = wireless_networks[i];
                wireless_networks[i] = wireless_networks[j];
                wireless_networks[j] = swap;
            }
    SendMessageW(wireless_list, LB_RESETCONTENT, 0, 0);
    for (unsigned i = 0; i < wireless_count; i++)
    {
        LRESULT row = SendMessageW(wireless_list, LB_ADDSTRING, 0, (LPARAM)wireless_networks[i].ssid);
        if (row == LB_ERR || row == LB_ERRSPACE) continue;
        SendMessageW(wireless_list, LB_SETITEMDATA, row, i);
        if (preferred < 0 && !wcscmp(wireless_networks[i].ssid, previous)) preferred = (int)row;
        if (preferred < 0 && !previous[0] && wireless_networks[i].active) preferred = (int)row;
    }
    if (wireless_count)
    {
        if (preferred < 0) preferred = 0;
        SendMessageW(wireless_list, LB_SETCURSEL, preferred, 0);
        SetWindowTextW(wireless_hint, L"");
    }
    else SetWindowTextW(wireless_hint, L"No wireless networks found. Check the adapter and refresh the list.");
    wireless_update_button();
    InvalidateRect(wireless_list, NULL, TRUE);
}

static void wireless_connect_selected(void)
{
    int row = (int)SendMessageW(wireless_list, LB_GETCURSEL, 0, 0);
    LRESULT item;
    WCHAR ssid[512], bssid[128], script[1200];
    if (row == LB_ERR) return;
    item = SendMessageW(wireless_list, LB_GETITEMDATA, row, 0);
    if (item < 0 || (unsigned)item >= wireless_count) return;
    wcscpy(wireless_connect_ssid, wireless_networks[item].ssid);
    shell_quote_w(wireless_networks[item].ssid, ssid, ARRAYSIZE(ssid));
    shell_quote_w(wireless_networks[item].bssid, bssid, ARRAYSIZE(bssid));
    swprintf(script, ARRAYSIZE(script), L"nmcli --wait 20 device wifi connect %ls bssid %ls >/dev/null 2>&1",
             ssid, bssid);
    if (!spawn(script, NULL))
    {
        SetWindowTextW(wireless_hint, L"NetworkManager could not start the connection request.");
        return;
    }
    wireless_connect_started = GetTickCount64();
    SetWindowTextW(wireless_hint, L"Connecting to the selected wireless network...");
    EnableWindow(wireless_connect_button, FALSE);
    SetTimer(wireless_window, TIMER_WIRELESS_CONNECT, 1500, NULL);
}

/* These are XP's wireless UI icons, not the notification-area glyphs. */
static HICON wireless_icon(int id, int size)
{
    static HMODULE module;
    static struct { int id, size; HICON icon; } cache[20];
    static unsigned count;
    for (unsigned i = 0; i < count; i++)
        if (cache[i].id == id && cache[i].size == size) return cache[i].icon;
    if (!module) module = xp_module(L"xpsp2res.dll");
    HICON icon = module ? LoadImageW(module, MAKEINTRESOURCEW(id), IMAGE_ICON, size, size, 0) : NULL;
    if (count < ARRAYSIZE(cache))
    {
        cache[count].id = id; cache[count].size = size; cache[count++].icon = icon;
    }
    return icon;
}

static void wireless_gradient(HDC dc, RECT rect, COLORREF start, COLORREF end, BOOL horizontal)
{
    int length = horizontal ? rect.right - rect.left : rect.bottom - rect.top;
    HGDIOBJ old = SelectObject(dc, GetStockObject(DC_PEN));
    for (int i = 0; i < length; i++)
    {
        int n = max(1, length - 1);
        COLORREF color = RGB(GetRValue(start) + ((int)GetRValue(end) - GetRValue(start)) * i / n,
                             GetGValue(start) + ((int)GetGValue(end) - GetGValue(start)) * i / n,
                             GetBValue(start) + ((int)GetBValue(end) - GetBValue(start)) * i / n);
        SetDCPenColor(dc, color);
        if (horizontal)
        {
            MoveToEx(dc, rect.left + i, rect.top, NULL); LineTo(dc, rect.left + i, rect.bottom);
        }
        else
        {
            MoveToEx(dc, rect.left, rect.top + i, NULL); LineTo(dc, rect.right, rect.top + i);
        }
    }
    SelectObject(dc, old);
}

static void wireless_task_card(HDC dc, RECT rect, const WCHAR *title, HFONT heading)
{
    HBRUSH body = CreateSolidBrush(RGB(214, 223, 247));
    FillRect(dc, &rect, body); DeleteObject(body);
    RECT header = {rect.left + 1, rect.top + 1, rect.right - 1, rect.top + 21};
    wireless_gradient(dc, header, RGB(255, 255, 255), RGB(198, 211, 247), TRUE);
    FrameRect(dc, &rect, GetStockObject(WHITE_BRUSH));
    SelectObject(dc, heading);
    SetTextColor(dc, RGB(33, 93, 198));
    header.left += 7;
    DrawTextW(dc, title, -1, &header, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

static void wireless_task_link(HDC dc, int icon_id, int top, int bottom, const WCHAR *text)
{
    HICON icon = wireless_icon(icon_id, 16);
    if (icon) DrawIconEx(dc, 14, top, icon, 16, 16, 0, NULL, DI_NORMAL);
    RECT label = {34, top + 1, 141, bottom};
    DrawTextW(dc, text, -1, &label, DT_WORDBREAK | DT_NOPREFIX);
}

static int wireless_task_at(POINT point)
{
    if (point.x < 7 || point.x >= 146) return 0;
    if (point.y >= 34 && point.y < 62) return WIFI_REFRESH;
    if (point.y >= 68 && point.y < 105) return WIFI_SETTINGS;
    if (point.y >= 141 && point.y < 174) return WIFI_HELP;
    if (point.y >= 176 && point.y < 210) return WIFI_SETTINGS;
    if (point.y >= 211 && point.y < 248) return WIFI_SETTINGS;
    return 0;
}

static void wireless_draw_tasks(const DRAWITEMSTRUCT *draw)
{
    HFONT heading = CreateFontW(-10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
    HFONT link = CreateFontW(-9, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
    HGDIOBJ old_font = SelectObject(draw->hDC, heading);
    SetBkMode(draw->hDC, TRANSPARENT);
    /* The Luna task-pane colors from XP's shellstyle.dll. */
    wireless_gradient(draw->hDC, draw->rcItem, RGB(123, 162, 231), RGB(99, 117, 214), FALSE);
    wireless_task_card(draw->hDC, (RECT){7, 8, 146, 105}, L"Network Tasks", heading);
    wireless_task_card(draw->hDC, (RECT){7, 117, 146, 248}, L"Related Tasks", heading);
    SelectObject(draw->hDC, link);
    wireless_task_link(draw->hDC, 2020, 36, 63, L"Refresh network list");
    wireless_task_link(draw->hDC, 2003, 70, 102, L"Set up a wireless network\nfor a home or small office");
    wireless_task_link(draw->hDC, 2024, 143, 173, L"Learn about wireless networking");
    wireless_task_link(draw->hDC, 2022, 178, 209, L"Change the order of preferred networks");
    wireless_task_link(draw->hDC, 2023, 213, 244, L"Change advanced settings");
    SelectObject(draw->hDC, old_font);
    DeleteObject(link); DeleteObject(heading);
}

static void wireless_draw_row(const DRAWITEMSTRUCT *draw)
{
    if (draw->itemID == (UINT)-1) return;
    LRESULT index = SendMessageW(wireless_list, LB_GETITEMDATA, draw->itemID, 0);
    if (index < 0 || (unsigned)index >= wireless_count) return;
    NMWIRELESS *network = &wireless_networks[index];
    RECT rect = draw->rcItem;
    BOOL selected = (draw->itemState & ODS_SELECTED) != 0;
    HBRUSH background = CreateSolidBrush(selected ? RGB(49, 106, 197) : RGB(241, 243, 251));
    FillRect(draw->hDC, &rect, background); DeleteObject(background);
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, selected ? RGB(255,255,255) : RGB(0,0,0));
    HICON antenna = wireless_icon(2000, 32);
    if (antenna) DrawIconEx(draw->hDC, rect.left + 6, rect.top + 7, antenna, 32, 32, 0, NULL, DI_NORMAL);
    int bars = max(0, min(5, (network->signal + 19) / 20));
    HICON signal = wireless_icon(2010 + bars, 24);
    if (signal) DrawIconEx(draw->hDC, rect.right - 24, rect.top + (selected ? 20 : 14), signal,
                           24, 24, 0, NULL, DI_NORMAL);
    HFONT title = CreateFontW(-10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
    HGDIOBJ old_font = SelectObject(draw->hDC, title);
    RECT text = {rect.left + 50, rect.top + 4, rect.right - 37, rect.top + 20};
    DrawTextW(draw->hDC, network->ssid, -1, &text, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(draw->hDC, wireless_text_font);
    text.top = rect.top + 29; text.bottom = text.top + 16;
    if (network->secured)
    {
        HICON lock = wireless_icon(2001, 16);
        if (lock) DrawIconEx(draw->hDC, text.left, text.top - 2, lock, 16, 16, 0, NULL, DI_NORMAL);
        text.left += 18;
    }
    WCHAR detail[112];
    if (network->secured)
        swprintf(detail, ARRAYSIZE(detail), L"Security-enabled wireless network (%ls)", network->security);
    else wcscpy(detail, L"Unsecured wireless network");
    DrawTextW(draw->hDC, detail, -1, &text, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (selected)
    {
        text = (RECT){rect.left + 50, rect.top + 51, rect.right - 30, rect.bottom - 9};
        const WCHAR *help = network->active ? L"You are currently connected to this network." :
            network->secured ? L"This network requires a network key. If you want to connect to this network, click Connect." :
                               L"This network is not secured. If you want to connect to this network, click Connect.";
        DrawTextW(draw->hDC, help, -1, &text, DT_WORDBREAK | DT_NOPREFIX);
    }
    SelectObject(draw->hDC, old_font); DeleteObject(title);
    if (draw->itemState & ODS_FOCUS)
    {
        InflateRect(&rect, -1, -1); DrawFocusRect(draw->hDC, &rect);
    }
}

static LRESULT CALLBACK wireless_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        wireless_text_font = CreateFontW(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
        HFONT font = wireless_text_font ? wireless_text_font : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY,
            0, 0, 150, 400, hwnd, (HMENU)(INT_PTR)WIFI_TASKS, GetModuleHandleW(NULL), NULL);
        CreateWindowW(L"STATIC", L"Choose a wireless network", WS_CHILD | WS_VISIBLE | SS_LEFT,
            164, 12, 400, 24, hwnd, (HMENU)(INT_PTR)610, GetModuleHandleW(NULL), NULL);
        CreateWindowW(L"STATIC", L"Click an item in the list below to connect to a wireless network in range or to get more information.",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 164, 39, 400, 42, hwnd, (HMENU)(INT_PTR)611, GetModuleHandleW(NULL), NULL);
        wireless_list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWVARIABLE | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
            160, 88, 400, 240, hwnd, (HMENU)(INT_PTR)WIFI_LIST, GetModuleHandleW(NULL), NULL);
        wireless_hint = CreateWindowW(L"STATIC", L"Reading available wireless networks...",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 160, 340, 400, 24, hwnd, NULL, GetModuleHandleW(NULL), NULL);
        wireless_connect_button = CreateWindowW(L"BUTTON", L"&Connect", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            460, 370, 92, 26, hwnd, (HMENU)(INT_PTR)WIFI_CONNECT, GetModuleHandleW(NULL), NULL);
        for (int id = 610; id <= 611; id++) SendMessageW(GetDlgItem(hwnd, id), WM_SETFONT, (WPARAM)font, TRUE);
        wireless_title_font = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
        if (wireless_title_font)
            SendMessageW(GetDlgItem(hwnd, 610), WM_SETFONT, (WPARAM)wireless_title_font, TRUE);
        SendMessageW(wireless_list, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(wireless_hint, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(wireless_connect_button, WM_SETFONT, (WPARAM)font, TRUE);
        RECT client;
        GetClientRect(hwnd, &client);
        wireless_layout(hwnd, client.right, client.bottom);
        wireless_refresh();
        spawn(L"nmcli device wifi rescan >/dev/null 2>&1", NULL);
        SetTimer(hwnd, TIMER_WIRELESS, 2000, NULL);
        return 0;
    }
    case WM_SIZE:
        wireless_layout(hwnd, LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_MEASUREITEM:
        if (((MEASUREITEMSTRUCT *)lparam)->CtlID == WIFI_LIST)
        {
            ((MEASUREITEMSTRUCT *)lparam)->itemHeight = 48;
            return TRUE;
        }
        break;
    case WM_DRAWITEM:
        if (((DRAWITEMSTRUCT *)lparam)->CtlID == WIFI_TASKS) wireless_draw_tasks((DRAWITEMSTRUCT *)lparam);
        else if (((DRAWITEMSTRUCT *)lparam)->CtlID == WIFI_LIST) wireless_draw_row((DRAWITEMSTRUCT *)lparam);
        return TRUE;
    case WM_TIMER:
        if (wparam == TIMER_WIRELESS)
        {
            wireless_refresh();
            SetTimer(hwnd, TIMER_WIRELESS, 10000, NULL);
        }
        else if (wparam == TIMER_WIRELESS_CONNECT)
        {
            poll_network(TRUE);
            if (net_ssid[0] && !wcscmp(net_ssid, wireless_connect_ssid))
            {
                KillTimer(hwnd, TIMER_WIRELESS_CONNECT);
                SetWindowTextW(wireless_hint, L"Connected to the selected wireless network.");
                wireless_refresh();
            }
            else if (GetTickCount64() - wireless_connect_started > 25000)
            {
                KillTimer(hwnd, TIMER_WIRELESS_CONNECT);
                SetWindowTextW(wireless_hint, L"Connection did not complete. Check the saved password in Settings.");
                wireless_update_button();
            }
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case WIFI_TASKS:
        {
            POINT point;
            GetCursorPos(&point);
            ScreenToClient(GetDlgItem(hwnd, WIFI_TASKS), &point);
            int command = wireless_task_at(point);
            if (command) PostMessageW(hwnd, WM_COMMAND, command, 0);
            return 0;
        }
        case WIFI_LIST:
            if (HIWORD(wparam) == LBN_DBLCLK) wireless_connect_selected();
            else if (HIWORD(wparam) == LBN_SELCHANGE) wireless_update_button();
            return 0;
        case WIFI_CONNECT: wireless_connect_selected(); return 0;
        case WIFI_REFRESH:
            spawn(L"nmcli device wifi rescan >/dev/null 2>&1", NULL);
            wireless_refresh();
            SetTimer(hwnd, TIMER_WIRELESS, 2000, NULL);
            return 0;
        case WIFI_SETTINGS: run_settings(CMD_NETWORK_SETTINGS); return 0;
        case WIFI_HELP:
            MessageBoxW(hwnd, L"Select a wireless network and click Connect. A padlock marks a secured network. "
                        L"The signal bars show how strongly your computer receives that network.",
                        L"Wireless networking", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        break;
    case WM_SETCURSOR:
        if ((HWND)wparam == GetDlgItem(hwnd, WIFI_TASKS))
        {
            POINT point;
            GetCursorPos(&point);
            ScreenToClient((HWND)wparam, &point);
            if (wireless_task_at(point))
            {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                return TRUE;
            }
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_WIRELESS);
        KillTimer(hwnd, TIMER_WIRELESS_CONNECT);
        if (wireless_title_font) DeleteObject(wireless_title_font);
        wireless_title_font = NULL;
        if (wireless_text_font) DeleteObject(wireless_text_font);
        wireless_text_font = NULL;
        wireless_window = wireless_list = wireless_hint = wireless_connect_button = NULL;
        wireless_count = 0;
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static void show_wireless_networks(void)
{
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW cls = {.style = CS_DBLCLKS, .lpfnWndProc = wireless_proc, .hInstance = instance,
                     .hCursor = LoadCursorW(NULL, IDC_ARROW), .hIcon = wireless_icon(2021, 16),
                     .hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1), .lpszClassName = WIRELESS_CLASS};
    if (wireless_window)
    {
        ShowWindow(wireless_window, SW_RESTORE);
        SetForegroundWindow(wireless_window);
        return;
    }
    if (!GetClassInfoW(instance, WIRELESS_CLASS, &cls) && !RegisterClassW(&cls)) return;
    wireless_window = CreateWindowExW(WS_EX_DLGMODALFRAME, WIRELESS_CLASS, L"Wireless Network Connection",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 550, 420,
        NULL, NULL, instance, NULL);
    if (wireless_window)
    {
        ShowWindow(wireless_window, SW_SHOW);
        SetForegroundWindow(wireless_window);
    }
}

/* ---- Main window -------------------------------------------------------- */

static void run_settings(UINT command)
{
    switch (command)
    {
    case CMD_MIXER:
        if (open_volume_control()) break;
        /* Without XP's sndvol32: the Linux mixer. */
        spawn(L"command -v pavucontrol >/dev/null && exec pavucontrol; "
              L"kcmshell6 kcm_pulseaudio 2>/dev/null || systemsettings kcm_pulseaudio", NULL);
        break;
    case CMD_SOUND:
        spawn(L"kcmshell6 kcm_pulseaudio 2>/dev/null || systemsettings kcm_pulseaudio 2>/dev/null || pavucontrol", NULL);
        break;
    case CMD_NETWORK_SETTINGS:
        spawn(L"kcmshell6 kcm_networkmanagement 2>/dev/null || systemsettings kcm_networkmanagement 2>/dev/null "
              L"|| nm-connection-editor", NULL);
        break;
    case CMD_POWER:
        spawn(L"kcmshell6 kcm_powerdevilprofilesconfig 2>/dev/null || systemsettings kcm_powerdevilprofilesconfig",
              NULL);
        break;
    }
}

static void context_menu(UINT id)
{
    HMENU menu = CreatePopupMenu();
    POINT cursor;
    UINT command;
    if (id == ID_VOLUME)
    {
        AppendMenuW(menu, MF_STRING, CMD_MIXER, L"&Open Volume Control");
        AppendMenuW(menu, MF_STRING, CMD_SOUND, L"&Adjust Audio Properties");
        SetMenuDefaultItem(menu, CMD_MIXER, FALSE);
    }
    else if (id == ID_NETWORK)
    {
        AppendMenuW(menu, MF_STRING, CMD_STATUS, L"&Status");
        AppendMenuW(menu, MF_STRING, CMD_NETWORK, L"&View Available Wireless Networks");
        AppendMenuW(menu, MF_STRING, CMD_CONNECTIONS, L"Open &Network Connections");
        SetMenuDefaultItem(menu, CMD_STATUS, FALSE);
    }
    else
    {
        AppendMenuW(menu, MF_STRING, CMD_METER, L"Open &Power Meter");
        AppendMenuW(menu, MF_STRING, CMD_POWER, L"&Adjust Power Properties");
        SetMenuDefaultItem(menu, CMD_METER, FALSE);
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, CMD_EXIT, L"E&xit notification icons");
    GetCursorPos(&cursor);
    SetForegroundWindow(window);
    command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, window, NULL);
    PostMessageW(window, WM_NULL, 0, 0);
    DestroyMenu(menu);
    if (command == CMD_EXIT) DestroyWindow(window);
    else if (command == CMD_STATUS) show_network_status();
    else if (command == CMD_METER) show_power_meter();
    else if (command == CMD_CONNECTIONS) show_network_connections();
    else if (command == CMD_NETWORK) show_wireless_networks();
    else if (command) run_settings(command);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_OPEN_WIRELESS)
    {
        show_wireless_networks();
        return 0;
    }
    if (message == WM_OPEN_CONNECTIONS)
    {
        show_network_connections();
        return 0;
    }
    if (message == taskbar_created && taskbar_created)
    {
        add_all();
        return 0;
    }
    switch (message)
    {
    case WM_TIMER:
        if (wparam == TIMER_FIND)
        {
            if (find_tray())
            {
                KillTimer(hwnd, TIMER_FIND);
                add_all();
                SetTimer(hwnd, TIMER_POLL, 1000, NULL);
            }
        }
        else if (wparam == TIMER_POLL)
        {
            poll();
            flyouts_update();
        }
        else if (wparam == TIMER_APPLY)
        {
            KillTimer(hwnd, TIMER_APPLY);
            if (wanted_volume >= 0)
            {
                set_volume(wanted_volume);
                if (muted && wanted_volume > 0) set_mute(0);
                wanted_volume = -1;
                show_volume();
            }
        }
        return 0;
    case WM_TRAY:
        switch (LOWORD(lparam))
        {
        case WM_LBUTTONUP:
            if (wparam == ID_VOLUME) show_popup();
            else if (wparam == ID_NETWORK) show_wireless_networks();
            else show_power_meter();
            break;
        case WM_LBUTTONDBLCLK:
            if (wparam == ID_VOLUME) run_settings(CMD_MIXER);
            break;
        case WM_RBUTTONUP:
            context_menu((UINT)wparam);
            break;
        }
        return 0;
    case WM_DESTROY:
        for (UINT id = 1; id < ID_COUNT; id++)
            if (icons[id].shown) tray_send(NIM_DELETE, id, NULL, NULL, 0);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, WCHAR *command_line, int show)
{
    WNDCLASSW main_class = {.lpfnWndProc = window_proc, .hInstance = instance, .lpszClassName = L"XPTrayHelper"};
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_BAR_CLASSES | ICC_PROGRESS_CLASS};
    MSG message;
    BOOL open_connections = command_line && wcsstr(command_line, L"connections") != NULL;
    BOOL open_wireless = command_line && wcsstr(command_line, L"wireless") != NULL;
    (void)previous; (void)show;

    CreateMutexW(NULL, TRUE, L"XPTrayHelperRunning");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (open_connections || open_wireless)
        {
            HWND existing = FindWindowW(L"XPTrayHelper", L"XP notification icons");
            if (existing) SendMessageTimeoutW(existing, open_wireless ? WM_OPEN_WIRELESS : WM_OPEN_CONNECTIONS,
                                             0, 0, SMTO_ABORTIFHUNG, 3000, NULL);
        }
        return 0;
    }
    InitCommonControlsEx(&controls);
    RegisterClassW(&main_class);
    /* stobject's Power Meter template names systray's own dialog class, and
     * sndvol32's Volume template ("sndvol32 /t") its own. */
    {
        WNDCLASSW dialog_class = {.style = CS_GLOBALCLASS | CS_DBLCLKS, .lpfnWndProc = DefDlgProcW,
                                  .cbWndExtra = DLGWINDOWEXTRA, .hInstance = instance,
                                  .hCursor = LoadCursorW(NULL, IDC_ARROW),
                                  .hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1), .lpszClassName = L"SystemTray_Main"};
        RegisterClassW(&dialog_class);
        dialog_class.lpszClassName = L"Tray Volume";
        RegisterClassW(&dialog_class);
    }
    taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    window = CreateWindowW(L"XPTrayHelper", L"XP notification icons", WS_POPUP, 0, 0, 0, 0, NULL, NULL, instance, NULL);
    if (!window) return 1;
    if (open_connections) PostMessageW(window, WM_OPEN_CONNECTIONS, 0, 0);
    if (open_wireless) PostMessageW(window, WM_OPEN_WIRELESS, 0, 0);
    SetTimer(window, TIMER_FIND, 500, NULL);
    while (GetMessageW(&message, NULL, 0, 0))
    {
        if (net_sheet && PropSheet_IsDialogMessage(net_sheet, &message))
        {
            if (!PropSheet_GetCurrentPageHwnd(net_sheet))
            {
                DestroyWindow(net_sheet);
                net_sheet = NULL;
            }
            continue;
        }
        if (popup && IsDialogMessageW(popup, &message)) continue;
        if (power_meter && IsDialogMessageW(power_meter, &message)) continue;
        if (wireless_window && IsDialogMessageW(wireless_window, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
