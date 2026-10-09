/* Public uxtheme font regression; the active msstyles stays user supplied. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
int wmain(void)
{
    HDC dc=GetDC(NULL);
    HTHEME theme=OpenThemeData(NULL,L"StartPanel");
    int part,checked=0,failed=0;
    if(!dc||!theme) { fputs("requires an active StartPanel theme\n",stderr); return 2; }
    for(part=1;part<=11;++part)
    {
        LOGFONTW with_dc={0},without_dc={0};
        HRESULT a=GetThemeFont(theme,dc,part,0,TMT_FONT,&with_dc);
        HRESULT b=GetThemeFont(theme,NULL,part,0,TMT_FONT,&without_dc);
        if(a!=b) { fprintf(stderr,"part %d: NULL DC changes result %lx/%lx\n",part,a,b);failed=1;continue; }
        if(FAILED(a))continue;
        ++checked;
        if(with_dc.lfHeight>=0||without_dc.lfHeight!=with_dc.lfHeight
           ||lstrcmpW(with_dc.lfFaceName,without_dc.lfFaceName)
           ||with_dc.lfWeight!=without_dc.lfWeight||with_dc.lfItalic!=without_dc.lfItalic)
        {
            fprintf(stderr,"part %d: font with DC height=%ld, NULL DC height=%ld\n",part,with_dc.lfHeight,without_dc.lfHeight);
            failed=1;
        }
    }
    CloseThemeData(theme);ReleaseDC(NULL,dc);
    if(!checked){fputs("active theme has no StartPanel fonts\n",stderr);return 2;}
    if(!failed)printf("PASS: %d StartPanel fonts have matching valid metrics with optional NULL DC\n",checked);
    return failed ? 1 : 0;
}
