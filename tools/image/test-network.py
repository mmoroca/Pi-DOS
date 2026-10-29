#!/usr/bin/env python3
"""Check production network startup and optionally the ARM64 IPX/SLIRP runtime."""
import argparse
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime', action='store_true')
parser.add_argument('--sdl-library-path', type=Path)
args = parser.parse_args()
base = ROOT / 'output/smoke'
base.mkdir(parents=True, exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='network.', dir=base))
wrapper = work / 'generate.c'
wrapper.write_text('''
#include "dos_network.h"
int main(int argc, char **argv) {
 if(argc!=6) return 1;
 FILE *out=fopen(argv[5],"w");
 if(!out) return 2;
 int good=pi_dos_dos_network(argv[1],argv[2],argv[3],argv[4][0]=='1',out);
 return fclose(out) || !good;
}
''')
generator = work / 'generate'
subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-Isrc/init', '-Isrc/common', str(wrapper), 'src/init/dos_network.c',
                'src/init/config.c', '-o', str(generator)], cwd=ROOT, check=True)
config = work / 'PIDOS.CFG'
drivez = work / 'drivez'; drivez.mkdir()
data = work / 'data'; (data / 'SYSTEM').mkdir(parents=True)
autoexec = work / 'GENERATE.BAT'

def generate(text, writable=True):
    config.write_text(text)
    subprocess.run([str(generator), str(config), str(drivez), str(data),
                    str(int(writable)), str(autoexec)], check=True)
    assert config.read_text() == text
    return (drivez / 'NETWORK/NETUP.BAT').read_text()

assert 'CALL' not in generate('') and 'CALL' not in autoexec.read_text()
for mode, command in [('server', 'STARTSERVER 3213'), ('client', 'CONNECT 127.0.0.1 3213')]:
    up = generate('[ipx]\nipx=true\n[pidos]\nipx_mode='+mode+'\nipx_server=127.0.0.1\nipx_port=3213\n')
    assert command in up and 'CALL Z:\\NETWORK\\NETUP.BAT' in autoexec.read_text()
up = generate('[ne2000]\nne2000=true\n[pidos]\nnet_startup=dhcp\n')
assert 'NE2000.COM 0x60 10 0x300' in up and '-retries 2 -timeout 5' in up
assert 'packetint 0x60' in (data / 'SYSTEM/NET/MTCP.CFG').read_text()
up = generate('[ne2000]\nne2000=true\n[pidos]\nnet_startup=static\nnet_ip=192.168.2.10\n')
assert 'DHCP.EXE' not in up
assert 'IPADDR 192.168.2.10' in (data / 'SYSTEM/NET/MTCP.CFG').read_text()
up = generate('[ne2000]\nne2000=true\nbackend=pcap\n[pidos]\nnet_startup=native-ipx\n')
assert 'IPXODI.COM' in up and 'NE2000.COM 0x60' not in up
assert 'FRAME Ethernet_802.2' in (data / 'SYSTEM/NET/NET.CFG').read_text()
up = generate('[ne2000]\nne2000=true\nbackend=pcap\n[pidos]\nnet_startup=windows\n')
assert 'NE2000.COM 0x60' not in up
wfw = (drivez / 'NETWORK/WFWNET.BAT').read_text()
assert 'DEVICE.COM C:\\WINDOWS\\IFSHLP.SYS' in wfw and 'C:\\WINDOWS\\NET START' in wfw
for settings in ('ipx_server=host&EXIT\nipx_mode=client', 'ipx_port=0\nipx_mode=server',
                 'net_startup=static\nnet_ip=300.1.2.3',
                 'net_startup=native-ipx\nnet_ipx_path=C:\\IPX&EXIT'):
    up = generate('[ne2000]\nne2000=true\n[ipx]\nipx=true\n[pidos]\n'+settings+'\n')
    assert 'Invalid Network settings' in up and '&EXIT' not in up
up = generate('[ne2000]\nne2000=true\n[pidos]\nnet_startup=dhcp\n', False)
assert 'needs writable C:' in up and 'DHCP.EXE' not in up
print('PASS: generated IPX, TCP/IP, native IPX and Windows startup; invalid values and read-only storage.')

if args.runtime:
    if not args.sdl_library_path:
        parser.error('--runtime needs --sdl-library-path with ARM64 dummy SDL2')
    qemu = ROOT / 'output/tools/qemu-user/usr/bin/qemu-aarch64'
    runtime = ROOT / 'output/buildroot-arm64/target/pi-dos/pi-dos'
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    for key in ('PI_DOS_SUPERVISED', 'PI_DOS_SYSTEM_STORAGE', 'PI_DOS_USB_FLOPPY',
                'PI_DOS_USB_CDROM', 'PI_DOS_USB_STORAGE'):
        env.pop(key, None)
    def command(directory, settings):
        return [str(qemu), '-L', str(ROOT / 'output/buildroot-arm64/target'),
                '-E', 'LD_LIBRARY_PATH='+str(args.sdl_library_path.resolve()), str(runtime),
                '-defaultconf', '-defaultmapper', '-nopromptfolder', '-defaultdir', str(directory),
                '-fastlaunch', '-nomenu', '-nogui', '-time-limit', '35',
                '-set', 'sdl output=surface', '-set', 'sdl videodriver=dummy',
                '-set', 'sdl waitonerror=false', '-set', 'cpu core=normal',
                '-set', 'cpu cycles=fixed 20000', '-set', 'midi mpu401=none',
                *sum((['-set', s] for s in settings), []),
                '-c', 'mount c "'+str(directory)+'"', '-c', 'c:', '-c', 'TEST.BAT']
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', 0)); port = sock.getsockname()[1]
    server = work / 'server'; server.mkdir()
    client = work / 'client'; client.mkdir()
    assembly = server / 'WAIT.ASM'
    assembly.write_text('''bits 16
org 100h
mov ah,0
int 1ah
mov bx,dx
again:
mov ah,0
int 1ah
sub dx,bx
cmp dx,218
jb again
mov ax,4c00h
int 21h
''')
    subprocess.run(['nasm', '-f', 'bin', str(assembly), '-o', str(server / 'WAIT.COM')], check=True)
    (server / 'TEST.BAT').write_text('@ECHO OFF\nIPXNET STARTSERVER '+str(port)+
        ' >SERVER.OUT\nECHO READY>READY.OUT\nWAIT.COM\nIPXNET STOPSERVER\nEXIT\n')
    (client / 'TEST.BAT').write_text('@ECHO OFF\nIPXNET CONNECT 127.0.0.1 '+str(port)+
        ' >CLIENT.OUT\nIPXNET STATUS >>CLIENT.OUT\nIPXNET PING >>CLIENT.OUT\nEXIT\n')
    with (server / 'run.log').open('w') as log:
        host = subprocess.Popen(command(server, ['ipx ipx=true']), env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic()+20
            while not (server / 'READY.OUT').exists():
                if host.poll() is not None or time.monotonic()>deadline:
                    raise RuntimeError('IPX server did not start: '+str(server / 'run.log'))
                time.sleep(.1)
            with (client / 'run.log').open('w') as peerlog:
                subprocess.run(command(client, ['ipx ipx=true']), env=env,
                               stdout=peerlog, stderr=subprocess.STDOUT, check=True, timeout=45)
            output = (client / 'CLIENT.OUT').read_bytes()
            assert b'Client connected' in output and b'Response from' in output, output
            # The server deliberately stays alive while the peer runs.
            host.kill()
            host.wait(timeout=5)
        finally:
            if host.poll() is None: host.kill(); host.wait()
    print('PASS: two actual ARM64 runtime instances connect and exchange IPX tunnel pings over UDP.')
    # Exercise the actual generated launcher, packet driver and mTCP DHCP under SLIRP.
    guest = work / 'slirp'; guest.mkdir()
    (guest / 'SYSTEM').mkdir()
    guestz = guest / 'drivez'; guestz.mkdir()
    subprocess.run(['python3', str(ROOT / 'tools/dos/network-assets.py'),
                    '--install', str(guestz)], check=True)
    config.write_text('[ne2000]\nne2000=true\n[pidos]\nnet_startup=dhcp\n')
    subprocess.run([str(generator), str(config), str(guestz), str(guest), '1', str(guest / 'START.BAT')], check=True)
    (guest / 'TEST.BAT').write_text('@ECHO OFF\nCALL START.BAT >NET.OUT\n'
        'TYPE C:\\SYSTEM\\NET\\MTCP.CFG >>NET.OUT\nEXIT\n')
    with (guest / 'run.log').open('w') as log:
        subprocess.run(command(guest, ['ne2000 ne2000=true', 'ne2000 backend=slirp',
                                     'ne2000 nicbase=300', 'ne2000 nicirq=10']), env=env,
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=50)
    output = (guest / 'NET.OUT').read_bytes()
    assert b'IPADDR 10.0.2.15' in output and b'NAMESERVER 10.0.2.3' in output, output
    assert b'DHCP failed' not in output and b'Packet driver failed' not in output, output
    print('PASS: actual ARM64 NE2000 packet driver and mTCP obtain the SLIRP DHCP lease.')
print('Evidence: '+str(work))
