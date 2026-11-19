/* TrayVitals installer / uninstaller.
 *
 *   TrayVitals-Setup.exe               shows the setup window and installs (or updates) TrayVitals
 *   TrayVitals-Setup.exe /update       updates an installed TrayVitals without the window, same settings
 *   uninstall.exe /uninstall [/quiet] removes it (registered in Settings > Apps)
 *
 * The TrayVitals.exe to install is embedded as resource 200.
 *
 * The user picks the install folder (default: Program Files\TrayVitals). TrayVitals' own
 * "start as administrator at login" task always runs a copy kept in Program Files\TrayVitals,
 * so a user-writable install folder can never be swapped for something that runs as admin.
 * When that task exists, setup refreshes that copy too.
 *
 * TrayVitals was called TraySense before 1.10.0. Setup moves such an install over (folder, settings,
 * shortcuts, startup) and removes what it left behind. It only touches an install this project's own
 * setup made, found through its uninstall entry, never another program that is called TraySense.
 */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <bcrypt.h>
#include <commctrl.h>
#include <setupapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <wchar.h>
#include <wincodec.h>
#include "setup.h"
#include "../version.h"

#define APP       L"TrayVitals"
#define VERSION   TV_VERSION
#define UNKEY     L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TrayVitals"
#define PAWNKEY   L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PawnIO"
#define RUNKEY    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define SETKEY    L"Software\\TrayVitals"
#define PAYLOAD   200
#define PAWN_RES  201

/* PawnIO 2.2.0 setup (github.com/namazso/PawnIO.Setup), the same build and checksum
   TrayVitals itself pins. Checked again before it is run. */
static const BYTE PAWNIO_SHA256[32] = {
    0x1f,0x51,0x9a,0x22,0xe4,0x71,0x87,0xf7,0x0a,0x13,0x79,0xa4,0x8c,0xa6,0x04,0x98,
    0x1c,0x4f,0xcf,0x69,0x4f,0x4e,0x65,0xb7,0x34,0xaa,0xa7,0x4a,0x9f,0xba,0x30,0x32 };

enum { PAWN_FAILED, PAWN_OK, PAWN_OK_REBOOT, PAWN_SKIPPED };

static wchar_t g_dir[MAX_PATH];
static int     g_desktop = 1, g_startup = 1, g_launch = 1, g_pawn = 1, g_hasTask, g_pawnPresent;
static int     g_style = -1;   /* menu style picked in the setup window (0 modern, 1 classic); -1 = leave as is */

/* The old name, for moving a TraySense install over. */
#define OLD_APP    L"TraySense"
#define OLD_UNKEY  L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TraySense"
#define OLD_SETKEY L"Software\\TraySense"
static wchar_t g_oldDir[MAX_PATH];   /* the old TraySense install folder, if setup found one */
static int     g_oldTask;            /* its "start as administrator at login" task exists */

/* ---------- small helpers ---------- */

static void fmt(wchar_t *out, int n, const wchar_t *f, ...)
{
    va_list a; va_start(a, f);
    _vsnwprintf(out, n, f, a);
    out[n - 1] = 0;
    va_end(a);
}

static int msg(HWND owner, const wchar_t *text, UINT flags)
{
    return MessageBoxW(owner, text, APP L" Setup", flags | MB_SETFOREGROUND);
}

static int exists(const wchar_t *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }

static void reg_sz(HKEY root, const wchar_t *key, const wchar_t *name, const wchar_t *val)
{
    RegSetKeyValueW(root, key, name, REG_SZ, val, (lstrlenW(val) + 1) * sizeof(wchar_t));
}
static void reg_dw(HKEY root, const wchar_t *key, const wchar_t *name, DWORD val)
{
    RegSetKeyValueW(root, key, name, REG_DWORD, &val, sizeof val);
}
static int reg_get_sz(HKEY root, const wchar_t *key, const wchar_t *name, wchar_t *out)
{
    DWORD sz = MAX_PATH * sizeof(wchar_t);
    out[0] = 0;
    return RegGetValueW(root, key, name, RRF_RT_REG_SZ, NULL, out, &sz) == ERROR_SUCCESS && out[0];
}

/* Program Files\TrayVitals: where TrayVitals keeps the copy its admin login task runs. */
static int pf_dir(wchar_t *out)
{
    DWORD sz = MAX_PATH * sizeof(wchar_t);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", L"ProgramFilesDir",
                     RRF_RT_REG_SZ, NULL, out, &sz) != ERROR_SUCCESS || lstrlenW(out) > MAX_PATH - 48) return 0;
    lstrcatW(out, L"\\TrayVitals");
    return 1;
}

static int start_menu_lnk(wchar_t *out)
{
    wchar_t dir[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_COMMON_PROGRAMS, NULL, 0, dir))) return 0;
    fmt(out, MAX_PATH, L"%s\\TrayVitals.lnk", dir);
    return 1;
}

static int desktop_lnk(wchar_t *out)
{
    wchar_t dir[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, dir))) return 0;
    fmt(out, MAX_PATH, L"%s\\TrayVitals.lnk", dir);
    return 1;
}

static DWORD run_timeout(const wchar_t *exe, const wchar_t *args, DWORD ms)
{
    wchar_t cmd[MAX_PATH * 2 + 1100];
    DWORD code = 1;
    fmt(cmd, MAX_PATH * 2 + 1100, L"\"%s\" %s", exe, args);
#ifdef SETUP_DRYRUN   /* test builds only: record the command instead of running it */
    dryrun_log(cmd);
    return 0;
#endif
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si); si.cb = sizeof si;
    if (CreateProcessW(exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        if (WaitForSingleObject(pi.hProcess, ms) == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
        else code = 1;   /* timed out: treat as failed */
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    return code;
}
static DWORD run_wait(const wchar_t *exe, const wchar_t *args)      { return run_timeout(exe, args, 15000); }
static DWORD run_wait_long(const wchar_t *exe, const wchar_t *args) { return run_timeout(exe, args, 180000); }

static DWORD schtasks(const wchar_t *args)
{
    wchar_t exe[MAX_PATH];
    UINT n = GetSystemDirectoryW(exe, MAX_PATH - 16);
    if (!n || n >= MAX_PATH - 16) return 1;
    lstrcatW(exe, L"\\schtasks.exe");
    return run_wait(exe, args);
}

static int task_exists(void) { return schtasks(L"/Query /TN TrayVitals") == 0; }

/* Program Files\TraySense: where the old version kept the copy its admin login task ran. */
static int old_pf_dir(wchar_t *out)
{
    DWORD sz = MAX_PATH * sizeof(wchar_t);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", L"ProgramFilesDir",
                     RRF_RT_REG_SZ, NULL, out, &sz) != ERROR_SUCCESS || lstrlenW(out) > MAX_PATH - 48) return 0;
    lstrcatW(out, L"\\TraySense");
    return 1;
}

/* A running copy of this program: TrayVitals.exe, or TraySense.exe started from the old install
   (its folder or its Program Files copy), but never some other program called TraySense. */
static int ours(const PROCESSENTRY32W *pe)
{
    if (!lstrcmpiW(pe->szExeFile, L"TrayVitals.exe")) return 1;
    if (lstrcmpiW(pe->szExeFile, L"TraySense.exe") || !g_oldDir[0]) return 0;
    wchar_t path[MAX_PATH], opf[MAX_PATH];
    DWORD n = MAX_PATH;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe->th32ProcessID);
    if (!h) return 0;
    BOOL ok = QueryFullProcessImageNameW(h, 0, path, &n);
    CloseHandle(h);
    wchar_t *s = ok ? wcsrchr(path, L'\\') : NULL;
    if (!s) return 0;
    *s = 0;
    return !lstrcmpiW(path, g_oldDir) || (old_pf_dir(opf) && !lstrcmpiW(path, opf));
}

static int is_running(void)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe; pe.dwSize = sizeof pe; int found = 0;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe))
        found = ours(&pe);
    CloseHandle(snap);
    return found;
}

static void close_running(void)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe; pe.dwSize = sizeof pe;
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (!ours(&pe)) continue;
        HANDLE p = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
        if (p) { TerminateProcess(p, 0); WaitForSingleObject(p, 5000); CloseHandle(p); }
    }
    CloseHandle(snap);
}

static const void *resource(int id, DWORD *size)
{
    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
    HGLOBAL g = r ? LoadResource(NULL, r) : NULL;
    *size = r ? SizeofResource(NULL, r) : 0;
    return g ? LockResource(g) : NULL;
}

static int write_bytes(const wchar_t *dst, const void *data, DWORD size)
{
    DWORD w = 0;
    if (!data || !size) return 0;
    for (int tries = 0; tries < 20; tries++) {
        HANDLE f = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            BOOL ok = WriteFile(f, data, size, &w, NULL) && w == size;
            CloseHandle(f);
            return ok;
        }
        Sleep(250);   /* a just-closed TrayVitals may still hold the file for a moment */
    }
    return 0;
}

static int write_payload(const wchar_t *dst)
{
    DWORD size;
    const void *data = resource(PAYLOAD, &size);
    return write_bytes(dst, data, size);
}

/* ---------- PawnIO (CPU temperature driver) ---------- */

static int sha256_is(const void *data, DWORD len, const BYTE *want)
{
    BCRYPT_ALG_HANDLE alg; BCRYPT_HASH_HANDLE h; BYTE out[32]; int ok = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0)) return 0;
    if (!BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0)) {
        if (!BCryptHashData(h, (PUCHAR)data, len, 0) && !BCryptFinishHash(h, out, 32, 0)) ok = !memcmp(out, want, 32);
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

static int pawnio_installed(void)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, PAWNKEY, 0, KEY_READ, &k) == ERROR_SUCCESS) { RegCloseKey(k); return 1; }
    HANDLE d = CreateFileW(L"\\\\?\\GLOBALROOT\\Device\\PawnIO", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, 0, NULL);
    if (d != INVALID_HANDLE_VALUE) { CloseHandle(d); return 1; }
    return GetLastError() == ERROR_ACCESS_DENIED;   /* the driver exists, we just can't open it */
}

static int pawn_result(DWORD code)
{
    return code == 0 ? PAWN_OK : code == 3010 ? PAWN_OK_REBOOT : PAWN_FAILED;
}

/* Runs the bundled PawnIO setup silently. It is written to Program Files\TrayVitals
   (only admins can change files there) and deleted afterwards, like TrayVitals does itself. */
static int pawnio_install(const wchar_t *pf)
{
    DWORD size;
    const void *data = resource(PAWN_RES, &size);
    if (!data || !size || !sha256_is(data, size, PAWNIO_SHA256)) return PAWN_FAILED;
    wchar_t exe[MAX_PATH];
    int madeDir = CreateDirectoryW(pf, NULL);
    fmt(exe, MAX_PATH, L"%s\\PawnIO_setup.exe", pf);
    int r = PAWN_FAILED;
    if (write_bytes(exe, data, size)) r = pawn_result(run_wait_long(exe, L"-install -silent"));
    DeleteFileW(exe);
    if (madeDir) RemoveDirectoryW(pf);
    if (r != PAWN_FAILED && !pawnio_installed()) r = PAWN_FAILED;
    return r;
}

/* PawnIO's own uninstaller leaves its driver service and its driver package (pawnio.inf in the
   Windows driver store) behind. Removes both. Returns 1 if Windows needs a restart to finish. */
static int pawnio_remove_driver(void)
{
    int reboot = 0;

    /* 1. Stop and delete the "PawnIO" driver service, but only if it really is PawnIO's driver. */
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
#ifdef SETUP_DRYRUN
    SC_HANDLE svc = scm ? OpenServiceW(scm, L"PawnIO", SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG) : NULL;
#else
    SC_HANDLE svc = scm ? OpenServiceW(scm, L"PawnIO", SERVICE_STOP | SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG | DELETE) : NULL;
#endif
    if (svc) {
        union { QUERY_SERVICE_CONFIGW c; BYTE b[8192]; } cfg;
        DWORD need = 0;
        int ours = 0;
        if (QueryServiceConfigW(svc, &cfg.c, sizeof cfg, &need) && cfg.c.lpBinaryPathName) {
            wchar_t lower[1024];
            lstrcpynW(lower, cfg.c.lpBinaryPathName, 1024);
            CharLowerW(lower);
            ours = wcsstr(lower, L"pawnio.sys") != NULL;
        }
#ifdef SETUP_DRYRUN
        dryrun_log(ours ? L"stop and delete service PawnIO" : L"leave service PawnIO alone (not PawnIO.sys)");
        ours = 0;
#endif
        if (ours) {
            SERVICE_STATUS st;
            if (ControlService(svc, SERVICE_CONTROL_STOP, &st) || GetLastError() == ERROR_SERVICE_NOT_ACTIVE) {
                for (int i = 0; i < 40 && QueryServiceStatus(svc, &st) && st.dwCurrentState != SERVICE_STOPPED; i++) Sleep(250);
            }
            if (!DeleteService(svc) && GetLastError() != ERROR_SERVICE_MARKED_FOR_DELETE) reboot = 1;
            if (QueryServiceStatus(svc, &st) && st.dwCurrentState != SERVICE_STOPPED) reboot = 1;   /* still loaded */
        }
        CloseServiceHandle(svc);
    }
    if (scm) CloseServiceHandle(scm);

    /* 2. Remove every pawnio.inf driver package (oemNN.inf) from the driver store. */
    typedef BOOL (WINAPI *DIUNINSTALLDRIVERW)(HWND, PCWSTR, DWORD, PBOOL);
    HMODULE nd = LoadLibraryExW(L"newdev.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    DIUNINSTALLDRIVERW diUninstall = nd ? (DIUNINSTALLDRIVERW)(void *)GetProcAddress(nd, "DiUninstallDriverW") : NULL;
    typedef BOOL (WINAPI *GETSTORELOCW)(PCWSTR, void *, PCWSTR, PWSTR, DWORD, PDWORD);
    HMODULE sa = LoadLibraryExW(L"setupapi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    GETSTORELOCW getStoreLoc = sa ? (GETSTORELOCW)(void *)GetProcAddress(sa, "SetupGetInfDriverStoreLocationW") : NULL;
    wchar_t infDir[MAX_PATH], pattern[MAX_PATH];
    UINT n = GetWindowsDirectoryW(infDir, MAX_PATH - 24);
    if (getStoreLoc && n && n < MAX_PATH - 24) {
        lstrcatW(infDir, L"\\INF");
        fmt(pattern, MAX_PATH, L"%s\\oem*.inf", infDir);
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                wchar_t oem[MAX_PATH], store[MAX_PATH];
                fmt(oem, MAX_PATH, L"%s\\%s", infDir, fd.cFileName);
                if (!getStoreLoc(oem, NULL, NULL, store, MAX_PATH, NULL)) continue;
                /* store = ...\FileRepository\pawnio.inf_amd64_<hash>\pawnio.inf */
                wchar_t *file = wcsrchr(store, L'\\');
                if (!file) continue;
                *file = 0;
                wchar_t *folder = wcsrchr(store, L'\\');
                *file = L'\\';
                if (!folder || _wcsnicmp(folder + 1, L"pawnio.inf_", 11)) continue;
#ifdef SETUP_DRYRUN
                { wchar_t m[MAX_PATH * 2]; fmt(m, MAX_PATH * 2, L"remove driver package %s (%s)", fd.cFileName, store); dryrun_log(m); }
                continue;
#endif
                BOOL needReboot = FALSE;
                if (!(diUninstall && diUninstall(NULL, store, 0, &needReboot)) &&
                    !SetupUninstallOEMInfW(fd.cFileName, SUOI_FORCEDELETE, NULL))
                    reboot = 1;
                if (needReboot) reboot = 1;
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    if (sa) FreeLibrary(sa);
    if (nd) FreeLibrary(nd);
    return reboot;
}

/* Runs PawnIO's own registered uninstaller silently, then clears what it leaves behind. */
static int pawnio_uninstall(void)
{
    wchar_t cmd[1024], args[1024] = L"";
    DWORD sz = sizeof cmd;
    int silentIncluded = 1;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, PAWNKEY, L"QuietUninstallString", RRF_RT_REG_SZ, NULL, cmd, &sz) != ERROR_SUCCESS) {
        sz = sizeof cmd;
        if (RegGetValueW(HKEY_LOCAL_MACHINE, PAWNKEY, L"UninstallString", RRF_RT_REG_SZ, NULL, cmd, &sz) != ERROR_SUCCESS)
            return PAWN_FAILED;
        silentIncluded = 0;
    }
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(cmd, &argc);
    if (!argv || argc < 1 || !exists(argv[0])) { if (argv) LocalFree(argv); return PAWN_FAILED; }
    for (int i = 1; i < argc; i++) {
        if (lstrlenW(args) + lstrlenW(argv[i]) + 4 >= 1024) break;
        if (args[0]) lstrcatW(args, L" ");
        lstrcatW(args, argv[i]);
    }
    if (!silentIncluded && !wcsstr(args, L"-silent")) lstrcatW(args, args[0] ? L" -silent" : L"-silent");
    wchar_t exe[MAX_PATH];
    lstrcpynW(exe, argv[0], MAX_PATH);
    LocalFree(argv);
    int r = pawn_result(run_wait_long(exe, args));
    if (r == PAWN_OK) {
        HKEY k;   /* double-check it really unregistered */
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, PAWNKEY, 0, KEY_READ, &k) == ERROR_SUCCESS) { RegCloseKey(k); r = PAWN_FAILED; }
    }
    if (r != PAWN_FAILED && pawnio_remove_driver()) r = PAWN_OK_REBOOT;
    return r;
}

static int make_shortcut(const wchar_t *lnk, const wchar_t *target, const wchar_t *dir)
{
    IShellLinkW *sl = NULL; IPersistFile *pf = NULL; int ok = 0;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl))) return 0;
    IShellLinkW_SetPath(sl, target);
    IShellLinkW_SetWorkingDirectory(sl, dir);
    IShellLinkW_SetDescription(sl, L"CPU, GPU and RAM usage and temperatures on the taskbar");
    if (SUCCEEDED(IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf))) {
        ok = SUCCEEDED(IPersistFile_Save(pf, lnk, TRUE));
        IPersistFile_Release(pf);
    }
    IShellLinkW_Release(sl);
    return ok;
}

/* Deletes a file, retrying while the process that held it exits. */
static void del_retry(const wchar_t *p)
{
    for (int i = 0; i < 20 && exists(p); i++) {
        if (DeleteFileW(p)) return;
        Sleep(250);
    }
    if (exists(p)) MoveFileExW(p, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
}

/* Removes only TrayVitals' own files from a folder, then the folder if it is left empty. */
static void remove_files(const wchar_t *dir)
{
    wchar_t p[MAX_PATH];
    if (!dir[0]) return;
    fmt(p, MAX_PATH, L"%s\\TrayVitals.exe", dir); del_retry(p);
    fmt(p, MAX_PATH, L"%s\\uninstall.exe", dir); del_retry(p);
    fmt(p, MAX_PATH, L"%s\\task.xml", dir);      DeleteFileW(p);
    RemoveDirectoryW(dir);   /* fails, harmlessly, if anything else is in there */
}

/* Starts a program as the signed-in user, not elevated like setup itself. */
static void launch_unelevated(const wchar_t *exe)
{
    wchar_t explorer[MAX_PATH], arg[MAX_PATH + 4];
    UINT n = GetWindowsDirectoryW(explorer, MAX_PATH - 16);
    if (n && n < MAX_PATH - 16) {
        lstrcatW(explorer, L"\\explorer.exe");
        fmt(arg, MAX_PATH + 4, L"\"%s\"", exe);
        if ((INT_PTR)ShellExecuteW(NULL, L"open", explorer, arg, NULL, SW_SHOWNORMAL) > 32) return;
    }
    ShellExecuteW(NULL, L"open", exe, NULL, NULL, SW_SHOWNORMAL);
}

/* ---------- moving a TraySense install over ---------- */

/* Finds an install made by this project's own setup under its old name: its uninstall entry,
   pointing at a folder that ends in \TraySense (setup always made it so). */
static int old_install(void)
{
    wchar_t d[MAX_PATH];
    g_oldDir[0] = 0;
    if (!reg_get_sz(HKEY_LOCAL_MACHINE, OLD_UNKEY, L"InstallLocation", d)) return 0;
    int n = lstrlenW(d);
    while (n > 3 && d[n - 1] == L'\\') d[--n] = 0;
    if (n < 10 || _wcsnicmp(d + n - 10, L"\\TraySense", 10)) return 0;
    lstrcpyW(g_oldDir, d);
    return 1;
}
/* The same place under the new name: ...\TraySense -> ...\TrayVitals. */
static void old_to_new_dir(wchar_t *out)
{
    lstrcpyW(out, g_oldDir);
    out[lstrlenW(out) - 10] = 0;
    lstrcatW(out, L"\\TrayVitals");
}
/* Removes only the old version's own files from a folder, then the folder if it's left empty. */
static void remove_old_files(const wchar_t *dir)
{
    wchar_t p[MAX_PATH];
    if (!dir[0]) return;
    fmt(p, MAX_PATH, L"%s\\TraySense.exe", dir); del_retry(p);
    fmt(p, MAX_PATH, L"%s\\uninstall.exe", dir); del_retry(p);
    fmt(p, MAX_PATH, L"%s\\task.xml", dir);      DeleteFileW(p);
    RemoveDirectoryW(dir);
}
/* Settings: copies what the new key doesn't have yet (so choices made in this setup win). */
static void copy_old_settings(void)
{
    HKEY o, n;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, OLD_SETKEY, 0, KEY_READ, &o)) return;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, SETKEY, 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &n, NULL)) { RegCloseKey(o); return; }
    for (DWORD i = 0;; i++) {
        wchar_t name[256]; BYTE data[4096];
        DWORD nl = 256, dl = sizeof data, type;
        LONG r = RegEnumValueW(o, i, name, &nl, NULL, &type, data, &dl);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) continue;
        if (RegQueryValueExW(n, name, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) continue;
        RegSetValueExW(n, name, 0, type, data, dl);
    }
    RegCloseKey(n);
    RegCloseKey(o);
}
/* After the new version is in place: removes what the old one left behind. */
static void cleanup_old(void)
{
    wchar_t p[MAX_PATH], lnk[MAX_PATH];
    DWORD v = 0, sz = sizeof v;
    if (!g_oldDir[0]) return;
    if (g_oldTask) schtasks(L"/Delete /F /TN " OLD_APP);
    RegDeleteKeyValueW(HKEY_CURRENT_USER, RUNKEY, OLD_APP);
    RegDeleteTreeW(HKEY_CURRENT_USER, OLD_SETKEY);
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_PROGRAMS, NULL, 0, p))) {
        fmt(lnk, MAX_PATH, L"%s\\TraySense.lnk", p);
        DeleteFileW(lnk);
    }
    if (reg_get_sz(HKEY_LOCAL_MACHINE, OLD_UNKEY, L"DesktopShortcut", lnk)) DeleteFileW(lnk);
    if (!RegGetValueW(HKEY_LOCAL_MACHINE, OLD_UNKEY, L"PawnIOBySetup", RRF_RT_REG_DWORD, NULL, &v, &sz) && v)
        reg_dw(HKEY_LOCAL_MACHINE, UNKEY, L"PawnIOBySetup", 1);   /* so uninstalling still offers to remove PawnIO */
    remove_old_files(g_oldDir);
    if (old_pf_dir(p)) remove_old_files(p);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, OLD_UNKEY);
}
/* ---------- install folder ---------- */

static int ends_with_trayvitals(const wchar_t *p)
{
    int n = lstrlenW(p);
    while (n > 0 && p[n - 1] == L'\\') n--;
    return n >= 11 && !_wcsnicmp(p + n - 11, L"\\TrayVitals", 11);
}

/* Turns what the user typed or picked into a full path that always ends in \TrayVitals,
   so setup never installs loose files into (or later empties) a folder like D:\ or Documents. */
static int normalize_dir(const wchar_t *in, wchar_t *out)
{
    wchar_t full[MAX_PATH];
    while (*in == L' ') in++;
    /* Only full paths: a drive (C:\...) or a network share (\\server\share\...). */
    int drive = ((in[0] >= L'A' && in[0] <= L'Z') || (in[0] >= L'a' && in[0] <= L'z')) && in[1] == L':' && in[2] == L'\\';
    int unc = in[0] == L'\\' && in[1] == L'\\' && in[2] && in[2] != L'?' && in[2] != L'.';
    if (!drive && !unc) return 0;
    DWORD n = GetFullPathNameW(in, MAX_PATH, full, NULL);
    if (!n || n >= MAX_PATH - 16) return 0;
    if (!((full[1] == L':' && full[2] == L'\\') || (full[0] == L'\\' && full[1] == L'\\'))) return 0;
    if (wcspbrk(full + 2, L"<>\"|?*:")) return 0;
    while (lstrlenW(full) > 3 && full[lstrlenW(full) - 1] == L'\\') full[lstrlenW(full) - 1] = 0;
    if (ends_with_trayvitals(full)) lstrcpyW(out, full);
    else fmt(out, MAX_PATH, full[lstrlenW(full) - 1] == L'\\' ? L"%sTrayVitals" : L"%s\\TrayVitals", full);
    return 1;
}

static void browse(HWND dlg)
{
    IFileOpenDialog *fd = NULL;
    IShellItem *item = NULL, *start = NULL;
    wchar_t cur[MAX_PATH], parent[MAX_PATH];
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&fd)))
        return;
    DWORD opt = 0;
    IFileOpenDialog_GetOptions(fd, &opt);
    IFileOpenDialog_SetOptions(fd, opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    IFileOpenDialog_SetTitle(fd, L"Choose where to install TrayVitals (a TrayVitals folder is made inside it)");
    IFileOpenDialog_SetOkButtonLabel(fd, L"Install here");

    /* Open the picker at the folder that would contain the current choice. */
    GetDlgItemTextW(dlg, IDC_DIR, cur, MAX_PATH);
    lstrcpyW(parent, cur);
    if (ends_with_trayvitals(parent)) {
        wchar_t *s = wcsrchr(parent, L'\\');
        if (s && s - parent > 2) *s = 0; else if (s) s[1] = 0;
    }
    if (SUCCEEDED(SHCreateItemFromParsingName(parent, NULL, &IID_IShellItem, (void **)&start))) {
        IFileOpenDialog_SetFolder(fd, start);
        IShellItem_Release(start);
    }

    if (SUCCEEDED(IFileOpenDialog_Show(fd, dlg)) && SUCCEEDED(IFileOpenDialog_GetResult(fd, &item))) {
        LPWSTR path = NULL;
        if (SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path)) && path) {
            wchar_t dir[MAX_PATH];
            if (normalize_dir(path, dir)) SetDlgItemTextW(dlg, IDC_DIR, dir);
            CoTaskMemFree(path);
        }
        IShellItem_Release(item);
    }
    IFileOpenDialog_Release(fd);
}

static HFONT g_titleFont;

/* ---- menu style pictures (PNG resources, decoded with Windows' WIC) ---- */
static const GUID CLSID_WICFactory_ = {0xcacaf262,0x9370,0x4615,{0xa1,0x3b,0x9f,0x55,0x39,0xda,0x4c,0x0a}};
static const GUID IID_WICFactory_   = {0xec5ec8a9,0xc395,0x4314,{0x9c,0x77,0x54,0xd7,0xa9,0x35,0xff,0x70}};
static const GUID WICFormat32bppBGR_ = {0x6fddc324,0x4e03,0x4bfe,{0xb1,0x85,0x3d,0x77,0x76,0x8d,0xc9,0x0e}};

typedef struct { HBITMAP bmp; int w, h; } PIC;
static PIC g_pic[2];

/* Decodes PNG resource id, scaled down to fit maxW x maxH. */
static PIC png_load(int id, int maxW, int maxH)
{
    PIC p = { NULL, 0, 0 };
    DWORD size;
    const void *data = resource(id, &size);
    IWICImagingFactory *f = NULL; IWICStream *s = NULL; IWICBitmapDecoder *d = NULL;
    IWICBitmapFrameDecode *fr = NULL; IWICBitmapScaler *sc = NULL; IWICFormatConverter *cv = NULL;
    UINT w, h;
    if (!data || !size || maxW < 1 || maxH < 1) return p;
    if (FAILED(CoCreateInstance(&CLSID_WICFactory_, NULL, CLSCTX_INPROC_SERVER, &IID_WICFactory_, (void **)&f))) return p;
    if (SUCCEEDED(IWICImagingFactory_CreateStream(f, &s)) &&
        SUCCEEDED(IWICStream_InitializeFromMemory(s, (BYTE *)data, size)) &&
        SUCCEEDED(IWICImagingFactory_CreateDecoderFromStream(f, (IStream *)s, NULL, WICDecodeMetadataCacheOnDemand, &d)) &&
        SUCCEEDED(IWICBitmapDecoder_GetFrame(d, 0, &fr)) &&
        SUCCEEDED(IWICBitmapFrameDecode_GetSize(fr, &w, &h)) && w && h) {
        double k = (double)maxW / w < (double)maxH / h ? (double)maxW / w : (double)maxH / h;
        if (k > 1) k = 1;
        UINT tw = (UINT)(w * k + 0.5), th = (UINT)(h * k + 0.5);
        if (!tw) tw = 1;
        if (!th) th = 1;
        if (SUCCEEDED(IWICImagingFactory_CreateBitmapScaler(f, &sc)) &&
            SUCCEEDED(IWICBitmapScaler_Initialize(sc, (IWICBitmapSource *)fr, tw, th, WICBitmapInterpolationModeFant)) &&
            SUCCEEDED(IWICImagingFactory_CreateFormatConverter(f, &cv)) &&
            SUCCEEDED(IWICFormatConverter_Initialize(cv, (IWICBitmapSource *)sc, &WICFormat32bppBGR_,
                                                     WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom))) {
            BITMAPINFO bi; ZeroMemory(&bi, sizeof bi);
            bi.bmiHeader.biSize = sizeof bi.bmiHeader;
            bi.bmiHeader.biWidth = (LONG)tw; bi.bmiHeader.biHeight = -(LONG)th;
            bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
            void *bits = NULL;
            HBITMAP b = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
            if (b && SUCCEEDED(IWICFormatConverter_CopyPixels(cv, NULL, tw * 4, tw * th * 4, (BYTE *)bits))) {
                p.bmp = b; p.w = (int)tw; p.h = (int)th;
            } else if (b) DeleteObject(b);
        }
    }
    if (cv) IWICFormatConverter_Release(cv);
    if (sc) IWICBitmapScaler_Release(sc);
    if (fr) IWICBitmapFrameDecode_Release(fr);
    if (d) IWICBitmapDecoder_Release(d);
    if (s) IWICStream_Release(s);
    IWICImagingFactory_Release(f);
    return p;
}

/* A picture card: the menu's picture, framed; a thick highlight when it's the chosen style. */
static void draw_pic(HWND dlg, DRAWITEMSTRUCT *di)
{
    int k = di->CtlID == IDC_IMG_CLASSIC;
    int chosen = IsDlgButtonChecked(dlg, k ? IDC_STYLE_CLASSIC : IDC_STYLE_MODERN) == BST_CHECKED;
    RECT r = di->rcItem;
    HDC dc = di->hDC;
    FillRect(dc, &r, GetSysColorBrush(COLOR_BTNFACE));
    HBRUSH frame = CreateSolidBrush(chosen ? GetSysColor(COLOR_HIGHLIGHT) : RGB(200, 200, 200));
    RECT fr = r;
    for (int i = 0; i < (chosen ? 3 : 1); i++) { FrameRect(dc, &fr, frame); InflateRect(&fr, -1, -1); }
    DeleteObject(frame);
    PIC *p = &g_pic[k];
    if (p->bmp) {
        HDC m = CreateCompatibleDC(dc);
        HGDIOBJ o = SelectObject(m, p->bmp);
        int x = r.left + (r.right - r.left - p->w) / 2, y = r.top + (r.bottom - r.top - p->h) / 2;
        BitBlt(dc, x, y, p->w, p->h, m, 0, 0, SRCCOPY);
        SelectObject(m, o);
        DeleteDC(m);
    }
}

/* ---------- setup window: four pages ---------- */
enum { PG_WELCOME, PG_STYLE, PG_OPTIONS, PG_INSTALL, PG_N };
static const int PAGE_CTLS[PG_N][6] = {
    { IDC_WELCOME, 0 },
    { IDC_IMG_MODERN, IDC_IMG_CLASSIC, IDC_STYLE_MODERN, IDC_STYLE_CLASSIC, 0 },
    { IDC_DESKTOP, IDC_STARTUP, IDC_PAWNIO, IDC_LAUNCH, IDC_NOTE, 0 },
    { IDC_STATIC_DIR, IDC_DIR, IDC_BROWSE, IDC_SUMMARY, 0 },
};
static int g_page, g_upgrade;
static wchar_t g_oldVer[64];

static void summary(HWND dlg)
{
    wchar_t s[700];
    int classic = IsDlgButtonChecked(dlg, IDC_STYLE_CLASSIC) == BST_CHECKED;
    fmt(s, 700, L"%s\n\n"
                L"\x2022  Menu style: %s\n"
                L"\x2022  Desktop shortcut: %s\n"
                L"\x2022  Start with Windows: %s\n"
                L"\x2022  CPU temperature support: %s\n"
                L"\x2022  Start TrayVitals when done: %s\n\n"
                L"Click %s to begin. Use Back to change anything.",
        g_upgrade ? L"Ready to update TrayVitals. Your settings are kept." : L"Ready to install TrayVitals.",
        classic ? L"Classic (Windows' standard menu)" : L"Modern",
        IsDlgButtonChecked(dlg, IDC_DESKTOP) == BST_CHECKED ? L"yes" : L"no",
        IsDlgButtonChecked(dlg, IDC_STARTUP) == BST_CHECKED ? L"yes" : L"no",
        g_pawnPresent ? L"already installed" : IsDlgButtonChecked(dlg, IDC_PAWNIO) == BST_CHECKED ? L"install PawnIO" : L"no",
        IsDlgButtonChecked(dlg, IDC_LAUNCH) == BST_CHECKED ? L"yes" : L"no",
        g_upgrade ? L"Update" : L"Install");
    SetDlgItemTextW(dlg, IDC_SUMMARY, s);
}

static void show_page(HWND dlg, int pg)
{
    static const wchar_t *TITLE[PG_N] = { NULL, L"Choose a menu style", L"Choose options", L"Choose where to install" };
    static const wchar_t *SUB[PG_N] = {
        NULL,
        L"This is what opens when you right-click TrayVitals. Click a picture to choose. "
        L"You can switch any time later from the menu (Menu style).",
        L"You can change these later from TrayVitals' menu.",
        L"TrayVitals will be installed in this folder.",
    };
    g_page = pg;
    for (int p = 0; p < PG_N; p++)
        for (int i = 0; PAGE_CTLS[p][i]; i++) ShowWindow(GetDlgItem(dlg, PAGE_CTLS[p][i]), p == pg ? SW_SHOW : SW_HIDE);
    wchar_t step[40];
    fmt(step, 40, L"Step %d of %d", pg + 1, PG_N);
    SetDlgItemTextW(dlg, IDC_STEP, step);
    if (pg == PG_WELCOME) {
        SetDlgItemTextW(dlg, IDC_TITLE, g_upgrade ? L"Update TrayVitals to " VERSION : L"Welcome to TrayVitals " VERSION);
        SetDlgItemTextW(dlg, IDC_STATIC_INFO, g_upgrade ? L"This takes a few seconds, and your settings are kept."
                                                        : L"This takes less than a minute.");
    } else {
        SetDlgItemTextW(dlg, IDC_TITLE, TITLE[pg]);
        SetDlgItemTextW(dlg, IDC_STATIC_INFO, SUB[pg]);
    }
    if (pg == PG_INSTALL) summary(dlg);
    EnableWindow(GetDlgItem(dlg, IDC_BACK), pg > 0);
    SetDlgItemTextW(dlg, IDOK, pg < PG_INSTALL ? L"&Continue >" : g_upgrade ? L"&Update" : L"&Install");
    SendMessageW(dlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(dlg, pg == PG_STYLE ? (IsDlgButtonChecked(dlg, IDC_STYLE_CLASSIC) == BST_CHECKED
                                                       ? IDC_STYLE_CLASSIC : IDC_STYLE_MODERN) : IDOK), TRUE);
}

static INT_PTR CALLBACK setup_proc(HWND dlg, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_INITDIALOG: {
        wchar_t prev[MAX_PATH], note[512] = L"", lnk[MAX_PATH], welcome[1200];
        int upgrade = reg_get_sz(HKEY_LOCAL_MACHINE, UNKEY, L"InstallLocation", prev);
        int fromOld = !upgrade && g_oldDir[0];   /* TraySense installed: move it over */
        const wchar_t *ukey = fromOld ? OLD_UNKEY : UNKEY;
        if (fromOld) { old_to_new_dir(prev); upgrade = 1; }
        g_upgrade = upgrade;
        SetDlgItemTextW(dlg, IDC_DIR, upgrade ? prev : g_dir);
        SendDlgItemMessageW(dlg, IDC_DIR, EM_LIMITTEXT, MAX_PATH - 16, 0);

        int hadDesktop = reg_get_sz(HKEY_LOCAL_MACHINE, ukey, L"DesktopShortcut", lnk) && exists(lnk);
        CheckDlgButton(dlg, IDC_DESKTOP, (!upgrade || hadDesktop) ? BST_CHECKED : BST_UNCHECKED);

        int runKey = RegGetValueW(HKEY_CURRENT_USER, RUNKEY, fromOld ? OLD_APP : APP, RRF_RT_REG_SZ, NULL, NULL, NULL) == ERROR_SUCCESS;
        CheckDlgButton(dlg, IDC_STARTUP, (!upgrade || runKey || g_hasTask) ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dlg, IDC_LAUNCH, BST_CHECKED);
        {   /* menu style: keep what TrayVitals has; new installs get the modern menu */
            DWORD st = 0, sz = sizeof st;
            if (RegGetValueW(HKEY_CURRENT_USER, SETKEY, L"MenuStyle", RRF_RT_REG_DWORD, NULL, &st, &sz)) {
                sz = sizeof st;
                RegGetValueW(HKEY_CURRENT_USER, OLD_SETKEY, L"MenuStyle", RRF_RT_REG_DWORD, NULL, &st, &sz);
            }
            CheckRadioButton(dlg, IDC_STYLE_MODERN, IDC_STYLE_CLASSIC, st == 1 ? IDC_STYLE_CLASSIC : IDC_STYLE_MODERN);
        }
        CheckDlgButton(dlg, IDC_PAWNIO, BST_CHECKED);
        if (g_pawnPresent) {
            SetDlgItemTextW(dlg, IDC_PAWNIO, L"CPU temperature support (PawnIO driver) is already installed");
            EnableWindow(GetDlgItem(dlg, IDC_PAWNIO), FALSE);
        }

        g_oldVer[0] = 0;
        if (upgrade) {
            wchar_t old[MAX_PATH], title[128];
            if (reg_get_sz(HKEY_LOCAL_MACHINE, ukey, L"DisplayVersion", old) && old[0] >= L'0' && old[0] <= L'9' &&
                wcschr(old, L'.') && !wcschr(old, L'-')) {
                lstrcpynW(g_oldVer, old, 64);
                fmt(title, 128, fromOld ? L"TrayVitals Setup - Update from TraySense %s to TrayVitals " VERSION
                                        : L"TrayVitals Setup - Update from %s to " VERSION, old);
            } else
                lstrcpyW(title, L"TrayVitals Setup - Update to " VERSION);
            SetWindowTextW(dlg, title);
        }
        if (g_hasTask)
            lstrcatW(note, L"TrayVitals already starts as administrator at login (needed for CPU temperature). "
                           L"Keeping \"Start with Windows\" on keeps that.");
        SetDlgItemTextW(dlg, IDC_NOTE, note);

        fmt(welcome, 1200,
            L"TrayVitals shows your PC's vital signs right on the taskbar, next to the clock: CPU, GPU, RAM and VRAM "
            L"usage, temperatures and power.\n\n"
            L"It's small and light, and it only goes online once a day to check for a new version "
            L"(you can turn that off).\n\n"
            L"%s%s%s%s"
            L"Click Continue to choose how TrayVitals looks and starts.",
            fromOld ? L"TraySense is now called TrayVitals. Your settings, shortcuts and startup choice move over, "
                      L"and the old TraySense files are removed.\n\n" : L"",
            g_oldVer[0] ? L"You have version " : upgrade ? L"An older TrayVitals is installed. " : L"",
            g_oldVer[0] ? g_oldVer : L"",
            g_oldVer[0] ? L" installed. Updating keeps all your settings.\n\n" : upgrade ? L"Updating keeps all your settings.\n\n" : L"");
        if (is_running()) {
            lstrcatW(welcome, L"\n\nTrayVitals is running and will be closed first.");
        }
        SetDlgItemTextW(dlg, IDC_WELCOME, welcome);

        HFONT f = (HFONT)SendMessageW(dlg, WM_GETFONT, 0, 0);
        LOGFONTW lf;
        if (f && GetObjectW(f, sizeof lf, &lf)) {
            LOGFONTW t = lf;
            t.lfWeight = FW_SEMIBOLD;
            t.lfHeight = lf.lfHeight * 3 / 2;
            g_titleFont = CreateFontIndirectW(&t);
            SendDlgItemMessageW(dlg, IDC_TITLE, WM_SETFONT, (WPARAM)g_titleFont, TRUE);
        }
        /* the two menu pictures, sized to their frames */
        for (int k = 0; k < 2; k++) {
            RECT r; GetClientRect(GetDlgItem(dlg, k ? IDC_IMG_CLASSIC : IDC_IMG_MODERN), &r);
            g_pic[k] = png_load(k ? IMG_CLASSIC : IMG_MODERN, r.right - 12, r.bottom - 12);
        }
        show_page(dlg, PG_WELCOME);
        SetForegroundWindow(dlg);
        return FALSE;   /* show_page set the focus */
    }
    case WM_DRAWITEM:
        if (wp == IDC_IMG_MODERN || wp == IDC_IMG_CLASSIC) { draw_pic(dlg, (DRAWITEMSTRUCT *)lp); return TRUE; }
        break;
    case WM_CTLCOLORSTATIC:
        if ((HWND)lp == GetDlgItem(dlg, IDC_STEP)) {   /* "Step 2 of 4" in gray */
            SetTextColor((HDC)wp, GetSysColor(COLOR_GRAYTEXT));
            SetBkColor((HDC)wp, GetSysColor(COLOR_BTNFACE));
            return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_IMG_MODERN:
        case IDC_IMG_CLASSIC:
            if (HIWORD(wp) == STN_CLICKED || HIWORD(wp) == STN_DBLCLK) {
                int c = LOWORD(wp) == IDC_IMG_CLASSIC ? IDC_STYLE_CLASSIC : IDC_STYLE_MODERN;
                CheckRadioButton(dlg, IDC_STYLE_MODERN, IDC_STYLE_CLASSIC, c);
                SendMessageW(dlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(dlg, c), TRUE);
                InvalidateRect(GetDlgItem(dlg, IDC_IMG_MODERN), NULL, FALSE);
                InvalidateRect(GetDlgItem(dlg, IDC_IMG_CLASSIC), NULL, FALSE);
                if (HIWORD(wp) == STN_DBLCLK) show_page(dlg, PG_STYLE + 1);   /* double-click: choose and go on */
            }
            return TRUE;
        case IDC_STYLE_MODERN:
        case IDC_STYLE_CLASSIC:
            InvalidateRect(GetDlgItem(dlg, IDC_IMG_MODERN), NULL, FALSE);
            InvalidateRect(GetDlgItem(dlg, IDC_IMG_CLASSIC), NULL, FALSE);
            return TRUE;
        case IDC_BROWSE: browse(dlg); return TRUE;
        case IDC_BACK:
            if (g_page > 0) show_page(dlg, g_page - 1);
            return TRUE;
        case IDOK: {
            if (g_page < PG_INSTALL) { show_page(dlg, g_page + 1); return TRUE; }
            wchar_t typed[MAX_PATH], dir[MAX_PATH];
            GetDlgItemTextW(dlg, IDC_DIR, typed, MAX_PATH);
            if (!normalize_dir(typed, dir)) {
                msg(dlg, L"Please choose a full folder path, like C:\\Program Files\\TrayVitals.", MB_ICONWARNING);
                return TRUE;
            }
            int r = SHCreateDirectoryExW(dlg, dir, NULL);
            DWORD a = GetFileAttributesW(dir);
            if ((r != ERROR_SUCCESS && r != ERROR_ALREADY_EXISTS && r != ERROR_FILE_EXISTS) ||
                a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_DIRECTORY)) {
                wchar_t t[MAX_PATH + 96];
                fmt(t, MAX_PATH + 96, L"Couldn't create this folder:\n%s\n\nPlease choose another one.", dir);
                msg(dlg, t, MB_ICONWARNING);
                return TRUE;
            }
            lstrcpyW(g_dir, dir);
            g_desktop = IsDlgButtonChecked(dlg, IDC_DESKTOP) == BST_CHECKED;
            g_startup = IsDlgButtonChecked(dlg, IDC_STARTUP) == BST_CHECKED;
            g_launch  = IsDlgButtonChecked(dlg, IDC_LAUNCH) == BST_CHECKED;
            g_pawn    = !g_pawnPresent && IsDlgButtonChecked(dlg, IDC_PAWNIO) == BST_CHECKED;
            g_style   = IsDlgButtonChecked(dlg, IDC_STYLE_CLASSIC) == BST_CHECKED ? 1 : 0;
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL: EndDialog(dlg, IDCANCEL); return TRUE;
        }
        break;
    case WM_DESTROY:
        if (g_titleFont) { DeleteObject(g_titleFont); g_titleFont = NULL; }
        for (int k = 0; k < 2; k++) if (g_pic[k].bmp) { DeleteObject(g_pic[k].bmp); g_pic[k].bmp = NULL; }
        break;
    }
    return FALSE;
}

/* ---------- install ---------- */

/* update: started by TrayVitals' "Update now" (setup /update). When TrayVitals is installed, update it
   without the setup window, keeping the same folder, shortcuts and "Start with Windows" choice. */
static int do_install(int update)
{
    wchar_t pf[MAX_PATH], prev[MAX_PATH], exe[MAX_PATH], unin[MAX_PATH], lnk[MAX_PATH], dlnk[MAX_PATH],
            self[MAX_PATH], buf[MAX_PATH * 2 + 512];
    if (!pf_dir(pf)) { msg(NULL, L"Couldn't find the Program Files folder.", MB_ICONERROR); return 1; }
    lstrcpyW(g_dir, pf);
    /* An old TraySense install, only when TrayVitals isn't installed yet. */
    int fromOld = !reg_get_sz(HKEY_LOCAL_MACHINE, UNKEY, L"InstallLocation", buf) && old_install();
    g_oldTask = fromOld && schtasks(L"/Query /TN " OLD_APP) == 0;
    g_hasTask = task_exists() || g_oldTask;
    g_pawnPresent = pawnio_installed();

    if (update && reg_get_sz(HKEY_LOCAL_MACHINE, UNKEY, L"InstallLocation", prev) && normalize_dir(prev, buf) &&
        exists(buf)) {
        lstrcpyW(g_dir, buf);
        g_desktop = reg_get_sz(HKEY_LOCAL_MACHINE, UNKEY, L"DesktopShortcut", lnk) && exists(lnk);
        g_startup = g_hasTask || RegGetValueW(HKEY_CURRENT_USER, RUNKEY, APP, RRF_RT_REG_SZ, NULL, NULL, NULL) == ERROR_SUCCESS;
        g_launch = 1;
        g_pawn = 0;   /* PawnIO stays as it is: installed, or left out by choice */
    } else if (update && fromOld) {   /* TraySense's "Update now": same choices, new name */
        old_to_new_dir(g_dir);
        g_desktop = reg_get_sz(HKEY_LOCAL_MACHINE, OLD_UNKEY, L"DesktopShortcut", lnk) && exists(lnk);
        g_startup = g_hasTask || RegGetValueW(HKEY_CURRENT_USER, RUNKEY, OLD_APP, RRF_RT_REG_SZ, NULL, NULL, NULL) == ERROR_SUCCESS;
        g_launch = 1;
        g_pawn = 0;
    } else if (DialogBoxW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_SETUP), NULL, setup_proc) != IDOK) return 1;
    if (fromOld && SHCreateDirectoryExW(NULL, g_dir, NULL) != ERROR_SUCCESS && !exists(g_dir)) {
        fmt(buf, MAX_PATH + 96, L"Couldn't create this folder:\n%s", g_dir);
        msg(NULL, buf, MB_ICONERROR);
        return 1;
    }

    reg_get_sz(HKEY_LOCAL_MACHINE, UNKEY, L"InstallLocation", prev);
    fmt(exe, MAX_PATH, L"%s\\TrayVitals.exe", g_dir);
    fmt(unin, MAX_PATH, L"%s\\uninstall.exe", g_dir);

    close_running();
    if (fromOld) copy_old_settings();   /* before anything below writes the new settings */
    if (!write_payload(exe)) {
        fmt(buf, MAX_PATH + 96, L"Couldn't write TrayVitals.exe to:\n%s", g_dir);
        msg(NULL, buf, MB_ICONERROR);
        return 1;
    }
    DWORD n = GetModuleFileNameW(NULL, self, MAX_PATH);
    if (n && n < MAX_PATH && lstrcmpiW(self, unin)) CopyFileW(self, unin, FALSE);

    /* Moved to a new folder: clear out the old one (only TrayVitals' own files). */
    if (prev[0] && lstrcmpiW(prev, g_dir) && lstrcmpiW(prev, pf)) remove_files(prev);

    /* A "Task" flag left behind without the task itself would make TrayVitals think it already
       starts at login; clear it so TrayVitals can set that up properly again. */
    if (!g_hasTask) reg_dw(HKEY_CURRENT_USER, SETKEY, L"Task", 0);

    /* Menu style picked in the setup window (an update with /update keeps the current one). */
    if (g_style >= 0) reg_dw(HKEY_CURRENT_USER, SETKEY, L"MenuStyle", (DWORD)g_style);

    /* Start with Windows. */
    int useTask = 0;
    if (g_startup) {
        if (g_hasTask) {
            /* The admin login task runs the Program Files copy; refresh it with this version. */
            wchar_t pfexe[MAX_PATH];
            fmt(pfexe, MAX_PATH, L"%s\\TrayVitals.exe", pf);
            CreateDirectoryW(pf, NULL);
            if (lstrcmpiW(pfexe, exe)) write_payload(pfexe);
            RegDeleteKeyValueW(HKEY_CURRENT_USER, RUNKEY, APP);
            if (g_oldTask && !task_exists()) {
                /* The old TraySense task: replace it with a TrayVitals one. TrayVitals makes its own task
                   (it needs the signed-in user's account), so setup asks the new copy to do that. */
                schtasks(L"/Delete /F /TN " OLD_APP);
                run_wait(pfexe, L"/make-task");
            }
            useTask = 1;
        } else {
            fmt(buf, MAX_PATH + 4, L"\"%s\"", exe);
            reg_sz(HKEY_CURRENT_USER, RUNKEY, APP, buf);
        }
    } else {
        RegDeleteKeyValueW(HKEY_CURRENT_USER, RUNKEY, APP);
        if (g_hasTask) {
            schtasks(L"/Delete /F /TN TrayVitals");
            if (g_oldTask) schtasks(L"/Delete /F /TN " OLD_APP);
            reg_dw(HKEY_CURRENT_USER, SETKEY, L"Task", 0);
            if (lstrcmpiW(pf, g_dir)) remove_files(pf);
        }
    }

    /* Shortcuts. */
    if (start_menu_lnk(lnk)) make_shortcut(lnk, exe, g_dir);
    int madeDesktop = 0;
    if (desktop_lnk(dlnk)) {
        if (g_desktop) madeDesktop = make_shortcut(dlnk, exe, g_dir);
        else DeleteFileW(dlnk);
    }

    /* An update over a version from before 1.7.0, which didn't record its version for TrayVitals:
       record the old one, so TrayVitals shows its patch notes when it starts. */
    if (prev[0] && RegGetValueW(HKEY_CURRENT_USER, SETKEY, L"LastVersion", RRF_RT_REG_SZ, NULL, NULL, NULL) != ERROR_SUCCESS) {
        wchar_t old[MAX_PATH];
        reg_sz(HKEY_CURRENT_USER, SETKEY, L"LastVersion",
               reg_get_sz(HKEY_LOCAL_MACHINE, UNKEY, L"DisplayVersion", old) ? old : L"older");
    }

    /* Settings > Apps entry. */
    fmt(buf, MAX_PATH + 32, L"\"%s\" /uninstall", unin);
    reg_sz(HKEY_LOCAL_MACHINE, UNKEY, L"UninstallString", buf);
    fmt(buf, MAX_PATH + 48, L"\"%s\" /uninstall /quiet", unin);
    reg_sz(HKEY_LOCAL_MACHINE, UNKEY, L"QuietUninstallString", buf);
    reg_sz(HKEY_LOCAL_MACHINE, UNKEY, L"DisplayName", L"TrayVitals");
    reg_sz(HKEY_LOCAL_MACHINE, UNKEY, L"DisplayVersion", VERSION);
    reg_sz(HKEY_LOCAL_MACHINE, UNKEY, L"Publisher", L"TrayVitals contributors");
    reg_sz(HKEY_LOCAL_MACHINE, UNKEY, L"DisplayIcon", exe);
    reg_sz(HKEY_LOCAL_MACHINE, UNKEY, L"InstallLocation", g_dir);
    if (madeDesktop) reg_sz(HKEY_LOCAL_MACHINE, UNKEY, L"DesktopShortcut", dlnk);
    else RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, UNKEY, L"DesktopShortcut");
    reg_dw(HKEY_LOCAL_MACHINE, UNKEY, L"NoModify", 1);
    reg_dw(HKEY_LOCAL_MACHINE, UNKEY, L"NoRepair", 1);
    reg_dw(HKEY_LOCAL_MACHINE, UNKEY, L"EstimatedSize", 400);

    /* Moved over from TraySense: remove its entry, shortcuts, startup entry, settings and files. */
    if (fromOld) cleanup_old();

    /* CPU temperature driver. */
    int pawn = PAWN_SKIPPED;
    if (g_pawn) {
        pawn = pawnio_install(pf);
        if (pawn != PAWN_FAILED) {
            reg_dw(HKEY_LOCAL_MACHINE, UNKEY, L"PawnIOBySetup", 1);
            /* Reading the sensor needs TrayVitals to run as administrator. Without the login task,
               let TrayVitals offer that switch again the next time it starts. */
            if (!(g_startup && g_hasTask)) RegDeleteKeyValueW(HKEY_CURRENT_USER, SETKEY, L"AskedCpuTemp");
        }
    }

    if (g_launch) {
        if (useTask) schtasks(L"/Run /TN TrayVitals");   /* starts it exactly as it starts at login */
        else launch_unelevated(exe);
    }

    /* No "finished" message: setup just closes (TrayVitals itself opens its patch notes after an update).
       Only speak up when the CPU temperature driver needs something from the user. */
    if (pawn == PAWN_OK_REBOOT)
        msg(NULL, L"TrayVitals " VERSION L" is installed.\n\n"
                  L"Restart Windows to finish installing CPU temperature support (PawnIO).", MB_ICONINFORMATION);
    else if (pawn == PAWN_FAILED)
        msg(NULL, L"TrayVitals " VERSION L" is installed, but CPU temperature support (PawnIO) couldn't be "
                  L"installed.\n\nYou can try again later from TrayVitals' menu: \"Turn on CPU temperature...\".",
            MB_ICONWARNING);
    return 0;
}

/* ---------- uninstall ---------- */

/* Removes everything. Returns the PawnIO result (PAWN_SKIPPED if it wasn't asked for or isn't there). */
static int do_uninstall_files(const wchar_t *dir, int removePawn)
{
    wchar_t pf[MAX_PATH], lnk[MAX_PATH];
    int pawn = PAWN_SKIPPED;
    close_running();   /* TrayVitals holds the PawnIO driver open; close it before removing the driver */
    if (removePawn && pawnio_installed()) pawn = pawnio_uninstall();
    schtasks(L"/Delete /F /TN TrayVitals");
    RegDeleteKeyValueW(HKEY_CURRENT_USER, RUNKEY, APP);
    RegDeleteTreeW(HKEY_CURRENT_USER, SETKEY);
    remove_files(dir);
    if (pf_dir(pf) && lstrcmpiW(pf, dir)) remove_files(pf);   /* the admin login task's copy */
    if (start_menu_lnk(lnk)) DeleteFileW(lnk);
    if (reg_get_sz(HKEY_LOCAL_MACHINE, UNKEY, L"DesktopShortcut", lnk)) DeleteFileW(lnk);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, UNKEY);
    return pawn;
}

static void removed_message(int pawn)
{
    const wchar_t *t =
        pawn == PAWN_OK        ? L"TrayVitals and the PawnIO driver have been removed." :
        pawn == PAWN_OK_REBOOT ? L"TrayVitals and the PawnIO driver have been removed.\n\n"
                                 L"Restart Windows to finish removing the driver." :
        pawn == PAWN_FAILED    ? L"TrayVitals has been removed, but the PawnIO driver couldn't be.\n\n"
                                 L"You can remove it from Settings > Apps > PawnIO." :
                                 L"TrayVitals has been removed.";
    msg(NULL, t, pawn == PAWN_FAILED ? MB_ICONWARNING : MB_ICONINFORMATION);
}

/* Asks to uninstall. Returns 0 = cancel, 1 = go; *removePawn says whether to take PawnIO too. */
static int confirm_uninstall(int pawnPresent, int *removePawn)
{
    *removePawn = pawnPresent;
    TASKDIALOGCONFIG tc;
    TASKDIALOG_BUTTON btn[1] = { { 100, L"Uninstall" } };
    ZeroMemory(&tc, sizeof tc);
    tc.cbSize = sizeof tc;
    tc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW |
                 (pawnPresent ? TDF_VERIFICATION_FLAG_CHECKED : 0);
    tc.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    tc.pszWindowTitle = L"Uninstall TrayVitals";
    tc.pszMainIcon = TD_WARNING_ICON;
    tc.pszMainInstruction = L"Uninstall TrayVitals?";
    tc.pszContent = L"This removes TrayVitals, its shortcuts, its settings and its startup entry.";
    tc.cButtons = 1;
    tc.pButtons = btn;
    tc.nDefaultButton = 100;
    if (pawnPresent) {
        tc.pszVerificationText = L"Also remove the PawnIO driver (used for CPU temperature)";
        tc.pszFooter = L"Untick this only if another app, such as LibreHardwareMonitor or Fan Control, also uses PawnIO.";
        tc.pszFooterIcon = TD_INFORMATION_ICON;
    }
    int button = 0;
    BOOL verify = FALSE;
    if (SUCCEEDED(TaskDialogIndirect(&tc, &button, NULL, &verify))) {
        if (button != 100) return 0;
        *removePawn = pawnPresent && verify;
        return 1;
    }
    /* Fallback if the task dialog isn't available. */
    if (msg(NULL, pawnPresent ? L"Remove TrayVitals, its shortcuts, settings and startup entry, "
                                L"and the PawnIO driver (used for CPU temperature)?"
                              : L"Remove TrayVitals, its shortcuts, its settings and its startup entry?",
            MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return 0;
    return 1;
}

static void installed_dir(wchar_t *dir)
{
    if (reg_get_sz(HKEY_LOCAL_MACHINE, UNKEY, L"InstallLocation", dir) && ends_with_trayvitals(dir)) return;
    /* Fall back to the folder this uninstall.exe lives in. */
    DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH);
    wchar_t *s = (n && n < MAX_PATH) ? wcsrchr(dir, L'\\') : NULL;
    if (s) *s = 0; else dir[0] = 0;
    if (!ends_with_trayvitals(dir)) dir[0] = 0;
}

static int do_uninstall(int quiet)
{
    wchar_t dir[MAX_PATH], self[MAX_PATH], tmp[MAX_PATH], tmpExe[MAX_PATH], args[MAX_PATH + 64];
    int pawnPresent = pawnio_installed(), removePawn = pawnPresent;
    if (!quiet && !confirm_uninstall(pawnPresent, &removePawn)) return 1;
    installed_dir(dir);
    /* uninstall.exe can't delete itself while it runs, so finish from a temporary copy. */
    DWORD n = GetModuleFileNameW(NULL, self, MAX_PATH);
    DWORD t = GetTempPathW(MAX_PATH - 40, tmp);
    if (n && n < MAX_PATH && t && t < MAX_PATH - 40) {
        fmt(tmpExe, MAX_PATH, L"%sTrayVitals-uninstall-%lu.exe", tmp, GetCurrentProcessId());
        if (CopyFileW(self, tmpExe, FALSE)) {
            fmt(args, MAX_PATH + 64, L"/uninstall-finish %lu %d %d \"%s\"", GetCurrentProcessId(), quiet, removePawn, dir);
            if ((INT_PTR)ShellExecuteW(NULL, L"open", tmpExe, args, tmp, SW_HIDE) > 32) return 0;
        }
    }
    int pawn = do_uninstall_files(dir, removePawn);
    if (!quiet) removed_message(pawn);
    return 0;
}

/* The temporary uninstaller copy removes itself (and copies left by earlier uninstalls)
   once it has exited, using a hidden cmd.exe that waits a couple of seconds first. */
static void delete_self_later(void)
{
    wchar_t self[MAX_PATH], tmp[MAX_PATH], pattern[MAX_PATH], sys[MAX_PATH], cmdexe[MAX_PATH], cmd[MAX_PATH * 3];
    DWORD n = GetModuleFileNameW(NULL, self, MAX_PATH);
    DWORD t = GetTempPathW(MAX_PATH - 40, tmp);
    if (!n || n >= MAX_PATH) return;
    /* Older copies from previous uninstalls (not running any more). */
    if (t && t < MAX_PATH - 40) {
        fmt(pattern, MAX_PATH, L"%sTrayVitals-uninstall-*.exe", tmp);
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                wchar_t p[MAX_PATH];
                fmt(p, MAX_PATH, L"%s%s", tmp, fd.cFileName);
                if (lstrcmpiW(p, self)) DeleteFileW(p);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    UINT s = GetSystemDirectoryW(sys, MAX_PATH - 16);
    if (s && s < MAX_PATH - 16) {
        fmt(cmdexe, MAX_PATH, L"%s\\cmd.exe", sys);
        fmt(cmd, MAX_PATH * 3, L"\"%s\" /d /c ping -n 4 127.0.0.1 >nul & del /f /q \"%s\"", cmdexe, self);
        STARTUPINFOW si; PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof si); si.cb = sizeof si;
        if (CreateProcessW(cmdexe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, sys, &si, &pi)) {
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
    }
    MoveFileExW(self, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);   /* backup: also gone after a restart */
}

static int do_uninstall_finish(DWORD parent, int quiet, int removePawn, const wchar_t *dirArg)
{
    wchar_t dir[MAX_PATH];
    HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, parent);
    if (p) { WaitForSingleObject(p, 10000); CloseHandle(p); }
    lstrcpynW(dir, dirArg, MAX_PATH);
    if (!ends_with_trayvitals(dir)) dir[0] = 0;
    int pawn = do_uninstall_files(dir, removePawn);
    if (!quiet) removed_message(pawn);
    delete_self_later();
    return 0;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE prev, LPWSTR cmdline, int show)
{
    (void)hi; (void)prev; (void)cmdline; (void)show;
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    /* uninstall.exe is a copy of setup: started under that name it always uninstalls,
       even when opened by double-click with no /uninstall switch. */
    wchar_t self[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, self, MAX_PATH);
    const wchar_t *base = (n && n < MAX_PATH) ? wcsrchr(self, L'\\') : NULL;
    int asUninstaller = base && !lstrcmpiW(base + 1, L"uninstall.exe");
    int quiet = 0, update = 0;
    for (int i = 1; argv && i < argc; i++) {
        if (!lstrcmpiW(argv[i], L"/quiet") || !lstrcmpiW(argv[i], L"/S")) quiet = 1;
        if (!lstrcmpiW(argv[i], L"/update")) update = 1;
    }

    int r;
    if (argv && argc >= 6 && !lstrcmpiW(argv[1], L"/uninstall-finish"))
        r = do_uninstall_finish((DWORD)_wtoi(argv[2]), _wtoi(argv[3]), _wtoi(argv[4]), argv[5]);
    else if (asUninstaller || (argv && argc >= 2 && !lstrcmpiW(argv[1], L"/uninstall")))
        r = do_uninstall(quiet);
    else
        r = do_install(update);
    CoUninitialize();
    if (argv) LocalFree(argv);
    return r;
}
