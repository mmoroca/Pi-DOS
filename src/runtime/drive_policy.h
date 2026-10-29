/* Appliance drive ownership; host environment, independent of DOS SET. */
#ifndef PI_DOS_DRIVE_POLICY_H
#define PI_DOS_DRIVE_POLICY_H
#include <cstdlib>
#include <cstring>

inline bool PiDosApplianceDrives()
{
    const char *mode = std::getenv("PI_DOS_SUPERVISED");
    return mode && !std::strcmp(mode, "1");
}
inline bool PiDosProtectedDrive(int drive)
{ return PiDosApplianceDrives() && (drive == 2 || drive == 25); }

inline const char *PiDosDrivePolicyMessage()
{ return "Pi-DOS manages drive mappings. Use MOUNT to list drives or IMGMOUNT with a free letter.\n"; }
inline const char *PiDosUnavailableDriveMessage()
{ return "Drive %c: is unavailable.\nType MOUNT to list available drives.\n"; }
#endif
