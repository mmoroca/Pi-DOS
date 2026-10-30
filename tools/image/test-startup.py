#!/usr/bin/env python3
"""Check CPU summary overrides and the 80-column boot table layout."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix='pi-dos-startup.') as directory:
    work = Path(directory)
    wrapper = work / 'startup.c'
    wrapper.write_text('''
#include "startup.h"
#include "mixer.h"
#include "timezone.h"
#include <string.h>
int main(int argc, char **argv) {
    if (argc == 4) {
        pi_dos_first_boot(stdout, argv[2], !strcmp(argv[3], "rw"), strcmp(argv[3], "none") != 0);
        return 0;
    }
    if (argc != 3) return 1;
    pi_dos_timezone(argv[2]);
    pi_dos_startup(stdout, argv[1], argv[2], "/dev/test", false, false, false);
    pi_dos_mixer(stdout, argv[2]);
    return 0;
}
''')
    binary = work / 'startup'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(ROOT / 'src/init'), '-I' + str(ROOT / 'src/common'),
                    str(wrapper), str(ROOT / 'src/init/startup.c'),
                    str(ROOT / 'src/init/mixer.c'), str(ROOT / 'src/init/config.c'),
                    str(ROOT / 'src/init/timezone.c'), str(ROOT / 'src/common/video_profiles.c'),
                    str(ROOT / 'src/init/midi.c'), str(ROOT / 'src/common/midi_profiles.c'),
                    str(ROOT / 'src/common/cpu_profiles.c'), '-lasound', '-o', str(binary)], check=True)
    defaults = ROOT / 'configs/pi-dos.conf'
    user = work / 'PIDOS.CFG'

    # Fresh installs run welcome/review only with persistent writable C:.
    welcome = (b'Z:\\BIN\\PIDOSCFG.EXE /FIRSTBOOT\n'
               b'IF ERRORLEVEL 10 CALL C:\\SYSTEM\\REBOOT.BAT\n')
    for state in (b'pending', b'clock', b'PENDING', b'done', b'invalid', None):
        user.write_bytes(b'[pidos]\n' + (b'first_boot=' + state + b'\n' if state else b''))
        for access in ('rw', 'ro', 'none'):
            output = subprocess.check_output([str(binary), str(defaults), str(user), access])
            assert output == (welcome if access == 'rw' and state in
                              (b'pending', b'clock', b'PENDING') else b'')
    user.unlink()
    assert not subprocess.check_output([str(binary), str(defaults), str(user), 'rw'])

    def check(config, cpu, cycles):
        user.write_bytes(config)
        output = subprocess.run([str(binary), str(defaults), str(user)],
                                check=True, capture_output=True).stdout
        lines = output.splitlines()
        date_row = next(line for line in lines if b' Date ' in line)
        time_row = next(line for line in lines if b' Time ' in line)
        assert lines.index(time_row) == lines.index(date_row) + 1
        assert date_row.index(b' Date ') < 38 and time_row.index(b' Time ') < 38
        assert b' DOS MIDI ' in time_row
        cpu_row = next(line for line in lines if b' DOS CPU ' in line)
        video_row = next(line for line in lines if b' DOS video ' in line)
        voodoo_row = next(line for line in lines if b' DOS Voodoo ' in line)
        memory_row = next(line for line in lines if b' DOS memory ' in line)
        assert lines.index(cpu_row) + 1 == lines.index(memory_row)
        assert lines.index(memory_row) + 1 == lines.index(video_row)
        assert lines.index(video_row) + 1 == lines.index(voodoo_row)
        sound_row = next(line for line in lines if b' DOS sound ' in line)
        assert lines.index(time_row) == lines.index(sound_row) + 1
        assert b'DOS audio' not in output
        assert b' DOS CPU    : ' + cpu in cpu_row
        region_row = next(line for line in lines if b'Regional ' in line)
        drives_row = next(line for line in lines if b'Drives ' in line)
        assert lines.index(region_row) == lines.index(drives_row) + 1
        assert b'DOS cycles' not in output
        assert b'~' not in cpu_row
        for line in lines:
            if line.startswith(b'ECHO \xba'):
                assert len(line[5:]) == 75, line
                assert b'|' not in line and b'>' not in line and b'<' not in line
        assert user.read_bytes() == config
        return output

    check(b'', b'Pentium 120 MHz', b'fixed 64180')
    for config, status in ((b'', b'Enabled'),
                           (b'[voodoo]\nvoodoo_card=false\n', b'Disabled'),
                           (b'[voodoo]\nvoodoo_card=software\n', b'Enabled')):
        output = check(config, b'Pentium 120 MHz', b'fixed 64180')
        assert b'DOS Voodoo : ' + status in output
    regional = check(b'[pidos]\ntimezone=Asia/Tokyo\n[config]\ncountry=1\n[dos]\nkeyboardlayout=us\n',
                     b'Pentium 120 MHz', b'fixed 64180')
    assert b'Regional   : Keyboard us / Country 1 / Asia/Tokyo' in regional
    for machine, label in ((b'cga',b'IBM CGA'),(b'hercules',b'Hercules'),(b'tandy',b'Tandy'),
                           (b'ega',b'IBM EGA'),(b'vgaonly',b'IBM VGA'),(b'svga_et4000',b'Tseng Labs ET4000'),
                           (b'svga_s3',b'S3 Trio64'),(b'svga_s3trio64',b'S3 Trio64'),(b'svga_dosbox',b'VESA VBE 3.0'),(b'svga_dosbox_vbe2',b'VESA VBE 2.0')):
        output = check(b'[dosbox]\nmachine='+machine+b'\n', b'Pentium 120 MHz', b'fixed 64180')
        assert b'DOS video  : ' + label in output
    check(b'[cpu]\ncycles=fixed 6075\n', b'Pentium ', b'fixed 6075')
    check(b'[cpu]\ncputype=auto\ncycles=auto\n', b'Auto (486/Pentium)', b'auto')
    for cycles, label in ((b'40072', b'Pentium 75 MHz'), (b'53430', b'Pentium 100 MHz'),
                          (b'64180', b'Pentium 120 MHz')):
        check(b'[cpu]\ncputype=pentium\ncycles=fixed ' + cycles + b'\n', label, b'fixed ' + cycles)
    check(b'[cpu]\ncputype=486\ncycles=fixed 23880\n', b'486DX2 66 MHz', b'fixed 23880')
    check(b'[cpu]\ncputype=486\ncycles=33445\n', b'486DX4 100 MHz', b'33445')
    check(b'[cpu]\ncputype=386\ncycles=fixed 9000\n', b'386 ', b'fixed 9000')
    result = check(b'[cpu]\ncputype=pentium_mmx\ncycles=fixed 95548\n',
                   b'Pentium MMX 166 MHz', b'fixed 95548')
    for settings, label in (
        (b'[sblaster]\nsbtype=none\noplmode=none\n', b'None'),
        (b'[sblaster]\nsbtype=none\noplmode=opl2\n', b'AdLib'),
        (b'[sblaster]\nsbtype=gb\noplmode=none\n', b'Game Blaster / CMS'),
        (b'[sblaster]\nsbtype=sb2.0\n', b'Sound Blaster 2.0'),
        (b'[sblaster]\nsbtype=sbpro2\n', b'Sound Blaster Pro 2'),
        (b'[sblaster]\nsbtype=none\noplmode=none\n[gus]\ngus=true\n', b'GUS Classic'),
        (b'[sblaster]\nsbtype=none\noplmode=none\n[speaker]\ntandy=on\n', b'Tandy sound')):
        summary = check(settings, b'Pentium 120 MHz', b'fixed 64180')
        assert b'DOS sound  : ' + label in summary
    for profile, label in ((b'none',b'Disabled'),(b'sc55',b'Roland SC-55'),
                           (b'roland-gm',b'Roland GM'),(b'mt32',b'Roland MT-32'),
                           (b'cm32l',b'Roland CM-32L'),(b'external',b'External USB MIDI')):
        summary = check(b'[pidos]\nmidi_profile='+profile+b'\n', b'Pentium 120 MHz', b'fixed 64180')
        assert b'DOS MIDI   : '+label in summary
    mixed = check(b'[pidos]\nmixer_master=65:75\nmixer_sb=0\nmixer_fm=201\n'
                  b'mixer_gus=10 & echo BAD\nmixer_cms=20:30:40\nmixer_speaker=50:60\nmixer_midi=80:90\n',
                  b'Pentium 120 MHz', b'fixed 64180')
    commands = [line for line in mixed.splitlines() if line.startswith(b'Z:')]
    assert commands == [b'Z:\\SYSTEM\\MIXER.COM /NOSHOW MASTER 65:75',
                        b'Z:\\SYSTEM\\MIXER.COM /NOSHOW SB 0:0',
                        b'Z:\\SYSTEM\\MIXER.COM /NOSHOW SPKR 50:60',
                        b'Z:\\SYSTEM\\MIXER.COM /NOSHOW SYNTH 80:90',
                        b'Z:\\SYSTEM\\MIXER.COM /NOSHOW MT32 80:90']
    assert all(len(line) < 126 for line in commands)
    output = ROOT / 'output/doscfg'
    output.mkdir(parents=True, exist_ok=True)
    result = check(b'[pidos]\ntimezone=Europe/Madrid\n', b'Pentium 120 MHz', b'fixed 64180')
    (output / 'boot-table-example.bat').write_bytes(result.replace(b'\n', b'\r\n'))

print('PASS: CPU/memory/video/Voodoo/sound/MIDI rows, Date/Time stacked on left, regional footer, presets and 75-column width.')
