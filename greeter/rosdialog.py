"""Draw ReactOS's logon screen (msgina's dialogs) with cairo and Pango.

The dialogs come from ReactOS's msgina.dll at run time: their templates
(the controls' text and places, in the chosen language), the banner and its
bar. They are drawn as ReactOS draws them with its classic look (no visual
style, the "Windows Standard" colors, Tahoma), over ReactOS's wallpaper.
"""
import os
from pathlib import Path

import cairo
import gi
gi.require_version('Pango', '1.0')
gi.require_version('PangoCairo', '1.0')
from gi.repository import Pango, PangoCairo

from xpres import Resources
from welcome import Bitmap, fontconfig

CONFIG = Path('/etc/win4wine/reactos-greeter.json')
REQUIRED = ('system32/msgina.dll', 'wallpaper/Nebula.bmp', 'fonts/tahoma.ttf', 'fonts/tahomabd.ttf')
LANGUAGES = {'en': 0x409, 'ja': 0x411}

# ReactOS's default classic colors (win32ss/gdi/ntgdi/stockobj.c).
FACE, HIGHLIGHT, SHADOW, DARK = (212, 208, 200), (255, 255, 255), (128, 128, 128), (64, 64, 64)
WINDOW, TEXT, GRAY = (255, 255, 255), (0, 0, 0), (128, 128, 128)
CAPTION, CAPTION_END, CAPTION_TEXT = (10, 36, 106), (166, 202, 240), (255, 255, 255)
SELECTION = (10, 36, 106)
CAPTION_HEIGHT, FRAME = 18, 3   # SM_CYCAPTION, SM_CXDLGFRAME (classic)

IDD_WELCOME, IDD_LOGON, IDD_SHUTDOWN = 1100, 1200, 1800
IDC_USERNAME, IDC_PASSWORD, IDC_DOMAIN, IDC_SHUTDOWN = 1201, 1202, 1203, 1204
IDC_ACTION, IDC_DESCRIPTION = 2001, 2002
IDS_SHUTDOWN, IDS_RESTART, IDS_SLEEP = 50001, 50002, 50003
IDS_SHUTDOWN_DESC, IDS_RESTART_DESC, IDS_SLEEP_DESC = 50006, 50007, 50008
IDS_LOGONTITLE, IDS_WRONG = 40015, 40016
IDI_KEYBOARD, IDI_SHUTDOWN = 109, 22


def setup(cache):
    """The configuration and Assets, with fontconfig pointed at ReactOS's fonts."""
    import json
    config = json.loads(CONFIG.read_text())
    root = Path(config['assets'])
    missing = [name for name in REQUIRED if not (root / name).is_file()]
    if missing:
        raise FileNotFoundError('missing ' + ', '.join(missing))
    os.environ['FONTCONFIG_FILE'] = str(fontconfig(root / 'fonts', Path(cache)))
    return config, Assets(root, config.get('lang', 'en'))


def muldiv(a, b, c):
    """MulDiv: a * b / c rounded half away from zero (dialog units to pixels)."""
    return (a * b + c // 2) // c


def rgb(cr, color):
    cr.set_source_rgb(*(c / 255 for c in color))


def fill(cr, color, x, y, w, h):
    rgb(cr, color)
    cr.rectangle(x, y, w, h)
    cr.fill()


def edge(cr, x, y, w, h, outer, inner):
    """A classic two-line 3D border; OUTER and INNER are (top-left, bottom-right) colors."""
    for (light, dark), inset in ((outer, 0), (inner, 1)):
        fill(cr, light, x + inset, y + inset, w - 2 * inset - 1, 1)
        fill(cr, light, x + inset, y + inset, 1, h - 2 * inset - 1)
        fill(cr, dark, x + inset, y + h - inset - 1, w - 2 * inset, 1)
        fill(cr, dark, x + w - inset - 1, y + inset, 1, h - 2 * inset)


RAISED = ((HIGHLIGHT, DARK), (FACE, SHADOW))        # buttons
FRAMED = ((FACE, DARK), (HIGHLIGHT, SHADOW))        # window frames
SUNKEN = ((SHADOW, HIGHLIGHT), (DARK, FACE))        # edits, combo boxes
PUSHED = ((DARK, HIGHLIGHT), (SHADOW, FACE))


def focus_rect(cr, x, y, w, h):
    """DrawFocusRect: a dotted rectangle, every other pixel."""
    rgb(cr, TEXT)
    for i in range(w):
        if i % 2 == 0:
            cr.rectangle(x + i, y, 1, 1)
            cr.rectangle(x + i, y + h - 1, 1, 1)
    for j in range(h):
        if j % 2 == 0:
            cr.rectangle(x, y + j, 1, 1)
            cr.rectangle(x + w - 1, y + j, 1, 1)
    cr.fill()


def icon_image(resources, number, size=32):
    """The largest 32-bit image of ICON group NUMBER at SIZE, as a Bitmap, or None."""
    import struct
    group = resources.find(14, number)
    if group is None:
        return None
    count = struct.unpack_from('<H', group, 4)[0]
    best = None
    for i in range(count):
        width, height, _, _, _, bits, _, ident = struct.unpack_from('<BBBBHHIH', group, 6 + 14 * i)
        width, height = width or 256, height or 256
        if width == size and (best is None or bits > best[0]):
            best = (bits, ident)
    if best is None:
        return None
    data = resources.find(3, best[1])
    if data is None or data[:4] == b'\x89PNG':
        return None
    header, width, height, _, bits = struct.unpack_from('<IiiHH', data)
    height //= 2
    if bits != 32:
        return None
    # An icon's DIB holds the colors over the AND mask; with 32 bits its alpha is enough.
    dib = bytearray(data[:header + 4 * width * height])
    struct.pack_into('<i', dib, 8, height)
    return Bitmap(bytes(dib), alpha=True)


class Assets:
    """ReactOS's files that the logon screen reads (greeter/install.sh copies them)."""

    def __init__(self, root, lang):
        self.root = Path(root)
        self.language = LANGUAGES.get(lang, 0x409)
        self.msgina = Resources(self.root / 'system32/msgina.dll')
        self.banner = Bitmap(self.msgina.bitmap(20000))
        self.bar = Bitmap(self.msgina.bitmap(20001))
        self.wallpaper = Bitmap((self.root / 'wallpaper/Nebula.bmp').read_bytes())
        self.icons = {n: icon_image(self.msgina, n) for n in (IDI_KEYBOARD, IDI_SHUTDOWN)}

    def dialog(self, number):
        return self.msgina.dialog(number, self.language)

    def string(self, number):
        import struct
        block = self.msgina.find(6, number // 16 + 1, self.language)
        if block is None:
            return ''
        position = 0
        for _ in range(number % 16):
            position += 2 + 2 * struct.unpack_from('<H', block, position)[0]
        length = struct.unpack_from('<H', block, position)[0]
        return block[position + 2:position + 2 + 2 * length].decode('utf-16-le')


class Control:
    def __init__(self, kind, text, ident, rect, style):
        self.kind, self.text, self.ident, self.rect, self.style = kind, text, ident, rect, style


class Screen:
    """The logon screen's state; render() draws it."""

    def __init__(self, assets, user='', domain=''):
        self.assets = assets
        self.page = 'welcome'          # welcome, logon, shutdown
        self.user, self.password, self.domain = user, '', domain
        self.focus = IDC_PASSWORD if user else IDC_USERNAME
        self.caret = True
        self.pressed = None
        self.message = None            # (title, text) of a message box over the logon dialog
        self.actions = [(IDS_SHUTDOWN, IDS_SHUTDOWN_DESC), (IDS_RESTART, IDS_RESTART_DESC)]
        self.action = 0
        self.can_sleep = False
        face = assets.dialog(IDD_LOGON)['font']
        ja = face and face[0] == 'MS UI Gothic'
        self.family = 'Tahoma, Droid Sans Fallback' if ja else 'Tahoma'
        self.px = round((face[1] if face else 8) * 96 / 72)
        self.base = self.base_units()

    # Text -----------------------------------------------------------

    def layout(self, cr, text, bold=False):
        layout = PangoCairo.create_layout(cr)
        options = cairo.FontOptions()
        options.set_antialias(cairo.ANTIALIAS_NONE)
        options.set_hint_style(cairo.HINT_STYLE_FULL)
        PangoCairo.context_set_font_options(layout.get_context(), options)
        font = Pango.FontDescription(self.family)
        font.set_absolute_size(self.px * Pango.SCALE)
        if bold:
            font.set_weight(Pango.Weight.BOLD)
        layout.set_font_description(font)
        mnemonic = None
        if '&' in text:
            plain, i = '', 0
            while i < len(text):
                if text[i] == '&' and i + 1 < len(text):
                    if text[i + 1] != '&' and mnemonic is None:
                        mnemonic = len(plain.encode())
                    i += 1
                plain += text[i]
                i += 1
            text = plain
        layout.set_text(text, -1)
        if mnemonic is not None:
            attributes = Pango.AttrList()
            underline = Pango.attr_underline_new(Pango.Underline.SINGLE)
            underline.start_index, underline.end_index = mnemonic, mnemonic + len(text.encode()[mnemonic:mnemonic + 1])
            end = mnemonic + 1
            while end < len(text.encode()) and (text.encode()[end] & 0xc0) == 0x80:
                end += 1
            underline.end_index = end
            attributes.insert(underline)
            layout.set_attributes(attributes)
        return layout

    def base_units(self):
        """GdiGetCharDimensions of the dialog font: the dialog units' base in pixels."""
        surface = cairo.ImageSurface(cairo.FORMAT_RGB24, 8, 8)
        layout = self.layout(cairo.Context(surface), 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz')
        width = layout.get_pixel_extents()[1].width
        return (width // 26 + 1) // 2, self.text_height()

    def text_height(self):
        """tmHeight of the dialog font: Tahoma's usWinAscent + usWinDescent at its pixel size."""
        import struct
        data = (self.assets.root / 'fonts/tahoma.ttf').read_bytes()
        tables = {data[12 + 16 * i:16 + 16 * i]: struct.unpack_from('>I', data, 20 + 16 * i)[0]
                  for i in range(struct.unpack_from('>H', data, 4)[0])}
        units = struct.unpack_from('>H', data, tables[b'head'] + 18)[0]
        ascent, descent = struct.unpack_from('>HH', data, tables[b'OS/2'] + 74)
        # The ascent and the descent are rounded on their own, as GDI does.
        return round(ascent * self.px / units) + round(descent * self.px / units)

    def text(self, cr, text, x, y, color=TEXT, bold=False, width=None, height=None, align='left', wrap=False):
        layout = self.layout(cr, text, bold)
        if wrap and width:
            layout.set_width(width * Pango.SCALE)
            layout.set_wrap(Pango.WrapMode.WORD_CHAR)
        extent = layout.get_pixel_extents()[1]
        if align == 'center' and width is not None:
            x += (width - extent.width) // 2
        if height is not None and not wrap:
            y += (height - extent.height) // 2
        rgb(cr, color)
        cr.move_to(x, y)
        PangoCairo.show_layout(cr, layout)
        return extent.width

    # Layout ---------------------------------------------------------

    def dialog_number(self):
        return {'welcome': IDD_WELCOME, 'logon': IDD_LOGON, 'shutdown': IDD_SHUTDOWN}[self.page]

    def controls(self):
        template = self.assets.dialog(self.dialog_number())
        bx, by = self.base
        result = []
        for kind, text, ident, x, y, w, h, style in template['controls']:
            rect = (muldiv(x, bx, 4), muldiv(y, by, 8), muldiv(w, bx, 4), muldiv(h, by, 8))
            result.append(Control(kind.lower(), text, ident & 0xffff, rect, style))
        return template, result

    def geometry(self, width, height):
        """The dialog's outer rectangle and its client origin, centred on the screen (DS_CENTER)."""
        template, _ = self.controls()
        bx, by = self.base
        client_w, client_h = muldiv(template['size'][0], bx, 4), muldiv(template['size'][1], by, 8)
        outer_w, outer_h = client_w + 2 * FRAME, client_h + 2 * FRAME + CAPTION_HEIGHT + 1
        # Measured on ReactOS 0.4.16 at 1024x768: its logon dialogs sit 14 pixels above the centre.
        x, y = (width - outer_w) // 2, max(0, (height - outer_h) // 2 - 14)
        return (x, y, outer_w, outer_h), (x + FRAME, y + FRAME + CAPTION_HEIGHT + 1), template['caption']

    def hit(self, width, height, px, py):
        """The id of the enabled control at the point, or None."""
        _, (cx, cy), _ = self.geometry(width, height)
        _, controls = self.controls()
        for c in controls:
            x, y, w, h = c.rect
            if c.kind == 'combobox':
                h = self.base[1] + 8   # the selection field, not the list's height in the template
            if c.kind in ('edit', 'button', 'combobox') and cx + x <= px < cx + x + w and cy + y <= py < cy + y + h:
                return c.ident
        return None

    def tab_order(self):
        _, controls = self.controls()
        return [c.ident for c in controls if c.kind in ('edit', 'combobox', 'button')]

    # Drawing --------------------------------------------------------

    def render(self, width, height):
        surface = cairo.ImageSurface(cairo.FORMAT_RGB24, width, height)
        cr = cairo.Context(surface)
        self.wallpaper(cr, width, height)
        if self.page:
            self.draw_dialog(cr, width, height)
        if self.message:
            self.draw_message(cr, width, height)
        surface.flush()
        return surface

    def wallpaper(self, cr, width, height):
        """ReactOS's default: the wallpaper filling the screen, cropped (WallpaperStyle 10)."""
        image = self.assets.wallpaper
        scale = max(width / image.width, height / image.height)
        cr.save()
        cr.translate((width - image.width * scale) / 2, (height - image.height * scale) / 2)
        cr.scale(scale, scale)
        cr.set_source_surface(image.surface, 0, 0)
        cr.get_source().set_filter(cairo.FILTER_GOOD)
        cr.paint()
        cr.restore()

    def window(self, cr, x, y, w, h, caption):
        """A classic dialog window: raised frame, gradient active caption, bold caption text."""
        fill(cr, FACE, x, y, w, h)
        edge(cr, x, y, w, h, *FRAMED)
        gradient = cairo.LinearGradient(x + FRAME, 0, x + w - FRAME, 0)
        gradient.add_color_stop_rgb(0, *(c / 255 for c in CAPTION))
        gradient.add_color_stop_rgb(1, *(c / 255 for c in CAPTION_END))
        cr.set_source(gradient)
        cr.rectangle(x + FRAME, y + FRAME, w - 2 * FRAME, CAPTION_HEIGHT)
        cr.fill()
        self.text(cr, caption, x + FRAME + 2, y + FRAME, CAPTION_TEXT, bold=True, height=CAPTION_HEIGHT)

    def draw_dialog(self, cr, width, height):
        (x, y, w, h), (cx, cy), caption = self.geometry(width, height)
        template, controls = self.controls()
        self.window(cr, x, y, w, h, caption)
        for c in controls:
            px, py, pw, ph = c.rect
            px += cx
            py += cy
            if c.kind == 'static' and c.text == 20000:
                cr.set_source_surface(self.assets.banner.surface, px, py)
                cr.paint()
            elif c.kind == 'static' and c.text == 20001:
                cr.set_source_surface(self.assets.bar.surface, px, py)
                cr.paint()
            elif c.kind == 'static' and isinstance(c.text, int):
                icon = self.assets.icons.get(c.text)
                if icon:
                    cr.set_source_surface(icon.surface, px, py)
                    cr.paint()
            elif c.kind == 'static':
                text = c.text
                if c.ident == IDC_DESCRIPTION:
                    text = self.assets.string(self.actions[self.action][1])
                self.text(cr, text, px, py, width=pw, wrap=c.ident == IDC_DESCRIPTION)
            elif c.kind == 'edit':
                fill(cr, WINDOW, px, py, pw, ph)
                edge(cr, px, py, pw, ph, *SUNKEN)
                value = self.user if c.ident == IDC_USERNAME else '*' * len(self.password)
                # A single-line edit's text starts below its 2-pixel border and 1-pixel margin.
                end = self.text(cr, value, px + 4, py + 3) if value else 0
                if self.focus == c.ident and self.caret and not self.message:
                    fill(cr, TEXT, px + 4 + end, py + 3, 1, self.base[1])
            elif c.kind == 'combobox':
                ph = self.base[1] + 8   # the selection field's height
                fill(cr, WINDOW, px, py, pw, ph)
                edge(cr, px, py, pw, ph, *SUNKEN)
                arrow = ph - 4
                bx = px + pw - 2 - arrow
                fill(cr, FACE, bx, py + 2, arrow, arrow)
                edge(cr, bx, py + 2, arrow, arrow, *RAISED)
                rgb(cr, TEXT)
                ax, ay = bx + arrow // 2, py + 2 + arrow // 2 + 2
                for i in range(4):
                    cr.rectangle(ax - 3 + i, ay - 2 + i, 7 - 2 * i, 1)
                cr.fill()
                value = self.domain if c.ident == IDC_DOMAIN else self.assets.string(self.actions[self.action][0])
                field_w = pw - arrow - 6
                if self.focus == c.ident and not self.message:
                    fill(cr, SELECTION, px + 3, py + 3, field_w, ph - 6)
                    self.text(cr, value, px + 4, py, HIGHLIGHT, height=ph)
                    focus_rect(cr, px + 3, py + 3, field_w, ph - 6)
                else:
                    self.text(cr, value, px + 4, py, height=ph)
            elif c.kind == 'button':
                default = c.style & 1   # BS_DEFPUSHBUTTON
                pressed = self.pressed == c.ident
                fill(cr, FACE, px, py, pw, ph)
                if default or self.focus == c.ident:
                    rgb(cr, TEXT)
                    cr.rectangle(px + 0.5, py + 0.5, pw - 1, ph - 1)
                    cr.set_line_width(1)
                    cr.stroke()
                    edge(cr, px + 1, py + 1, pw - 2, ph - 2, *(PUSHED if pressed else RAISED))
                else:
                    edge(cr, px, py, pw, ph, *(PUSHED if pressed else RAISED))
                shift = 1 if pressed else 0
                self.text(cr, c.text, px + shift, py + shift, width=pw, height=ph, align='center')
                if self.focus == c.ident and not self.message:
                    focus_rect(cr, px + 4, py + 4, pw - 8, ph - 8)

    def draw_message(self, cr, width, height):
        """A message box (MB_ICONERROR style without the icon) over the logon dialog."""
        title, text = self.message
        bx, by = self.base
        w = 70 * bx
        probe = self.layout(cr, text)
        probe.set_width((w - 24) * Pango.SCALE)
        probe.set_wrap(Pango.WrapMode.WORD_CHAR)
        text_h = probe.get_pixel_extents()[1].height
        h = 2 * FRAME + CAPTION_HEIGHT + 1 + 12 + text_h + 14 + 23 + 12
        x, y = (width - w) // 2, (height - h) // 2
        self.window(cr, x, y, w, h, title)
        top = y + FRAME + CAPTION_HEIGHT + 1 + 12
        self.text(cr, text, x + 12, top, width=w - 24, wrap=True)
        button_w, button_h = 75, 23
        ox, oy = x + (w - button_w) // 2, top + text_h + 14
        fill(cr, FACE, ox, oy, button_w, button_h)
        rgb(cr, TEXT)
        cr.rectangle(ox + 0.5, oy + 0.5, button_w - 1, button_h - 1)
        cr.set_line_width(1)
        cr.stroke()
        edge(cr, ox + 1, oy + 1, button_w - 2, button_h - 2, *RAISED)
        self.text(cr, 'OK', ox, oy, width=button_w, height=button_h, align='center')
        focus_rect(cr, ox + 4, oy + 4, button_w - 8, button_h - 8)

    def background(self, width, height):
        """The wallpaper alone, left on the root window while the session starts."""
        surface = cairo.ImageSurface(cairo.FORMAT_RGB24, width, height)
        self.wallpaper(cairo.Context(surface), width, height)
        surface.flush()
        return surface
