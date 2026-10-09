# Shell layout measurement

Build with `tests/build.sh`, then run `.local/bridge/shell-layout-test.exe`
on Wine or copy this self-authored executable to the reference XP machine.
An optional first argument is a text output filename, for example:

```text
shell-layout-test.exe C:\shell-layout.txt
```

To send output explicitly to the reference machine’s serial port instead, use
`shell-layout-test.exe COM1` (115200 baud, 8N1, no flow control, 2-second write
timeout), matching `reference-metrics`. No network connection is used.

The probe reads public window/client rectangles and styles, ReBar band
rectangles, header sizes and borders, and three rows of displayed pixels
around each child edge. Colors are Win32 COLORREF values (`00bbggrr`);
`ffffffff` means the pixel was outside the screen or unavailable. ReBar
messages above WM_USER require explicit target-process storage; only small
control-message data buffers are allocated, read and released. No executable
code or theme resources are read, written or exported.

A temporary standard window displays a synthetic solid 16×16 icon. The log
separately reports its actual painted pixels, the changed pixels when an ASCII
title is added, the HTSYSMENU input region, and GetTitleBarInfo. Rectangle ends
are exclusive. The text difference includes theme shadows and antialiasing;
it measures painted bounds, not the text layout rectangle. Compare runs with
the same theme, fonts, DPI, resolution and taskbar lock setting. Keep the
caption unobscured while the probe runs. It destroys its own window and icon;
it does not alter shell layout or persist settings.
