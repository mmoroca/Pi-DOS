#!/usr/bin/env python3
"""Check profile handoff, asset failures and stable port identities without hardware."""
from pathlib import Path
import configparser
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='pi-dos-midi.') as temporary:
    work = Path(temporary)
    wrapper = work / 'worker.c'
    wrapper.write_text('''
#define _GNU_SOURCE
#include "config.h"
#include "midi.h"
#include "midi_ports.h"
#include <time.h>
static int scans, sleeps;
static int test_resolve(const char *id, int *client, int *port) {
    const char *ready = getenv("TEST_MIDI_READY_AFTER");
    ++scans;
    if (!ready) return pidos_midi_resolve(id, client, port);
    if (strcmp(id, "pidos:0000000000000001") || scans < atoi(ready)) return 0;
    *client = 24; *port = 0; return 1;
}
static int test_sleep(const struct timespec *delay, struct timespec *remaining) {
    if (!getenv("TEST_MIDI_READY_AFTER")) return nanosleep(delay, remaining);
    if (delay->tv_sec || delay->tv_nsec != 100000000) abort();
    ++sleeps; return 0;
}
#define pidos_midi_resolve test_resolve
#define nanosleep test_sleep
#include "midi.c"
#undef pidos_midi_resolve
#undef nanosleep
int main(int argc, char **argv) {
    if (argc != 4) return 1;
    char first[23], again[23], other[23], label[80];
    pidos_midi_identity(-1, "USB MIDI", "Output", 0, first);
    pidos_midi_identity(-1, "USB MIDI", "Output", 0, again);
    pidos_midi_identity(-1, "USB MIDI", "Output", 1, other);
    if (strcmp(first, again) || !strcmp(first, other) || strlen(first) != 22) return 2;
    if (!pi_dos_runtime_config(argv[1], argv[2]) || !pi_dos_midi_prepare(argv[1], argv[2], argv[3])) return 3;
    const char *expected = getenv("TEST_MIDI_SCANS");
    if (expected && (scans != atoi(expected) || sleeps != (scans ? scans - 1 : 0))) return 4;
    pi_dos_midi_label(argv[1], label, sizeof(label)); puts(label);
    return 0;
}
''')
    worker = work / 'worker'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(ROOT/'src/init'), '-I'+str(ROOT/'src/common'),
                    str(wrapper), str(ROOT/'src/init/config.c'),
                    str(ROOT/'src/common/midi_profiles.c'), '-lasound', '-o', str(worker)], check=True)
    source = work / 'PIDOS.CFG'
    generated = work / 'runtime.conf'
    assets = ROOT / 'output/midi-assets'
    def check(profile, label, device, root=assets, extras=b'', mode=b'intelligent', ready_after=None, scans=0):
        original = (b'[pidos]\nmidi_profile='+profile.encode()+b'\n'+extras+
                    b'[midi]\nmpu401='+mode+b'\nmpubase=330\nmpuirq=-1\n'
                    b'[sblaster]\nsbtype=sb16\n[autoexec]\necho user startup\n')
        source.write_bytes(original)
        env = os.environ.copy()
        if ready_after is not None:
            env.update(TEST_MIDI_READY_AFTER=str(ready_after), TEST_MIDI_SCANS=str(scans))
        result = subprocess.run([str(worker), str(source), str(generated), str(root)],
                                check=True, capture_output=True, text=True, env=env)
        assert result.stdout.strip() == label, result
        parsed = configparser.ConfigParser(strict=False, allow_no_value=True)
        parsed.read(generated)
        assert parsed['midi']['mididevice'] == device
        assert b'[pidos]' not in generated.read_bytes()
        assert b'echo user startup\n' in generated.read_bytes()
        assert source.read_bytes() == original
        return parsed['midi']
    check('none', 'Disabled', 'none')
    for profile, label, filename in (('sc55','Roland SC-55','SC55.SF2'),('roland-gm','Roland GM','GMROLAND.SF2')):
        midi = check(profile, label, 'synth')
        assert midi['midiconfig'] == str(assets/filename)
        assert midi['mpu401'] == 'intelligent'
    for profile, label, model, directory in (('mt32','Roland MT-32','mt32','MT32'),('cm32l','Roland CM-32L','cm32l','CM32L')):
        midi = check(profile, label, 'mt32')
        assert midi['mt32.model'] == model and midi['mt32.romdir'] == str(assets/directory)
    empty = work / 'empty'; empty.mkdir()
    check('sc55', 'Missing SoundFont', 'none', empty)
    check('mt32', 'Missing MIDI ROMs', 'none', empty)
    destination = b'midi_port=pidos:0000000000000001\n'
    for ready in (1, 4, 51):
        midi = check('external', 'External USB MIDI', 'alsa', extras=destination, ready_after=ready, scans=ready)
        assert midi['midiconfig'] == 'pidos:0000000000000001'
        assert midi['mpu401'] == 'intelligent'
    check('external', 'USB MIDI unavailable', 'none', extras=destination, ready_after=52, scans=51)
    check('external', 'USB MIDI unavailable', 'none', ready_after=1)
    check('external', 'Disabled', 'none', extras=destination, mode=b'none', ready_after=1)
    check('unknown', 'Invalid MIDI profile', 'none')
    source.write_bytes(b'[midi]\nmpu401=uart\nmididevice=alsa\nmidiconfig=128:0\n')
    subprocess.run([str(worker), str(source), str(generated), str(assets)], check=True, capture_output=True)
    assert generated.read_bytes() == source.read_bytes(), 'Legacy advanced configuration must survive'

print('PASS: MIDI profiles, ROM/SF2 handoff, delayed USB detection, bounded retries, disabled MIDI, legacy settings and stable port identities.')
