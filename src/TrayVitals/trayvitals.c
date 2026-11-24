#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <dxgi.h>
#include <wchar.h>
#include <math.h>
#include <winioctl.h>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#include <winhttp.h>
#include <bcrypt.h>
#include <sddl.h>
#include <uiautomationclient.h>
#include <richedit.h>
#include <commctrl.h>
#include <shlobj.h>
#include "../version.h"
#include "patchnotes.h"

static HMODULE sysdll(const wchar_t *name)
{
    return LoadLibraryExW(name, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
}

typedef PDH_STATUS (WINAPI *T_PdhOpenQueryW)(LPCWSTR, DWORD_PTR, PDH_HQUERY *);
typedef PDH_STATUS (WINAPI *T_PdhAddEnglishCounterW)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER *);
typedef PDH_STATUS (WINAPI *T_PdhCollectQueryData)(PDH_HQUERY);
typedef PDH_STATUS (WINAPI *T_PdhGetFormattedCounterArrayW)(PDH_HCOUNTER, DWORD, LPDWORD, LPDWORD, PPDH_FMT_COUNTERVALUE_ITEM_W);
typedef PDH_STATUS (WINAPI *T_PdhCloseQuery)(PDH_HQUERY);
typedef HINTERNET (WINAPI *T_WinHttpOpen)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
typedef HINTERNET (WINAPI *T_WinHttpConnect)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
typedef HINTERNET (WINAPI *T_WinHttpOpenRequest)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR *, DWORD);
typedef BOOL (WINAPI *T_WinHttpSendRequest)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
typedef BOOL (WINAPI *T_WinHttpReceiveResponse)(HINTERNET, LPVOID);
typedef BOOL (WINAPI *T_WinHttpQueryHeaders)(HINTERNET, DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD);
typedef BOOL (WINAPI *T_WinHttpQueryDataAvailable)(HINTERNET, LPDWORD);
typedef BOOL (WINAPI *T_WinHttpReadData)(HINTERNET, LPVOID, DWORD, LPDWORD);
typedef BOOL (WINAPI *T_WinHttpCloseHandle)(HINTERNET);
typedef NTSTATUS (WINAPI *T_BCryptOpenAlgorithmProvider)(BCRYPT_ALG_HANDLE *, LPCWSTR, LPCWSTR, ULONG);
typedef NTSTATUS (WINAPI *T_BCryptCreateHash)(BCRYPT_ALG_HANDLE, BCRYPT_HASH_HANDLE *, PUCHAR, ULONG, PUCHAR, ULONG, ULONG);
typedef NTSTATUS (WINAPI *T_BCryptHashData)(BCRYPT_HASH_HANDLE, PUCHAR, ULONG, ULONG);
typedef NTSTATUS (WINAPI *T_BCryptFinishHash)(BCRYPT_HASH_HANDLE, PUCHAR, ULONG, ULONG);
typedef NTSTATUS (WINAPI *T_BCryptDestroyHash)(BCRYPT_HASH_HANDLE);
typedef NTSTATUS (WINAPI *T_BCryptCloseAlgorithmProvider)(BCRYPT_ALG_HANDLE, ULONG);

#define FN(n) static T_##n p_##n;
FN(PdhOpenQueryW) FN(PdhAddEnglishCounterW) FN(PdhCollectQueryData) FN(PdhGetFormattedCounterArrayW) FN(PdhCloseQuery)
FN(WinHttpOpen) FN(WinHttpConnect) FN(WinHttpOpenRequest) FN(WinHttpSendRequest) FN(WinHttpReceiveResponse)
FN(WinHttpQueryHeaders) FN(WinHttpQueryDataAvailable) FN(WinHttpReadData) FN(WinHttpCloseHandle)
FN(BCryptOpenAlgorithmProvider) FN(BCryptCreateHash) FN(BCryptHashData) FN(BCryptFinishHash)
FN(BCryptDestroyHash) FN(BCryptCloseAlgorithmProvider)
#undef FN
#define GETFN(m, n) ((p_##n = (T_##n)(void *)GetProcAddress(m, #n)) != NULL)

static int load_pdh(void)
{
    static int done, ok;
    if (done) return ok;
    done = 1;
    HMODULE m = sysdll(L"pdh.dll");
    ok = m && GETFN(m, PdhOpenQueryW) && GETFN(m, PdhAddEnglishCounterW) && GETFN(m, PdhCollectQueryData) &&
         GETFN(m, PdhGetFormattedCounterArrayW) && GETFN(m, PdhCloseQuery);
    return ok;
}

static int load_net(void)
{
    static int done, ok;
    if (done) return ok;
    done = 1;
    HMODULE w = sysdll(L"winhttp.dll"), b = sysdll(L"bcrypt.dll");
    ok = w && b && GETFN(w, WinHttpOpen) && GETFN(w, WinHttpConnect) && GETFN(w, WinHttpOpenRequest) &&
         GETFN(w, WinHttpSendRequest) && GETFN(w, WinHttpReceiveResponse) && GETFN(w, WinHttpQueryHeaders) &&
         GETFN(w, WinHttpQueryDataAvailable) && GETFN(w, WinHttpReadData) && GETFN(w, WinHttpCloseHandle) &&
         GETFN(b, BCryptOpenAlgorithmProvider) && GETFN(b, BCryptCreateHash) && GETFN(b, BCryptHashData) &&
         GETFN(b, BCryptFinishHash) && GETFN(b, BCryptDestroyHash) && GETFN(b, BCryptCloseAlgorithmProvider);
    return ok;
}

#define PdhOpenQueryW                p_PdhOpenQueryW
#define PdhAddEnglishCounterW        p_PdhAddEnglishCounterW
#define PdhCollectQueryData          p_PdhCollectQueryData
#define PdhGetFormattedCounterArrayW p_PdhGetFormattedCounterArrayW
#define PdhCloseQuery                p_PdhCloseQuery
#define WinHttpOpen                  p_WinHttpOpen
#define WinHttpConnect               p_WinHttpConnect
#define WinHttpOpenRequest           p_WinHttpOpenRequest
#define WinHttpSendRequest           p_WinHttpSendRequest
#define WinHttpReceiveResponse       p_WinHttpReceiveResponse
#define WinHttpQueryHeaders          p_WinHttpQueryHeaders
#define WinHttpQueryDataAvailable    p_WinHttpQueryDataAvailable
#define WinHttpReadData              p_WinHttpReadData
#define WinHttpCloseHandle           p_WinHttpCloseHandle
#define BCryptOpenAlgorithmProvider  p_BCryptOpenAlgorithmProvider
#define BCryptCreateHash             p_BCryptCreateHash
#define BCryptHashData               p_BCryptHashData
#define BCryptFinishHash             p_BCryptFinishHash
#define BCryptDestroyHash            p_BCryptDestroyHash
#define BCryptCloseAlgorithmProvider p_BCryptCloseAlgorithmProvider

static int sys_path(wchar_t *out, const wchar_t *file)
{
    UINT n = GetSystemDirectoryW(out, MAX_PATH);
    if (!n || n + lstrlenW(file) + 2 >= MAX_PATH) return 0;
    lstrcatW(out, L"\\");
    lstrcatW(out, file);
    return 1;
}

static HANDLE g_self = INVALID_HANDLE_VALUE;

static void lock_self(void)
{
    wchar_t p[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, p, MAX_PATH);
    if (n && n < MAX_PATH)
        g_self = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
}

static int copy_self(const wchar_t *dst)
{
    LARGE_INTEGER sz, zero = { 0 };
    DWORD got, w;
    int ok = 0;
    if (g_self == INVALID_HANDLE_VALUE || !GetFileSizeEx(g_self, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (16 << 20))
        return 0;
    BYTE *buf = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)sz.QuadPart);
    if (!buf) return 0;
    if (SetFilePointerEx(g_self, zero, NULL, FILE_BEGIN) &&
        ReadFile(g_self, buf, (DWORD)sz.QuadPart, &got, NULL) && got == (DWORD)sz.QuadPart) {
        HANDLE f = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            ok = WriteFile(f, buf, got, &w, NULL) && w == got;
            CloseHandle(f);
            if (!ok) DeleteFileW(dst);
        }
    }
    HeapFree(GetProcessHeap(), 0, buf);
    return ok;
}

static int app_dir(wchar_t *out)
{
    DWORD sz = MAX_PATH * sizeof(wchar_t);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", L"ProgramFilesDir",
                     RRF_RT_REG_SZ, NULL, out, &sz) != ERROR_SUCCESS) return 0;
    if (lstrlenW(out) > MAX_PATH - 48) return 0;
    lstrcatW(out, L"\\TrayVitals");
    if (!CreateDirectoryW(out, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    DWORD a = GetFileAttributesW(out);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) && !(a & FILE_ATTRIBUTE_REPARSE_POINT);
}

#define APP      L"TrayVitals"
#ifndef REGKEY
#define REGKEY   L"Software\\TrayVitals"   /* preview builds keep their settings apart */
#endif
#ifndef MAIN_CLASS
#define MAIN_CLASS APP                    /* the hidden main window; a second launch finds it by this */
#endif
#ifndef TV_MENU_MODERN
#define TV_MENU_MODERN 1                  /* 1: TrayVitals' own menu panel, 0: Windows' menu, themed */
#endif
#define RUNKEY   L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define WM_TRAY  (WM_APP + 1)

/* Look: theme (THEMES in ui.h), see-through menus/popups (Off/Low/Medium/High), label opacity. */
static DWORD g_theme, g_uiAlpha, g_labelAlpha;
static DWORD g_menuStyle = TV_MENU_MODERN ? 0 : 1;   /* 0: modern panel, 1: Windows' menu ("MenuStyle") */
static const int LABEL_ALPHA_PCT[4] = { 100, 85, 70, 55 };

/* New sensors go at the end: each one's bit in the saved "Mask2" setting must not move. */
enum { M_CPU, M_CPUT, M_RAM, M_GPU, M_GPUT, M_VRAM, M_DISKT, M_CPUP, M_GPUP, M_N };
static const wchar_t *NAMES[M_N] = { L"CPU usage", L"CPU temperature", L"RAM usage",
                                     L"GPU usage", L"GPU temperature", L"VRAM usage", L"SSD temperature",
                                     L"CPU power", L"GPU power" };
/* Order sensors are listed in the menu and added to the tray. */
static const int ORDER[M_N] = { M_CPU, M_CPUT, M_CPUP, M_RAM, M_GPU, M_GPUT, M_GPUP, M_VRAM, M_DISKT };
#define MASK_POWER ((1u << M_CPUP) | (1u << M_GPUP))
#define MASK_VER   2   /* settings saved before version 2 didn't know the power sensors */

static const COLORREF COL_D[M_N] = { RGB(80,190,255), 0, RGB(185,145,255), RGB(118,215,90), 0, RGB(118,215,90), 0,
                                     RGB(80,190,255), RGB(118,215,90) };
static const COLORREF COL_L[M_N] = { RGB(0,100,180),  0, RGB(110,60,200),  RGB(35,135,30),  0, RGB(35,135,30), 0,
                                     RGB(0,100,180),  RGB(35,135,30) };

static HWND     g_wnd;
static UINT     g_taskbarCreated;
static UINT     g_showMenuMsg;   /* sent by a second launch: open the menu (it may be the only way to reach it) */
static DWORD    g_mask = (1u << M_N) - 1, g_interval = 2000;
static double   g_val[M_N];
static int      g_ok[M_N], g_size, g_light;
static wchar_t  g_gpuName[64];
static double   g_ramUsed, g_ramTotal, g_vramUsed;

static PDH_HQUERY   g_q;
static PDH_HCOUNTER g_cTherm, g_cEng, g_cMem;
static BYTE        *g_buf;
static DWORD        g_bufSz;

static LUID       g_luid;
static int        g_hasGpu;
static ULONGLONG  g_vramTotal;
static wchar_t    g_luidStr[40];
static UINT       g_kmt;
typedef LONG (APIENTRY *PFN_KMT)(void *);
static PFN_KMT    pKmtQuery;
typedef struct { LUID Luid; UINT hAdapter; } KMT_OPEN;
typedef struct { UINT hAdapter; UINT Type; void *pData; UINT Size; } KMT_QAI;
typedef struct { UINT32 PhysIdx; ULONGLONG MemFreq, MaxMemFreq, MaxMemFreqOC, MemBw, PcieBw;
                 ULONG FanRPM, Power, Temperature; UCHAR PowerStateOverride; } KMT_PERF;
#define KMTQAITYPE_ADAPTERPERFDATA 62
#define KMTQAITYPE_ADAPTERADDRESS  6
typedef struct { UINT Bus, Device, Function; } KMT_ADDR;               /* D3DKMT_ADAPTERADDRESS */
typedef struct { char busIdLegacy[16]; unsigned domain, bus, device, pciDeviceId, pciSubSystemId;
                 char busId[32]; } NV_PCI;                               /* nvmlPciInfo_t (v3) */
typedef int (*PFN_NVTEMP)(void *, int, unsigned *);
static PFN_NVTEMP pNvTemp;
typedef int (*PFN_NVPOWER)(void *, unsigned *);   /* nvmlDeviceGetPowerUsage: milliwatts */
static PFN_NVPOWER pNvPower;
static void      *g_nvDev;
static UINT       g_gpuVendor;   /* PCI vendor of the chosen GPU (0x10DE = NVIDIA) */

static const GUID IID_F1 = {0x770aae78,0xf26f,0x4dba,{0xa8,0x29,0x25,0x3c,0x83,0xd1,0xb3,0x87}};
static const GUID G_DISPLAY = {0x6fe69556,0x704a,0x47a0,{0x8f,0x24,0xc2,0x8d,0x93,0x6f,0xda,0x47}};

static ULONGLONG ft(FILETIME f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; }

static void read_cpu(void)
{
    static ULONGLONG pi, pt;
    FILETIME i, k, u;
    if (!GetSystemTimes(&i, &k, &u)) return;
    ULONGLONG id = ft(i), tot = ft(k) + ft(u);
    if (pt && tot > pt) {
        g_val[M_CPU] = 100.0 * (1.0 - (double)(id - pi) / (double)(tot - pt));
        g_ok[M_CPU] = 1;
    }
    pi = id; pt = tot;
}

static void read_ram(void)
{
    MEMORYSTATUSEX m = { sizeof m };
    if (!GlobalMemoryStatusEx(&m)) return;
    g_ramTotal = m.ullTotalPhys / 1073741824.0;
    g_ramUsed  = (m.ullTotalPhys - m.ullAvailPhys) / 1073741824.0;
    g_val[M_RAM] = 100.0 * (double)(m.ullTotalPhys - m.ullAvailPhys) / (double)m.ullTotalPhys;
    g_ok[M_RAM] = 1;
}

static PDH_FMT_COUNTERVALUE_ITEM_W *pdh_items(PDH_HCOUNTER c, DWORD *n)
{
    DWORD sz = g_bufSz;
    *n = 0;
    if (!c) return NULL;
    PDH_STATUS s = PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &sz, n,
                                                (PPDH_FMT_COUNTERVALUE_ITEM_W)g_buf);
    if ((DWORD)s == (DWORD)PDH_MORE_DATA) {
        BYTE *nb = g_buf ? HeapReAlloc(GetProcessHeap(), 0, g_buf, sz)
                         : HeapAlloc(GetProcessHeap(), 0, sz);
        if (!nb) return NULL;
        g_buf = nb; g_bufSz = sz;
        s = PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &sz, n,
                                         (PPDH_FMT_COUNTERVALUE_ITEM_W)g_buf);
    }
    return s == ERROR_SUCCESS ? (PDH_FMT_COUNTERVALUE_ITEM_W *)g_buf : NULL;
}

static int good(PDH_FMT_COUNTERVALUE_ITEM_W *it)
{
    return it->FmtValue.CStatus == PDH_CSTATUS_VALID_DATA || it->FmtValue.CStatus == PDH_CSTATUS_NEW_DATA;
}

static int has_luid(const wchar_t *s)
{
    size_t L = wcslen(g_luidStr);
    for (; *s; s++)
        if (_wcsnicmp(s, g_luidStr, L) == 0) return 1;
    return 0;
}

static void read_therm(void)
{
    DWORD n; double mx = 0;
    PDH_FMT_COUNTERVALUE_ITEM_W *it = pdh_items(g_cTherm, &n);
    g_ok[M_CPUT] = 0;
    if (!it) return;
    for (DWORD i = 0; i < n; i++)
        if (good(&it[i]) && it[i].FmtValue.doubleValue > mx) mx = it[i].FmtValue.doubleValue;
    if (mx > 200.0) { g_val[M_CPUT] = mx - 273.15; g_ok[M_CPUT] = 1; }
}

static void read_gpu_usage(void)
{
    DWORD n; double eng[64] = {0}, mx = 0;
    PDH_FMT_COUNTERVALUE_ITEM_W *it = pdh_items(g_cEng, &n);
    g_ok[M_GPU] = 0;
    if (!it) return;
    for (DWORD i = 0; i < n; i++) {
        if (!good(&it[i]) || !has_luid(it[i].szName)) continue;
        const wchar_t *e = wcsstr(it[i].szName, L"_eng_");
        if (!e) continue;
        int k = _wtoi(e + 5);
        if (k >= 0 && k < 64) eng[k] += it[i].FmtValue.doubleValue;
    }
    for (int k = 0; k < 64; k++) if (eng[k] > mx) mx = eng[k];
    g_val[M_GPU] = mx > 100.0 ? 100.0 : mx;
    g_ok[M_GPU] = 1;
}

static void read_vram(void)
{
    DWORD n; double used = 0; int found = 0;
    PDH_FMT_COUNTERVALUE_ITEM_W *it = pdh_items(g_cMem, &n);
    g_ok[M_VRAM] = 0;
    if (!it || !g_vramTotal) return;
    for (DWORD i = 0; i < n; i++)
        if (good(&it[i]) && has_luid(it[i].szName)) { used += it[i].FmtValue.doubleValue; found = 1; }
    if (!found) return;
    g_vramUsed = used / 1073741824.0;
    g_val[M_VRAM] = 100.0 * used / (double)g_vramTotal;
    g_ok[M_VRAM] = 1;
}

static int kmt_temp(double *t)
{
    if (!g_kmt || !pKmtQuery) return 0;
    KMT_PERF p; ZeroMemory(&p, sizeof p);
    KMT_QAI q = { g_kmt, KMTQAITYPE_ADAPTERPERFDATA, &p, sizeof p };
    if (pKmtQuery(&q) != 0 || p.Temperature == 0) return 0;
    *t = p.Temperature / 10.0;
    return 1;
}

static void read_gpu_temp(void)
{
    double t; unsigned nt;
    g_ok[M_GPUT] = 0;
    if (kmt_temp(&t)) { g_val[M_GPUT] = t; g_ok[M_GPUT] = 1; }
    else if (pNvTemp && pNvTemp(g_nvDev, 0, &nt) == 0) { g_val[M_GPUT] = nt; g_ok[M_GPUT] = 1; }
}

/* AMD Radeon: the GPU's power in watts, from the AMD driver's own library (atiadlxx.dll, "ADL").
   Structures and numbers are from AMD's ADL SDK
   (github.com/GPUOpen-LibrariesAndSDKs/display-library, MIT license). */
#define ADL_MAX_PATH 256
typedef struct { int iSize, iAdapterIndex; char strUDID[ADL_MAX_PATH];
                 int iBusNumber, iDeviceNumber, iFunctionNumber, iVendorID;
                 char strAdapterName[ADL_MAX_PATH], strDisplayName[ADL_MAX_PATH];
                 int iPresent, iExist;
                 char strDriverPath[ADL_MAX_PATH], strDriverPathExt[ADL_MAX_PATH], strPNPString[ADL_MAX_PATH];
                 int iOSDisplayIndex; } ADL_ADAPTERINFO;                         /* AdapterInfo (Windows) */
typedef struct { int size; struct { int supported, value; } sensors[256]; } ADL_PMLOG;   /* ADLPMLogDataOutput */
#define ADL_PMLOG_ASIC_POWER  23    /* watts; RX 5000 / 6000 */
#define ADL_PMLOG_BOARD_POWER 73    /* watts; newer cards, where ASIC_POWER isn't the whole card */
#define ADL_OD6_TOTAL_POWER   0     /* ODN_GPU_TOTAL_POWER; older cards (RX 400 / 500, Vega), watts * 256 */
typedef void *(__stdcall *ADL_MALLOC)(int);
typedef int (*PFN_ADLCREATE)(ADL_MALLOC, int, void **);
typedef int (*PFN_ADLDESTROY)(void *);
typedef int (*PFN_ADLCOUNT)(void *, int *);
typedef int (*PFN_ADLINFO)(void *, ADL_ADAPTERINFO *, int);
typedef int (*PFN_ADLPMLOG)(void *, int, ADL_PMLOG *);
typedef int (*PFN_ADLOD6POWER)(void *, int, int, int *);
static void           *g_adl;
static int             g_adlIdx;
static ADL_PMLOG      *g_adlLog;
static PFN_ADLPMLOG    pAdlPmLog;
static PFN_ADLOD6POWER pAdlOd6Power;
#ifndef ADL_LOAD
#define ADL_LOAD() sysdll(L"atiadlxx.dll")
#endif

static void *__stdcall adl_alloc(int n) { return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n > 0 ? (SIZE_T)n : 1); }

/* Only values from 1 to 999 W count; anything else is treated as "no reading". */
static int adl_power(double *w)
{
    if (!g_adl) return 0;
    if (pAdlPmLog && g_adlLog) {
        ZeroMemory(g_adlLog, sizeof *g_adlLog);
        g_adlLog->size = sizeof *g_adlLog;
        if (pAdlPmLog(g_adl, g_adlIdx, g_adlLog) == 0) {
            static const int ids[2] = { ADL_PMLOG_BOARD_POWER, ADL_PMLOG_ASIC_POWER };
            for (int i = 0; i < 2; i++) {
                int v = g_adlLog->sensors[ids[i]].value;
                if (g_adlLog->sensors[ids[i]].supported && v > 0 && v < 1000) { *w = v; return 1; }
            }
        }
    }
    int v = 0;
    if (pAdlOd6Power && pAdlOd6Power(g_adl, g_adlIdx, ADL_OD6_TOTAL_POWER, &v) == 0 && v >= 256 && v < 1000 * 256) {
        *w = v / 256.0;
        return 1;
    }
    return 0;
}

/* Opens ADL and finds the same card Windows picked, by its PCI bus number (ADL lists one entry per
   display output, so a card can appear several times). Without a bus number it's only safe when
   every entry is on one bus, which means a single AMD GPU. */
static void init_adl(unsigned bus)
{
    HMODULE m = ADL_LOAD();
    if (!m) return;
    PFN_ADLCREATE   create  = (PFN_ADLCREATE)(void *)GetProcAddress(m, "ADL2_Main_Control_Create");
    PFN_ADLDESTROY  destroy = (PFN_ADLDESTROY)(void *)GetProcAddress(m, "ADL2_Main_Control_Destroy");
    PFN_ADLCOUNT    count   = (PFN_ADLCOUNT)(void *)GetProcAddress(m, "ADL2_Adapter_NumberOfAdapters_Get");
    PFN_ADLINFO     info    = (PFN_ADLINFO)(void *)GetProcAddress(m, "ADL2_Adapter_AdapterInfo_Get");
    PFN_ADLPMLOG    log     = (PFN_ADLPMLOG)(void *)GetProcAddress(m, "ADL2_New_QueryPMLogData_Get");
    PFN_ADLOD6POWER od6     = (PFN_ADLOD6POWER)(void *)GetProcAddress(m, "ADL2_Overdrive6_CurrentPower_Get");
    void *ctx = NULL;
    int n = 0, idx = -1;
    if (create && destroy && count && info && (log || od6) && create(adl_alloc, 1, &ctx) == 0 && ctx
        && count(ctx, &n) == 0 && n > 0 && n <= 256) {
        ADL_ADAPTERINFO *ai = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)n * sizeof *ai);
        if (ai) {
            for (int i = 0; i < n; i++) ai[i].iSize = sizeof *ai;
            if (info(ctx, ai, n * (int)sizeof *ai) == 0) {
                int oneBus = 1;
                for (int i = 0; i < n && idx < 0; i++)
                    if (bus != ~0u && (unsigned)ai[i].iBusNumber == bus) idx = ai[i].iAdapterIndex;
                for (int i = 1; i < n; i++)
                    if (ai[i].iBusNumber != ai[0].iBusNumber) oneBus = 0;
                if (idx < 0 && oneBus) idx = ai[0].iAdapterIndex;
            }
            HeapFree(GetProcessHeap(), 0, ai);
        }
    }
    if (idx >= 0) {
        g_adl = ctx; g_adlIdx = idx; pAdlPmLog = log; pAdlOd6Power = od6;
        if (log) g_adlLog = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *g_adlLog);
    } else {
        if (ctx) destroy(ctx);
        FreeLibrary(m);
    }
}

/* Board power in watts, from the NVIDIA or AMD driver. */
static void read_gpu_power(void)
{
    unsigned mw;
    double w;
    g_ok[M_GPUP] = 0;
    if (pNvPower && pNvPower(g_nvDev, &mw) == 0 && mw < 2000000) { g_val[M_GPUP] = mw / 1000.0; g_ok[M_GPUP] = 1; }
    else if (adl_power(&w)) { g_val[M_GPUP] = w; g_ok[M_GPUP] = 1; }
}

static void init_gpu(void)
{
    HMODULE d = sysdll(L"dxgi.dll");
    if (d) {
        typedef HRESULT (WINAPI *CF)(const GUID *, void **);
        CF cf = (CF)(void *)GetProcAddress(d, "CreateDXGIFactory1");
        IDXGIFactory1 *f;
        if (cf && SUCCEEDED(cf(&IID_F1, (void **)&f))) {
            IDXGIAdapter1 *a;
            for (UINT i = 0; IDXGIFactory1_EnumAdapters1(f, i, &a) != DXGI_ERROR_NOT_FOUND; i++) {
                DXGI_ADAPTER_DESC1 dd;
                if (SUCCEEDED(IDXGIAdapter1_GetDesc1(a, &dd)) && !(dd.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    && dd.VendorId != 0x1414 && (!g_hasGpu || dd.DedicatedVideoMemory > g_vramTotal)) {
                    g_vramTotal = dd.DedicatedVideoMemory;
                    g_luid = dd.AdapterLuid;
                    g_gpuVendor = dd.VendorId;
                    g_hasGpu = 1;
                    lstrcpynW(g_gpuName, dd.Description, 64);
                }
                IDXGIAdapter1_Release(a);
            }
            IDXGIFactory1_Release(f);
        }
    }
    if (!g_hasGpu) return;
    _snwprintf(g_luidStr, 40, L"luid_0x%08X_0x%08X", (unsigned)g_luid.HighPart, (unsigned)g_luid.LowPart);

    HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    PFN_KMT pOpen = (PFN_KMT)(void *)GetProcAddress(gdi, "D3DKMTOpenAdapterFromLuid");
    pKmtQuery     = (PFN_KMT)(void *)GetProcAddress(gdi, "D3DKMTQueryAdapterInfo");
    KMT_OPEN o = { g_luid, 0 };
    if (pOpen && pOpen(&o) == 0) g_kmt = o.hAdapter;

    /* NVIDIA's library: the GPU's power in watts (Windows only reports it as a share of the limit),
       and its temperature when Windows doesn't report that. Only when the chosen card is NVIDIA,
       and only for that same card: NVIDIA's numbering ignores other brands and can differ from
       Windows', so the card is matched by its PCI bus number. */
    if (g_gpuVendor != 0x10DE && g_gpuVendor != 0x1002) return;
    unsigned bus = ~0u;
    if (g_kmt && pKmtQuery) {
        KMT_ADDR ad; ZeroMemory(&ad, sizeof ad);
        KMT_QAI q = { g_kmt, KMTQAITYPE_ADAPTERADDRESS, &ad, sizeof ad };
        if (pKmtQuery(&q) == 0) bus = ad.Bus;
    }
    if (g_gpuVendor == 0x1002) { init_adl(bus); return; }   /* AMD: power through ADL */
    HMODULE nv = sysdll(L"nvml.dll");
    if (!nv) nv = LoadLibraryExW(L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll", NULL,
                                 LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!nv) return;
    int (*init)(void) = (int (*)(void))(void *)GetProcAddress(nv, "nvmlInit_v2");
    int (*cnt)(unsigned *) = (int (*)(unsigned *))(void *)GetProcAddress(nv, "nvmlDeviceGetCount_v2");
    int (*hnd)(unsigned, void **) = (int (*)(unsigned, void **))(void *)GetProcAddress(nv, "nvmlDeviceGetHandleByIndex_v2");
    int (*pci)(void *, NV_PCI *) = (int (*)(void *, NV_PCI *))(void *)GetProcAddress(nv, "nvmlDeviceGetPciInfo_v3");
    PFN_NVTEMP tp = (PFN_NVTEMP)(void *)GetProcAddress(nv, "nvmlDeviceGetTemperature");
    PFN_NVPOWER pp = (PFN_NVPOWER)(void *)GetProcAddress(nv, "nvmlDeviceGetPowerUsage");
    unsigned n = 0;
    void *dev = NULL;
    if (init && cnt && hnd && tp && init() == 0 && cnt(&n) == 0) {
        for (unsigned i = 0; i < n && !dev; i++) {
            void *d; NV_PCI pi;
            if (hnd(i, &d) != 0) continue;
            ZeroMemory(&pi, sizeof pi);
            if (bus != ~0u && pci && pci(d, &pi) == 0 && pi.bus == bus) dev = d;
        }
        /* Couldn't match by bus: safe only when there is a single NVIDIA card. */
        if (!dev && n == 1 && hnd(0, &dev) != 0) dev = NULL;
    }
    if (dev) { g_nvDev = dev; pNvTemp = tp; pNvPower = pp; }
    else FreeLibrary(nv);
}

#define IOCTL_PIO_LOAD 0xA1B22084u
#define IOCTL_PIO_EXEC 0xA1B22104u
enum { PIO_UNTRIED, PIO_OK, PIO_NODRIVER, PIO_NOMODULE, PIO_NOADMIN, PIO_FAILED, PIO_UNSUPPORTED };
static int              g_pioState;
static HANDLE           g_pio = INVALID_HANDLE_VALUE, g_pciMutex;
static int              g_cpuVendor, g_tjmax, g_tctlOffset;
static CRITICAL_SECTION g_pioCs;
/* CPU package power from the processor's energy counter (RAPL): joules per counter step,
   plus the previous reading. Intel: MSR 0x606 / 0x611. AMD Ryzen: MSR 0xC0010299 / 0xC001029B. */
static double           g_eUnit;
static ULONGLONG        g_ePrev, g_eTime;

static int pio_exec(const char *fn, ULONGLONG in, ULONGLONG *out)
{
    BYTE buf[40]; ULONGLONG o = 0; DWORD got = 0;
    ZeroMemory(buf, sizeof buf);
    lstrcpynA((char *)buf, fn, 32);
    memcpy(buf + 32, &in, 8);
    if (!DeviceIoControl(g_pio, IOCTL_PIO_EXEC, buf, sizeof buf, &o, sizeof o, &got, NULL) || got < 8) return 0;
    *out = o;
    return 1;
}

static int cpuid4(unsigned leaf, unsigned r[4])
{
#ifdef _MSC_VER
    int x[4]; __cpuid(x, (int)leaf); memcpy(r, x, sizeof x); return 1;
#else
    return __get_cpuid(leaf, &r[0], &r[1], &r[2], &r[3]);
#endif
}

/* Some first- and second-generation Ryzen chips report their temperature with a fixed amount added
   on top (so fans spin up earlier). Same list as the Linux k10temp driver; all are CPU family 17h. */
static int tctl_offset_for(const char *brand)
{
    static const struct { const char *name; int off; } T[] = {
        { "AMD Ryzen 5 1600X", 20 }, { "AMD Ryzen 7 1700X", 20 }, { "AMD Ryzen 7 1800X", 20 },
        { "AMD Ryzen 7 2700X", 10 },
        { "AMD Ryzen Threadripper 19", 27 },   /* 1900X, 1920X, 1950X */
        { "AMD Ryzen Threadripper 29", 27 },   /* 2920X, 2950X, 2970WX, 2990WX */
    };
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++)
        if (strstr(brand, T[i].name)) return T[i].off;
    return 0;
}

static int amd_tctl_offset(void)
{
    unsigned r[4], fam;
    char brand[49];
    if (!cpuid4(1, r)) return 0;
    fam = (r[0] >> 8) & 0xF;
    if (fam == 0xF) fam += (r[0] >> 20) & 0xFF;
    if (fam != 0x17 || !cpuid4(0x80000000, r) || r[0] < 0x80000004) return 0;
    for (unsigned i = 0; i < 3; i++) {
        if (!cpuid4(0x80000002 + i, r)) return 0;
        memcpy(brand + i * 16, r, 16);
    }
    brand[48] = 0;
    return tctl_offset_for(brand);
}

static void pio_init(void)
{
    EnterCriticalSection(&g_pioCs);
    if (g_pio != INVALID_HANDLE_VALUE) { CloseHandle(g_pio); g_pio = INVALID_HANDLE_VALUE; }
    unsigned b;
#ifdef _MSC_VER
    int r[4]; __cpuid(r, 0); b = (unsigned)r[1];
#else
    unsigned a, c, d;
    if (!__get_cpuid(0, &a, &b, &c, &d)) b = 0;
#endif
    g_cpuVendor = b == 0x756e6547 ? 1 : b == 0x68747541 ? 2 : 0;
    if (!g_cpuVendor) { g_pioState = PIO_UNSUPPORTED; goto out; }

    g_pio = CreateFileW(L"\\\\?\\GLOBALROOT\\Device\\PawnIO", GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (g_pio == INVALID_HANDLE_VALUE) {
        g_pioState = GetLastError() == ERROR_ACCESS_DENIED ? PIO_NOADMIN : PIO_NODRIVER;
        goto out;
    }

    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(g_cpuVendor == 1 ? 101 : 102), (LPCWSTR)RT_RCDATA);
    HGLOBAL hg = r ? LoadResource(NULL, r) : NULL;
    const void *src = hg ? LockResource(hg) : NULL;
    DWORD sz = r ? SizeofResource(NULL, r) : 0, got = 0;
    int ok = src && sz;
    if (!ok) { g_pioState = PIO_NOMODULE; }
    else if (!DeviceIoControl(g_pio, IOCTL_PIO_LOAD, (void *)src, sz, NULL, 0, &got, NULL)) g_pioState = PIO_FAILED;
    else {
        ULONGLONG v;
        g_pioState = PIO_OK;
        if (g_cpuVendor == 1) {
            g_tjmax = pio_exec("ioctl_read_msr", 0x1A2, &v) ? (int)((v >> 16) & 0xFF) : 0;
            if (g_tjmax < 60 || g_tjmax > 130) g_tjmax = 100;
        } else {
            if (!g_pciMutex) g_pciMutex = CreateMutexW(NULL, FALSE, L"Global\\Access_PCI");
            g_tctlOffset = amd_tctl_offset();
        }
        /* Energy unit: bits 12..8 give it as 1 / 2^n joules. */
        g_eUnit = 0; g_ePrev = 0; g_eTime = 0;
        if (pio_exec("ioctl_read_msr", g_cpuVendor == 1 ? 0x606 : 0xC0010299, &v)) {
            unsigned esu = (unsigned)((v >> 8) & 0x1F);
            if (esu) g_eUnit = 1.0 / (double)(1u << esu);
        }
    }
    if (g_pioState != PIO_OK) { CloseHandle(g_pio); g_pio = INVALID_HANDLE_VALUE; }
out:
    LeaveCriticalSection(&g_pioCs);
}

static int pio_temp(double *t)
{
    ULONGLONG v; int ok = 0;
    if (g_pioState != PIO_OK) return 0;
    EnterCriticalSection(&g_pioCs);
    if (g_pio != INVALID_HANDLE_VALUE) {
        if (g_cpuVendor == 1) {
            if (pio_exec("ioctl_read_msr", 0x1B1, &v)) { *t = g_tjmax - (double)((v >> 16) & 0x7F); ok = 1; }
        } else {
            DWORD w = g_pciMutex ? WaitForSingleObject(g_pciMutex, 20) : WAIT_OBJECT_0;
            if (w == WAIT_OBJECT_0 || w == WAIT_ABANDONED) {
                if (pio_exec("ioctl_read_smn", 0x00059800, &v)) {
                    double c = ((v >> 21) & 0x7FF) / 8.0;
                    if (v & 0x80000) c -= 49.0;
                    *t = c - g_tctlOffset; ok = 1;
                }
                if (g_pciMutex) ReleaseMutex(g_pciMutex);
            }
        }
    }
    LeaveCriticalSection(&g_pioCs);
    return ok && *t > 0 && *t < 125;
}

/* CPU package power in watts: energy used since the last reading divided by the time passed.
   Returns 0 on the first reading (nothing to compare with yet) or if it can't be read. */
static int pio_power(double *w)
{
    ULONGLONG v; LARGE_INTEGER now, freq;
    int ok = 0;
    if (g_pioState != PIO_OK || g_eUnit <= 0) return 0;
    EnterCriticalSection(&g_pioCs);
    if (g_pio != INVALID_HANDLE_VALUE &&
        pio_exec("ioctl_read_msr", g_cpuVendor == 1 ? 0x611 : 0xC001029B, &v) &&
        QueryPerformanceCounter(&now) && QueryPerformanceFrequency(&freq) && freq.QuadPart > 0) {
        ULONGLONG e = v & 0xFFFFFFFFull;   /* 32-bit counter that wraps around */
        if (g_eTime) {
            double dt = (double)(now.QuadPart - (LONGLONG)g_eTime) / (double)freq.QuadPart;
            if (dt >= 0.2) {
                double joules = (double)((e - g_ePrev) & 0xFFFFFFFFull) * g_eUnit;
                *w = joules / dt;
                ok = *w >= 0 && *w < 1000;
            }
        }
        if (!g_eTime || ok || (double)(now.QuadPart - (LONGLONG)g_eTime) / (double)freq.QuadPart >= 0.2) {
            g_ePrev = e;
            g_eTime = (ULONGLONG)now.QuadPart;
        }
    }
    LeaveCriticalSection(&g_pioCs);
    return ok;
}

#define PAWNIO_HOST L"github.com"
#define PAWNIO_PATH L"/namazso/PawnIO.Setup/releases/download/2.2.0/PawnIO_setup.exe"
static const BYTE PAWNIO_SHA256[32] = {
    0x1f,0x51,0x9a,0x22,0xe4,0x71,0x87,0xf7,0x0a,0x13,0x79,0xa4,0x8c,0xa6,0x04,0x98,
    0x1c,0x4f,0xcf,0x69,0x4f,0x4e,0x65,0xb7,0x34,0xaa,0xa7,0x4a,0x9f,0xba,0x30,0x32 };

static BYTE *http_get_ex(const wchar_t *host, const wchar_t *path, DWORD *len, DWORD *httpStatus)
{
    BYTE *buf = NULL; DWORD cap = 0, n = 0;
    HINTERNET s = WinHttpOpen(L"TrayVitals", 4 , NULL, NULL, 0);
    if (!s) s = WinHttpOpen(L"TrayVitals", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, NULL, NULL, NULL, WINHTTP_FLAG_SECURE) : NULL;
    DWORD status = 0, ssz = sizeof status;
    if (r && WinHttpSendRequest(r, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(r, NULL) &&
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &ssz, NULL) &&
        status == 200) {
        for (;;) {
            DWORD avail = 0, rd = 0;
            if (!WinHttpQueryDataAvailable(r, &avail) || !avail) break;
            if (n + avail > (16u << 20)) { n = 0; break; }
            if (n + avail + 1 > cap) {   /* + 1: room for the 0 added at the end (text replies) */
                cap = (n + avail + 1) * 2;
                BYTE *nb = buf ? HeapReAlloc(GetProcessHeap(), 0, buf, cap) : HeapAlloc(GetProcessHeap(), 0, cap);
                if (!nb) { n = 0; break; }
                buf = nb;
            }
            if (!WinHttpReadData(r, buf + n, avail, &rd) || !rd) break;
            n += rd;
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    if (!n && buf) { HeapFree(GetProcessHeap(), 0, buf); buf = NULL; }
    if (buf) buf[n] = 0;
    *len = n;
    if (httpStatus) *httpStatus = status;
    return buf;
}

static BYTE *http_get(const wchar_t *host, const wchar_t *path, DWORD *len)
{
    return http_get_ex(host, path, len, NULL);
}

static int sha256_is(const BYTE *data, DWORD len, const BYTE *want)
{
    BCRYPT_ALG_HANDLE alg; BCRYPT_HASH_HANDLE h; BYTE out[32]; int ok = 0;
    if (!load_net()) return 0;   /* the hashing functions are loaded there (the update helper hasn't gone online) */
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0)) return 0;
    if (!BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0)) {
        if (!BCryptHashData(h, (PUCHAR)data, len, 0) && !BCryptFinishHash(h, out, 32, 0)) ok = !memcmp(out, want, 32);
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

static int pawnio_install(const wchar_t **msg)
{
    DWORD len = 0, w, code = 1;
    if (!load_net()) { *msg = L"Couldn't load the Windows networking components."; return 0; }
    BYTE *bin = http_get(PAWNIO_HOST, PAWNIO_PATH, &len);
    if (!bin) { *msg = L"Couldn't download the PawnIO installer from GitHub. Check your internet connection."; return 0; }
    if (!sha256_is(bin, len, PAWNIO_SHA256)) {
        HeapFree(GetProcessHeap(), 0, bin);
        *msg = L"The downloaded PawnIO installer didn't match its known checksum, so it was not run.";
        return 0;
    }
    wchar_t dir[MAX_PATH], exe[MAX_PATH], cmd[MAX_PATH + 32];
    if (!app_dir(dir)) {
        HeapFree(GetProcessHeap(), 0, bin);
        *msg = L"Couldn't create the TrayVitals folder in Program Files.";
        return 0;
    }
    _snwprintf(exe, MAX_PATH, L"%s\\PawnIO_setup.exe", dir); exe[MAX_PATH - 1] = 0;
    HANDLE f = CreateFileW(exe, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    int wrote = f != INVALID_HANDLE_VALUE && WriteFile(f, bin, len, &w, NULL) && w == len;
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    HeapFree(GetProcessHeap(), 0, bin);
    if (!wrote) { DeleteFileW(exe); *msg = L"Couldn't save the PawnIO installer."; return 0; }

    _snwprintf(cmd, MAX_PATH + 32, L"\"%s\" -install -silent", exe); cmd[MAX_PATH + 31] = 0;
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si); si.cb = sizeof si;
    if (CreateProcessW(exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, dir, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 180000);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    DeleteFileW(exe);
    if (code != 0 && code != 3010) { *msg = L"The PawnIO installer reported an error."; return 0; }
    return 1;
}

static HANDLE g_disk = INVALID_HANDLE_VALUE;

static void init_disk(void)
{
    DWORD num = 0, got;
    wchar_t sysdrv[8] = L"\\\\.\\C:", p[40];
    wchar_t win[MAX_PATH];
    if (GetWindowsDirectoryW(win, MAX_PATH) >= 2) sysdrv[4] = win[0];
    HANDLE v = CreateFileW(sysdrv, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (v != INVALID_HANDLE_VALUE) {
        BYTE b[256];
        if (DeviceIoControl(v, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, b, sizeof b, &got, NULL))
            num = ((VOLUME_DISK_EXTENTS *)b)->Extents[0].DiskNumber;
        CloseHandle(v);
    }
    _snwprintf(p, 40, L"\\\\.\\PhysicalDrive%u", (unsigned)num); p[39] = 0;
    g_disk = CreateFileW(p, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
}

static void read_disk(void)
{
    STORAGE_PROPERTY_QUERY q; BYTE b[512]; DWORD got = 0;
    g_ok[M_DISKT] = 0;
    if (g_disk == INVALID_HANDLE_VALUE) return;
    ZeroMemory(&q, sizeof q);
    q.PropertyId = StorageDeviceTemperatureProperty;
    q.QueryType = PropertyStandardQuery;
    if (!DeviceIoControl(g_disk, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof q, b, sizeof b, &got, NULL)) return;
    STORAGE_TEMPERATURE_DATA_DESCRIPTOR *d = (STORAGE_TEMPERATURE_DATA_DESCRIPTOR *)b;
    if (got < sizeof *d || !d->InfoCount) return;
    int mx = -1000;
    for (WORD i = 0; i < d->InfoCount; i++) {
        if ((BYTE *)&d->TemperatureInfo[i + 1] > b + got) break;
        if (d->TemperatureInfo[i].Temperature > mx) mx = d->TemperatureInfo[i].Temperature;
    }
    if (mx > 0 && mx < 150) { g_val[M_DISKT] = mx; g_ok[M_DISKT] = 1; }
}

static void build_query(void)
{
    if (g_q) { PdhCloseQuery(g_q); g_q = 0; }
    g_cTherm = g_cEng = g_cMem = 0;
    if (!(g_mask & ((1u << M_CPUT) | (1u << M_GPU) | (1u << M_VRAM))) || !load_pdh()) return;
    if (PdhOpenQueryW(NULL, 0, &g_q) != ERROR_SUCCESS) { g_q = 0; return; }
    if ((g_mask & (1u << M_CPUT)) && g_pioState != PIO_OK &&
        PdhAddEnglishCounterW(g_q, L"\\Thermal Zone Information(*)\\Temperature", 0, &g_cTherm)) g_cTherm = 0;
    if (g_hasGpu && (g_mask & (1u << M_GPU)) &&
        PdhAddEnglishCounterW(g_q, L"\\GPU Engine(*)\\Utilization Percentage", 0, &g_cEng)) g_cEng = 0;
    if (g_hasGpu && (g_mask & (1u << M_VRAM)) &&
        PdhAddEnglishCounterW(g_q, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0, &g_cMem)) g_cMem = 0;
    PdhCollectQueryData(g_q);
}

static void sample(void)
{
    if (g_mask & (1u << M_CPU)) read_cpu();
    if (g_mask & (1u << M_RAM)) read_ram();
    if (g_q && PdhCollectQueryData(g_q) == ERROR_SUCCESS) {
        if (g_cTherm) read_therm();
        if (g_cEng)   read_gpu_usage();
        if (g_cMem)   read_vram();
    }
    if (g_mask & (1u << M_GPUT)) read_gpu_temp();
    if (g_mask & (1u << M_CPUT)) {
        double t = 0;
        if (pio_temp(&t)) { g_val[M_CPUT] = t; g_ok[M_CPUT] = 1; }
        else if (!g_cTherm) g_ok[M_CPUT] = 0;
    }
    if (g_mask & (1u << M_DISKT)) {
        static unsigned k;
        if (k++ % 5 == 0 || !g_ok[M_DISKT]) read_disk();
    }
    if (g_mask & (1u << M_CPUP)) {
        double w = 0;
        if (pio_power(&w)) { g_val[M_CPUP] = w; g_ok[M_CPUP] = 1; }
        else if (g_pioState != PIO_OK || g_eUnit <= 0) g_ok[M_CPUP] = 0;   /* keep last value on the first read */
    }
    if (g_mask & (1u << M_GPUP)) read_gpu_power();
}

enum { SH_CPU, SH_RAM, SH_GPU };
static const int SHAPE[M_N]  = { SH_CPU, SH_CPU, SH_RAM, SH_GPU, SH_GPU, SH_RAM, SH_RAM, SH_CPU, SH_GPU };
static const int ISTEMP[M_N] = { 0, 1, 0, 0, 1, 0, 1, 0, 0 };
static const int ISPWR[M_N]  = { 0, 0, 0, 0, 0, 0, 0, 1, 1 };   /* watts, not a 0-100 % value */
#define SS 4
static HDC   g_ldc;
static int   g_S;
static HFONT g_font[3][4];

static void reset_fonts(void)
{
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 4; b++)
            if (g_font[a][b]) { DeleteObject(g_font[a][b]); g_font[a][b] = 0; }
}

static void read_env(void)
{
    DWORD v = 0, sz = sizeof v;
    g_size = GetSystemMetrics(SM_CXSMICON);
    if (g_size < 16) g_size = 16;
    if (g_size > 64) g_size = 64;
    g_light = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                           L"SystemUsesLightTheme", RRF_RT_REG_DWORD, NULL, &v, &sz) == ERROR_SUCCESS && v;
    reset_fonts();
}

static int P(float v) { return (int)(v * g_S / 16.0f + 0.5f); }
static void box(float x0, float y0, float x1, float y1, int on)
{
    RECT r = { P(x0), P(y0), P(x1), P(y1) };
    FillRect(g_ldc, &r, (HBRUSH)GetStockObject(on ? WHITE_BRUSH : BLACK_BRUSH));
}
static void rbox(float x0, float y0, float x1, float y1, float rad, int on)
{
    SelectObject(g_ldc, GetStockObject(on ? WHITE_BRUSH : BLACK_BRUSH));
    SelectObject(g_ldc, GetStockObject(NULL_PEN));
    RoundRect(g_ldc, P(x0), P(y0), P(x1) + 1, P(y1) + 1, P(rad * 2), P(rad * 2));
}

static void shape(int sh, int solid, float in[4])
{
    switch (sh) {
    case SH_CPU:
        for (int i = 0; i < 4; i++) {
            float a = (float)(3 + 3 * i);
            box(a, 0, a + 1, 1.25f, 1);  box(a, 14.75f, a + 1, 16, 1);
            box(0, a, 1.25f, a + 1, 1);  box(14.75f, a, 16, a + 1, 1);
        }
        rbox(1, 1, 15, 15, 1.5f, 1);
        if (!solid) rbox(2, 2, 14, 14, 0.6f, 0);
        in[0] = 1.75f; in[1] = 1.75f; in[2] = 14.25f; in[3] = 14.25f;
        break;
    case SH_RAM: {
        static const float tx[6] = { 1, 3, 5, 10, 12, 14 };
        for (int i = 0; i < 6; i++) box(tx[i], 13, tx[i] + 1, 15.5f, 1);
        rbox(0, 0.5f, 16, 13.5f, 1, 1);
        if (!solid) rbox(1, 1.5f, 15, 12.5f, 0.4f, 0);
        in[0] = 0.75f; in[1] = 1.25f; in[2] = 15.25f; in[3] = 12.75f;
        break; }
    default:
        box(0, 0.5f, 1, 16, 1);
        box(0, 0.5f, 2, 1.5f, 1);
        box(5, 13, 9, 15.5f, 1);
        box(10, 13, 14, 15.5f, 1);
        rbox(1.75f, 0.5f, 16, 13.5f, 1, 1);
        if (!solid) rbox(2.75f, 1.5f, 15, 12.5f, 0.4f, 0);
        in[0] = 2.5f; in[1] = 1.25f; in[2] = 15.25f; in[3] = 12.75f;
    }
}

static HFONT font_for(int sh, int len, int iw, int ih)
{
    if (g_font[sh][len]) return g_font[sh][len];
    static const wchar_t z[] = L"000";
    for (int h = ih * 3 / 2; h > 4; h -= 1 + h / 24) {
        HFONT f = CreateFontW(-h, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                              NONANTIALIASED_QUALITY, 0, L"Bahnschrift SemiBold Condensed");
        HGDIOBJ o = SelectObject(g_ldc, f);
        SIZE s; TEXTMETRICW tm;
        GetTextExtentPoint32W(g_ldc, z, len, &s);
        GetTextMetricsW(g_ldc, &tm);
        SelectObject(g_ldc, o);
        if (s.cx <= iw && (tm.tmAscent - tm.tmInternalLeading) <= ih) return g_font[sh][len] = f;
        DeleteObject(f);
    }
    return g_font[sh][len] = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

static COLORREF heat(double t)
{
    static const double   T[5] = { 35, 55, 70, 80, 90 };
    static const COLORREF C[5] = { RGB(60,170,255), RGB(70,210,110), RGB(250,205,50), RGB(255,135,40), RGB(255,45,45) };
    if (t <= T[0]) return C[0];
    for (int i = 1; i < 5; i++)
        if (t < T[i]) {
            double k = (t - T[i - 1]) / (T[i] - T[i - 1]);
            COLORREF a = C[i - 1], b = C[i];
            return RGB(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * k,
                       GetGValue(a) + (GetGValue(b) - GetGValue(a)) * k,
                       GetBValue(a) + (GetBValue(b) - GetBValue(a)) * k);
        }
    return C[4];
}

static void over(double *r, double *g, double *b, double *a, COLORREF c, double ca)
{
    double na = ca + *a * (1 - ca);
    if (na <= 0) return;
    *r = (GetRValue(c) * ca + *r * *a * (1 - ca)) / na;
    *g = (GetGValue(c) * ca + *g * *a * (1 - ca)) / na;
    *b = (GetBValue(c) * ca + *b * *a * (1 - ca)) / na;
    *a = na;
}

static HICON make_icon(int m, const wchar_t *txt)
{
    static BYTE maskBits[8 * 64];
    static BYTE cov[3][64 * 64];
    int s = g_size, S = s * SS, len = lstrlenW(txt), sh = SHAPE[m];
    int solid = ISTEMP[m] && g_ok[m];
    g_S = S;

    BITMAPINFO bi; ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = S; bi.bmiHeader.biHeight = -S;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    DWORD *px;
    g_ldc = CreateCompatibleDC(NULL);
    HBITMAP big = CreateDIBSection(g_ldc, &bi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
    if (!big) { DeleteDC(g_ldc); return NULL; }
    HGDIOBJ ob = SelectObject(g_ldc, big);
    SetBkMode(g_ldc, TRANSPARENT);
    SetTextColor(g_ldc, RGB(255, 255, 255));

    float in[4] = { 0 };
    for (int layer = 0; layer < 3; layer++) {
        ZeroMemory(px, (SIZE_T)S * S * 4);
        if (layer == 0) shape(sh, solid, in);
        else if (layer == 1) {
            if (!ISTEMP[m] && !ISPWR[m] && g_ok[m]) {   /* fill level only for percentages */
                double p = g_val[m] < 0 ? 0 : g_val[m] > 100 ? 100 : g_val[m];
                float y = in[3] - (in[3] - in[1]) * (float)p / 100.0f;
                if (p > 0.5) box(in[0], y, in[2], in[3], 1);
            }
        } else {
            int ix = P(in[0]), iy = P(in[1]), iw = P(in[2]) - ix, ih = P(in[3]) - iy;
            int pad = S / 64;
            HGDIOBJ of = SelectObject(g_ldc, font_for(sh, len > 3 ? 3 : len, iw - 2 * pad, ih - 2 * pad));
            TEXTMETRICW tm; SIZE ts;
            GetTextMetricsW(g_ldc, &tm);
            GetTextExtentPoint32W(g_ldc, txt, len, &ts);
            int cap = tm.tmAscent - tm.tmInternalLeading;
            TextOutW(g_ldc, ix + (iw - ts.cx) / 2, iy + (ih - cap) / 2 - tm.tmInternalLeading, txt, len);
            SelectObject(g_ldc, of);
        }
        GdiFlush();
        for (int y = 0; y < s; y++)
            for (int x = 0; x < s; x++) {
                unsigned sum = 0;
                for (int j = 0; j < SS; j++) {
                    const DWORD *row = px + (y * SS + j) * S + x * SS;
                    for (int i = 0; i < SS; i++) sum += row[i] & 0xFF;
                }
                cov[layer][y * s + x] = (BYTE)(sum / (SS * SS));
            }
    }
    SelectObject(g_ldc, ob);
    DeleteObject(big);
    DeleteDC(g_ldc);

    COLORREF col = !g_ok[m] ? RGB(140, 140, 140) : ISTEMP[m] ? heat(g_val[m]) : (g_light ? COL_L : COL_D)[m];
    COLORREF fg  = g_light ? RGB(25, 25, 25) : RGB(255, 255, 255);
    bi.bmiHeader.biWidth = s; bi.bmiHeader.biHeight = -s;
    HBITMAP bmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
    if (!bmp) return NULL;
    for (int i = 0; i < s * s; i++) {
        double F = cov[0][i] / 255.0, B = cov[1][i] / 255.0, T = cov[2][i] / 255.0;
        double r = 0, g = 0, b = 0, a = 0;
        if (solid) over(&r, &g, &b, &a, col, F * (1 - T));
        else {
            over(&r, &g, &b, &a, col, B * 0.42);
            over(&r, &g, &b, &a, col, F);
            over(&r, &g, &b, &a, fg, T);
        }
        DWORD A = (DWORD)(a * 255 + 0.5);
        px[i] = A ? (A << 24) | ((DWORD)(r + .5) << 16) | ((DWORD)(g + .5) << 8) | (DWORD)(b + .5) : 0;
    }
    HBITMAP mask = CreateBitmap(s, s, 1, 1, maskBits);
    ICONINFO ii = { TRUE, 0, 0, mask, bmp };
    HICON ic = CreateIconIndirect(&ii);
    DeleteObject(mask);
    DeleteObject(bmp);
    return ic;
}


enum { MODE_BAR, MODE_TRAY };
static DWORD          g_mode = MODE_BAR;
static LONG           g_offset;
static volatile int   g_paused;
static volatile LONG  g_rebuild = 1, g_quit;
static HANDLE         g_wake;
#define WM_SAMPLED (WM_APP + 2)

/* Tray icon mode: ONE icon showing the reading picked in the menu ("Icon shows"); hovering it lists
   every reading. It is only redrawn when its own number changes. It keeps tray id 1, the id the
   first of the old one-icon-per-reading icons had, so a "show on taskbar" choice made for it in
   Windows' settings carries over. */
#define TRAY_ID 1
static DWORD   g_trayShows = M_CPU;
static int     g_trayOn, g_trayM = -1;
static HICON   g_trayIcon;
static wchar_t g_trayTxt[4], g_trayTip[128];

static int tray_metric(void)
{
    if ((g_mask >> g_trayShows) & 1) return (int)g_trayShows;
    for (int o = 0; o < M_N; o++) if ((g_mask >> ORDER[o]) & 1) return ORDER[o];
    return M_CPU;
}

/* "CPU 26% 54° 54 W" style lines, one per part, for everything that's switched on. */
static void tray_tip(wchar_t *t)
{
    static const int GROUP[][4] = { { M_CPU, M_CPUT, M_CPUP, -1 }, { M_RAM, -1 }, { M_GPU, M_GPUT, M_GPUP, -1 },
                                    { M_VRAM, -1 }, { M_DISKT, -1 } };
    static const wchar_t *LBL[] = { L"CPU", L"RAM", L"GPU", L"VRAM", L"SSD" };
    int o = 0;
    t[0] = 0;
    for (int g = 0; g < 5; g++) {
        wchar_t line[48]; int lo = 0, any = 0;
        lo = _snwprintf(line, 48, L"%s", LBL[g]);
        for (int i = 0; GROUP[g][i] >= 0; i++) {
            int m = GROUP[g][i];
            if (!((g_mask >> m) & 1)) continue;
            any = 1;
            int n = !g_ok[m] ? _snwprintf(line + lo, 48 - lo, L" --")
                  : ISTEMP[m] ? _snwprintf(line + lo, 48 - lo, L" %.0f\x00B0", g_val[m])
                  : ISPWR[m]  ? _snwprintf(line + lo, 48 - lo, L" %.0f W", g_val[m])
                  : m == M_RAM ? _snwprintf(line + lo, 48 - lo, L" %.0f%% (%.1f/%.0f GB)", g_val[m], g_ramUsed, g_ramTotal)
                  : _snwprintf(line + lo, 48 - lo, L" %.0f%%", g_val[m]);
            if (n > 0 && lo + n < 48) lo += n;
        }
        line[47] = 0;
        if (!any) continue;
        int ll = lstrlenW(line);
        if (o + ll + 2 >= 128) break;
        if (o) t[o++] = L'\n';
        lstrcpyW(t + o, line); o += ll;
    }
    if (!o) lstrcpyW(t, APP);
}

static void tray_op(DWORD op, UINT flags)
{
    NOTIFYICONDATAW n; ZeroMemory(&n, sizeof n);
    n.cbSize = sizeof n; n.hWnd = g_wnd; n.uID = TRAY_ID;
    n.uFlags = flags;
    n.uCallbackMessage = WM_TRAY;
    n.hIcon = g_trayIcon;
    lstrcpynW(n.szTip, g_trayTip, 128);
    Shell_NotifyIconW(op, &n);
}

static void tray_remove(void)
{
    if (g_trayOn) tray_op(NIM_DELETE, 0);
    g_trayOn = 0;
}

static void tray_refresh(int force)
{
    if (g_mode != MODE_TRAY) { tray_remove(); return; }
    int m = tray_metric();
    wchar_t t[4], tip[128];
    if (!g_ok[m]) lstrcpyW(t, L"--");
    else {
        int v = (int)(g_val[m] + 0.5);
        if (v < 0) v = 0;
        if (v > 999) v = 999;
        _snwprintf(t, 4, L"%d", v); t[3] = 0;
    }
    tray_tip(tip);
    int newIcon = force || !g_trayOn || m != g_trayM || lstrcmpW(t, g_trayTxt);
    int newTip = newIcon || lstrcmpW(tip, g_trayTip);
    if (!newTip) return;
    lstrcpyW(g_trayTip, tip);
    HICON old = NULL;
    if (newIcon) {
        old = g_trayIcon;
        g_trayIcon = make_icon(m, t);
        lstrcpyW(g_trayTxt, t);
        g_trayM = m;
    }
    if (!g_trayOn) { tray_op(NIM_ADD, NIF_ICON | NIF_TIP | NIF_MESSAGE); g_trayOn = 1; }
    else tray_op(NIM_MODIFY, newIcon ? NIF_ICON | NIF_TIP : NIF_TIP);
    if (old) DestroyIcon(old);
}

static HWND    g_bar, g_tb;
static int     g_child, g_barW, g_barH, g_dpi = 96;
static RECT    g_lastPos;
static wchar_t g_barSig[320];

/* Free space on the taskbar, measured with UI Automation by the scanner thread: where the app
   buttons end (appsR) and where the tray area (icons, clock, ...) starts (trayL). Kept relative
   to the taskbar's left edge, together with the taskbar size it was measured at, so it stays
   valid while an auto-hiding taskbar slides but not after a resize or a switch of taskbar. */
typedef struct { HWND tb; int w, h, appsR, trayL; } SCAN;
static CRITICAL_SECTION g_scanCs;
static SCAN          g_scan;
static volatile LONG g_scanFails;   /* failed measurements in a row */
static volatile LONG g_scanNoUia;   /* this taskbar can't be measured (e.g. Windows 10): use window fallback */
static HANDLE        g_scanWake;

typedef struct { int glyph, sh, x, y, w, al, f, row; wchar_t t[8]; COLORREF c; } SEG;
enum { G_CPU, G_RAM, G_GPU, G_DISK };

static int SC(int v) { return MulDiv(v, g_dpi, 96); }

static void get_dpi(void)
{
    typedef UINT (WINAPI *GDW)(HWND);
    static GDW f; static int tried;
    if (!tried) { tried = 1; f = (GDW)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"); }
    UINT d = (f && g_tb) ? f(g_tb) : 0;
    if (!d) { HDC dc = GetDC(NULL); d = GetDeviceCaps(dc, LOGPIXELSY); ReleaseDC(NULL, dc); }
    g_dpi = d < 96 ? 96 : (int)d;
}

static void glyph_cov(int sh, int gs, BYTE *out)
{
    int S = gs * SS;
    g_S = S;
    BITMAPINFO bi; ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = S; bi.bmiHeader.biHeight = -S;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    DWORD *px;
    g_ldc = CreateCompatibleDC(NULL);
    HBITMAP big = CreateDIBSection(g_ldc, &bi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
    ZeroMemory(out, (SIZE_T)gs * gs);
    if (big) {
        HGDIOBJ ob = SelectObject(g_ldc, big);
        ZeroMemory(px, (SIZE_T)S * S * 4);
        float in[4];
        shape(sh, 0, in);
        GdiFlush();
        for (int y = 0; y < gs; y++)
            for (int x = 0; x < gs; x++) {
                unsigned sum = 0;
                for (int j = 0; j < SS; j++) {
                    const DWORD *row = px + (y * SS + j) * S + x * SS;
                    for (int i = 0; i < SS; i++) sum += row[i] & 0xFF;
                }
                out[y * gs + x] = (BYTE)(sum / (SS * SS));
            }
        SelectObject(g_ldc, ob);
        DeleteObject(big);
    }
    DeleteDC(g_ldc);
}

static LRESULT CALLBACK barproc(HWND w, UINT msg, WPARAM wp, LPARAM lp);

/* Which taskbar the bar sits on: 0 = main taskbar, n = n-th taskbar on another
   display, counted left to right (then top to bottom). */
static DWORD g_display;
/* Where the bar sits in the taskbar's free space. */
enum { ALIGN_RIGHT, ALIGN_LEFT, ALIGN_CENTER };
static DWORD g_align = ALIGN_RIGHT;
#define MAX_TB 8

/* Fills out[] with the taskbars on other displays, sorted by position. */
static int other_taskbars(HWND *out)
{
    RECT rc[MAX_TB];
    int n = 0;
    HWND w = NULL;
    while (n < MAX_TB && (w = FindWindowExW(NULL, w, L"Shell_SecondaryTrayWnd", NULL)) != NULL) {
        RECT r;
        if (!IsWindowVisible(w) || !GetWindowRect(w, &r)) continue;
        int i = n++;
        while (i > 0 && (rc[i - 1].left > r.left || (rc[i - 1].left == r.left && rc[i - 1].top > r.top))) {
            rc[i] = rc[i - 1]; out[i] = out[i - 1]; i--;
        }
        rc[i] = r; out[i] = w;
    }
    return n;
}

static HWND find_taskbar(void)
{
    if (g_display) {
        HWND tbs[MAX_TB];
        int n = other_taskbars(tbs);
        if ((int)g_display <= n) return tbs[g_display - 1];
    }
    return FindWindowW(L"Shell_TrayWnd", NULL);
}

static void bar_create(void)
{
    g_bar = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"TrayVitalsBar", L"",
                            WS_POPUP, 0, 0, 1, 1, NULL, NULL, GetModuleHandleW(NULL), NULL);
    g_tb = find_taskbar();
    g_child = 0;
    if (g_bar && g_tb) {
        SetWindowLongPtrW(g_bar, GWL_STYLE, WS_CHILD | WS_CLIPSIBLINGS);
        if (SetParent(g_bar, g_tb)) g_child = 1;
        else SetWindowLongPtrW(g_bar, GWL_STYLE, WS_POPUP);
    }
    ZeroMemory(&g_lastPos, sizeof g_lastPos);
    g_barSig[0] = 0;
    /* New taskbar: measure it right away; the bar stays hidden until then. */
    InterlockedExchange(&g_scanFails, 0);
    InterlockedExchange(&g_scanNoUia, 0);
    if (g_scanWake) SetEvent(g_scanWake);
}

static void bar_to_popup(void)
{
    SetParent(g_bar, NULL);
    SetWindowLongPtrW(g_bar, GWL_STYLE, WS_POPUP);
    SetWindowLongPtrW(g_bar, GWL_EXSTYLE, GetWindowLongPtrW(g_bar, GWL_EXSTYLE) | WS_EX_TOPMOST);
    g_child = 0;
    ZeroMemory(&g_lastPos, sizeof g_lastPos);
}

static void bar_destroy(void)
{
    if (g_bar && IsWindow(g_bar)) DestroyWindow(g_bar);
    g_bar = 0;
}

static int g_compact, g_wideW, g_compW;
static IUIAutomation *g_uia;
static IUIAutomationCacheRequest *g_uiaCache;
static IUIAutomationCondition *g_uiaAll;

static const GUID CLSID_UIA = {0xff48dba4,0x60ef,0x4201,{0xaa,0x87,0x54,0x10,0x3e,0xef,0x59,0x4e}};
static const GUID IID_UIA   = {0x30cbe57d,0xd9d0,0x452a,{0xab,0x13,0x7a,0xc5,0xac,0x48,0x25,0xee}};
static const GUID IID_CF    = {0x00000001,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

static int uia_init(void)
{
    static int tried;
    if (tried) return g_uia != NULL;
    tried = 1;
    HMODULE m = sysdll(L"UIAutomationCore.dll");
    typedef HRESULT (WINAPI *DGCO)(const GUID *, const GUID *, void **);
    DGCO get = m ? (DGCO)(void *)GetProcAddress(m, "DllGetClassObject") : NULL;
    IClassFactory *cf = NULL;
    if (!get || FAILED(get(&CLSID_UIA, &IID_CF, (void **)&cf)) || !cf) return 0;
    HRESULT hr = IClassFactory_CreateInstance(cf, NULL, &IID_UIA, (void **)&g_uia);
    IClassFactory_Release(cf);
    if (FAILED(hr) || !g_uia) { g_uia = NULL; return 0; }
    if (FAILED(IUIAutomation_CreateCacheRequest(g_uia, &g_uiaCache)) ||
        FAILED(IUIAutomation_CreateTrueCondition(g_uia, &g_uiaAll))) {
        IUIAutomation_Release(g_uia); g_uia = NULL; return 0;
    }
    IUIAutomationCacheRequest_AddProperty(g_uiaCache, UIA_ClassNamePropertyId);
    IUIAutomationCacheRequest_AddProperty(g_uiaCache, UIA_BoundingRectanglePropertyId);
    IUIAutomationCacheRequest_AddProperty(g_uiaCache, UIA_IsOffscreenPropertyId);
    IUIAutomationCacheRequest_put_AutomationElementMode(g_uiaCache, AutomationElementMode_None);
    return 1;
}

static int starts(const wchar_t *s, const wchar_t *p)
{
    return s && !wcsncmp(s, p, wcslen(p));
}

/* Measures the free space on the current taskbar. Returns 1 when the result changed. */
static int uia_scan(void)
{
    static RECT rc[256];
    static BYTE app[256];
    HWND tb = g_tb;
    RECT tr;
    if (!tb || !GetWindowRect(tb, &tr)) return 0;
    if (!FindWindowExW(tb, NULL, L"Windows.UI.Composition.DesktopWindowContentBridge", NULL) || !uia_init())
        return !InterlockedExchange(&g_scanNoUia, 1);
    InterlockedExchange(&g_scanNoUia, 0);
    IUIAutomationElement *root = NULL;
    IUIAutomationElementArray *arr = NULL;
    int n = 0, cnt = 0;
    if (SUCCEEDED(IUIAutomation_ElementFromHandle(g_uia, (UIA_HWND)tb, &root)) && root &&
        SUCCEEDED(IUIAutomationElement_FindAllBuildCache(root, TreeScope_Descendants, g_uiaAll, g_uiaCache, &arr)) && arr)
        IUIAutomationElementArray_get_Length(arr, &cnt);
    int tbw = tr.right - tr.left;
    for (int i = 0; i < cnt && n < 256; i++) {
        IUIAutomationElement *e = NULL;
        if (FAILED(IUIAutomationElementArray_GetElement(arr, i, &e)) || !e) continue;
        BSTR cls = NULL; RECT r; BOOL off = FALSE;
        IUIAutomationElement_get_CachedClassName(e, &cls);
        IUIAutomationElement_get_CachedBoundingRectangle(e, &r);
        IUIAutomationElement_get_CachedIsOffscreen(e, &off);
        int w = r.right - r.left;
        if (!off && w > 0 && r.bottom > r.top && w < tbw / 3 && r.top < tr.bottom && r.bottom > tr.top &&
            !(cls && !lstrcmpW(cls, L"TrayVitalsBar"))) {
            rc[n] = r;
            app[n] = starts(cls, L"Taskbar.") || starts(cls, L"StartButton") || starts(cls, L"SearchBox");
            n++;
        }
        if (cls) SysFreeString(cls);
        IUIAutomationElement_Release(e);
    }
    if (arr) IUIAutomationElementArray_Release(arr);
    if (root) IUIAutomationElement_Release(root);

    int appsR = tr.left, trayL = tr.right, apps = 0;
    for (int i = 0; i < n; i++)
        if (app[i] && rc[i].right > appsR) { appsR = rc[i].right; apps = 1; }
    for (int i = 0; i < n; i++)
        if (!app[i] && rc[i].left >= appsR && rc[i].left < trayL) trayL = rc[i].left;
    HWND tn = FindWindowExW(tb, NULL, L"TrayNotifyWnd", NULL);
    RECT nr;
    if (tn && GetWindowRect(tn, &nr) && nr.right > nr.left && nr.left < trayL) trayL = nr.left;
    if (tb != g_tb) return 0;   /* switched taskbar while measuring: throw this result away */
    if (!apps || trayL <= appsR) {
        /* Keep the last good measurement; after a few misses in a row, fall back. */
        return InterlockedIncrement(&g_scanFails) == 3;
    }
    InterlockedExchange(&g_scanFails, 0);
    SCAN s = { tb, tbw, tr.bottom - tr.top, appsR - tr.left, trayL - tr.left };
    EnterCriticalSection(&g_scanCs);
    int changed = memcmp(&s, &g_scan, sizeof s) != 0;
    g_scan = s;
    LeaveCriticalSection(&g_scanCs);
    return changed;
}

/* While a game or video fills the screen (screen_busy, further down), the taskbar and its labels
   can't be seen, so both background jobs slow down to save resources: readings to every 5 seconds
   (the slowest setting) and the taskbar layout check from every second to every 5 seconds. */
#define GAME_INTERVAL 5000
static int screen_busy(void);

/* Keeps the free space up to date, about once a second, on its own thread. */
static DWORD WINAPI scanner(void *unused)
{
    (void)unused;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    int com = SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED));
    while (!g_quit) {
        int changed = 0;
        if (g_mode == MODE_BAR && g_tb && !g_paused)
            changed = com ? uia_scan() : !InterlockedExchange(&g_scanNoUia, 1);
        if (changed) PostMessageW(g_wnd, WM_SAMPLED, 0, 0);
        WaitForSingleObject(g_scanWake, g_paused ? INFINITE : screen_busy() ? GAME_INTERVAL : 1000);
    }
    if (g_uiaAll) IUIAutomationCondition_Release(g_uiaAll);
    if (g_uiaCache) IUIAutomationCacheRequest_Release(g_uiaCache);
    if (g_uia) IUIAutomation_Release(g_uia);
    if (com) CoUninitialize();
    return 0;
}

/* Without UI Automation (Windows 10): use the taskbar's own child windows. */
static void fallback_space(const RECT *tr, int *L, int *R)
{
    *L = tr->left; *R = tr->right;
    for (HWND c = GetWindow(g_tb, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        RECT r; wchar_t cls[64];
        if (c == g_bar || !IsWindowVisible(c) || !GetWindowRect(c, &r) || r.right <= r.left) continue;
        GetClassNameW(c, cls, 64);
        if ((!lstrcmpW(cls, L"TrayNotifyWnd") || !lstrcmpW(cls, L"ClockButton")) && r.left < *R) *R = r.left;
    }
    if (*R == tr->right) {
        HWND tn = FindWindowExW(g_tb, NULL, L"TrayNotifyWnd", NULL);
        RECT nr;
        if (tn && GetWindowRect(tn, &nr) && nr.right > nr.left) *R = nr.left;
    }
}

static void bar_hide(void)
{
    if (g_bar && IsWindowVisible(g_bar)) ShowWindow(g_bar, SW_HIDE);
    ZeroMemory(&g_lastPos, sizeof g_lastPos);
}

static void bar_place(void)
{
    RECT tr;
    if (!g_bar) return;
    if (!g_tb || !GetWindowRect(g_tb, &tr)) { bar_hide(); return; }
    int h = tr.bottom - tr.top, gap = SC(8), L, R;
    SCAN s;
    EnterCriticalSection(&g_scanCs);
    s = g_scan;
    LeaveCriticalSection(&g_scanCs);
    if (s.tb == g_tb && s.w == tr.right - tr.left && s.h == h) {
        L = tr.left + s.appsR + gap;   /* measured: right of the app buttons ... */
        R = tr.left + s.trayL - gap;   /* ... and left of the tray icons and clock */
    } else if (g_scanNoUia || g_scanFails >= 3) {
        fallback_space(&tr, &L, &R);
        R -= gap;
    } else {
        bar_hide();                    /* this taskbar hasn't been measured yet (under a second) */
        return;
    }
    int avail = R - L;
    int compact = g_compact ? g_wideW > avail - SC(10) : g_wideW > avail;
    if (compact != g_compact) {
        g_compact = compact;
        g_barSig[0] = 0;
        PostMessageW(g_wnd, WM_SAMPLED, 0, 0);
    }
    /* Too little room even for the compact layout: hide instead of covering anything. */
    if (avail <= 0 || g_compW > avail || g_barW > avail) { bar_hide(); return; }
    /* Right: against the tray. Left: next to the app buttons. Center: the middle of the taskbar,
       moved aside as far as needed to stay clear of anything there. "Move left/right" nudges it from
       there, but never past the app buttons or into the tray. */
    int x = g_align == ALIGN_LEFT   ? L :
            g_align == ALIGN_CENTER ? (tr.left + tr.right) / 2 - g_barW / 2 :
                                      R - g_barW;
    int y = tr.top;
    x += SC(g_offset);
    if (x > R - g_barW) x = R - g_barW;
    if (x < L) x = L;
    if (g_child) { POINT p = { x, y }; ScreenToClient(g_tb, &p); x = p.x; y = p.y; }
    RECT want = { x, y, x + g_barW, y + h };
    int top = g_child ? GetWindow(g_tb, GW_CHILD) == g_bar : 1;
    if (memcmp(&want, &g_lastPos, sizeof want) || !top || !IsWindowVisible(g_bar)) {
        SetWindowPos(g_bar, g_child ? HWND_TOP : HWND_TOPMOST, x, y, g_barW, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        g_lastPos = want;
    } else if (!g_child)
        SetWindowPos(g_bar, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static void seg_pct(SEG *s, int m, COLORREF fg)
{
    s->glyph = 0; s->c = fg; s->al = 255;
    if (!g_ok[m]) lstrcpyW(s->t, L"--");
    else _snwprintf(s->t, 8, L"%d%%", (int)(g_val[m] + 0.5));
    s->t[7] = 0;
}
static void seg_temp(SEG *s, int m, COLORREF fg)
{
    double hot = m == M_DISKT ? 65 : 80, crit = m == M_DISKT ? 75 : 90;
    s->glyph = 0; s->c = fg; s->al = 255;
    if (!g_ok[m]) { lstrcpyW(s->t, L"--\x00B0"); return; }
    double t = g_val[m];
    _snwprintf(s->t, 8, L"%d\x00B0", (int)(t + 0.5)); s->t[7] = 0;
    if (t >= crit) s->c = g_light ? RGB(200, 25, 25) : RGB(255, 75, 65);
    else if (t >= hot) s->c = g_light ? RGB(195, 95, 0) : RGB(255, 170, 55);
}
static void seg_power(SEG *s, int m, COLORREF fg)
{
    s->glyph = 0; s->c = fg; s->al = 255;
    _snwprintf(s->t, 8, L"%dW", (int)(g_val[m] + 0.5)); s->t[7] = 0;
}
static void seg_label(SEG *s, const wchar_t *t, COLORREF fg)
{
    s->glyph = 0; s->c = fg; s->al = 150;
    lstrcpynW(s->t, t, 8);
}

static void draw_gpu(HDC dc, int x0, int y0, int sz)
{
#define GX(v) (x0 + (int)((v) * sz / 16.0f + 0.5f))
#define GY(v) (y0 + (int)((v) * sz / 16.0f + 0.5f))
    int pw = sz / 16; if (pw < 1) pw = 1;
    HPEN pen = CreatePen(PS_SOLID, pw, RGB(255, 255, 255));
    HGDIOBJ op = SelectObject(dc, pen), obr = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, GX(2.5f), GY(3.5f), GX(14.5f), GY(12.5f), GX(3) - GX(0), GY(3) - GY(0));
    Ellipse(dc, GX(6), GY(5.5f), GX(11), GY(10.5f));
    SelectObject(dc, GetStockObject(NULL_PEN));
    SelectObject(dc, GetStockObject(WHITE_BRUSH));
    Ellipse(dc, GX(7.7f), GY(7.2f), GX(9.3f) + 1, GY(8.8f) + 1);
    RECT r1 = { GX(0), GY(2), GX(1), GY(14.5f) };
    RECT r2 = { GX(0), GY(2), GX(2.5f), GY(3) };
    RECT r3 = { GX(5), GY(13), GX(11), GY(14.5f) };
    FillRect(dc, &r1, (HBRUSH)GetStockObject(WHITE_BRUSH));
    FillRect(dc, &r2, (HBRUSH)GetStockObject(WHITE_BRUSH));
    FillRect(dc, &r3, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SelectObject(dc, op); SelectObject(dc, obr);
    DeleteObject(pen);
#undef GX
#undef GY
}

static HFONT g_barFont, g_smallFont, g_icoFont;
static int   g_barFontDpi, g_icoOK[4];
static const wchar_t ICO_CH[4] = { 0xEEA1, 0xEEA0, 0, 0xEDA2 };
static const MAT2 MAT_ID = { {0,1}, {0,0}, {0,0}, {0,1} };

static HFONT mkfont(HDC dc, const wchar_t *face, int px, int weight)
{
    HFONT f = CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, face);
    wchar_t got[LF_FACESIZE] = {0};
    HGDIOBJ o = SelectObject(dc, f);
    GetTextFaceW(dc, LF_FACESIZE, got);
    SelectObject(dc, o);
    if (lstrcmpiW(got, face)) { DeleteObject(f); return NULL; }
    return f;
}

static void bar_fonts(HDC dc)
{
    if (g_barFont && g_barFontDpi == g_dpi) return;
    if (g_barFont) DeleteObject(g_barFont);
    if (g_smallFont) DeleteObject(g_smallFont);
    if (g_icoFont) DeleteObject(g_icoFont);
    int tp = SC(14) * SS, sp = SC(12) * SS, ip = SC(16) * SS;
    g_barFont = mkfont(dc, L"Segoe UI Variable Text", tp, FW_NORMAL);
    if (!g_barFont) g_barFont = mkfont(dc, L"Segoe UI", tp, FW_NORMAL);
    if (!g_barFont) g_barFont = CreateFontW(-tp, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Tahoma");
    g_smallFont = mkfont(dc, L"Segoe UI Variable Text", sp, FW_NORMAL);
    if (!g_smallFont) g_smallFont = mkfont(dc, L"Segoe UI", sp, FW_NORMAL);
    if (!g_smallFont) g_smallFont = CreateFontW(-sp, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Tahoma");
    g_icoFont = mkfont(dc, L"Segoe Fluent Icons", ip, FW_NORMAL);
    if (!g_icoFont) g_icoFont = mkfont(dc, L"Segoe MDL2 Assets", ip, FW_NORMAL);
    for (int k = 0; k < 4; k++) {
        WORD gi = 0xFFFF;
        g_icoOK[k] = 0;
        if (!g_icoFont || !ICO_CH[k]) continue;
        HGDIOBJ o = SelectObject(dc, g_icoFont);
        if (GetGlyphIndicesW(dc, &ICO_CH[k], 1, &gi, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR && gi != 0xFFFF) g_icoOK[k] = 1;
        SelectObject(dc, o);
    }
    g_barFontDpi = g_dpi;
}

static void ink(HDC dc, wchar_t ch, int *top, int *h, int *left, int *w)
{
    GLYPHMETRICS gm; TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    if (GetGlyphOutlineW(dc, ch, GGO_METRICS, &gm, 0, NULL, &MAT_ID) == GDI_ERROR) {
        *top = tm.tmInternalLeading; *h = tm.tmAscent - tm.tmInternalLeading; *left = 0; *w = tm.tmAveCharWidth; return;
    }
    *top = tm.tmAscent - gm.gmptGlyphOrigin.y; *h = gm.gmBlackBoxY;
    *left = gm.gmptGlyphOrigin.x; *w = gm.gmBlackBoxX;
}

static int text_w(HDC dc, const wchar_t *t)
{
    SIZE s;
    GetTextExtentPoint32W(dc, t, lstrlenW(t), &s);
    return s.cx;
}

static int layout(HDC dc, SEG *sg, int n, int compact, int HB)
{
    int gs = SC(16) * SS, top, h, d1, d2, X = SC(4) * SS;
    if (!compact) {
        SelectObject(dc, g_barFont);
        ink(dc, L'0', &top, &h, &d1, &d2);
        for (int i = 0; i < n; i++) {
            if (sg[i].glyph) { if (i) X += SC(14) * SS; sg[i].w = gs; }
            else {
                X += (sg[i - 1].glyph ? SC(6) : sg[i].al < 255 ? SC(9) : sg[i - 1].al < 255 ? SC(4) : SC(5)) * SS;
                sg[i].w = text_w(dc, sg[i].t);
                sg[i].y = HB / 2 - (top + h / 2);
                sg[i].f = 0; sg[i].row = -1;
            }
            sg[i].x = X; X += sg[i].w;
        }
        return X + SC(4) * SS;
    }
    SelectObject(dc, g_smallFont);
    ink(dc, L'0', &top, &h, &d1, &d2);
    int gap = SC(3) * SS, yTop = HB / 2 - gap / 2 - h - top, yBot = HB / 2 + gap / 2 - top, yMid = HB / 2 - h / 2 - top;
    for (int i = 0; i < n;) {
        if (sg[i].glyph) {
            if (i) X += SC(10) * SS;
            sg[i].x = X; sg[i].w = gs; X += gs;
            i++;
            continue;
        }
        int a = i, b = (i + 1 < n && !sg[i + 1].glyph && sg[i + 1].al == 255) ? i + 1 : -1;
        X += (sg[a - 1].glyph ? SC(5) : SC(7)) * SS;
        int wa = text_w(dc, sg[a].t), wb = b >= 0 ? text_w(dc, sg[b].t) : 0;
        sg[a].x = X; sg[a].w = wa; sg[a].f = 1;
        if (b >= 0) {
            sg[a].y = yTop; sg[a].row = 0;
            sg[b].x = X; sg[b].w = wb; sg[b].f = 1; sg[b].y = yBot; sg[b].row = 1;
        } else { sg[a].y = yMid; sg[a].row = -1; }
        X += wa > wb ? wa : wb;
        i = b >= 0 ? b + 1 : a + 1;
    }
    return X + SC(4) * SS;
}

static void bar_draw(int force)
{
    static COLORREF colOf[2][1024];
    static BYTE alOf[2][1024];
    static BYTE gc[64 * 64], lut[256];
    if (!lut[255]) for (int i = 0; i < 256; i++) lut[i] = (BYTE)(255.0 * pow(i / 255.0, 0.8) + 0.5);
    if (!g_bar) return;
    RECT tr; int H = 40;
    if (g_tb && GetWindowRect(g_tb, &tr)) H = tr.bottom - tr.top;
    if (H < 16) H = 16;
    if (H > 160) H = 160;
    get_dpi();
    COLORREF fg = g_light ? RGB(0, 0, 0) : RGB(255, 255, 255);

    SEG sg[20]; int n = 0;
    int cpuT = (g_mask & (1u << M_CPUT)) && g_ok[M_CPUT];
    int cpuP = (g_mask & (1u << M_CPUP)) && g_ok[M_CPUP];
    if ((g_mask & (1u << M_CPU)) || cpuT || cpuP) {
        sg[n].glyph = 1; sg[n].sh = G_CPU; n++;
        if (g_mask & (1u << M_CPU)) seg_pct(&sg[n++], M_CPU, fg);
        if (cpuT) seg_temp(&sg[n++], M_CPUT, fg);
        if (cpuP) seg_power(&sg[n++], M_CPUP, fg);
    }
    if (g_mask & (1u << M_RAM)) {
        sg[n].glyph = 1; sg[n].sh = G_RAM; n++;
        seg_pct(&sg[n++], M_RAM, fg);
    }
    int gpuT = (g_mask & (1u << M_GPUT)) && g_ok[M_GPUT];
    int gpuP = (g_mask & (1u << M_GPUP)) && g_ok[M_GPUP];
    if ((g_mask & ((1u << M_GPU) | (1u << M_VRAM))) || gpuT || gpuP) {
        sg[n].glyph = 1; sg[n].sh = G_GPU; n++;
        if (g_mask & (1u << M_GPU)) seg_pct(&sg[n++], M_GPU, fg);
        if (gpuT) seg_temp(&sg[n++], M_GPUT, fg);
        if (gpuP) seg_power(&sg[n++], M_GPUP, fg);
        if (g_mask & (1u << M_VRAM)) {
            seg_label(&sg[n++], L"VRAM", fg);
            seg_pct(&sg[n++], M_VRAM, fg);
        }
    }
    if ((g_mask & (1u << M_DISKT)) && g_ok[M_DISKT]) {
        sg[n].glyph = 1; sg[n].sh = G_DISK; n++;
        seg_temp(&sg[n++], M_DISKT, fg);
    }

    wchar_t sig[320]; int p = _snwprintf(sig, 320, L"%d|%d|%d|%d|%d|", H, g_dpi, g_light, n, g_compact);
    for (int i = 0; i < n && p > 0 && p < 290; i++)
        if (!sg[i].glyph) p += _snwprintf(sig + p, 320 - p, L"%s%06X|", sg[i].t, (unsigned)sg[i].c);
    sig[319] = 0;
    if (!force && !lstrcmpW(sig, g_barSig)) { bar_place(); return; }
    lstrcpyW(g_barSig, sig);

    HDC mdc = CreateCompatibleDC(NULL);
    bar_fonts(mdc);
    int HB = H * SS;

    HGDIOBJ of = SelectObject(mdc, g_barFont);
    g_compW = (layout(mdc, sg, n, 1, HB) + SS - 1) / SS;
    g_wideW = (layout(mdc, sg, n, 0, HB) + SS - 1) / SS;
    int W = g_compact ? (layout(mdc, sg, n, 1, HB) + SS - 1) / SS : g_wideW;
    if (W > 1024) W = 1024;
    int WB = W * SS;

    BITMAPINFO bi; ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = WB; bi.bmiHeader.biHeight = -HB;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    DWORD *bp, *px;
    HBITMAP big = CreateDIBSection(mdc, &bi, DIB_RGB_COLORS, (void **)&bp, NULL, 0);
    bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H;
    HBITMAP bmp = CreateDIBSection(mdc, &bi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
    if (!big || !bmp) {
        if (big) DeleteObject(big);
        if (bmp) DeleteObject(bmp);
        SelectObject(mdc, of); DeleteDC(mdc); return;
    }
    HGDIOBJ ob = SelectObject(mdc, big);
    ZeroMemory(bp, (SIZE_T)WB * HB * 4);
    SetBkMode(mdc, TRANSPARENT);
    SetTextColor(mdc, RGB(255, 255, 255));
    for (int r = 0; r < 2; r++)
        for (int i = 0; i < W; i++) { colOf[r][i] = fg; alOf[r][i] = 255; }

    for (int i = 0; i < n; i++) {
        if (sg[i].glyph) continue;
        SelectObject(mdc, sg[i].f ? g_smallFont : g_barFont);
        TextOutW(mdc, sg[i].x, sg[i].y, sg[i].t, lstrlenW(sg[i].t));
        for (int r = 0; r < 2; r++) {
            if (sg[i].row >= 0 && sg[i].row != r) continue;
            for (int k = sg[i].x / SS - 1; k <= (sg[i].x + sg[i].w) / SS + 1 && k < W; k++)
                if (k >= 0) { colOf[r][k] = sg[i].c; alOf[r][k] = (BYTE)sg[i].al; }
        }
    }
    for (int i = 0; i < n; i++)
        if (sg[i].glyph && sg[i].sh == G_GPU) draw_gpu(mdc, sg[i].x, HB / 2 - sg[i].w / 2, sg[i].w);
    SelectObject(mdc, g_icoFont ? g_icoFont : g_barFont);
    for (int i = 0; i < n; i++) {
        if (!sg[i].glyph || sg[i].sh == G_GPU || !g_icoOK[sg[i].sh]) continue;
        int top, h, left, w;
        ink(mdc, ICO_CH[sg[i].sh], &top, &h, &left, &w);
        TextOutW(mdc, sg[i].x + (sg[i].w - w) / 2 - left, HB / 2 - (top + h / 2), &ICO_CH[sg[i].sh], 1);
    }
    GdiFlush();

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            unsigned sum = 0;
            for (int j = 0; j < SS; j++) {
                const DWORD *row = bp + (y * SS + j) * WB + x * SS;
                for (int i = 0; i < SS; i++) sum += row[i] & 0xFF;
            }
            int r = y >= H / 2;
            DWORD a = lut[sum / (SS * SS)] * alOf[r][x] / 255;
            COLORREF c = colOf[r][x];
            px[y * W + x] = a ? (a << 24) | ((GetRValue(c) * a / 255) << 16) | ((GetGValue(c) * a / 255) << 8) | (GetBValue(c) * a / 255)
                              : 0x01000000;
        }
    SelectObject(mdc, bmp);
    DeleteObject(big);

    int gs = SC(16), gy = (H - gs) / 2;
    for (int i = 0; i < n; i++) {
        if (!sg[i].glyph || sg[i].sh == G_GPU || g_icoOK[sg[i].sh]) continue;
        glyph_cov(sg[i].sh == G_CPU ? SH_CPU : SH_RAM, gs > 64 ? 64 : gs, gc);
        int X0 = sg[i].x / SS;
        for (int y = 0; y < gs && y < 64; y++)
            for (int xx = 0; xx < gs && xx < 64; xx++) {
                DWORD a = gc[y * gs + xx];
                int Xp = X0 + xx, Y = gy + y;
                if (!a || Xp >= W || Y < 0 || Y >= H) continue;
                px[Y * W + Xp] = (a << 24) | ((GetRValue(fg) * a / 255) << 16) | ((GetGValue(fg) * a / 255) << 8) | (GetBValue(fg) * a / 255);
            }
    }

    POINT src = { 0, 0 }; SIZE sz = { W, H };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(LABEL_ALPHA_PCT[g_labelAlpha & 3] * 255 / 100), AC_SRC_ALPHA };
    g_barW = W; g_barH = H;
    if (!UpdateLayeredWindow(g_bar, NULL, NULL, &sz, mdc, &src, 0, &bf, ULW_ALPHA) && g_child) {
        bar_to_popup();
        UpdateLayeredWindow(g_bar, NULL, NULL, &sz, mdc, &src, 0, &bf, ULW_ALPHA);
    }
    SelectObject(mdc, ob);
    SelectObject(mdc, of);
    DeleteObject(bmp);
    DeleteDC(mdc);
    bar_place();
}

static void refresh(int force)
{
    tray_refresh(force);
    if (g_mode != MODE_BAR) { bar_destroy(); return; }
    if (!g_bar || !IsWindow(g_bar) || !g_tb || !IsWindow(g_tb) || g_tb != find_taskbar()) {
        bar_destroy(); bar_create(); force = 1;
    }
    bar_draw(force);
}

static DWORD WINAPI sampler(void *unused)
{
    (void)unused;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    int com = SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED));
    read_cpu();
    for (;;) {
        if (g_quit) break;
        if (InterlockedExchange(&g_rebuild, 0)) build_query();
        if (!g_paused) {
            sample();
            PostMessageW(g_wnd, WM_SAMPLED, 0, 0);
        }
        DWORD wait = g_interval;
        if (wait < GAME_INTERVAL && screen_busy()) wait = GAME_INTERVAL;   /* gaming: slowest */
        WaitForSingleObject(g_wake, g_paused ? INFINITE : wait);
    }
    if (g_q) PdhCloseQuery(g_q);
    if (com) CoUninitialize();
    return 0;
}

#include "ui.h"   /* themes, the menu panel, popups and the What's new window */

static void save(void)
{
    DWORD ver = MASK_VER;
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"Mask2", REG_DWORD, &g_mask, sizeof g_mask);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"MaskVer", REG_DWORD, &ver, sizeof ver);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"Interval", REG_DWORD, &g_interval, sizeof g_interval);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"Mode", REG_DWORD, &g_mode, sizeof g_mode);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"Offset", REG_DWORD, &g_offset, sizeof g_offset);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"Display", REG_DWORD, &g_display, sizeof g_display);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"Align", REG_DWORD, &g_align, sizeof g_align);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"TrayShows", REG_DWORD, &g_trayShows, sizeof g_trayShows);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"Theme", REG_DWORD, &g_theme, sizeof g_theme);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"Glass", REG_DWORD, &g_uiAlpha, sizeof g_uiAlpha);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"LabelOpacity", REG_DWORD, &g_labelAlpha, sizeof g_labelAlpha);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"MenuStyle", REG_DWORD, &g_menuStyle, sizeof g_menuStyle);
}
static void load(void)
{
    DWORD v, sz = sizeof v;
    if (!RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"Mask2", RRF_RT_REG_DWORD, NULL, &v, &sz) && (v & ((1u << M_N) - 1))) {
        g_mask = v & ((1u << M_N) - 1);
        /* Saved by an older version that had no power sensors: switch them on once. */
        DWORD ver = 0; sz = sizeof ver;
        if (RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"MaskVer", RRF_RT_REG_DWORD, NULL, &ver, &sz) || ver < MASK_VER) {
            g_mask |= MASK_POWER;
            ver = MASK_VER;
            RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"Mask2", REG_DWORD, &g_mask, sizeof g_mask);
            RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"MaskVer", REG_DWORD, &ver, sizeof ver);
        }
    }
    sz = sizeof v;
    if (!RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"Interval", RRF_RT_REG_DWORD, NULL, &v, &sz) && v >= 500 && v <= 60000)
        g_interval = v;
    sz = sizeof v;
    if (!RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"Mode", RRF_RT_REG_DWORD, NULL, &v, &sz) && v <= MODE_TRAY)
        g_mode = v;
    sz = sizeof v;
    if (!RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"Offset", RRF_RT_REG_DWORD, NULL, &v, &sz))
        g_offset = (LONG)v;
    sz = sizeof v;
    if (!RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"Display", RRF_RT_REG_DWORD, NULL, &v, &sz) && v <= MAX_TB)
        g_display = v;
    sz = sizeof v;
    if (!RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"Align", RRF_RT_REG_DWORD, NULL, &v, &sz) && v <= ALIGN_CENTER)
        g_align = v;
    sz = sizeof v;
    if (!RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"TrayShows", RRF_RT_REG_DWORD, NULL, &v, &sz) && v < M_N)
        g_trayShows = v;
    sz = sizeof v;
    if (!RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"Theme", RRF_RT_REG_DWORD, NULL, &v, &sz) && v < 7) g_theme = v;
    sz = sizeof v;
    if (!RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"Glass", RRF_RT_REG_DWORD, NULL, &v, &sz) && v < 4) g_uiAlpha = v;
    sz = sizeof v;
    if (!RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"LabelOpacity", RRF_RT_REG_DWORD, NULL, &v, &sz) && v < 4) g_labelAlpha = v;
    sz = sizeof v;
    if (!RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"MenuStyle", RRF_RT_REG_DWORD, NULL, &v, &sz) && v < 2) g_menuStyle = v;
}
static int is_admin(void)
{
    BOOL a = FALSE; PSID g;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &g)) {
        CheckTokenMembership(NULL, g, &a);
        FreeSid(g);
    }
    return a;
}
static DWORD reg_dword(const wchar_t *name)
{
    DWORD v = 0, sz = sizeof v;
    RegGetValueW(HKEY_CURRENT_USER, REGKEY, name, RRF_RT_REG_DWORD, NULL, &v, &sz);
    return v;
}
static void schtasks(const wchar_t *args)
{
    wchar_t app[MAX_PATH], cmd[MAX_PATH * 2 + 64];
    if (!sys_path(app, L"schtasks.exe")) return;
    _snwprintf(cmd, MAX_PATH * 2 + 64, L"\"%s\" %s", app, args); cmd[MAX_PATH * 2 + 63] = 0;
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si); si.cb = sizeof si;
    if (CreateProcessW(app, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
}
static void task_create(const wchar_t *exe);
static void task_remove_copy(void);
static void relaunch_admin(const wchar_t *args);
static int autostart_on(void)
{
    return reg_dword(L"Task") ||
           RegGetValueW(HKEY_CURRENT_USER, RUNKEY, APP, RRF_RT_REG_SZ, NULL, NULL, NULL) == ERROR_SUCCESS;
}
static void autostart_toggle(void)
{
    wchar_t exe[MAX_PATH], cmd[MAX_PATH * 2 + 128];
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH), zero = 0;
    if (!n || n >= MAX_PATH) return;
    if (autostart_on()) {
        if (reg_dword(L"Task") && !is_admin()) {
            if (ui_ask(L"Turning this off needs administrator rights",
                       L"TrayVitals starts as administrator at login. Restart TrayVitals as administrator to change that?",
                       L"Restart as administrator", L"Cancel", UI_QUESTION))
                relaunch_admin(L"/restart");
            return;
        }
        RegDeleteKeyValueW(HKEY_CURRENT_USER, RUNKEY, APP);
        if (reg_dword(L"Task")) {
            schtasks(L"/Delete /F /TN TrayVitals");
            task_remove_copy();
            RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"Task", REG_DWORD, &zero, sizeof zero);
        }
        return;
    }
    if (!is_admin()) {
        _snwprintf(cmd, MAX_PATH + 4, L"\"%s\"", exe); cmd[MAX_PATH + 3] = 0;
        RegSetKeyValueW(HKEY_CURRENT_USER, RUNKEY, APP, REG_SZ, cmd, (lstrlenW(cmd) + 1) * sizeof(wchar_t));
        return;
    }
    task_create(exe);
}

static void xml_escape(wchar_t *out, int n, const wchar_t *in)
{
    int o = 0;
    for (; *in && o < n - 7; in++) {
        const wchar_t *r = *in == L'&' ? L"&amp;" : *in == L'<' ? L"&lt;" : *in == L'>' ? L"&gt;" : NULL;
        if (r) { lstrcpyW(out + o, r); o += lstrlenW(r); }
        else out[o++] = *in;
    }
    out[o] = 0;
}

static int user_sid(wchar_t *out, int n)
{
    HANDLE t; BYTE b[256]; DWORD got; LPWSTR str; int ok = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) return 0;
    if (GetTokenInformation(t, TokenUser, b, sizeof b, &got) && ConvertSidToStringSidW(((TOKEN_USER *)b)->User.Sid, &str)) {
        lstrcpynW(out, str, n);
        LocalFree(str);
        ok = 1;
    }
    CloseHandle(t);
    return ok;
}

static void task_remove_copy(void)
{
    wchar_t dir[MAX_PATH], dst[MAX_PATH], self[MAX_PATH];
    if (!is_admin() || !app_dir(dir)) return;
    _snwprintf(dst, MAX_PATH, L"%s\\TrayVitals.exe", dir); dst[MAX_PATH - 1] = 0;
    GetModuleFileNameW(NULL, self, MAX_PATH);
    if (!lstrcmpiW(self, dst) || !DeleteFileW(dst)) {
        MoveFileExW(dst, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(dir, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    } else RemoveDirectoryW(dir);
}

static void task_create(const wchar_t *exe)
{
    wchar_t dir[MAX_PATH], dst[MAX_PATH], xmlPath[MAX_PATH], sid[184], esc[MAX_PATH * 5], xml[3072], args[MAX_PATH + 48];
    DWORD one = 1, w;
    if (!app_dir(dir) || !user_sid(sid, 184)) return;
    _snwprintf(dst, MAX_PATH, L"%s\\TrayVitals.exe", dir); dst[MAX_PATH - 1] = 0;
    if (lstrcmpiW(exe, dst) && !copy_self(dst)) return;
    xml_escape(esc, MAX_PATH * 5, dst);
    _snwprintf(xmlPath, MAX_PATH, L"%s\\task.xml", dir); xmlPath[MAX_PATH - 1] = 0;
    _snwprintf(xml, 3072,
        L"\xFEFF<?xml version=\"1.0\" encoding=\"UTF-16\"?>\r\n"
        L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">"
        L"<Triggers><LogonTrigger><Enabled>true</Enabled><UserId>%s</UserId></LogonTrigger></Triggers>"
        L"<Principals><Principal id=\"Author\"><UserId>%s</UserId><LogonType>InteractiveToken</LogonType>"
        L"<RunLevel>HighestAvailable</RunLevel></Principal></Principals>"
        L"<Settings><DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>"
        L"<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries><ExecutionTimeLimit>PT0S</ExecutionTimeLimit>"
        L"<MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy><Priority>5</Priority></Settings>"
        L"<Actions Context=\"Author\"><Exec><Command>%s</Command></Exec></Actions></Task>", sid, sid, esc);
    xml[3071] = 0;
    HANDLE f = CreateFileW(xmlPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    WriteFile(f, xml, lstrlenW(xml) * sizeof(wchar_t), &w, NULL);
    CloseHandle(f);
    _snwprintf(args, MAX_PATH + 48, L"/Create /F /TN TrayVitals /XML \"%s\"", xmlPath); args[MAX_PATH + 47] = 0;
    schtasks(args);
    DeleteFileW(xmlPath);
    RegDeleteKeyValueW(HKEY_CURRENT_USER, RUNKEY, APP);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"Task", REG_DWORD, &one, sizeof one);
}

static void autostart_migrate(void)
{
#ifdef TV_PREVIEW
    return;   /* never creates the real TrayVitals' startup task */
#endif
    wchar_t exe[MAX_PATH];
    if (!is_admin() || reg_dword(L"Task") ||
        RegGetValueW(HKEY_CURRENT_USER, RUNKEY, APP, RRF_RT_REG_SZ, NULL, NULL, NULL) != ERROR_SUCCESS) return;
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (n && n < MAX_PATH) task_create(exe);
}

static void relaunch_admin(const wchar_t *args)
{
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    SHELLEXECUTEINFOW e; ZeroMemory(&e, sizeof e);
    e.cbSize = sizeof e; e.lpVerb = L"runas"; e.lpFile = exe; e.lpParameters = args; e.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&e)) DestroyWindow(g_wnd);
}

static void cpu_temp_setup(int ask)
{
    DWORD one = 1;
    if (ask) {
        if (reg_dword(L"AskedCpuTemp")) return;
        RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"AskedCpuTemp", REG_DWORD, &one, sizeof one);
    }
    pio_init();
    InterlockedExchange(&g_rebuild, 1);
    SetEvent(g_wake);
    switch (g_pioState) {
    case PIO_OK:
        if (!ask) ui_msg(L"CPU temperature is on", NULL, UI_INFO);
        return;
    case PIO_UNSUPPORTED:
    case PIO_FAILED:
    case PIO_NOMODULE:
        if (!ask) ui_msg(L"CPU temperature isn't supported on this processor",
                         L"Intel Core and AMD Ryzen / EPYC processors are supported.", UI_INFO);
        return;
    case PIO_NOADMIN:
        if (ui_ask(L"CPU temperature needs administrator rights",
                   L"Restart TrayVitals as administrator? It will start that way automatically from now on.",
                   L"Restart as administrator", L"Not now", UI_QUESTION))
            relaunch_admin(L"/restart");
        return;
    default:
        if (ui_ask(L"Show CPU temperature?",
                   L"Windows doesn't let apps read the CPU's temperature sensor on its own, so TrayVitals "
                   L"installs PawnIO: a small, signed, open-source driver (the one LibreHardwareMonitor uses). "
                   L"It's verified before it runs and can be removed any time from Settings > Apps.\n\n"
                   L"Windows will ask for permission.",
                   L"Install and turn on", L"Not now", UI_QUESTION))
            relaunch_admin(L"/restart /install-pawnio");
    }
}

/* ---- "What's new" window: the patch notes from patchnotes.h, drawn by ui.h ---- */
static void notes_show(void) { ui_notes(); }

/* Opens the patch notes once after TrayVitals was updated (not on a brand-new install). */
static void notes_after_update(void)
{
    wchar_t last[32] = L"";
    DWORD sz = sizeof last;
    int had = RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"LastVersion", RRF_RT_REG_SZ, NULL, last, &sz) == ERROR_SUCCESS;
    if (had && !lstrcmpW(last, TV_VERSION)) return;
    /* Versions before 1.7.0 didn't record a version; settings they saved show TrayVitals was used before. */
    int update = had ||
        RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"Mask2", RRF_RT_REG_DWORD, NULL, NULL, NULL) == ERROR_SUCCESS ||
        RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"Mode", RRF_RT_REG_DWORD, NULL, NULL, NULL) == ERROR_SUCCESS;
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"LastVersion", REG_SZ, TV_VERSION, sizeof TV_VERSION);
    if (update) notes_show();
}

/* ---- Updates ----
   Once a day TrayVitals asks GitHub for the latest release of TV_UPDATE_REPO (one small request, no account
   needed), but not while a game or video fills the screen. If it's newer than this version, TrayVitals
   says so, again only once nothing fills the screen.
   "Update now" downloads that release's TrayVitals-update.bin (the bare program, about 260 KB), checks it
   against the SHA-256 checksum GitHub lists for the file, swaps it in for this copy and restarts: the
   in-app update. Releases without that file, or if the swap fails, use the installer instead:
   TrayVitals-Setup.exe, checked the same way and run with /update. */
#ifndef TV_APP_ASSET
#define TV_APP_ASSET "TrayVitals-update.bin"   /* the release file with the bare program */
#endif
#define WM_UPDATE        (WM_APP + 3)   /* lp: UPDINFO * from the update thread */
#define TIMER_UPDATE     9
#define TIMER_UPD_NOTICE 10
#define UPD_DAY_MS       (24u * 60 * 60 * 1000)
#define UPD_FILE         L"TrayVitals-Setup.exe"
enum { UPD_AUTO, UPD_MANUAL, UPD_DOWNLOAD };
enum { UPD_BADHASH = -2, UPD_FAILED = -1, UPD_CURRENT = 0, UPD_NEWER = 1 };
typedef struct {
    int     job, result, hasSha;
    wchar_t ver[24];          /* the newer version, like 1.9.1 */
    char    url[512];         /* its installer on github.com; "" if the release has none */
    BYTE    sha[32];
    char    aurl[512];        /* its bare program (in-app update); "" if the release has none */
    BYTE    asha[32];
    int     inApp;            /* the download is the bare program */
    wchar_t file[MAX_PATH];   /* where the download was saved */
} UPDINFO;
static UPDINFO       g_upd;   /* the newer version found (g_upd.ver[0] != 0); main thread only */
static volatile LONG g_updBusy;

/* "v1.9.0" / "1.9" -> {1,9,0}. */
static int ver_parse(const wchar_t *s, int v[3])
{
    v[0] = v[1] = v[2] = 0;
    if (*s == L'v' || *s == L'V') s++;
    for (int i = 0; i < 3; i++) {
        if (*s < L'0' || *s > L'9') return 0;
        while (*s >= L'0' && *s <= L'9') {
            v[i] = v[i] * 10 + (*s++ - L'0');
            if (v[i] > 99999) return 0;
        }
        if (*s != L'.') return 1;
        s++;
    }
    return 1;
}

static int ver_newer(const int a[3], const int b[3])
{
    for (int i = 0; i < 3; i++) if (a[i] != b[i]) return a[i] > b[i];
    return 0;
}

/* Finds "key": "value" in the JSON text [p, end) and copies the value. Returns a pointer just past it,
   or NULL (not found, or not a string, e.g. null). */
static const char *json_str(const char *p, const char *end, const char *key, char *out, int n)
{
    int kl = lstrlenA(key);
    for (; p + kl + 2 < end; p++) {
        if (*p != '"' || memcmp(p + 1, key, kl) || p[kl + 1] != '"') continue;
        const char *q = p + kl + 2;
        while (q < end && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) q++;
        if (q >= end || *q != ':') continue;
        for (q++; q < end && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n'); q++) ;
        if (q >= end || *q != '"') return NULL;
        int o = 0;
        for (q++; q < end && *q != '"'; q++) {
            if (*q == '\\' && q + 1 < end) q++;
            if (o < n - 1) out[o++] = *q;
        }
        out[o] = 0;
        return q < end ? q + 1 : NULL;
    }
    return NULL;
}

static int hexval(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

#define GH_PREFIX "https://github.com/"

/* Reads the reply to /repos/{repo}/releases/latest. */
static void upd_parse(const char *j, DWORD len, UPDINFO *u)
{
    const char *end = j + len, *from = j, *after;
    char tag[32], url[512], dg[96];
    int nv[3], cur[3];
    u->result = UPD_FAILED;
    if (!json_str(j, end, "tag_name", tag, sizeof tag)) return;
    MultiByteToWideChar(CP_UTF8, 0, tag, -1, u->ver, 24);
    u->ver[23] = 0;
    if (!ver_parse(u->ver, nv) || !ver_parse(TV_VERSION, cur)) return;
    if (u->ver[0] == L'v' || u->ver[0] == L'V') memmove(u->ver, u->ver + 1, lstrlenW(u->ver) * sizeof(wchar_t));
    if (!ver_newer(nv, cur)) { u->result = UPD_CURRENT; return; }
    u->result = UPD_NEWER;
    /* Each file attached to the release lists its "digest" and then, last, its "browser_download_url".
       Two of them matter: the installer and the bare program. Neither is used without its checksum. */
    while ((after = json_str(from, end, "browser_download_url", url, sizeof url))) {
        const char *names[2] = { TV_UPDATE_ASSET, TV_APP_ASSET };
        for (int k = 0; k < 2; k++) {
            int ul = lstrlenA(url), al = lstrlenA(names[k]);
            if (!(ul > al + (int)sizeof GH_PREFIX - 1 && !_strnicmp(url, GH_PREFIX, sizeof GH_PREFIX - 1) &&
                  url[ul - al - 1] == '/' && !_stricmp(url + ul - al, names[k]) && !strpbrk(url, " \"<>\\"))) continue;
            const char *q = from, *q2;
            BYTE sha[32];
            int got = 0, ok = 0;
            while ((q2 = json_str(q, after, "digest", dg, sizeof dg))) { got = 1; q = q2; }
            if (got && !_strnicmp(dg, "sha256:", 7) && lstrlenA(dg) == 7 + 64) {
                ok = 1;
                for (int i = 0; i < 32 && ok; i++) {
                    int hi = hexval(dg[7 + 2 * i]), lo = hexval(dg[8 + 2 * i]);
                    if (hi < 0 || lo < 0) ok = 0; else sha[i] = (BYTE)(hi << 4 | lo);
                }
            }
            if (!ok) continue;
            if (k == 0) { memcpy(u->sha, sha, 32); u->hasSha = 1; lstrcpyA(u->url, url); }
            else { memcpy(u->asha, sha, 32); lstrcpyA(u->aurl, url); }
        }
        from = after;
    }
}

static void upd_check(UPDINFO *u)
{
    wchar_t path[200];
    DWORD len = 0, status = 0;
    _snwprintf(path, 200, L"/repos/%s/releases/latest", TV_UPDATE_REPO); path[199] = 0;
    char *j = (char *)http_get_ex(L"api.github.com", path, &len, &status);
    u->result = UPD_FAILED;
    if (!j) {
        if (status == 404) u->result = UPD_CURRENT;   /* no release published yet: nothing newer */
        return;
    }
    upd_parse(j, len, u);
    HeapFree(GetProcessHeap(), 0, j);
}

/* Where the installer is saved. Running as administrator: Program Files\TrayVitals, which only
   administrators can change, so nothing can swap the checked file before it runs. Otherwise the
   user's temp folder (Windows then asks for permission to run it, showing the file). */
static int upd_path(wchar_t *out, int create)
{
    wchar_t dir[MAX_PATH];
    if (is_admin()) {
        if (create) { if (!app_dir(dir)) return 0; }
        else {
            DWORD sz = sizeof dir;
            if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", L"ProgramFilesDir",
                             RRF_RT_REG_SZ, NULL, dir, &sz) != ERROR_SUCCESS || lstrlenW(dir) > MAX_PATH - 48) return 0;
            lstrcatW(dir, L"\\TrayVitals");
        }
    } else {
        DWORD n = GetTempPathW(MAX_PATH, dir);
        if (!n || n > MAX_PATH - 48) return 0;
        lstrcatW(dir, L"TrayVitals-update");
        if (create && !CreateDirectoryW(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    }
    _snwprintf(out, MAX_PATH, L"%s\\" UPD_FILE, dir); out[MAX_PATH - 1] = 0;
    return 1;
}

/* Removes the installer left from the last update (it can't delete itself while it runs). */
static void upd_cleanup(void)
{
    wchar_t f[MAX_PATH], b[MAX_PATH];
    if (!upd_path(f, 0)) return;
    lstrcpyW(b, f);
    wchar_t *s = wcsrchr(b, L'\\');
    if (s) { lstrcpyW(s + 1, L"TrayVitals-update.bin"); DeleteFileW(b); }   /* a leftover in-app download */
    if (!DeleteFileW(f) || is_admin()) return;
    *wcsrchr(f, L'\\') = 0;
    RemoveDirectoryW(f);
}

static int g_updNoApp;   /* the in-app update failed once: use the installer from now on */

static void upd_download(UPDINFO *u)
{
    wchar_t path[512];
    DWORD len = 0, w;
    u->result = UPD_FAILED;
    u->inApp = u->aurl[0] && !g_updNoApp;
    const char *url = u->inApp ? u->aurl : u->url;
    const BYTE *sha = u->inApp ? u->asha : u->sha;
    if ((!u->inApp && !u->hasSha) || _strnicmp(url, GH_PREFIX, sizeof GH_PREFIX - 1) ||
        !MultiByteToWideChar(CP_UTF8, 0, url + sizeof GH_PREFIX - 2, -1, path, 512)) return;
    BYTE *bin = http_get(L"github.com", path, &len);
    if (!bin) return;
    if (!sha256_is(bin, len, sha)) { u->result = UPD_BADHASH; HeapFree(GetProcessHeap(), 0, bin); return; }
    if (upd_path(u->file, 1)) {
        if (u->inApp) { wchar_t *s = wcsrchr(u->file, L'\\'); if (s) lstrcpyW(s + 1, L"TrayVitals-update.bin"); }
        HANDLE f = CreateFileW(u->file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        int ok = f != INVALID_HANDLE_VALUE && WriteFile(f, bin, len, &w, NULL) && w == len;
        if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
        if (ok) u->result = UPD_NEWER; else DeleteFileW(u->file);
    }
    HeapFree(GetProcessHeap(), 0, bin);
}

static DWORD WINAPI upd_thread(void *p)
{
    UPDINFO *u = p;
    u->result = UPD_FAILED;
    if (load_net()) {
        if (u->job == UPD_DOWNLOAD) upd_download(u);
        else upd_check(u);
    }
    if (!PostMessageW(g_wnd, WM_UPDATE, 0, (LPARAM)u)) HeapFree(GetProcessHeap(), 0, u);
    return 0;
}

static int upd_start(int job)
{
    if (!TV_UPDATE_REPO[0] || InterlockedExchange(&g_updBusy, 1)) return 0;
    UPDINFO *u = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *u);
    if (u) {
        if (job == UPD_DOWNLOAD) *u = g_upd;
        u->job = job;
        HANDLE t = CreateThread(NULL, 256 * 1024, upd_thread, u, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
        if (t) { CloseHandle(t); return 1; }
        HeapFree(GetProcessHeap(), 0, u);
    }
    InterlockedExchange(&g_updBusy, 0);
    return 0;
}

static int upd_auto_on(void) { return TV_UPDATE_REPO[0] && !reg_dword(L"NoUpdateCheck"); }

/* Next automatic check: a day after the last one that got an answer, but not sooner than minMs. */
static void upd_schedule(DWORD minMs)
{
    if (!upd_auto_on()) { KillTimer(g_wnd, TIMER_UPDATE); return; }
    ULONGLONG last = 0, now; DWORD sz = sizeof last; FILETIME f;
    RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"UpdateLastCheck", RRF_RT_REG_QWORD, NULL, &last, &sz);
    GetSystemTimeAsFileTime(&f); now = ft(f);
    ULONGLONG due = last + (ULONGLONG)UPD_DAY_MS * 10000, ms = due > now ? (due - now) / 10000 : 0;
    if (ms > UPD_DAY_MS) ms = UPD_DAY_MS;   /* clock was moved back */
    if (ms < minMs) ms = minMs;
    SetTimer(g_wnd, TIMER_UPDATE, (UINT)ms, NULL);
}

static void open_releases(void)
{
    ShellExecuteW(NULL, L"open", L"https://github.com/" TV_UPDATE_REPO L"/releases/latest", NULL, NULL, SW_SHOWNORMAL);
}

static void upd_now(void)
{
    if (g_upd.url[0] || g_upd.aurl[0]) upd_start(UPD_DOWNLOAD);
    else open_releases();
}

static void upd_prompt(void)
{
    wchar_t head[96], body[400];
    _snwprintf(head, 96, L"TrayVitals %s is available", g_upd.ver); head[95] = 0;
    _snwprintf(body, 400, L"You have version " TV_VERSION L".\n\n%s", !g_upd.url[0]
                   ? L"Get it from the TrayVitals download page."
                   : is_admin() ? L"Updating takes a few seconds and keeps your settings. TrayVitals restarts when it's done."
                                : L"Updating takes a few seconds and keeps your settings. Windows will ask for permission, "
                                  L"then TrayVitals restarts.");
    body[399] = 0;
    UIBTN b[3] = { { 100, g_upd.url[0] ? L"Update now" : L"Open download page" },
                   { 101, L"Later" }, { 102, L"Skip this version" } };
    if (g_upd.aurl[0] && !g_updNoApp) {
        _snwprintf(body, 400, L"You have version " TV_VERSION L".\n\n%s", is_admin()
                       ? L"Updating takes a few seconds (a small download) and keeps your settings. TrayVitals restarts when it's done."
                       : L"Updating takes a few seconds (a small download) and keeps your settings. Windows will ask for "
                         L"permission, then TrayVitals restarts.");
        body[399] = 0;
    }
    UIBTN b2[3] = { { 100, (g_upd.url[0] || g_upd.aurl[0]) ? L"Update now" : L"Open download page" },
                    { 101, L"Later" }, { 102, L"Skip this version" } };
    memcpy(b, b2, sizeof b);
    int c = ui_dialog(head, body, b, 3, UI_UPDATE | UI_SOLID);
    if (c == 100) upd_now();
    else if (c == 102)
        RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"UpdateSkip", REG_SZ, g_upd.ver, (lstrlenW(g_upd.ver) + 1) * sizeof(wchar_t));
}

/* Is a game, video or presentation filling the screen? Windows reports exclusive full screen and
   presentations itself; a borderless full-screen game or video is spotted by its window covering
   the whole monitor. (A maximized window leaves the taskbar visible, so it doesn't count.) */
static int screen_busy(void)
{
    QUERY_USER_NOTIFICATION_STATE s;
    if (SUCCEEDED(SHQueryUserNotificationState(&s)) && s != QUNS_ACCEPTS_NOTIFICATIONS && s != QUNS_QUIET_TIME) return 1;
    HWND f = GetForegroundWindow();
    wchar_t cls[64] = L"";
    RECT r;
    MONITORINFO mi = { sizeof mi };
    if (!f || !IsWindowVisible(f) || IsIconic(f)) return 0;
    GetClassNameW(f, cls, 64);
    if (!lstrcmpW(cls, L"Progman") || !lstrcmpW(cls, L"WorkerW") || !lstrcmpW(cls, L"Shell_TrayWnd")) return 0;
    if (!GetWindowRect(f, &r) || !GetMonitorInfoW(MonitorFromWindow(f, MONITOR_DEFAULTTONEAREST), &mi)) return 0;
    return r.left <= mi.rcMonitor.left && r.top <= mi.rcMonitor.top &&
           r.right >= mi.rcMonitor.right && r.bottom >= mi.rcMonitor.bottom;
}

/* An automatic check found an update: say so, but never over a game, video or presentation.
   While one fills the screen, look again every minute, so the popup comes soon after it ends. */
static void upd_notice(void)
{
    if (screen_busy()) { SetTimer(g_wnd, TIMER_UPD_NOTICE, 60 * 1000, NULL); return; }
    upd_prompt();
}

/* ---- One-time request to AMD Radeon users: is GPU power right? ----
   GPU power on AMD cards (1.10.0) was written from AMD's documentation, without a Radeon card to test on.
   So TrayVitals asks AMD users, once, to compare its number with AMD Software's and report it on GitHub.
   Remove this (set TV_ASK_AMD_POWER to 0) once enough cards are confirmed. */
#ifndef TV_ASK_AMD_POWER
#define TV_ASK_AMD_POWER 1
#endif
#define TIMER_AMD_ASK 11
#define AMD_ASK_DELAY_MS (3 * 60 * 1000)   /* a few minutes after starting, after What's new and update checks */

/* Percent-encodes s for a URL query value. */
static void url_enc(wchar_t *out, int cap, const wchar_t *s)
{
    static const wchar_t hex[] = L"0123456789ABCDEF";
    int n = 0;
    for (; *s && n < cap - 4; s++) {
        wchar_t ch = *s;
        if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9') ||
            ch == L'-' || ch == L'.' || ch == L'_' || ch == L'~') out[n++] = ch;
        else if (ch < 0x80) { out[n++] = L'%'; out[n++] = hex[ch >> 4]; out[n++] = hex[ch & 15]; }
    }
    out[n] = 0;
}

/* Cards whose GPU power is already confirmed: these aren't asked any more.
   Radeon RX 9000 (RX 9060 / 9070): matches AMD Software within a few watts (RX 9070 XT, 1.11.3). */
static int amd_confirmed(void)
{
    return wcsstr(g_gpuName, L"RX 90") != NULL;
}

static void amd_ask(void)
{
    DWORD asked = 0, sz = sizeof asked;
    if (!TV_ASK_AMD_POWER || g_gpuVendor != 0x1002 || !TV_UPDATE_REPO[0] || amd_confirmed()) return;
    RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"AmdPowerAsked", RRF_RT_REG_DWORD, NULL, &asked, &sz);
    if (asked) return;
    if (screen_busy()) { SetTimer(g_wnd, TIMER_AMD_ASK, 60 * 1000, NULL); return; }   /* not over a game */

    wchar_t now[80] = L"";
    if ((g_mask & (1u << M_GPUP)) && g_ok[M_GPUP])
        wsprintfW(now, L" Right now it reads %d W.", (int)(g_val[M_GPUP] + 0.5));
    wchar_t body[700];
    wsprintfW(body,
        L"GPU power on AMD Radeon cards is new in TrayVitals, and we couldn't test it on every card.%s\n\n"
        L"Could you compare it with AMD Software (Alt+R > Performance > Metrics), once while idle and once in a "
        L"game, and send us both numbers? It takes a minute and helps fix it for everyone with your card.\n\n"
        L"Report on GitHub opens a short form in your browser, with your card already filled in.", now);
    UIBTN b[3] = { { 100, L"Report on GitHub" }, { 101, L"Later" }, { 102, L"Don't ask again" } };
    int r = ui_dialog(L"Does GPU power look right on your Radeon?", body, b, 3, UI_QUESTION | UI_SOLID);
    if (r == 100 || r == 102) {   /* "Later", Esc or closing asks again next time TrayVitals starts */
        asked = 1;
        RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"AmdPowerAsked", REG_DWORD, &asked, sizeof asked);
    }
    if (r == 100) {
        wchar_t gpu[200], raw[32], tv[100], url[600];
        url_enc(gpu, 200, g_gpuName);
        lstrcpyW(raw, L"Not shown");
        if ((g_mask & (1u << M_GPUP)) && g_ok[M_GPUP]) wsprintfW(raw, L"%d W", (int)(g_val[M_GPUP] + 0.5));
        url_enc(tv, 100, raw);
        wsprintfW(url, L"https://github.com/" TV_UPDATE_REPO L"/issues/new?template=amd_power.yml&gpu=%s&version="
                  TV_VERSION L"&reading=%s", gpu, tv);
        ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
    }
}

/* ---- In-app update: swapping in the new program ---- */
#define UNKEY_TV L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TrayVitals"

/* The copies to replace: this one, the installed one (where Settings > Apps says it is) and the
   Program Files copy the "start as administrator at login" task runs. Each only once. */
static int upd_targets(wchar_t t[3][MAX_PATH])
{
    wchar_t p[MAX_PATH], c[3][MAX_PATH];
    int n = 0, k = 0;
    DWORD sz, len = GetModuleFileNameW(NULL, p, MAX_PATH);
    if (len && len < MAX_PATH) lstrcpyW(c[k++], p);
    sz = sizeof p;
    if (!RegGetValueW(HKEY_LOCAL_MACHINE, UNKEY_TV, L"InstallLocation", RRF_RT_REG_SZ, NULL, p, &sz) && lstrlenW(p) < MAX_PATH - 20) {
        lstrcatW(p, L"\\TrayVitals.exe");
        lstrcpyW(c[k++], p);
    }
    sz = sizeof p;
    if (!RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", L"ProgramFilesDir",
                      RRF_RT_REG_SZ, NULL, p, &sz) && lstrlenW(p) < MAX_PATH - 30) {
        lstrcatW(p, L"\\TrayVitals\\TrayVitals.exe");
        lstrcpyW(c[k++], p);
    }
#ifdef TV_UPDTEST
    k = 1;   /* tests: only the test program itself */
#endif
    for (int i = 0; i < k; i++) {
        int dup = GetFileAttributesW(c[i]) == INVALID_FILE_ATTRIBUTES;   /* only copies that exist */
        for (int j = 0; j < n && !dup; j++) if (!lstrcmpiW(t[j], c[i])) dup = 1;
        if (!dup) lstrcpyW(t[n++], c[i]);
    }
    return n;
}

/* Replaces every copy with the new program. Each old copy is renamed to .old first (a running
   program can be renamed, not overwritten); if anything fails, all of them are put back.
   Returns 1 when every copy is the new program. Needs administrator rights. */
static int upd_swap(const BYTE *bin, DWORD len, const wchar_t *ver)
{
    wchar_t t[3][MAX_PATH], old[3][MAX_PATH];
    int n = upd_targets(t), done = 0, ok = 1;
    DWORD w;
    if (g_self != INVALID_HANDLE_VALUE) { CloseHandle(g_self); g_self = INVALID_HANDLE_VALUE; }   /* our own lock */
    for (int i = 0; i < n && ok; i++) {
        _snwprintf(old[i], MAX_PATH, L"%s.old", t[i]); old[i][MAX_PATH - 1] = 0;
        DeleteFileW(old[i]);
        if (!MoveFileExW(t[i], old[i], MOVEFILE_REPLACE_EXISTING)) { ok = 0; break; }
        done = i + 1;
        HANDLE f = CreateFileW(t[i], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        ok = f != INVALID_HANDLE_VALUE && WriteFile(f, bin, len, &w, NULL) && w == len;
        if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    }
    if (!ok) {
        for (int i = 0; i < done; i++) { DeleteFileW(t[i]); MoveFileExW(old[i], t[i], MOVEFILE_REPLACE_EXISTING); }
        return 0;
    }
    if (n) RegSetKeyValueW(HKEY_LOCAL_MACHINE, UNKEY_TV, L"DisplayVersion", REG_SZ, ver, (lstrlenW(ver) + 1) * sizeof(wchar_t));
    return n > 0;
}

/* Reads a downloaded file and checks it against its checksum. The caller frees the buffer. */
static BYTE *upd_read(const wchar_t *file, const BYTE *sha, DWORD *len)
{
    HANDLE f = CreateFileW(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    LARGE_INTEGER sz;
    BYTE *b = NULL;
    DWORD got = 0;
    if (f == INVALID_HANDLE_VALUE) return NULL;
    if (GetFileSizeEx(f, &sz) && sz.QuadPart > 0 && sz.QuadPart < (16 << 20) &&
        (b = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)sz.QuadPart)) &&
        !(ReadFile(f, b, (DWORD)sz.QuadPart, &got, NULL) && got == (DWORD)sz.QuadPart && sha256_is(b, got, sha))) {
        HeapFree(GetProcessHeap(), 0, b);
        b = NULL;
    }
    CloseHandle(f);
    *len = got;
    return b;
}

/* Starts a program as the signed-in user, not as administrator (through Explorer). */
static void start_unelevated(const wchar_t *exe)
{
    wchar_t ex[MAX_PATH], arg[MAX_PATH + 4];
    if (sys_path(ex, L"..\\explorer.exe")) {
        _snwprintf(arg, MAX_PATH + 4, L"\"%s\"", exe); arg[MAX_PATH + 3] = 0;
        if ((INT_PTR)ShellExecuteW(NULL, L"open", ex, arg, NULL, SW_SHOWNORMAL) > 32) return;
    }
    ShellExecuteW(NULL, L"open", exe, NULL, NULL, SW_SHOWNORMAL);
}

/* The in-app update, once the new program is downloaded and checked. Returns 1 if it's on its way
   (this copy then closes), 0 if it didn't work (the caller falls back to the installer). */
static int upd_apply(UPDINFO *u)
{
    wchar_t self[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, self, MAX_PATH), len;
    if (!n || n >= MAX_PATH) return 0;
    if (is_admin()) {
        BYTE *bin = upd_read(u->file, u->asha, &len);
        int ok = bin && upd_swap(bin, len, u->ver);
        if (bin) HeapFree(GetProcessHeap(), 0, bin);
        DeleteFileW(u->file);
        if (!ok) return 0;
        STARTUPINFOW si; PROCESS_INFORMATION pi;
        wchar_t cmd[MAX_PATH + 16];
        ZeroMemory(&si, sizeof si); si.cb = sizeof si;
        _snwprintf(cmd, MAX_PATH + 16, L"\"%s\" /restart", self); cmd[MAX_PATH + 15] = 0;
        if (CreateProcessW(self, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
        DestroyWindow(g_wnd);
        return 1;
    }
    /* Not administrator: one permission prompt starts a helper (this same program) that checks the file
       again, swaps it in, waits for this copy to close and starts the new one as the signed-in user. */
    wchar_t args[MAX_PATH + 200], hex[65];
    for (int i = 0; i < 32; i++) _snwprintf(hex + 2 * i, 3, L"%02x", u->asha[i]);
    hex[64] = 0;
    _snwprintf(args, MAX_PATH + 200, L"/apply-update %lu %s %s \"%s\"", GetCurrentProcessId(), hex, u->ver, u->file);
    args[MAX_PATH + 199] = 0;
    SHELLEXECUTEINFOW e; ZeroMemory(&e, sizeof e);
    e.cbSize = sizeof e; e.fMask = SEE_MASK_NOASYNC; e.hwnd = g_wnd;
    e.lpVerb = L"runas"; e.lpFile = self; e.lpParameters = args; e.nShow = SW_HIDE;
    if (ShellExecuteExW(&e)) { DestroyWindow(g_wnd); return 1; }
    if (GetLastError() == ERROR_CANCELLED) return 1;   /* "No" to Windows' question: nothing to fall back to */
    return 0;
}

/* The helper's side of the above (started as administrator with /apply-update). */
static int upd_helper(void)
{
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int r = 1;
    if (argv && argc >= 6 && is_admin() && lstrlenW(argv[3]) == 64) {
        BYTE sha[32];
        int ok = 1;
        for (int i = 0; i < 32 && ok; i++) {
            char h[3] = { (char)argv[3][2 * i], (char)argv[3][2 * i + 1], 0 };
            int hi = hexval(h[0]), lo = hexval(h[1]);
            if (hi < 0 || lo < 0) ok = 0; else sha[i] = (BYTE)(hi << 4 | lo);
        }
        DWORD len, pid = (DWORD)_wtoi(argv[2]);
        BYTE *bin = ok ? upd_read(argv[5], sha, &len) : NULL;   /* checked again: the file sat in the user's temp folder */
        HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (p) { WaitForSingleObject(p, 10000); CloseHandle(p); }   /* the old copy closes first */
        if (bin && upd_swap(bin, len, argv[4])) {
            wchar_t self[MAX_PATH];
            DWORD n = GetModuleFileNameW(NULL, self, MAX_PATH);
            if (n && n < MAX_PATH) start_unelevated(self);
            r = 0;
        }
        if (bin) HeapFree(GetProcessHeap(), 0, bin);
        DeleteFileW(argv[5]);
        if (r) {   /* didn't work: start the old copy again, so TrayVitals doesn't just disappear */
            wchar_t self[MAX_PATH];
            DWORD n = GetModuleFileNameW(NULL, self, MAX_PATH);
            if (n && n < MAX_PATH) start_unelevated(self);
        }
    }
    if (argv) LocalFree(argv);
    return r;
}

/* Removes the .old copies a finished in-app update left behind. */
static void upd_old_cleanup(void)
{
    wchar_t t[3][MAX_PATH], o[MAX_PATH];
    int n = upd_targets(t);
    for (int i = 0; i < n; i++) {
        _snwprintf(o, MAX_PATH, L"%s.old", t[i]); o[MAX_PATH - 1] = 0;
        if (GetFileAttributesW(o) != INVALID_FILE_ATTRIBUTES && !DeleteFileW(o) && is_admin())
            MoveFileExW(o, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
}

static void upd_run(const wchar_t *file)
{
    SHELLEXECUTEINFOW e; ZeroMemory(&e, sizeof e);
    e.cbSize = sizeof e; e.fMask = SEE_MASK_NOASYNC; e.hwnd = g_wnd;
    e.lpVerb = L"runas"; e.lpFile = file; e.lpParameters = L"/update"; e.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&e)) { DestroyWindow(g_wnd); return; }   /* setup replaces this copy and restarts it */
    if (GetLastError() != ERROR_CANCELLED)
        ui_msg(L"Couldn't start the TrayVitals update", NULL, UI_WARN | UI_SOLID);
}

static void upd_done(UPDINFO *u)
{
    InterlockedExchange(&g_updBusy, 0);
    if (u->job == UPD_DOWNLOAD) {
        if (u->result == UPD_NEWER && u->inApp) {
            if (!upd_apply(u)) {   /* the in-app update didn't work: the installer, as before */
                g_updNoApp = 1;
                if (g_upd.url[0]) { HeapFree(GetProcessHeap(), 0, u); upd_start(UPD_DOWNLOAD); return; }
                ui_msg(L"Couldn't install the update", L"Get it from the TrayVitals download page.", UI_WARN | UI_SOLID);
                open_releases();
            }
        }
        else if (u->result == UPD_NEWER) upd_run(u->file);
        else if (ui_ask(u->result == UPD_BADHASH ? L"The update didn't pass its check" : L"Couldn't download the update",
                        u->result == UPD_BADHASH
                            ? L"The downloaded file didn't match its checksum, so it was not run. Open the download page instead?"
                            : L"Check your internet connection. Open the download page instead?",
                        L"Open download page", L"Close", UI_WARN | UI_SOLID))
            open_releases();
    } else {
        if (u->result != UPD_FAILED) {
            FILETIME f; GetSystemTimeAsFileTime(&f);
            ULONGLONG t = ft(f);
            RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"UpdateLastCheck", REG_QWORD, &t, sizeof t);
        }
        if (u->result == UPD_NEWER) {
            wchar_t skip[24] = L""; DWORD sz = sizeof skip;
            RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"UpdateSkip", RRF_RT_REG_SZ, NULL, skip, &sz);
            g_upd = *u;
            if (u->job == UPD_MANUAL) upd_prompt();
            else if (lstrcmpW(skip, u->ver)) upd_notice();
        } else {
            if (u->result == UPD_CURRENT) ZeroMemory(&g_upd, sizeof g_upd);
            if (u->job == UPD_MANUAL)
                ui_msg(u->result == UPD_CURRENT ? L"TrayVitals is up to date" : L"Couldn't check for updates",
                       u->result == UPD_CURRENT ? L"You have the latest version, " TV_VERSION L"."
                                                : L"Check your internet connection and try again.",
                       (u->result == UPD_CURRENT ? UI_INFO : UI_WARN) | UI_SOLID);
        }
        if (u->job == UPD_AUTO) upd_schedule(u->result == UPD_FAILED ? 60 * 60 * 1000 : 60 * 1000);
    }
    HeapFree(GetProcessHeap(), 0, u);
}

/* ---- Tray icon hidden by Windows ----
   Windows 11 puts new tray icons in the hidden-icons area, and when the hidden-icons menu (^) is off
   they can't be seen at all. Only the user may change that, so TrayVitals says so and offers to open
   the right Settings page. Windows records each icon's state under Control Panel\NotifyIconSettings
   (the program path, with folders like Program Files written as a {known folder id}). */
#define TIMER_TRAYCHECK       7   /* after switching to tray icons from the menu: always tell */
#define TIMER_TRAYCHECK_START 8   /* at startup in tray icon mode: tell only once, ever */

static int exe_matches(const wchar_t *stored, const wchar_t *exe)
{
    if (!lstrcmpiW(stored, exe)) return 1;
    const wchar_t *close = stored[0] == L'{' ? wcschr(stored, L'}') : NULL;
    if (!close || close - stored > 38) return 0;
    wchar_t g[40], full[MAX_PATH * 2];
    lstrcpynW(g, stored, (int)(close - stored) + 2);
    GUID id; PWSTR base = NULL;
    if (FAILED(CLSIDFromString(g, &id)) || FAILED(SHGetKnownFolderPath(&id, 0, NULL, &base)) || !base) return 0;
    _snwprintf(full, MAX_PATH * 2, L"%s%s", base, close + 1); full[MAX_PATH * 2 - 1] = 0;
    CoTaskMemFree(base);
    return !lstrcmpiW(full, exe);
}

/* 1 = Windows hides the tray icon, 0 = it's shown on the taskbar, -1 = can't tell (e.g. Windows 10). */
static int tray_icon_hidden(void)
{
    wchar_t exe[MAX_PATH], name[64], path[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    HKEY k;
    int res = -1;
    if (!n || n >= MAX_PATH || RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\NotifyIconSettings", 0, KEY_READ, &k))
        return -1;
    for (DWORD i = 0; res < 0; i++) {
        DWORD nl = 64, sz = sizeof path, uid = 0, usz = sizeof uid, prom = 0, psz = sizeof prom;
        if (RegEnumKeyExW(k, i, name, &nl, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
        if (RegGetValueW(k, name, L"UID", RRF_RT_REG_DWORD, NULL, &uid, &usz) || uid != TRAY_ID) continue;
        if (RegGetValueW(k, name, L"ExecutablePath", RRF_RT_REG_SZ, NULL, path, &sz) || !exe_matches(path, exe)) continue;
        RegGetValueW(k, name, L"IsPromoted", RRF_RT_REG_DWORD, NULL, &prom, &psz);
        res = prom ? 0 : 1;
    }
    RegCloseKey(k);
    return res;
}

/* Windows keeps an entry in Settings > Personalization > Taskbar > Other system tray icons for every
   tray icon a program ever showed, all listed under the program's name. Older TrayVitals versions used
   one icon per reading, so the list fills up with identical "TrayVitals" entries and it isn't clear
   which one to turn on. Remove TrayVitals' own leftovers: this copy's entries other than its one icon,
   and entries of TrayVitals copies at other paths (an older install, a moved or deleted copy).
   Returns how many were removed. Nothing belonging to other programs is touched. */
static int tray_settings_cleanup(void)
{
#ifdef TV_PREVIEW
    return 0;   /* would remove the installed TrayVitals' entries */
#endif
    enum { MAXDEL = 64 };
    static wchar_t del[MAXDEL][64];
    wchar_t exe[MAX_PATH], name[64], path[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    HKEY k;
    int nd = 0;
    if (!n || n >= MAX_PATH ||
        RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\NotifyIconSettings", 0, KEY_READ | DELETE | KEY_SET_VALUE, &k))
        return 0;
    for (DWORD i = 0; nd < MAXDEL; i++) {
        DWORD nl = 64, sz = sizeof path, uid = 0, usz = sizeof uid;
        if (RegEnumKeyExW(k, i, name, &nl, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
        if (RegGetValueW(k, name, L"ExecutablePath", RRF_RT_REG_SZ, NULL, path, &sz)) continue;
        const wchar_t *file = wcsrchr(path, L'\\');
        file = file ? file + 1 : path;
        int len = lstrlenW(file);
        /* TrayVitals.exe, or a renamed/test copy like TrayVitals_test.exe */
        if (_wcsnicmp(file, L"TrayVitals", 10) || len < 14 || lstrcmpiW(file + len - 4, L".exe")) continue;
        int mine = exe_matches(path, exe);
        int hasUid = !RegGetValueW(k, name, L"UID", RRF_RT_REG_DWORD, NULL, &uid, &usz);
        if (mine && (!hasUid || uid == TRAY_ID)) continue;   /* this copy's one icon: keep */
        lstrcpynW(del[nd++], name, 64);
    }
    for (int i = 0; i < nd; i++) RegDeleteKeyW(k, del[i]);
    RegCloseKey(k);
    return nd;
}

static int hidden_menu_off(void)
{
    DWORD v = 1, sz = sizeof v;
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\TrayNotify",
                 L"SystemTrayChevronVisibility", RRF_RT_REG_DWORD, NULL, &v, &sz);
    return v == 0;
}

static void tray_hidden_warning(int startup)
{
    DWORD one = 1;
    if (g_mode != MODE_TRAY || !g_trayOn || tray_icon_hidden() != 1) return;
    if (startup && reg_dword(L"TrayHiddenWarned")) return;
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"TrayHiddenWarned", REG_DWORD, &one, sizeof one);

    wchar_t body[700];
    _snwprintf(body, 700, L"%s\n\nTo show it, open taskbar settings, expand \"Other system tray icons\" and turn "
                          L"TrayVitals on.%s",
               hidden_menu_off()
                   ? L"Windows hides new tray icons, and the hidden icons menu (^ next to the clock) is turned "
                     L"off on this PC, so TrayVitals' icon can't be seen at all."
                   : L"Windows put TrayVitals' icon in the hidden icons menu (^ next to the clock) instead of on "
                     L"the taskbar.",
               L"");
    body[699] = 0;

    UIBTN b[2] = { { 100, L"Open taskbar settings" }, { 101, L"Switch back to big labels" } };
    int choice = ui_dialog(L"Windows is hiding TrayVitals' tray icon", body, b, 2, UI_INFO);
    if (choice == 100) ShellExecuteW(NULL, L"open", L"ms-settings:taskbar", NULL, NULL, SW_SHOWNORMAL);
    else if (choice == 101) { g_mode = MODE_BAR; refresh(1); save(); }
}

/* Where the menu opens: next to the taskbar labels (on whichever display they're set to show on),
   or next to the tray icon, or else at the tray end of the taskbar; never on top of the taskbar,
   and opening away from the screen edge the taskbar is on. */
static int icon_rect(UINT id, RECT *r)
{
    NOTIFYICONIDENTIFIER ni; ZeroMemory(&ni, sizeof ni);
    ni.cbSize = sizeof ni; ni.hWnd = g_wnd; ni.uID = id;
    return SUCCEEDED(Shell_NotifyIconGetRect(&ni, r)) && r->right > r->left && r->bottom > r->top;
}

/* The taskbar (main or another display's) that contains point p. */
static HWND taskbar_at(POINT p, RECT *r)
{
    HWND tbs[MAX_TB + 1];
    int n = other_taskbars(tbs + 1);
    tbs[0] = FindWindowW(L"Shell_TrayWnd", NULL);
    for (int i = 0; i <= n; i++)
        if (tbs[i] && GetWindowRect(tbs[i], r) && PtInRect(r, p)) return tbs[i];
    return NULL;
}

static POINT g_clickPt;   /* where the tray icon was clicked */
static int   g_clicked;   /* menu opened by clicking the tray icon */

static UINT menu_pos(POINT *pt, TPMPARAMS *tp)
{
    HWND tb = g_mode == MODE_BAR ? find_taskbar() : FindWindowW(L"Shell_TrayWnd", NULL);
    RECT tr, a; int haveA = 0;
    ZeroMemory(tp, sizeof *tp);   /* cbSize 0 = no area to keep clear */

    if (g_mode == MODE_TRAY) {
        /* Newer Windows 11 builds show tray icons on every display's taskbar, and Windows may report
           the icon's spot on a different taskbar than the one clicked. So: when the icon was clicked,
           use the spot that was clicked; otherwise Windows' answer, on whichever taskbar it falls. */
        POINT c; int have = 0;
        if (g_clicked) { c = g_clickPt; have = 1; }
        else if (g_trayOn && icon_rect(TRAY_ID, &a)) {
            c.x = (a.left + a.right) / 2; c.y = (a.top + a.bottom) / 2; have = 1;
        }
        RECT r;
        HWND t2 = have ? taskbar_at(c, &r) : NULL;   /* an icon in the hidden-icons flyout is on no taskbar */
        if (t2) { tb = t2; SetRect(&a, c.x - 1, c.y - 1, c.x + 1, c.y + 1); haveA = 1; }
    }
    if (!tb || !GetWindowRect(tb, &tr)) { GetCursorPos(pt); return TPM_LEFTALIGN | TPM_TOPALIGN; }
    if (g_mode == MODE_BAR && g_bar && IsWindowVisible(g_bar) && GetWindowRect(g_bar, &a)) haveA = 1;

    MONITORINFO mi = { sizeof mi };
    GetMonitorInfoW(MonitorFromWindow(tb, MONITOR_DEFAULTTONEAREST), &mi);
    RECT mr = mi.rcMonitor;
    int horiz = (tr.right - tr.left) >= (tr.bottom - tr.top);
    tp->cbSize = sizeof *tp;
    tp->rcExclude = tr;   /* the menu never covers the taskbar */

    if (horiz) {
        int atTop = (tr.top + tr.bottom) / 2 < (mr.top + mr.bottom) / 2;
        UINT h;
        if (haveA) { pt->x = (a.left + a.right) / 2; h = TPM_CENTERALIGN; }
        else       { pt->x = tr.right; h = TPM_RIGHTALIGN; }   /* tray end of the taskbar */
        pt->y = atTop ? tr.bottom : tr.top;
        return h | (atTop ? TPM_TOPALIGN : TPM_BOTTOMALIGN) | TPM_VERTICAL;
    } else {
        int atLeft = (tr.left + tr.right) / 2 < (mr.left + mr.right) / 2;
        UINT v;
        if (haveA) { pt->y = (a.top + a.bottom) / 2; v = TPM_VCENTERALIGN; }
        else       { pt->y = tr.bottom; v = TPM_BOTTOMALIGN; }
        pt->x = atLeft ? tr.right : tr.left;
        return v | (atLeft ? TPM_LEFTALIGN : TPM_RIGHTALIGN) | TPM_HORIZONTAL;
    }
}

/* ---- The menu ----
   Two looks run the same commands (menu_cmd): TrayVitals' own panel (ui.h), which stays open while
   you flip settings, and Windows' standard menu, themed light/dark. TV_MENU_MODERN picks one. */
enum { CMD_ICONDROP = 369, CMD_THEME = 370, CMD_GLASS = 380, CMD_LABELS = 390, CMD_PREVIEW = 395, CMD_STYLE = 396 };
static int g_iconDrop;   /* the panel's "Small icon shows" drop-down is open */
static int g_reopenMenu; /* menu style just changed: open the menu again right away */
static void preview_popups(void);

/* Why a reading shows nothing, or NULL. */
static const wchar_t *sensor_hint(int m)
{
    if (!(g_mask >> m & 1)) return NULL;
    if (m == M_CPUP && g_pioState != PIO_OK && g_pioState != PIO_UNSUPPORTED) return L"needs CPU temperature support";
    if ((m == M_CPUT || m == M_GPUT || m == M_DISKT || m == M_CPUP || m == M_GPUP) && !g_ok[m]) return L"no sensor found";
    return NULL;
}
static int show_autostart(void)
{
#ifdef TV_PREVIEW
    return 0;   /* the preview never touches the real TrayVitals' startup entry */
#else
    return 1;
#endif
}

static void menu_cmd(int cmd)
{
    static const DWORD iv[3] = { 1000, 2000, 5000 };
    if (cmd >= 100 && cmd < 100 + M_N) {
        DWORD nm = g_mask ^ (1u << (cmd - 100));
        if (!nm) return;
        g_mask = nm;
        InterlockedExchange(&g_rebuild, 1);
        SetEvent(g_wake);
        refresh(1);
        save();
    } else if (cmd >= 200 && cmd <= 202) {
        g_interval = iv[cmd - 200];
        SetEvent(g_wake);
        save();
    } else if (cmd == 300) { if (show_autostart()) autostart_toggle(); }
    else if (cmd == 310) {
        g_mode = g_mode == MODE_BAR ? MODE_TRAY : MODE_BAR;
        refresh(1);
        save();
        /* Windows hides new tray icons; check shortly, once Windows has registered the icon. */
        if (g_mode == MODE_TRAY) SetTimer(g_wnd, TIMER_TRAYCHECK, 1500, NULL);
    }
    else if (cmd >= 360 && cmd < 360 + M_N) {
        g_trayShows = (DWORD)(cmd - 360);
        g_iconDrop = 0;
        refresh(1);
        save();
    }
    else if (cmd == CMD_ICONDROP) g_iconDrop = !g_iconDrop;
    else if (cmd == CMD_STYLE || cmd == CMD_STYLE + 1) {
        g_menuStyle = (DWORD)(cmd - CMD_STYLE);
        save();
        g_reopenMenu = 1;
        if (g_showMenuMsg) PostMessageW(g_wnd, g_showMenuMsg, 0, 0);   /* open it again in the new style */
    }
    else if (cmd >= 311 && cmd <= 313) {
        g_offset = cmd == 313 ? 0 : g_offset + (cmd == 311 ? -10 : 10);
        ZeroMemory(&g_lastPos, sizeof g_lastPos);
        bar_place();
        save();
    }
    else if (cmd >= 330 && cmd <= 330 + MAX_TB) {
        g_display = (DWORD)(cmd - 330);
        bar_destroy();
        g_tb = 0;
        refresh(1);
        save();
    }
    else if (cmd >= 340 && cmd <= 342) {
        g_align = cmd == 340 ? ALIGN_LEFT : cmd == 341 ? ALIGN_CENTER : ALIGN_RIGHT;
        g_offset = 0;   /* a nudge made for the old alignment would be off for the new one */
        ZeroMemory(&g_lastPos, sizeof g_lastPos);
        bar_place();
        save();
    }
    else if (cmd >= CMD_THEME && cmd < CMD_THEME + TH_N) { g_theme = (DWORD)(cmd - CMD_THEME); ui_native_theme(); ui_retheme(); save(); }
    else if (cmd >= CMD_GLASS && cmd < CMD_GLASS + 4)    { g_uiAlpha = (DWORD)(cmd - CMD_GLASS); ui_retheme(); save(); }
    else if (cmd >= CMD_LABELS && cmd < CMD_LABELS + 4)  { g_labelAlpha = (DWORD)(cmd - CMD_LABELS); refresh(1); save(); }
    else if (cmd == CMD_PREVIEW) preview_popups();
    else if (cmd == 320) cpu_temp_setup(0);
    else if (cmd == 350) notes_show();
    else if (cmd == 351) upd_start(UPD_MANUAL);
    else if (cmd == 352) upd_now();
    else if (cmd == 353) {
        DWORD off = upd_auto_on() ? 1 : 0;
        RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"NoUpdateCheck", REG_DWORD, &off, sizeof off);
        upd_schedule(60 * 1000);
    }
    else if (cmd == 399) DestroyWindow(g_wnd);
}

/* ---- the modern panel ---- */
static PITEM *pi_add(PITEM *it, int *n, int max, int type, const wchar_t *text, int cmd, int stay)
{
    if (*n >= max) return &it[max - 1];
    PITEM *p = &it[(*n)++];
    ZeroMemory(p, sizeof *p);
    p->type = type; p->cmd = cmd; p->stay = stay;
    if (text) lstrcpynW(p->text, text, 400);
    return p;
}
static void pi_chip(PITEM *p, const wchar_t *t, int cmd, int on)
{
    if (p->nch >= 10) return;
    PCHIP *c = &p->ch[p->nch++];
    lstrcpynW(c->t, t, 28); c->cmd = cmd; c->on = on;
}
static int menu_build(PITEM *it, int max)
{
    int n = 0;
    PITEM *p;
    wchar_t note[400] = L"";
    pi_add(it, &n, max, PI_HEAD, L"Readings", 0, 0);
    p = pi_add(it, &n, max, PI_GRID, NULL, 0, 1);
    for (int o = 0; o < M_N; o++) {
        int m = ORDER[o];
        const wchar_t *h = sensor_hint(m);
        pi_chip(p, NAMES[m], 100 + m, (g_mask >> m) & 1);
        p->ch[p->nch - 1].warn = h != NULL;
        if (h && lstrlenW(note) < 300) {
            if (note[0]) lstrcatW(note, L"\n");
            lstrcatW(note, NAMES[m]); lstrcatW(note, L": "); lstrcatW(note, h); lstrcatW(note, L".");
        }
    }
    if (note[0]) pi_add(it, &n, max, PI_NOTE, note, 0, 0);
    if (g_pioState != PIO_OK && g_pioState != PIO_UNSUPPORTED) {
        p = pi_add(it, &n, max, PI_ACTION, L"Turn on CPU temperature\x2026", 320, 0);
        p->icon = 0xE9D9;
    }

    pi_add(it, &n, max, PI_HEAD, L"Taskbar", 0, 0);
    p = pi_add(it, &n, max, PI_TOGGLE, L"Big labels on taskbar", 310, 1);
    p->on = g_mode == MODE_BAR;
    if (g_mode == MODE_BAR) {
        p = pi_add(it, &n, max, PI_CHIPS, L"Align", 0, 1);
        pi_chip(p, L"Left", 340, g_align == ALIGN_LEFT);
        pi_chip(p, L"Center", 341, g_align == ALIGN_CENTER);
        pi_chip(p, L"Right", 342, g_align == ALIGN_RIGHT);
        p = pi_add(it, &n, max, PI_CHIPS, L"Nudge", 0, 1);
        pi_chip(p, L"Left", 311, 0);  p->ch[0].icon = 0xE76B;
        pi_chip(p, L"Reset", 313, 0);
        pi_chip(p, L"Right", 312, 0); p->ch[2].icon = 0xE76C;
        HWND tbs[MAX_TB];
        int nt = other_taskbars(tbs);
        if (nt > 0 || g_display) {
            int cur = (int)g_display <= nt ? (int)g_display : 0;
            p = pi_add(it, &n, max, PI_CHIPS, L"Display", 0, 1);
            pi_chip(p, L"Main", 330, cur == 0);
            for (int i = 0; i < nt && i < 3; i++) {
                wchar_t l[16];
                if (nt == 1) lstrcpyW(l, L"Second");
                else { _snwprintf(l, 16, L"Other %d", i + 1); l[15] = 0; }
                pi_chip(p, l, 331 + i, cur == i + 1);
            }
        }
        p = pi_add(it, &n, max, PI_CHIPS, L"Opacity", 0, 1);
        for (int k = 0; k < 4; k++) {
            wchar_t l[8]; _snwprintf(l, 8, L"%d%%", LABEL_ALPHA_PCT[k]); l[7] = 0;
            pi_chip(p, l, CMD_LABELS + k, (int)g_labelAlpha == k);
        }
    }
    p = pi_add(it, &n, max, PI_DROPDOWN, g_mode == MODE_TRAY ? L"Icon shows" : L"Small icon", CMD_ICONDROP, 1);
    lstrcpynW(p->right, NAMES[tray_metric()], 48);
    p->on = g_iconDrop;
    if (g_iconDrop) {
        int cur = tray_metric();
        p = pi_add(it, &n, max, PI_LIST, NULL, 0, 1);
        for (int o = 0; o < M_N; o++) {
            int m = ORDER[o];
            if ((g_mask >> m) & 1) pi_chip(p, NAMES[m], 360 + m, m == cur);
        }
    }

    pi_add(it, &n, max, PI_HEAD, L"Appearance", 0, 0);
    p = pi_add(it, &n, max, PI_SWATCH, L"Theme", 0, 1);
    lstrcpynW(p->right, THEMES[g_theme < TH_N ? g_theme : 0].name, 48);
    for (int k = 0; k < TH_N; k++) pi_chip(p, THEMES[k].name, CMD_THEME + k, (int)g_theme == k);
    p = pi_add(it, &n, max, PI_CHIPS, L"Transparency", 0, 1);
    for (int k = 0; k < 4; k++) pi_chip(p, UI_ALPHA_NAME[k], CMD_GLASS + k, (int)g_uiAlpha == k);
    p = pi_add(it, &n, max, PI_CHIPS, L"Menu style", 0, 0);   /* closes, then opens in the new style */
    pi_chip(p, L"Modern", CMD_STYLE, g_menuStyle == 0);
    pi_chip(p, L"Classic", CMD_STYLE + 1, g_menuStyle == 1);

    pi_add(it, &n, max, PI_HEAD, L"General", 0, 0);
    p = pi_add(it, &n, max, PI_CHIPS, L"Refresh", 0, 1);
    pi_chip(p, L"1 s", 200, g_interval == 1000);
    pi_chip(p, L"2 s", 201, g_interval == 2000);
    pi_chip(p, L"5 s", 202, g_interval == 5000);
    if (show_autostart()) {
        p = pi_add(it, &n, max, PI_TOGGLE, L"Start with Windows", 300, 1);
        p->on = autostart_on();
    }
    if (TV_UPDATE_REPO[0]) {
        if (g_upd.ver[0]) {
            wchar_t t[64];
            _snwprintf(t, 64, L"Update to version %s", g_upd.ver); t[63] = 0;
            p = pi_add(it, &n, max, PI_ACTION, t, 352, 0);
            p->icon = 0xE896; p->accent = 1;
        }
        p = pi_add(it, &n, max, PI_TOGGLE, L"Check for updates every day", 353, 1);
        p->on = upd_auto_on();
        p = pi_add(it, &n, max, PI_ACTION, L"Check for updates now", 351, 0);
        p->icon = 0xE895; p->dis = g_updBusy != 0;
    }
#ifdef TV_PREVIEW
    p = pi_add(it, &n, max, PI_ACTION, L"Preview the popups", CMD_PREVIEW, 0);
    p->icon = 0xE946;
#endif
    pi_add(it, &n, max, PI_SEP, NULL, 0, 0);
    p = pi_add(it, &n, max, PI_FOOTER, NULL, 0, 0);
    pi_chip(p, L"What's new", 350, 0); p->ch[0].icon = 0xE81C;
    pi_chip(p, L"Exit", 399, 0);       p->ch[1].icon = 0xE7E8;
    return n;
}
static PANEL g_panel;
static void menu_modern(void)
{
    static int open;
    static DWORD closedAt;
    /* A click on the tray icon or labels while the panel is open closes it (the panel loses the
       focus); that same click must not open it again right away. */
    if (open || (GetTickCount() - closedAt < 300 && !g_reopenMenu)) return;
    g_reopenMenu = 0;
    open = 1;
    POINT pt; TPMPARAMS tp;
    UINT where = menu_pos(&pt, &tp);
    SetForegroundWindow(g_wnd);
    g_iconDrop = 0;
    int cmd = ui_panel(&g_panel, menu_build, menu_cmd, pt, where);
    closedAt = GetTickCount();
    open = 0;
    if (cmd) menu_cmd(cmd);
}

/* ---- Windows' own menu ---- */
static void menu_classic(void)
{
    HMENU h = CreatePopupMenu(), sub = CreatePopupMenu();
    for (int o = 0; o < M_N; o++) {
        int m = ORDER[o];
        wchar_t lbl[80];
        const wchar_t *hint = sensor_hint(m);
        lstrcpyW(lbl, NAMES[m]);
        if (m == M_CPUT && g_pioState == PIO_OK) lstrcatW(lbl, L"  (CPU sensor)");
        else if (hint) { lstrcatW(lbl, L"  ("); lstrcatW(lbl, hint); lstrcatW(lbl, L")"); }
        AppendMenuW(h, MF_STRING | ((g_mask >> m & 1) ? MF_CHECKED : 0), 100 + m, lbl);
    }
    if (g_pioState != PIO_OK && g_pioState != PIO_UNSUPPORTED) AppendMenuW(h, MF_STRING, 320, L"Turn on CPU temperature...");
    AppendMenuW(h, MF_SEPARATOR, 0, NULL);
    AppendMenuW(h, MF_STRING | (g_mode == MODE_BAR ? MF_CHECKED : 0), 310, L"Big labels on taskbar");
    if (g_mode == MODE_BAR) {
        AppendMenuW(h, MF_STRING, 311, L"Move left");
        AppendMenuW(h, MF_STRING, 312, L"Move right");
        AppendMenuW(h, MF_STRING, 313, L"Reset position");
        HMENU asub = CreatePopupMenu();
        AppendMenuW(asub, MF_STRING | (g_align == ALIGN_LEFT ? MF_CHECKED : 0), 340, L"Left");
        AppendMenuW(asub, MF_STRING | (g_align == ALIGN_CENTER ? MF_CHECKED : 0), 341, L"Center");
        AppendMenuW(asub, MF_STRING | (g_align == ALIGN_RIGHT ? MF_CHECKED : 0), 342, L"Right");
        AppendMenuW(h, MF_POPUP, (UINT_PTR)asub, L"Align");
        HWND tbs[MAX_TB];
        int n = other_taskbars(tbs);
        if (n > 0 || g_display) {
            HMENU dsub = CreatePopupMenu();
            int cur = (int)g_display <= n ? (int)g_display : 0;
            AppendMenuW(dsub, MF_STRING | (cur == 0 ? MF_CHECKED : 0), 330, L"Main display");
            for (int i = 0; i < n; i++) {
                wchar_t lbl[40];
                if (n == 1) lstrcpyW(lbl, L"Second display");
                else _snwprintf(lbl, 40, L"Other display %d (from left)", i + 1);
                lbl[39] = 0;
                AppendMenuW(dsub, MF_STRING | (cur == i + 1 ? MF_CHECKED : 0), 331 + i, lbl);
            }
            AppendMenuW(h, MF_POPUP, (UINT_PTR)dsub, L"Show on display");
        }
    }
    {   /* Which reading the small tray icon shows; can be set up while big labels are on, too. */
        HMENU isub = CreatePopupMenu();
        int cur = tray_metric();
        for (int o = 0; o < M_N; o++) {
            int m = ORDER[o];
            if ((g_mask >> m) & 1) AppendMenuW(isub, MF_STRING | (m == cur ? MF_CHECKED : 0), 360 + m, NAMES[m]);
        }
        AppendMenuW(h, MF_POPUP, (UINT_PTR)isub, g_mode == MODE_TRAY ? L"Icon shows" : L"Small icon shows");
    }
    {   /* Themes and transparency are the modern menu's; here only the switch back to it. */
        HMENU ms = CreatePopupMenu();
        AppendMenuW(ms, MF_STRING | (g_menuStyle == 0 ? MF_CHECKED : 0), CMD_STYLE, L"Modern (themes, transparency)");
        AppendMenuW(ms, MF_STRING | (g_menuStyle == 1 ? MF_CHECKED : 0), CMD_STYLE + 1, L"Classic (this menu)");
        AppendMenuW(h, MF_POPUP, (UINT_PTR)ms, L"Menu style");
    }
    AppendMenuW(sub, MF_STRING | (g_interval == 1000 ? MF_CHECKED : 0), 200, L"1 second");
    AppendMenuW(sub, MF_STRING | (g_interval == 2000 ? MF_CHECKED : 0), 201, L"2 seconds");
    AppendMenuW(sub, MF_STRING | (g_interval == 5000 ? MF_CHECKED : 0), 202, L"5 seconds (lowest power)");
    AppendMenuW(h, MF_POPUP, (UINT_PTR)sub, L"Refresh rate");
    if (show_autostart()) AppendMenuW(h, MF_STRING | (autostart_on() ? MF_CHECKED : 0), 300, L"Start with Windows");
    AppendMenuW(h, MF_SEPARATOR, 0, NULL);
    if (TV_UPDATE_REPO[0]) {
        if (g_upd.ver[0]) {
            wchar_t t[64];
            _snwprintf(t, 64, L"Update to version %s", g_upd.ver); t[63] = 0;
            AppendMenuW(h, MF_STRING, 352, t);
        }
        HMENU usub = CreatePopupMenu();
        AppendMenuW(usub, MF_STRING | (g_updBusy ? MF_GRAYED : 0), 351, L"Check now");
        AppendMenuW(usub, MF_STRING | (upd_auto_on() ? MF_CHECKED : 0), 353, L"Check every day");
        AppendMenuW(h, MF_POPUP, (UINT_PTR)usub, L"Updates");
    }
#ifdef TV_PREVIEW
    AppendMenuW(h, MF_STRING, CMD_PREVIEW, L"Preview the popups");
#endif
    AppendMenuW(h, MF_STRING, 350, L"What's new (version " TV_VERSION L")");
    AppendMenuW(h, MF_STRING, 399, L"Exit");

    POINT pt; TPMPARAMS tp;
    UINT where = menu_pos(&pt, &tp);
    SetForegroundWindow(g_wnd);
    int cmd = TrackPopupMenuEx(h, where | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, g_wnd,
                               tp.cbSize ? &tp : NULL);
    PostMessageW(g_wnd, WM_NULL, 0, 0);
    DestroyMenu(h);
    if (cmd) menu_cmd(cmd);
}

static void menu(void)
{
    if (ui_busy()) return;
    if (g_menuStyle == 1) menu_classic();
    else menu_modern();
}

/* Preview builds: every popup once, with sample text, to see how they look. */
static void preview_popups(void)
{
    UIBTN up[3] = { { 100, L"Update now" }, { 101, L"Later" }, { 102, L"Skip this version" } };
    if (!ui_dialog(L"TrayVitals 1.11.0 is available",
                   L"You have version " TV_VERSION L".\n\nUpdating takes a few seconds and keeps your settings. "
                   L"Windows will ask for permission, then TrayVitals restarts.", up, 3, UI_UPDATE | UI_SOLID)) return;
    ui_msg(L"TrayVitals is up to date", L"You have the latest version, " TV_VERSION L".", UI_INFO | UI_SOLID);
    UIBTN tb[2] = { { 100, L"Open taskbar settings" }, { 101, L"Switch back to big labels" } };
    if (!ui_dialog(L"Windows is hiding TrayVitals' tray icon",
                   L"Windows put TrayVitals' icon in the hidden icons menu (^ next to the clock) instead of on the "
                   L"taskbar.\n\nTo show it, open taskbar settings, expand \"Other system tray icons\" and turn TrayVitals on.",
                   tb, 2, UI_INFO)) return;
    if (!ui_ask(L"Show CPU temperature?",
                L"Windows doesn't let apps read the CPU's temperature sensor on its own, so TrayVitals installs PawnIO: "
                L"a small, signed, open-source driver. Windows will ask for permission.",
                L"Install and turn on", L"Not now", UI_QUESTION)) return;
    ui_ask(L"Couldn't download the update", L"Check your internet connection. Open the download page instead?",
           L"Open download page", L"Close", UI_WARN | UI_SOLID);
    /* The AMD power question, as Radeon users see it (it never shows by itself in a preview). */
    UIBTN ab[3] = { { 100, L"Report on GitHub" }, { 101, L"Later" }, { 102, L"Don't ask again" } };
    ui_dialog(L"Does GPU power look right on your Radeon?",
              L"GPU power on AMD Radeon cards is new in TrayVitals, and we couldn't test it on every card. "
              L"Right now it reads 187 W.\n\n"
              L"Could you compare it with AMD Software (Alt+R > Performance > Metrics), once while idle and once in a "
              L"game, and send us both numbers? It takes a minute and helps fix it for everyone with your card.\n\n"
              L"Report on GitHub opens a short form in your browser, with your card already filled in.",
              ab, 3, UI_QUESTION | UI_SOLID);
}

static LRESULT CALLBACK barproc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_RBUTTONUP:     menu(); return 0;
    case WM_LBUTTONUP: {
        wchar_t tm[MAX_PATH];
        if (sys_path(tm, L"taskmgr.exe")) ShellExecuteW(NULL, L"open", tm, NULL, NULL, SW_SHOWNORMAL);
        return 0;
    }
    case WM_NCDESTROY:     if (w == g_bar) g_bar = 0; break;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

static LRESULT CALLBACK wndproc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SAMPLED:
        refresh(0);
        return 0;
    case WM_TRAY:
        if (lp == WM_RBUTTONUP || lp == WM_LBUTTONUP || lp == WM_CONTEXTMENU) {
            DWORD mp = GetMessagePos();   /* where the mouse was when the icon was clicked */
            g_clickPt.x = (short)LOWORD(mp); g_clickPt.y = (short)HIWORD(mp);
            g_clicked = 1;
            menu();
            g_clicked = 0;
        }
        return 0;
    case WM_SETTINGCHANGE:
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        read_env(); refresh(1);
        return 0;
    case WM_POWERBROADCAST:
        if (wp == PBT_POWERSETTINGCHANGE) {
            POWERBROADCAST_SETTING *ps = (POWERBROADCAST_SETTING *)lp;
            if (IsEqualGUID(&ps->PowerSetting, &G_DISPLAY)) {
                g_paused = *(DWORD *)ps->Data == 0;
                SetEvent(g_wake);
                SetEvent(g_scanWake);
            }
        }
        return TRUE;
    case WM_TIMER:
        if (wp == TIMER_TRAYCHECK || wp == TIMER_TRAYCHECK_START) {
            KillTimer(w, wp);
            tray_hidden_warning(wp == TIMER_TRAYCHECK_START);
            return 0;
        }
        if (wp == TIMER_UPDATE) {
            KillTimer(w, wp);
            if (screen_busy()) { SetTimer(w, TIMER_UPDATE, 5 * 60 * 1000, NULL); return 0; }   /* not during a game */
            upd_cleanup();
            if (upd_auto_on() && !upd_start(UPD_AUTO)) upd_schedule(60 * 60 * 1000);
            return 0;
        }
        if (wp == TIMER_UPD_NOTICE) {
            KillTimer(w, wp);
            if (g_upd.ver[0]) upd_notice();
            return 0;
        }
        if (wp == TIMER_AMD_ASK) {
            KillTimer(w, wp);
            amd_ask();
            return 0;
        }
        break;
    case WM_UPDATE:
        upd_done((UPDINFO *)lp);
        return 0;
    case WM_DESTROY:
        tray_remove();
        bar_destroy();
        PostQuitMessage(0);
        return 0;
    }
    if (msg == g_showMenuMsg && msg) {
        menu();
        return 0;
    }
    if (msg == g_taskbarCreated && msg) {
        g_trayOn = 0;   /* Explorer restarted: its tray is empty, so add the icon again */
        bar_destroy();
        g_tb = 0;
        refresh(1);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

/* TrayVitals was called TraySense before 1.10.0. Setup moves the settings over; this is the backup for
   when it couldn't (for example setup ran under another administrator's account). It copies only
   settings this project wrote (they have LastVersion), only once, and leaves the old key alone, in
   case another program called TraySense uses it too. */
static void old_settings(void)
{
#ifndef TV_PREVIEW
    HKEY o, n;
    if (RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"LastVersion", RRF_RT_REG_SZ, NULL, NULL, NULL) == ERROR_SUCCESS ||
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\TraySense", L"LastVersion", RRF_RT_REG_SZ, NULL, NULL, NULL) != ERROR_SUCCESS)
        return;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\TraySense", 0, KEY_READ, &o)) return;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGKEY, 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &n, NULL)) { RegCloseKey(o); return; }
    for (DWORD i = 0;; i++) {
        wchar_t name[256]; BYTE data[4096];
        DWORD nlen = 256, dl = sizeof data, type;
        LONG r = RegEnumValueW(o, i, name, &nlen, NULL, &type, data, &dl);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS || RegQueryValueExW(n, name, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) continue;
        RegSetValueExW(n, name, 0, type, data, dl);
    }
    RegCloseKey(n);
    RegCloseKey(o);
#endif
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE prev, LPSTR cmd, int show)
{
    (void)prev; (void)cmd; (void)show;
    {
        typedef BOOL (WINAPI *SDDD)(DWORD);
        SDDD f = (SDDD)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetDefaultDllDirectories");
        if (f) f(LOAD_LIBRARY_SEARCH_SYSTEM32);
        SetDllDirectoryW(L"");
    }
    if (wcsstr(GetCommandLineW(), L"/apply-update")) return upd_helper();   /* in-app update helper, then quit */
    if (wcsstr(GetCommandLineW(), L"/make-task")) {
        /* Setup, moving an old TraySense install over: make TrayVitals' own "start as administrator at
           login" task in place of TraySense's, then quit. Only when run as administrator. */
        wchar_t exe[MAX_PATH];
        DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
        if (is_admin() && n && n < MAX_PATH) task_create(exe);
        return 0;
    }
    if (is_admin()) lock_self();
    int restart = wcsstr(GetCommandLineW(), L"/restart") != NULL;
    g_showMenuMsg = RegisterWindowMessageW(L"TrayVitals.ShowMenu");
    for (int tries = 0;; tries++) {
#ifndef SINGLE_INSTANCE_NAME
#define SINGLE_INSTANCE_NAME L"Local\\TrayVitals.single"   /* test builds use their own name */
#endif
        HANDLE mx = CreateMutexW(NULL, TRUE, SINGLE_INSTANCE_NAME);
        if (GetLastError() != ERROR_ALREADY_EXISTS) break;
        CloseHandle(mx);
        if (!restart) {
            /* Already running (from the Start menu, a shortcut or the .exe): open its menu, so it can be
               reached even when its tray icons are hidden and the taskbar labels are off. */
            HWND other = FindWindowW(MAIN_CLASS, APP);
            DWORD pid = 0;
            if (other && g_showMenuMsg) {
                GetWindowThreadProcessId(other, &pid);
                if (pid) AllowSetForegroundWindow(pid);
                PostMessageW(other, g_showMenuMsg, 0, 0);
            }
            return 0;
        }
        if (tries > 40) return 0;
        Sleep(150);
    }

    SetProcessDPIAware();
    {
        typedef BOOL (WINAPI *SPI)(HANDLE, int, void *, DWORD);
        SPI spi = (SPI)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetProcessInformation");
        struct { ULONG Version, ControlMask, StateMask; } pt = { 1, 1, 1 };
        if (spi) spi(GetCurrentProcess(), 4 , &pt, sizeof pt);
    }

    InitializeCriticalSection(&g_pioCs);
    old_settings();
    load();
    ui_native_theme();
    read_env();
    init_gpu();
    init_disk();
    if (wcsstr(GetCommandLineW(), L"/install-pawnio") && is_admin()) {
        HANDLE probe = CreateFileW(L"\\\\?\\GLOBALROOT\\Device\\PawnIO", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, 0, NULL);
        if (probe != INVALID_HANDLE_VALUE) CloseHandle(probe);
        else {
            const wchar_t *why = L"";
            if (!pawnio_install(&why)) ui_msg(L"CPU temperature couldn't be turned on", why, UI_WARN);
        }
    }
    pio_init();
    autostart_migrate();

    WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = wndproc; wc.hInstance = hi; wc.lpszClassName = MAIN_CLASS;
    RegisterClassW(&wc);
    wc.lpfnWndProc = barproc; wc.lpszClassName = L"TrayVitalsBar";
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    RegisterClassW(&wc);
    g_wnd = CreateWindowExW(WS_EX_TOOLWINDOW, MAIN_CLASS, APP, WS_POPUP, 0, 0, 0, 0, NULL, NULL, hi, NULL);
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    {
        typedef BOOL (WINAPI *CWMF)(HWND, UINT, DWORD, void *);
        CWMF f = (CWMF)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "ChangeWindowMessageFilterEx");
        if (f) {
            f(g_wnd, g_taskbarCreated, 1, NULL); f(g_wnd, WM_TRAY, 1, NULL);
            if (g_showMenuMsg) f(g_wnd, g_showMenuMsg, 1, NULL);   /* also when this copy runs as administrator */
        }
    }
    RegisterPowerSettingNotification(g_wnd, &G_DISPLAY, DEVICE_NOTIFY_WINDOW_HANDLE);

    g_wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    InitializeCriticalSection(&g_scanCs);
    g_scanWake = CreateEventW(NULL, FALSE, FALSE, NULL);
    HANDLE th = CreateThread(NULL, 64 * 1024, sampler, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    HANDLE sth = CreateThread(NULL, 64 * 1024, scanner, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    tray_settings_cleanup();
    refresh(1);
    notes_after_update();
    if (g_mode == MODE_TRAY) SetTimer(g_wnd, TIMER_TRAYCHECK_START, 3000, NULL);
#ifndef TV_PREVIEW
    if (TV_ASK_AMD_POWER && g_gpuVendor == 0x1002 && !amd_confirmed()) SetTimer(g_wnd, TIMER_AMD_ASK, AMD_ASK_DELAY_MS, NULL);
#endif
    upd_cleanup();
    upd_old_cleanup();
    upd_schedule(60 * 1000);   /* first check a minute after starting, so it never slows down sign-in */

    MSG m;
    int trimmed = 0;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
        if (!trimmed && m.message == WM_SAMPLED) {
            trimmed = 1;
#ifndef TV_PREVIEW   /* a preview never asks to install or restart anything on its own */
            if ((g_mask & (1u << M_CPUT)) && g_pioState != PIO_OK && g_pioState != PIO_UNSUPPORTED && !g_ok[M_CPUT])
                cpu_temp_setup(1);
#endif
            SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
        }
    }
    g_quit = 1;
    SetEvent(g_wake);
    SetEvent(g_scanWake);
    if (th) WaitForSingleObject(th, 2000);
    if (sth) WaitForSingleObject(sth, 2000);
    reset_fonts();
    return 0;
}
