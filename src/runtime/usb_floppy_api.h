/* Entry points shared by the DOSBox-X disk interrupt and IOCTL handlers. */
#ifndef PI_DOS_USB_FLOPPY_API_H
#define PI_DOS_USB_FLOPPY_API_H
#if defined(LINUX) && !defined(OSFREE)
class DOS_Drive;
class imageDisk;
bool PI_DOS_IsUSBFloppy(DOS_Drive *drive);
bool PI_DOS_IsUSBFloppyImage(imageDisk *disk);
bool PI_DOS_USBFloppyDPB(DOS_Drive *drive, uint8_t number);
bool PI_DOS_USBFloppyBIOS();
bool PI_DOS_USBFloppyIoctl(DOS_Drive *drive, uint8_t category, uint8_t function, PhysPt ptr, bool query);
#endif
#endif
