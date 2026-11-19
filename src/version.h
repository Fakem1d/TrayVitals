/* TraySense version, shared by the app, the installer and both .rc files.
   Numbering: a new feature raises the middle number (1.6.0 -> 1.7.0); a fix raises the last (1.7.0 -> 1.7.1).
   When changing it, also add a section at the top of TraySense-main/patchnotes.h. */
#define TS_VER_MAJOR 1
#define TS_VER_MINOR 9
#define TS_VER_PATCH 0
#define TS_VERSION_A "1.9.0"
#define TS_VERSION   L"1.9.0"

/* Where TraySense looks for new versions: a GitHub repository, written "owner/name".
   Publish each version there as a release tagged like v1.9.0 (the same number as above); the
   build workflow then attaches TraySense-Setup.exe. Leave it empty ("") to build without update checks. */
#ifndef TS_UPDATE_REPO
#define TS_UPDATE_REPO  L"Fakem1d/Traysense"
#endif
#ifndef TS_UPDATE_ASSET
#define TS_UPDATE_ASSET "TraySense-Setup.exe"   /* the release file that is downloaded and run */
#endif
