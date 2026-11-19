/* A preview copy of TrayVitals: runs next to an installed TrayVitals, with its own settings
   (HKCU\Software\TrayVitals-Preview). It never touches startup, updates or Windows' tray settings,
   never asks to install anything, and its menu has "Preview the popups". Handy for trying changes:

     cd src/TrayVitals
     windres trayvitals.rc -O coff -o trayvitals.res
     gcc -Os -s -mwindows preview.c trayvitals.res -o TrayVitals-Preview.exe -lole32 -loleaut32
*/
#define TV_PREVIEW 1
#define SINGLE_INSTANCE_NAME L"Local\\TrayVitals.preview"
#define REGKEY L"Software\\TrayVitals-Preview"
#define MAIN_CLASS L"TrayVitalsPreview"
#define TV_UPDATE_REPO L""
#include "trayvitals.c"
