/* TrayVitals version, shared by the app, the installer and both .rc files.
   Numbering: a new feature raises the middle number (1.6.0 -> 1.7.0); a fix raises the last (1.7.0 -> 1.7.1).
   When changing it, also add a section at the top of TrayVitals/patchnotes.h. */
#define TV_VER_MAJOR 1
#define TV_VER_MINOR 10
#define TV_VER_PATCH 0
#define TV_VERSION_A "1.10.0"
#define TV_VERSION   L"1.10.0"

/* Where TrayVitals looks for new versions: a GitHub repository, written "owner/name".
   Publish each version there as a release tagged like v1.9.0 (the same number as above); the
   build workflow then attaches TrayVitals-Setup.exe. Leave it empty ("") to build without update checks. */
#ifndef TV_UPDATE_REPO
#define TV_UPDATE_REPO  L"Fakem1d/TrayVitals"
#endif
#ifndef TV_UPDATE_ASSET
#define TV_UPDATE_ASSET "TrayVitals-Setup.exe"   /* the release file that is downloaded and run */
#endif
