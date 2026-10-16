/* notify-broadcast: which messages with a pointer SendNotifyMessage, SendMessageCallback and
 * PostMessage take, for one window of another thread and for HWND_BROADCAST.
 *   notify-broadcast-test OUT
 * Windows 7 SP1: for one window all of them fail with 1159 (ERROR_MESSAGE_SYNC_ONLY), also
 * WM_SETTINGCHANGE with a NULL string; SendNotifyMessage to HWND_BROADCAST takes
 * WM_SETTINGCHANGE (W and A, NULL too) and WM_DEVMODECHANGE, and the window of the other
 * thread gets the string; SendMessageCallback and PostMessage to HWND_BROADCAST fail with 1159.
 * Windows 7 hands SendNotifyMessageA's string over as its bytes, unconverted, even to a Unicode
 * window ("6957 346e ..."); Wine converts it to Unicode ("0057 0069 ..."). */
#include <windows.h>
#include <stdio.h>

static HWND target;
static HANDLE ready;
static FILE *out;

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_SETTINGCHANGE || msg == WM_DEVMODECHANGE)
    {
        const WCHAR *str = (const WCHAR *)lp;
        int i;
        if (!lp) fprintf( out, "  got %04x NULL\n", msg );
        else
        {
            /* the first characters in hex: an ANSI string would show as packed bytes */
            fprintf( out, "  got %04x", msg );
            for (i = 0; i < 4 && str[i]; i++) fprintf( out, " %04x", str[i] );
            fprintf( out, "\n" );
        }
        fflush( out );
    }
    else if (msg == WM_SETTEXT) fprintf( out, "  got WM_SETTEXT\n" ), fflush( out );
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static DWORD WINAPI thread( void *arg )
{
    (void)arg;
    WNDCLASSW wc = { 0, proc, 0, 0, 0, 0, 0, 0, 0, L"Win4WineNotifyProbe" };
    MSG msg;
    RegisterClassW( &wc );
    target = CreateWindowW( wc.lpszClassName, L"probe", WS_OVERLAPPED, 0, 0, 10, 10, 0, 0, 0, 0 );
    SetEvent( ready );
    while (GetMessageW( &msg, 0, 0, 0 )) DispatchMessageW( &msg );
    return 0;
}

static void CALLBACK callback( HWND hwnd, UINT msg, ULONG_PTR data, LRESULT result )
{
    (void)hwnd; (void)msg; (void)data; (void)result;
}

#define TRY(call) do { BOOL r; SetLastError( 0 ); r = call; fprintf( out, "%s -> %d %lu\n", #call, r, GetLastError() ); \
                       fflush( out ); Sleep( 300 ); } while (0)

int wmain( int argc, WCHAR **argv )
{
    if (argc < 2 || !(out = _wfopen( argv[1], L"w" ))) return 1;
    ready = CreateEventW( NULL, TRUE, FALSE, NULL );
    CreateThread( NULL, 0, thread, NULL, 0, NULL );
    WaitForSingleObject( ready, INFINITE );
    TRY( SendNotifyMessageW( target, WM_SETTINGCHANGE, 0, (LPARAM)L"Win4WineProbe" ) );
    TRY( SendNotifyMessageW( target, WM_SETTINGCHANGE, 0, 0 ) );
    TRY( SendNotifyMessageW( target, WM_DEVMODECHANGE, 0, (LPARAM)L"Win4WineProbe" ) );
    TRY( SendNotifyMessageW( target, WM_SETTEXT, 0, (LPARAM)L"text" ) );
    TRY( SendMessageCallbackW( target, WM_SETTINGCHANGE, 0, (LPARAM)L"Win4WineProbe", callback, 0 ) );
    TRY( PostMessageW( target, WM_SETTINGCHANGE, 0, (LPARAM)L"Win4WineProbe" ) );
    TRY( SendNotifyMessageW( HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Win4WineProbe" ) );
    TRY( SendNotifyMessageA( HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Win4WineProbeA" ) );
    TRY( SendNotifyMessageW( HWND_BROADCAST, WM_SETTINGCHANGE, 0, 0 ) );
    TRY( SendNotifyMessageW( HWND_BROADCAST, WM_DEVMODECHANGE, 0, (LPARAM)L"Win4WineProbe" ) );
    TRY( SendMessageCallbackW( HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Win4WineProbe", callback, 0 ) );
    TRY( PostMessageW( HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Win4WineProbe" ) );
    fclose( out );
    return 0;
}
