/* Store the active visual style's system fonts as the user's fonts.
 * Usage: theme-fonts
 *
 * Applying a visual style on Windows saves its fonts (GetThemeSysFont: the
 * caption, small caption, menu, status, message box and icon title fonts) as
 * the user's nonclient metrics and icon title font. Wine's uxtheme sets them
 * for the session without saving them, so a prefix made for Windows 7's
 * shell keeps Wine's Tahoma; prefix/win7.py runs this once the style is
 * active. It must run in the prefix's language: Windows 7's aero.msstyles
 * names its fonts in its MUI file ("メイリオ, 9" in Japanese). */
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
#include <string.h>

int wmain(void)
{
    static const int ids[] = { TMT_CAPTIONFONT, TMT_SMALLCAPTIONFONT, TMT_MENUFONT, TMT_STATUSFONT,
                               TMT_MSGBOXFONT, TMT_ICONTITLEFONT };
    LOGFONTW fonts[6];
    NONCLIENTMETRICSW metrics;
    HTHEME theme = OpenThemeData(NULL, L"Window");
    unsigned int i;

    if (!theme)
    {
        fprintf(stderr, "no active visual style\n");
        return 1;
    }
    for (i = 0; i < ARRAYSIZE(ids); i++)
    {
        if (FAILED(GetThemeSysFont(theme, ids[i], &fonts[i])))
        {
            fprintf(stderr, "the visual style has no font %d\n", ids[i]);
            return 1;
        }
        wprintf(L"%d %ld %ld %ls\n", ids[i], fonts[i].lfHeight, fonts[i].lfWeight, fonts[i].lfFaceName);
    }
    CloseThemeData(theme);
    memset(&metrics, 0, sizeof(metrics));
    /* Without iPaddedBorderWidth, SET keeps the padded border that Windows 7's
     * style set (the whole structure would store the padded border as 0). */
    metrics.cbSize = FIELD_OFFSET(NONCLIENTMETRICSW, iPaddedBorderWidth);
    /* the fonts are stored in 96 DPI units */
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0)) return 1;
    metrics.lfCaptionFont = fonts[0];
    metrics.lfSmCaptionFont = fonts[1];
    metrics.lfMenuFont = fonts[2];
    metrics.lfStatusFont = fonts[3];
    metrics.lfMessageFont = fonts[4];
    if (!SystemParametersInfoW(SPI_SETNONCLIENTMETRICS, metrics.cbSize, &metrics, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE) ||
        !SystemParametersInfoW(SPI_SETICONTITLELOGFONT, sizeof(fonts[5]), &fonts[5], SPIF_UPDATEINIFILE | SPIF_SENDCHANGE))
    {
        fprintf(stderr, "cannot save the fonts\n");
        return 1;
    }
    return 0;
}
