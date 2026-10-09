/* Public API regression for our own registry profile, isolated prefixes only. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
#include <stddef.h>
#include <wchar.h>

static const WCHAR key_name[]=L"Software\\Win4Wine\\ThemeProfiles\\JapaneseXPLuna";
static const WCHAR *names[]={L"CaptionFont",L"SmallCaptionFont",L"MenuFont",L"StatusFont",L"MessageFont",L"IconFont",L"StartFont"};
static const int ids[]={TMT_CAPTIONFONT,TMT_SMALLCAPTIONFONT,TMT_MENUFONT,TMT_STATUSFONT,TMT_MSGBOXFONT,TMT_ICONTITLEFONT};
static int failures;
static void check(BOOL ok,const char *label)
{
    if(!ok){fprintf(stderr,"FAIL: %s\n",label);failures++;}
}
static void dword(HKEY key,LPCWSTR name,DWORD value)
{
    check(!RegSetValueExW(key,name,0,REG_DWORD,(BYTE*)&value,sizeof(value)),"write own profile DWORD");
}
static HRESULT query(HTHEME window,HTHEME start,int role,LOGFONTW *font)
{
    ZeroMemory(font,sizeof(*font));
    return role==6?GetThemeFont(start,NULL,BP_PUSHBUTTON,PBS_NORMAL,TMT_FONT,font):GetThemeSysFont(window,ids[role],font);
}
static BOOL equal(const LOGFONTW *a,const LOGFONTW *b)
{
    return !memcmp(a,b,offsetof(LOGFONTW,lfFaceName)) && !lstrcmpW(a->lfFaceName,b->lfFaceName);
}
static void compare(HTHEME window,HTHEME start,int role,const LOGFONTW *expected)
{
    LOGFONTW actual={0};
    HRESULT hr=query(window,start,role,&actual);
    if(FAILED(hr)||!equal(&actual,expected))
    {
        fprintf(stderr,"role %d: hr=%08lx face=%ls height=%ld weight=%ld italic=%u; expected face=%ls height=%ld weight=%ld italic=%u\n",
                role,(unsigned long)hr,actual.lfFaceName,actual.lfHeight,actual.lfWeight,actual.lfItalic,
                expected->lfFaceName,expected->lfHeight,expected->lfWeight,expected->lfItalic);
        failures++;
    }
}
int wmain(int argc,WCHAR **argv)
{
    HKEY key;
    LOGFONTW saved[7],baseline[7],expected[7],custom,actual;
    DWORD type,size,enabled,version;
    HTHEME window,start,ordinary;
    NONCLIENTMETRICSW n={0},changed;
    HDC dc;
    int dpi;
    WCHAR theme_path[MAX_PATH];DWORD path_size=sizeof(theme_path),path_type;
    HRESULT ordinary_hr;
    LOGFONTW ordinary_font={0};
    if(argc!=2||wcscmp(argv[1],L"--isolated-prefix"))return 2;
    if(!SetProcessDPIAware()||!GetEnvironmentVariableW(L"WIN4WINE_EXTERNAL_SHELL",NULL,0))return 2;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,key_name,0,KEY_READ|KEY_WRITE,&key))return 2;
    size=sizeof(enabled);
    if(RegQueryValueExW(key,L"Enabled",NULL,&type,(BYTE*)&enabled,&size)||type!=REG_DWORD||enabled!=1)return 2;
    size=sizeof(version);
    if(RegQueryValueExW(key,L"Version",NULL,&type,(BYTE*)&version,&size)||type!=REG_DWORD||version!=1)return 2;
    if(RegQueryValueExW(key,L"ThemePath",NULL,&path_type,(BYTE*)theme_path,&path_size)||path_type!=REG_SZ)return 2;
    for(int i=0;i<7;i++)
    {
        size=sizeof(saved[i]);
        if(RegQueryValueExW(key,names[i],NULL,&type,(BYTE*)&saved[i],&size)||type!=REG_BINARY||size!=sizeof(saved[i]))return 2;
    }
    dc=GetDC(NULL);dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(NULL,dc);
    window=OpenThemeData(NULL,L"Window");start=OpenThemeData(NULL,L"Start::Button");ordinary=OpenThemeData(NULL,L"Button");
    if(!window||!start||!ordinary||!IsThemeActive())return 2;
    ordinary_hr=GetThemeFont(ordinary,NULL,BP_PUSHBUTTON,PBS_NORMAL,TMT_FONT,&ordinary_font);
    dword(key,L"Enabled",0);
    for(int i=0;i<7;i++)check(SUCCEEDED(query(window,start,i,&baseline[i])),"disabled baseline API");
    dword(key,L"Enabled",1);
    check(SUCCEEDED(GetThemeSysFont((HTHEME)(ULONG_PTR)1,TMT_CAPTIONFONT,&actual))
          &&equal(&actual,&baseline[0]),"non-profile handle keeps Wine's original metric behavior");
    check(GetThemeFont(NULL,NULL,BP_PUSHBUTTON,PBS_NORMAL,TMT_FONT,&actual)==E_HANDLE,
          "NULL font handle keeps original error");
    for(int i=0;i<7;i++)
    {
        expected[i]=saved[i];expected[i].lfHeight=MulDiv(saved[i].lfHeight,dpi,96);
        compare(window,start,i,&expected[i]);
    }
    check(GetThemeFont(ordinary,NULL,BP_PUSHBUTTON,PBS_NORMAL,TMT_FONT,&actual)==ordinary_hr,"ordinary Button query unchanged");
    if(SUCCEEDED(ordinary_hr))check(equal(&actual,&ordinary_font),"ordinary Button font unchanged");
    /* A user-selected profile font wins over the defaults; no SPI write on GET. */
    custom=saved[0];lstrcpyW(custom.lfFaceName,L"Tahoma");custom.lfHeight=-15;custom.lfWeight=400;
    check(!RegSetValueExW(key,names[0],0,REG_BINARY,(BYTE*)&custom,sizeof(custom)),"write custom profile caption");
    custom.lfHeight=MulDiv(-15,dpi,96);compare(window,start,0,&custom);
    check(!RegSetValueExW(key,names[0],0,REG_BINARY,(BYTE*)&saved[0],sizeof(saved[0])),"restore caption");
    /* Invalid or missing data falls back to the unprofiled property. */
    check(!RegSetValueExW(key,names[6],0,REG_BINARY,(BYTE*)&saved[6],1),"write invalid font length");
    compare(window,start,6,&baseline[6]);
    check(!RegDeleteValueW(key,names[6]),"remove own profile start font");compare(window,start,6,&baseline[6]);
    check(!RegSetValueExW(key,names[6],0,REG_BINARY,(BYTE*)&saved[6],sizeof(saved[6])),"restore start font");
    for(int bad=0;bad<3;bad++)
    {
        custom=saved[6];
        if(bad==0)custom.lfHeight=16;
        if(bad==1)for(unsigned i=0;i<LF_FACESIZE;i++)custom.lfFaceName[i]='X';
        if(bad==2)memset(&custom,0xff,sizeof(custom));
        check(!RegSetValueExW(key,names[6],0,REG_BINARY,(BYTE*)&custom,sizeof(custom)),"write malformed LOGFONT");
        compare(window,start,6,&baseline[6]);
    }
    check(!RegSetValueExW(key,names[6],0,REG_SZ,(BYTE*)&saved[6],sizeof(saved[6])),"write wrong registry type");
    compare(window,start,6,&baseline[6]);
    check(!RegSetValueExW(key,names[6],0,REG_BINARY,(BYTE*)&saved[6],sizeof(saved[6])),"restore valid LOGFONT");
    dword(key,L"Version",99);for(int i=0;i<7;i++)compare(window,start,i,&baseline[i]);dword(key,L"Version",version);
    check(!RegSetValueExW(key,L"ThemePath",0,REG_SZ,(BYTE*)L"C:\\not-the-active-theme.msstyles",sizeof(L"C:\\not-the-active-theme.msstyles")),"write mismatching path");
    for(int i=0;i<7;i++)compare(window,start,i,&baseline[i]);
    check(!RegSetValueExW(key,L"ThemePath",0,path_type,(BYTE*)theme_path,path_size),"restore profile path");
    for(int setting=0;setting<2;setting++)
    {
        LPCWSTR value=setting?L"SizeName":L"ColorName";
        WCHAR saved_name[128];DWORD bytes=sizeof(saved_name),kind;
        check(!RegQueryValueExW(key,value,NULL,&kind,(BYTE*)saved_name,&bytes),"read theme selection");
        check(!RegSetValueExW(key,value,0,REG_SZ,(BYTE*)L"NoMatch",sizeof(L"NoMatch")),"write mismatching selection");
        for(int i=0;i<7;i++)compare(window,start,i,&baseline[i]);
        check(!RegSetValueExW(key,value,0,kind,(BYTE*)saved_name,bytes),"restore theme selection");
    }
    check(SetEnvironmentVariableW(L"WIN4WINE_EXTERNAL_SHELL",NULL),"disable process opt-in");
    for(int i=0;i<7;i++)compare(window,start,i,&baseline[i]);
    check(SetEnvironmentVariableW(L"WIN4WINE_EXTERNAL_SHELL",L"1"),"restore process opt-in");
    n.cbSize=sizeof(n);
    if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(n),&n,0))
    {
        changed=n;changed.lfCaptionFont=saved[0];changed.lfCaptionFont.lfHeight=MulDiv(-17,dpi,96);
        lstrcpyW(changed.lfCaptionFont.lfFaceName,L"Tahoma");
        check(SystemParametersInfoW(SPI_SETNONCLIENTMETRICS,sizeof(changed),&changed,0),"temporary user SPI caption");
        check(SUCCEEDED(GetThemeSysFont(NULL,TMT_CAPTIONFONT,&actual))&&equal(&actual,&changed.lfCaptionFont),"NULL theme keeps user SPI override");
        compare(window,start,0,&expected[0]);
        check(SystemParametersInfoW(SPI_SETNONCLIENTMETRICS,sizeof(n),&n,0),"restore user SPI metrics");
    }
    else check(FALSE,"read user SPI metrics");
    dword(key,L"Enabled",0);for(int i=0;i<7;i++)compare(window,start,i,&baseline[i]);dword(key,L"Enabled",enabled);
    CloseThemeData(ordinary);CloseThemeData(start);CloseThemeData(window);RegCloseKey(key);
    if(!failures)printf("PASS: profile fonts, DPI %d, user overrides, missing/invalid data, path/version/disabled fallback, SPI semantics\n",dpi);
    return failures?1:0;
}
