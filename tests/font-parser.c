/* Self-authored theme strings; actual Wine parser is supplied by font_parser.py. */

int wmain(void){
 struct {const WCHAR *input,*face; LONG weight; BYTE italic,under,strike;} cases[]={
 {L"Tahoma, 8, Bold",L"Tahoma",700,0,0,0},
 {L"Tahoma, 8, Bold Italic",L"Tahoma",700,1,0,0},
 {L"Tahoma, 8, Italic Bold",L"Tahoma",700,1,0,0},
 {L"  Tahoma  , 8, Bold  , Italic  ",L"Tahoma",700,1,0,0},
 {L"Tahoma, 8, Bold, Italic, Underline, Strikeout",L"Tahoma",700,1,1,1},
 {L"Tahoma, 8, Bold Italic Underline Strikeout",L"Tahoma",700,1,1,1},
 {L"Tahoma, 8, regular",L"Tahoma",400,0,0,0}};
 int failed=0; HDC dc=GetDC(NULL);
 for(unsigned i=0;i<ARRAY_SIZE(cases);i++){
 THEME_PROPERTY prop={0};prop.lpValue=cases[i].input;prop.dwValueLen=lstrlenW(cases[i].input);LOGFONTW f={0};HRESULT hr=MSSTYLES_GetPropertyFont(&prop,dc,&f);
 int bad=FAILED(hr)||lstrcmpW(f.lfFaceName,cases[i].face)||f.lfHeight!=-MulDiv(8,GetDeviceCaps(dc,LOGPIXELSY),72)||f.lfWeight!=cases[i].weight||f.lfItalic!=cases[i].italic||f.lfUnderline!=cases[i].under||f.lfStrikeOut!=cases[i].strike;
 printf("fixture%u %s face=%ls height=%ld weight=%ld italic=%u underline=%u strike=%u\n",i,bad?"FAIL":"PASS",f.lfFaceName,f.lfHeight,f.lfWeight,f.lfItalic,f.lfUnderline,f.lfStrikeOut);failed+=bad;
 }

 /* A property span is length-delimited and need not have a trailing NUL. */
 SYSTEM_INFO system; GetSystemInfo(&system);
 BYTE *pages=VirtualAlloc(NULL,system.dwPageSize*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 DWORD previous; WCHAR token[32]; const WCHAR bounded[]=L"Tahoma  ";
 if(!pages||!VirtualProtect(pages+system.dwPageSize,system.dwPageSize,PAGE_NOACCESS,&previous))return 2;
 WCHAR *span=(WCHAR *)(pages+system.dwPageSize-sizeof(bounded)+sizeof(WCHAR));
 memcpy(span,bounded,sizeof(bounded)-sizeof(WCHAR));
 BOOL ok=MSSTYLES_GetNextToken(span,(WCHAR *)(pages+system.dwPageSize),NULL,token,ARRAY_SIZE(token));
 if(!ok||lstrcmpW(token,L"Tahoma")){fprintf(stderr,"bounded token FAIL\n");failed++;}
 else puts("bounded token PASS");
 VirtualFree(pages,0,MEM_RELEASE);
 ReleaseDC(NULL,dc);return failed?1:0;
}
