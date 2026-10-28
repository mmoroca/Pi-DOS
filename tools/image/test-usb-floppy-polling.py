#!/usr/bin/env python3
"""Check the actual kernel polling helper against USB classes and speed encodings."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
kernel = ROOT / 'output/buildroot-arm64/build/linux-custom'
patch = ROOT / 'patches/linux/0001-usb-floppy-status-polling.patch'
source = kernel / 'drivers/usb/core/config.c'

with tempfile.TemporaryDirectory(prefix='pi-dos-floppy-polling.') as temporary:
    work = Path(temporary)
    copy = work / 'drivers/usb/core/config.c'
    copy.parent.mkdir(parents=True)
    copy.write_bytes(source.read_bytes())
    reverse = ['patch', '-s', '-p1', '--force', '--dry-run', '--reverse', '-i', str(patch)]
    if 'static void usb_floppy_status_interval(' not in copy.read_text():
        assert subprocess.run(reverse, cwd=work, capture_output=True).returncode != 0
        subprocess.run(['patch', '-s', '-p1', '--batch', '-i', str(patch)], cwd=work, check=True)
    assert subprocess.run(reverse, cwd=work, capture_output=True).returncode == 0
    text = copy.read_text()
    assert 'usb_floppy_status_interval(udev, &ifp->desc, &endpoint->desc);' in text
    start = text.index('static void usb_floppy_status_interval(')
    helper = text[start:text.index('\n}', start) + 2]
    (work / 'storage.h').write_bytes((kernel / 'include/linux/usb/storage.h').read_bytes())
    prefix = r'''
#include <assert.h>
#include <stdarg.h>
#include <endian.h>
#include <linux/usb/ch9.h>
#include "storage.h"
struct device { int unused; };
struct usb_device { struct device dev; enum usb_device_speed speed; };
static unsigned int notices;
static void dev_notice(struct device *dev, const char *format, ...)
{ (void)dev; (void)format; ++notices; }
'''
    tests = r'''
int main(void)
{
    struct usb_device dev = { .speed = USB_SPEED_FULL };
    struct usb_interface_descriptor intf = {
        .bInterfaceClass = USB_CLASS_MASS_STORAGE,
        .bInterfaceSubClass = USB_SC_UFI,
        .bInterfaceProtocol = USB_PR_CBI,
    };
    struct usb_endpoint_descriptor ep = {
        .bEndpointAddress = USB_DIR_IN | 3,
        .bmAttributes = USB_ENDPOINT_XFER_INT,
        .wMaxPacketSize = htole16(2),
    };
    /* Full-speed intervals are milliseconds; high/super-speed are exponents
     * of 125-us microframes. Exercise every legal interval in both encodings. */
    enum usb_device_speed speeds[] = {
        USB_SPEED_FULL, USB_SPEED_HIGH, USB_SPEED_SUPER, USB_SPEED_SUPER_PLUS,
    };
    for (unsigned int s = 0; s < sizeof(speeds) / sizeof(speeds[0]); ++s) {
        dev.speed = speeds[s];
        unsigned int cap = dev.speed == USB_SPEED_FULL ? 8 : 7;
        unsigned int maximum = dev.speed == USB_SPEED_FULL ? 255 : 16;
        for (unsigned int interval = 1; interval <= maximum; ++interval) {
            ep.bInterval = interval;
            notices = 0;
            usb_floppy_status_interval(&dev, &intf, &ep);
            assert(ep.bInterval == (interval > cap ? cap : interval));
            assert(notices == (interval > cap));
        }
    }
    /* No broad change to USB disks, HID, other storage protocols, bulk
     * payload endpoints, OUT endpoints or unsupported device speeds. */
    dev.speed = USB_SPEED_FULL;
    ep.bInterval = 255;
    for (unsigned int subclass = 0; subclass < 256; ++subclass) {
        if (subclass == USB_SC_UFI) continue;
        intf.bInterfaceSubClass = subclass;
        usb_floppy_status_interval(&dev, &intf, &ep);
        assert(ep.bInterval == 255);
    }
    intf.bInterfaceSubClass = USB_SC_UFI;
    for (unsigned int protocol = 1; protocol < 256; ++protocol) {
        intf.bInterfaceProtocol = protocol;
        usb_floppy_status_interval(&dev, &intf, &ep);
        assert(ep.bInterval == 255);
    }
    intf.bInterfaceProtocol = USB_PR_CBI;
    intf.bInterfaceClass = USB_CLASS_HID;
    usb_floppy_status_interval(&dev, &intf, &ep);
    assert(ep.bInterval == 255);
    intf.bInterfaceClass = USB_CLASS_MASS_STORAGE;
    ep.bmAttributes = USB_ENDPOINT_XFER_BULK;
    usb_floppy_status_interval(&dev, &intf, &ep);
    assert(ep.bInterval == 255);
    ep.bmAttributes = USB_ENDPOINT_XFER_INT;
    ep.bEndpointAddress = 3;
    usb_floppy_status_interval(&dev, &intf, &ep);
    assert(ep.bInterval == 255);
    ep.bEndpointAddress = USB_DIR_IN | 3;
    dev.speed = USB_SPEED_LOW;
    usb_floppy_status_interval(&dev, &intf, &ep);
    assert(ep.bInterval == 255);
    dev.speed = USB_SPEED_FULL;
    ep.wMaxPacketSize = htole16(1);
    usb_floppy_status_interval(&dev, &intf, &ep);
    assert(ep.bInterval == 255);
    return 0;
}
'''
    check = work / 'check.c'
    check.write_text(prefix + helper + tests)
    executable = work / 'check'
    subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    str(check), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)

print('PASS: UFI/CBI status polling cap, full/high/super-speed encoding, faster intervals, unrelated USB classes/transports/endpoints.')
