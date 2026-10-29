/* Called only at DOS operation boundaries, never from interrupt/timer callbacks. */
#ifndef PI_DOS_USB_STORAGE_API_H
#define PI_DOS_USB_STORAGE_API_H
#if defined(LINUX) && !defined(OSFREE)
void PiDosUsbStoragePoll();
void PiDosUsbStorageShutdown();
#else
inline void PiDosUsbStoragePoll() {}
inline void PiDosUsbStorageShutdown() {}
#endif
#endif
