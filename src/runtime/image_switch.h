/* Included by bios_disk.cpp after the DOS drive and BIOS disk definitions. */
#ifndef PI_DOS_IMAGE_SWITCH_H
#define PI_DOS_IMAGE_SWITCH_H

void PiDosNextImage(bool cdrom)
{
    /* Only IMGMOUNT image sets participate. Physical media and single images
     * must not receive spurious media changes. C: and Z: stay protected. */
    for (int drive=cdrom?3:0; drive<(cdrom?25:2); ++drive) {
        if (!Drives[drive] || DriveManager::GetDisksSize(drive)<2) continue;
        if (cdrom ? dynamic_cast<isoDrive *>(Drives[drive])==nullptr :
                    dynamic_cast<fatDrive *>(Drives[drive])==nullptr) continue;
        DriveManager::CycleDisks(drive, true);
        Drives[drive]->EmptyCache();
        if (cdrom && !dos_kernel_disabled)
            static_cast<isoDrive *>(Drives[drive])->MediaChangeImmediate();
        else
            Drives[drive]->MediaChange();
        if (!cdrom) imageDiskChange[drive]=true;
    }
}

#endif
