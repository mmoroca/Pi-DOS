/* Shared entry point for every Windows driver and Pi-DOS option page. */
#include <windows.h>
#include "video.h"

BOOL FAR PASCAL __export ControlDialog(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    if (message == WM_COMMAND) {
        switch (wparam) {
        case IDCANCEL: EndDialog(dialog, 0); return TRUE;
        case IDC_DISPLAY: pv_display(dialog); return TRUE;
        case IDC_AUDIO: pv_audio(dialog); return TRUE;
        case IDC_OPTIONS: pv_options(dialog); return TRUE;
        case IDC_RESTORE:
            pv_restore(dialog);
            return TRUE;
        }
    }
    return FALSE;
}

void pv_control(HWND owner)
{
    DialogBox(pv_instance(), MAKEINTRESOURCE(IDD_CONTROL), owner, ControlDialog);
}
