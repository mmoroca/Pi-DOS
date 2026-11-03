#!/usr/bin/env python3
"""Check monitor handoff and LCD viewport bounds without a physical display."""
import configparser
import ctypes
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


class Viewport(ctypes.Structure):
    _fields_ = [(name, ctypes.c_int) for name in ('x', 'y', 'w', 'h')]


with tempfile.TemporaryDirectory(prefix='pi-dos-video.') as temporary:
    work = Path(temporary)
    wrapper = work / 'geometry.c'
    wrapper.write_text('''#include "lcd_video.h"
PidosViewport fit(int w, int h, int sw, int sh, int correct) {
    return pidos_lcd_fit(w, h, sw, sh, correct);
}
''')
    library = work / 'video.so'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                    '-I'+str(ROOT/'src/init'), '-I'+str(ROOT/'src/common'), str(wrapper),
                    str(ROOT/'src/init/video.c'), str(ROOT/'src/init/config.c'),
                    str(ROOT/'src/common/video_profiles.c'), '-o', str(library)], check=True)
    video = ctypes.CDLL(str(library))
    video.pi_dos_video_prepare.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    video.pi_dos_video_prepare.restype = ctypes.c_bool
    video.fit.argtypes = [ctypes.c_int] * 5
    video.fit.restype = Viewport
    libc = ctypes.CDLL(None)
    libc.getenv.argtypes = [ctypes.c_char_p]
    libc.getenv.restype = ctypes.c_char_p
    source, generated = work/'PIDOS.CFG', work/'video.conf'

    def prepare(contents):
        if contents is not None:
            source.write_bytes(contents)
        assert video.pi_dos_video_prepare(str(source).encode() if contents is not None else None,
                                          str(generated).encode())
        if contents is not None:
            assert source.read_bytes() == contents
        result = configparser.ConfigParser()
        result.read(generated)
        return result

    defaults = prepare(None)
    assert defaults['sdl']['fullresolution'] == 'original'
    assert defaults['render']['modeswitch'] == 'true'
    assert defaults['render']['scaler'] == 'none'
    assert libc.getenv(b'PI_DOS_LCD_MODE') is None
    assert libc.getenv(b'PI_DOS_LCD_ASPECT') is None
    assert libc.getenv(b'PI_DOS_LCD_LINEAR') is None
    assert libc.getenv(b'PI_DOS_LCD_HUD') is None

    # Every offered LCD mode is present in the checked-in EDID report.
    modes = ('640x480', '800x600', '832x624', '1024x768', '1280x720', '1920x1080')
    report = (ROOT/'dac-crt.txt').read_text()
    for mode in modes:
        assert mode in report
        result = prepare(('[pidos]\nmonitor=lcd\nlcd_mode='+mode+
                          '\nlcd_aspect=original\n').encode())
        assert result['sdl']['fullresolution'] == mode
        assert result['render']['scaler'] == 'none'
        assert result['render']['aspect'] == 'false'  # only the final viewport corrects aspect
        assert result['render']['doublescan'] == 'false'
        assert libc.getenv(b'PI_DOS_LCD_ASPECT') == b'original'
        assert libc.getenv(b'PI_DOS_LCD_LINEAR') == b'false'

    # Removed profile keys cannot re-enable effects in an existing configuration.
    for obsolete in (b'dark', b'black'):
        result = prepare(b'[pidos]\nmonitor=lcd\nlcd_scanlines='+obsolete+b'\n')
        assert result['render']['scaler'] == 'none'

    for value, enabled in ((b'true', b'true'), (b'TRUE', b'true'),
                           (b'false', b'false'), (b'broken', b'false')):
        prepare(b'[pidos]\nmonitor=lcd\nlcd_linear='+value+b'\n')
        assert libc.getenv(b'PI_DOS_LCD_LINEAR') == enabled
    for value, enabled in ((b'true', b'true'), (b'TRUE', b'true'),
                           (b'false', b'false'), (b'broken', b'false')):
        prepare(b'[pidos]\nmonitor=lcd\nlcd_hud='+value+b'\n')
        assert libc.getenv(b'PI_DOS_LCD_HUD') == enabled
    prepare(b'[pidos]\nmonitor=lcd\nlcd_hud=true\n')
    assert libc.getenv(b'PI_DOS_LCD_HUD') == b'true'
    prepare(b'[pidos]\nmonitor=crt\nlcd_hud=true\n')
    assert libc.getenv(b'PI_DOS_LCD_HUD') is None
    prepare(b'[pidos]\nmonitor=lcd\nlcd_linear=true\n')
    crt = prepare(b'[pidos]\nmonitor=crt\nlcd_mode=1280x720\nlcd_linear=true\n')
    assert crt['sdl']['fullresolution'] == 'original'
    assert crt['render']['modeswitch'] == 'true' and crt['render']['doublescan'] == 'true'
    assert crt['render']['scaler'] == 'none' and libc.getenv(b'PI_DOS_LCD_MODE') is None
    assert libc.getenv(b'PI_DOS_LCD_LINEAR') is None
    assert libc.getenv(b'PI_DOS_LCD_HUD') is None
    prepare(b'[pidos]\nmonitor=lcd\nlcd_linear=true\n')
    prepare(b'[render]\naspect=true\nscaler=normal3x\n')
    assert libc.getenv(b'PI_DOS_LCD_LINEAR') is None
    assert generated.read_bytes() == b''  # legacy advanced configuration has no overrides
    fallback = prepare(b'[pidos]\nmonitor=broken\nlcd_mode=9999x9999\nlcd_aspect=broken\nlcd_scanlines=broken\n')
    assert fallback['sdl']['fullresolution'] == 'original' and fallback['render']['scaler'] == 'none'
    assert libc.getenv(b'PI_DOS_LCD_LINEAR') is None
    fallback = prepare(b'[pidos]\nmonitor=lcd\nlcd_mode=9999x9999\nlcd_aspect=broken\n')
    assert fallback['sdl']['fullresolution'] == '1024x768'
    assert libc.getenv(b'PI_DOS_LCD_ASPECT') == b'4:3'
    assert libc.getenv(b'PI_DOS_LCD_LINEAR') == b'false'

    def fit(w, h, sw, sh, correct):
        result = video.fit(w, h, sw, sh, correct)
        return tuple(getattr(result, field) for field in ('x','y','w','h'))

    assert fit(1024,768,320,200,1) == (0,0,1024,768)
    assert fit(1024,768,320,200,0) == (0,64,1024,640)
    assert fit(1920,1080,320,200,1) == (240,0,1440,1080)
    assert fit(1920,1080,720,400,0) == (0,7,1920,1066)
    # High-resolution guests must fit smaller LCD outputs without cropping/overflow.
    for mode in modes:
        width, height = map(int, mode.split('x'))
        for sw, sh in ((320,200),(720,400),(640,350),(1024,768),(1600,1200),(1920,1080)):
            for correct in (0, 1):
                x,y,w,h = fit(width,height,sw,sh,correct)
                assert x >= 0 and y >= 0 and w > 0 and h > 0
                assert x+w <= width and y+h <= height
                assert abs((width-w)-2*x) <= 1 and abs((height-h)-2*y) <= 1
    assert not video.pi_dos_video_prepare(None, str(work/'missing/video.conf').encode())

print('PASS: CRT defaults, LCD/CRT filtering handoff, EDID modes, retired-option handling, legacy preservation, fallbacks and viewport bounds.')
