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
    lstrcatW(out, L"\\TraySense");
    if (!CreateDirectoryW(out, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    DWORD a = GetFileAttributesW(out);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) && !(a & FILE_ATTRIBUTE_REPARSE_POINT);
}

#define APP      L"TraySense"
#define REGKEY   L"Software\\TraySense"
#define RUNKEY   L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define WM_TRAY  (WM_APP + 1)

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

/* Board power in watts, from the NVIDIA driver. */
static void read_gpu_power(void)
{
    unsigned mw;
    g_ok[M_GPUP] = 0;
    if (pNvPower && pNvPower(g_nvDev, &mw) == 0 && mw < 2000000) { g_val[M_GPUP] = mw / 1000.0; g_ok[M_GPUP] = 1; }
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
    if (g_gpuVendor != 0x10DE) return;
    unsigned bus = ~0u;
    if (g_kmt && pKmtQuery) {
        KMT_ADDR ad; ZeroMemory(&ad, sizeof ad);
        KMT_QAI q = { g_kmt, KMTQAITYPE_ADAPTERADDRESS, &ad, sizeof ad };
        if (pKmtQuery(&q) == 0) bus = ad.Bus;
    }
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
    HINTERNET s = WinHttpOpen(L"TraySense", 4 , NULL, NULL, 0);
    if (!s) s = WinHttpOpen(L"TraySense", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
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
        *msg = L"Couldn't create the TraySense folder in Program Files.";
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
    g_bar = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"TraySenseBar", L"",
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
            !(cls && !lstrcmpW(cls, L"TraySenseBar"))) {
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
        WaitForSingleObject(g_scanWake, g_paused ? INFINITE : 1000);
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
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
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
        WaitForSingleObject(g_wake, g_paused ? INFINITE : g_interval);
    }
    if (g_q) PdhCloseQuery(g_q);
    if (com) CoUninitialize();
    return 0;
}

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
            if (MessageBoxW(g_wnd, L"TraySense starts as administrator at login. Turning that off needs administrator "
                                   L"rights.\n\nRestart TraySense as administrator?", APP, MB_YESNO | MB_ICONQUESTION) == IDYES)
                relaunch_admin(L"/restart");
            return;
        }
        RegDeleteKeyValueW(HKEY_CURRENT_USER, RUNKEY, APP);
        if (reg_dword(L"Task")) {
            schtasks(L"/Delete /F /TN TraySense");
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
    _snwprintf(dst, MAX_PATH, L"%s\\TraySense.exe", dir); dst[MAX_PATH - 1] = 0;
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
    _snwprintf(dst, MAX_PATH, L"%s\\TraySense.exe", dir); dst[MAX_PATH - 1] = 0;
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
    _snwprintf(args, MAX_PATH + 48, L"/Create /F /TN TraySense /XML \"%s\"", xmlPath); args[MAX_PATH + 47] = 0;
    schtasks(args);
    DeleteFileW(xmlPath);
    RegDeleteKeyValueW(HKEY_CURRENT_USER, RUNKEY, APP);
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"Task", REG_DWORD, &one, sizeof one);
}

static void autostart_migrate(void)
{
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
        if (!ask) MessageBoxW(g_wnd, L"CPU temperature is on.", APP, MB_ICONINFORMATION);
        return;
    case PIO_UNSUPPORTED:
    case PIO_FAILED:
    case PIO_NOMODULE:
        if (!ask) MessageBoxW(g_wnd, L"CPU temperature isn't supported on this processor "
                                     L"(Intel Core or AMD Ryzen / EPYC are supported).", APP, MB_ICONINFORMATION);
        return;
    case PIO_NOADMIN:
        if (MessageBoxW(g_wnd, L"Reading the CPU temperature sensor needs administrator rights.\n\n"
                               L"Restart TraySense as administrator? It will start that way automatically from now on.",
                        APP, MB_YESNO | MB_ICONQUESTION) == IDYES)
            relaunch_admin(L"/restart");
        return;
    default:
        if (MessageBoxW(g_wnd, L"Show CPU temperature?\n\n"
                               L"Windows doesn't let apps read the CPU's temperature sensor on its own, so TraySense "
                               L"installs PawnIO: a small, signed, open-source driver (the one LibreHardwareMonitor uses). "
                               L"It's verified before it runs and can be removed any time from Settings > Apps.\n\n"
                               L"Windows will ask for permission.",
                        APP L" \x2013 CPU temperature", MB_YESNO | MB_ICONQUESTION) == IDYES)
            relaunch_admin(L"/restart /install-pawnio");
    }
}

/* ---- "What's new" window: the patch notes from patchnotes.h ---- */
#define NOTES_CLASS L"TraySenseNotes"
static HWND  g_notes;
static HFONT g_notesUi, g_notesHead;

static LRESULT CALLBACK notesproc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) { DestroyWindow(w); return 0; }
        break;
    case WM_CLOSE:
        DestroyWindow(w);
        return 0;
    case WM_DESTROY:
        g_notes = NULL;
        if (g_notesUi)   { DeleteObject(g_notesUi);   g_notesUi = NULL; }
        if (g_notesHead) { DeleteObject(g_notesHead); g_notesHead = NULL; }
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

/* Fills the rich edit box: version lines in bold, "\x2022 " lines as real bullets that wrap neatly. */
static void notes_fill(HWND ed)
{
    enum { MAXL = 128 };
    static wchar_t text[sizeof PATCH_NOTES / sizeof PATCH_NOTES[0]];
    LONG start[MAXL], len[MAXL]; int bullet[MAXL], head[MAXL], n = 0;
    const wchar_t *s = PATCH_NOTES;
    int o = 0;
    LONG pos = 0;   /* position as the rich edit counts it: a line break is one character */
    while (*s && n < MAXL) {
        const wchar_t *e = wcsstr(s, L"\r\n");
        int ln = e ? (int)(e - s) : lstrlenW(s);
        bullet[n] = ln >= 2 && s[0] == L'\x2022' && s[1] == L' ';
        head[n] = ln > 0 && !bullet[n];
        if (bullet[n]) { s += 2; ln -= 2; }
        memcpy(text + o, s, ln * sizeof(wchar_t)); o += ln;
        start[n] = pos; len[n] = ln; pos += ln + 1; n++;
        if (!e) break;
        text[o++] = L'\r'; text[o++] = L'\n';
        s = e + 2;
    }
    text[o] = 0;

    CHARFORMAT2W cf; ZeroMemory(&cf, sizeof cf);
    cf.cbSize = sizeof cf;
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_BOLD | CFM_COLOR;
    cf.dwEffects = CFE_AUTOCOLOR;
    cf.yHeight = 200;                               /* 10 pt, in twips */
    lstrcpyW(cf.szFaceName, L"Segoe UI");
    SendMessageW(ed, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    SetWindowTextW(ed, text);
    SendMessageW(ed, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    for (int i = 0; i < n; i++) {
        CHARRANGE r = { start[i], start[i] + len[i] };
        SendMessageW(ed, EM_EXSETSEL, 0, (LPARAM)&r);
        PARAFORMAT2 pf; ZeroMemory(&pf, sizeof pf);
        pf.cbSize = sizeof pf;
        if (head[i]) {
            CHARFORMAT2W h = cf;
            h.dwMask = CFM_BOLD | CFM_SIZE;
            h.dwEffects = CFE_BOLD;
            h.yHeight = 240;                        /* 12 pt */
            SendMessageW(ed, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&h);
            pf.dwMask = PFM_SPACEAFTER;
            pf.dySpaceAfter = 60;
        } else if (bullet[i]) {
            pf.dwMask = PFM_NUMBERING | PFM_OFFSET | PFM_STARTINDENT | PFM_SPACEAFTER;
            pf.wNumbering = PFN_BULLET;
            pf.dxStartIndent = 120;
            pf.dxOffset = 240;                      /* wrapped lines line up with the text, not the bullet */
            pf.dySpaceAfter = 40;
        } else continue;
        SendMessageW(ed, EM_SETPARAFORMAT, 0, (LPARAM)&pf);
    }
    CHARRANGE top = { 0, 0 };
    SendMessageW(ed, EM_EXSETSEL, 0, (LPARAM)&top);
}

static void notes_show(void)
{
    if (g_notes) { ShowWindow(g_notes, SW_SHOWNORMAL); SetForegroundWindow(g_notes); return; }
    HINSTANCE hi = GetModuleHandleW(NULL);
    static int registered;
    if (!registered) {
        WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
        wc.lpfnWndProc = notesproc; wc.hInstance = hi; wc.lpszClassName = NOTES_CLASS;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        registered = RegisterClassW(&wc) != 0;
    }
    if (!sysdll(L"msftedit.dll")) return;

    HDC dc = GetDC(NULL);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) ReleaseDC(NULL, dc);
    if (dpi < 96) dpi = 96;
#define NS(v) MulDiv((v), dpi, 96)

    NONCLIENTMETRICSW ncm; ZeroMemory(&ncm, sizeof ncm);
    ncm.cbSize = sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g_notesUi = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW lf = ncm.lfMessageFont;
    lf.lfHeight = lf.lfHeight * 3 / 2; lf.lfWeight = FW_SEMIBOLD;
    g_notesHead = CreateFontIndirectW(&lf);

    int cw = NS(560), ch = NS(500);
    RECT r = { 0, 0, cw, ch };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRectEx(&r, style, FALSE, WS_EX_CONTROLPARENT);
    int ww = r.right - r.left, wh = r.bottom - r.top;

    POINT pt; GetCursorPos(&pt);
    MONITORINFO mi = { sizeof mi };
    GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY), &mi);
    RECT wa = mi.rcWork;
    if (wh > wa.bottom - wa.top) wh = wa.bottom - wa.top;
    int x = wa.left + (wa.right - wa.left - ww) / 2, y = wa.top + (wa.bottom - wa.top - wh) / 2;

    g_notes = CreateWindowExW(WS_EX_CONTROLPARENT, NOTES_CLASS, APP L" \x2013 What's new", style,
                              x, y, ww, wh, NULL, NULL, hi, NULL);
    if (!g_notes) return;
    RECT cr; GetClientRect(g_notes, &cr);
    int m = NS(16), bw = NS(96), bh = NS(30), headH = NS(32);

    HWND head = CreateWindowExW(0, L"STATIC", APP L" " TS_VERSION, WS_CHILD | WS_VISIBLE | SS_LEFT,
                                m, m, cr.right - 2 * m, headH, g_notes, NULL, hi, NULL);
    SendMessageW(head, WM_SETFONT, (WPARAM)g_notesHead, FALSE);
    HWND sub = CreateWindowExW(0, L"STATIC", L"What's new in each version, newest first.",
                               WS_CHILD | WS_VISIBLE | SS_LEFT, m, m + headH, cr.right - 2 * m, NS(20),
                               g_notes, NULL, hi, NULL);
    SendMessageW(sub, WM_SETFONT, (WPARAM)g_notesUi, FALSE);

    int edTop = m + headH + NS(28), edBottom = cr.bottom - m - bh - NS(12);
    HWND ed = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
                              WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_READONLY |
                              ES_AUTOVSCROLL | ES_NOHIDESEL,
                              m, edTop, cr.right - 2 * m, edBottom - edTop, g_notes, NULL, hi, NULL);
    if (ed) {
        RECT ir; SendMessageW(ed, EM_GETRECT, 0, (LPARAM)&ir);
        InflateRect(&ir, -NS(8), -NS(6));
        SendMessageW(ed, EM_SETRECT, 0, (LPARAM)&ir);
        notes_fill(ed);
    }
    HWND ok = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                              cr.right - m - bw, cr.bottom - m - bh, bw, bh, g_notes, (HMENU)IDOK, hi, NULL);
    SendMessageW(ok, WM_SETFONT, (WPARAM)g_notesUi, FALSE);
#undef NS
    ShowWindow(g_notes, SW_SHOWNORMAL);
    if (ed) SendMessageW(ed, WM_VSCROLL, SB_TOP, 0);   /* start at the newest version */
    SetForegroundWindow(g_notes);
    SetFocus(ok);
}

/* Opens the patch notes once after TraySense was updated (not on a brand-new install). */
static void notes_after_update(void)
{
    wchar_t last[32] = L"";
    DWORD sz = sizeof last;
    int had = RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"LastVersion", RRF_RT_REG_SZ, NULL, last, &sz) == ERROR_SUCCESS;
    if (had && !lstrcmpW(last, TS_VERSION)) return;
    /* Versions before 1.7.0 didn't record a version; settings they saved show TraySense was used before. */
    int update = had ||
        RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"Mask2", RRF_RT_REG_DWORD, NULL, NULL, NULL) == ERROR_SUCCESS ||
        RegGetValueW(HKEY_CURRENT_USER, REGKEY, L"Mode", RRF_RT_REG_DWORD, NULL, NULL, NULL) == ERROR_SUCCESS;
    RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"LastVersion", REG_SZ, TS_VERSION, sizeof TS_VERSION);
    if (update) notes_show();
}

/* ---- Updates ----
   Once a day TraySense asks GitHub for the latest release of TS_UPDATE_REPO (one small request, no account
   needed). If it's newer than this version, TraySense says so. "Update now" downloads that release's
   TraySense-Setup.exe, checks it against the SHA-256 checksum GitHub lists for the file, and runs it with
   /update: it installs over this copy with the same settings and starts TraySense again. */
#define WM_UPDATE        (WM_APP + 3)   /* lp: UPDINFO * from the update thread */
#define TIMER_UPDATE     9
#define TIMER_UPD_NOTICE 10
#define UPD_DAY_MS       (24u * 60 * 60 * 1000)
#define UPD_FILE         L"TraySense-Setup.exe"
enum { UPD_AUTO, UPD_MANUAL, UPD_DOWNLOAD };
enum { UPD_BADHASH = -2, UPD_FAILED = -1, UPD_CURRENT = 0, UPD_NEWER = 1 };
typedef struct {
    int     job, result, hasSha;
    wchar_t ver[24];          /* the newer version, like 1.9.1 */
    char    url[512];         /* its installer on github.com; "" if the release has none */
    BYTE    sha[32];
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
    if (!ver_parse(u->ver, nv) || !ver_parse(TS_VERSION, cur)) return;
    if (u->ver[0] == L'v' || u->ver[0] == L'V') memmove(u->ver, u->ver + 1, lstrlenW(u->ver) * sizeof(wchar_t));
    if (!ver_newer(nv, cur)) { u->result = UPD_CURRENT; return; }
    u->result = UPD_NEWER;
    /* Each file attached to the release lists its "digest" and then, last, its "browser_download_url". */
    while ((after = json_str(from, end, "browser_download_url", url, sizeof url))) {
        int ul = lstrlenA(url), al = lstrlenA(TS_UPDATE_ASSET);
        if (ul > al + (int)sizeof GH_PREFIX - 1 && !_strnicmp(url, GH_PREFIX, sizeof GH_PREFIX - 1) &&
            url[ul - al - 1] == '/' && !_stricmp(url + ul - al, TS_UPDATE_ASSET) && !strpbrk(url, " \"<>\\")) {
            const char *q = from, *q2;
            int got = 0;
            while ((q2 = json_str(q, after, "digest", dg, sizeof dg))) { got = 1; q = q2; }
            if (got && !_strnicmp(dg, "sha256:", 7) && lstrlenA(dg) == 7 + 64) {
                int ok = 1;
                for (int i = 0; i < 32 && ok; i++) {
                    int hi = hexval(dg[7 + 2 * i]), lo = hexval(dg[8 + 2 * i]);
                    if (hi < 0 || lo < 0) ok = 0; else u->sha[i] = (BYTE)(hi << 4 | lo);
                }
                u->hasSha = ok;
            }
            if (u->hasSha) lstrcpyA(u->url, url);   /* without a checksum it is never run */
            return;
        }
        from = after;
    }
}

static void upd_check(UPDINFO *u)
{
    wchar_t path[200];
    DWORD len = 0, status = 0;
    _snwprintf(path, 200, L"/repos/%s/releases/latest", TS_UPDATE_REPO); path[199] = 0;
    char *j = (char *)http_get_ex(L"api.github.com", path, &len, &status);
    u->result = UPD_FAILED;
    if (!j) {
        if (status == 404) u->result = UPD_CURRENT;   /* no release published yet: nothing newer */
        return;
    }
    upd_parse(j, len, u);
    HeapFree(GetProcessHeap(), 0, j);
}

/* Where the installer is saved. Running as administrator: Program Files\TraySense, which only
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
            lstrcatW(dir, L"\\TraySense");
        }
    } else {
        DWORD n = GetTempPathW(MAX_PATH, dir);
        if (!n || n > MAX_PATH - 48) return 0;
        lstrcatW(dir, L"TraySense-update");
        if (create && !CreateDirectoryW(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    }
    _snwprintf(out, MAX_PATH, L"%s\\" UPD_FILE, dir); out[MAX_PATH - 1] = 0;
    return 1;
}

/* Removes the installer left from the last update (it can't delete itself while it runs). */
static void upd_cleanup(void)
{
    wchar_t f[MAX_PATH];
    if (!upd_path(f, 0) || !DeleteFileW(f) || is_admin()) return;
    *wcsrchr(f, L'\\') = 0;
    RemoveDirectoryW(f);
}

static void upd_download(UPDINFO *u)
{
    wchar_t path[512];
    DWORD len = 0, w;
    u->result = UPD_FAILED;
    if (!u->hasSha || _strnicmp(u->url, GH_PREFIX, sizeof GH_PREFIX - 1) ||
        !MultiByteToWideChar(CP_UTF8, 0, u->url + sizeof GH_PREFIX - 2, -1, path, 512)) return;
    BYTE *bin = http_get(L"github.com", path, &len);
    if (!bin) return;
    if (!sha256_is(bin, len, u->sha)) { u->result = UPD_BADHASH; HeapFree(GetProcessHeap(), 0, bin); return; }
    if (upd_path(u->file, 1)) {
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
    if (!TS_UPDATE_REPO[0] || InterlockedExchange(&g_updBusy, 1)) return 0;
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

static int upd_auto_on(void) { return TS_UPDATE_REPO[0] && !reg_dword(L"NoUpdateCheck"); }

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
    ShellExecuteW(NULL, L"open", L"https://github.com/" TS_UPDATE_REPO L"/releases/latest", NULL, NULL, SW_SHOWNORMAL);
}

/* Task dialog with custom buttons; returns the button's id, 0 if closed, -1 if task dialogs aren't available. */
static int task_dialog(const wchar_t *head, const wchar_t *body, const TASKDIALOG_BUTTON *b, int nb)
{
    typedef HRESULT (WINAPI *TDI)(const TASKDIALOGCONFIG *, int *, int *, BOOL *);
    HMODULE cc = GetModuleHandleW(L"comctl32.dll");
    if (!cc) cc = LoadLibraryW(L"comctl32.dll");
    TDI tdi = cc ? (TDI)(void *)GetProcAddress(cc, "TaskDialogIndirect") : NULL;
    if (!tdi) return -1;
    TASKDIALOGCONFIG tc; ZeroMemory(&tc, sizeof tc);
    int choice = 0;
    tc.cbSize = sizeof tc;
    tc.hwndParent = g_wnd;
    tc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    tc.pszWindowTitle = APP;
    tc.pszMainIcon = TD_INFORMATION_ICON;
    tc.pszMainInstruction = head;
    tc.pszContent = body;
    tc.cButtons = nb; tc.pButtons = b; tc.nDefaultButton = b[0].nButtonID;
    return SUCCEEDED(tdi(&tc, &choice, NULL, NULL)) ? (choice == IDCANCEL ? 0 : choice) : -1;
}

static void upd_now(void)
{
    if (g_upd.url[0]) upd_start(UPD_DOWNLOAD);
    else open_releases();
}

static void upd_prompt(void)
{
    wchar_t head[96], body[400];
    _snwprintf(head, 96, L"TraySense %s is available", g_upd.ver); head[95] = 0;
    _snwprintf(body, 400, L"You have version " TS_VERSION L".\n\n%s", !g_upd.url[0]
                   ? L"Get it from the TraySense download page."
                   : is_admin() ? L"Updating takes a few seconds and keeps your settings. TraySense restarts when it's done."
                                : L"Updating takes a few seconds and keeps your settings. Windows will ask for permission, "
                                  L"then TraySense restarts.");
    body[399] = 0;
    TASKDIALOG_BUTTON b[3] = { { 100, g_upd.url[0] ? L"Update now" : L"Open download page" },
                               { 101, L"Later" }, { 102, L"Skip this version" } };
    int c = task_dialog(head, body, b, 3);
    if (c < 0) {
        wchar_t mb[600];
        _snwprintf(mb, 600, L"%s.\n\n%s\n\nUpdate now?", head, body); mb[599] = 0;
        c = MessageBoxW(g_wnd, mb, APP, MB_YESNO | MB_ICONINFORMATION) == IDYES ? 100 : 0;
    }
    if (c == 100) upd_now();
    else if (c == 102)
        RegSetKeyValueW(HKEY_CURRENT_USER, REGKEY, L"UpdateSkip", REG_SZ, g_upd.ver, (lstrlenW(g_upd.ver) + 1) * sizeof(wchar_t));
}

/* An automatic check found an update: say so, but not over a full-screen game, video or presentation. */
static void upd_notice(void)
{
    QUERY_USER_NOTIFICATION_STATE s;
    if (SUCCEEDED(SHQueryUserNotificationState(&s)) && s != QUNS_ACCEPTS_NOTIFICATIONS && s != QUNS_QUIET_TIME) {
        SetTimer(g_wnd, TIMER_UPD_NOTICE, 10 * 60 * 1000, NULL);
        return;
    }
    upd_prompt();
}

static void upd_run(const wchar_t *file)
{
    SHELLEXECUTEINFOW e; ZeroMemory(&e, sizeof e);
    e.cbSize = sizeof e; e.fMask = SEE_MASK_NOASYNC; e.hwnd = g_wnd;
    e.lpVerb = L"runas"; e.lpFile = file; e.lpParameters = L"/update"; e.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&e)) { DestroyWindow(g_wnd); return; }   /* setup replaces this copy and restarts it */
    if (GetLastError() != ERROR_CANCELLED)
        MessageBoxW(g_wnd, L"Couldn't start the TraySense update.", APP, MB_ICONWARNING);
}

static void upd_done(UPDINFO *u)
{
    InterlockedExchange(&g_updBusy, 0);
    if (u->job == UPD_DOWNLOAD) {
        if (u->result == UPD_NEWER) upd_run(u->file);
        else if (MessageBoxW(g_wnd, u->result == UPD_BADHASH
                    ? L"The downloaded update didn't match its checksum, so it was not run.\n\nOpen the download page instead?"
                    : L"Couldn't download the update. Check your internet connection.\n\nOpen the download page instead?",
                    APP, MB_YESNO | MB_ICONWARNING) == IDYES)
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
                MessageBoxW(g_wnd, u->result == UPD_CURRENT ? L"TraySense is up to date (version " TS_VERSION L")."
                                   : L"Couldn't check for updates. Check your internet connection and try again.",
                            APP, u->result == UPD_CURRENT ? MB_ICONINFORMATION : MB_ICONWARNING);
        }
        if (u->job == UPD_AUTO) upd_schedule(u->result == UPD_FAILED ? 60 * 60 * 1000 : 60 * 1000);
    }
    HeapFree(GetProcessHeap(), 0, u);
}

/* ---- Tray icon hidden by Windows ----
   Windows 11 puts new tray icons in the hidden-icons area, and when the hidden-icons menu (^) is off
   they can't be seen at all. Only the user may change that, so TraySense says so and offers to open
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
   tray icon a program ever showed, all listed under the program's name. Older TraySense versions used
   one icon per reading, so the list fills up with identical "TraySense" entries and it isn't clear
   which one to turn on. Remove TraySense's own leftovers: this copy's entries other than its one icon,
   and entries of TraySense copies at other paths (an older install, a moved or deleted copy).
   Returns how many were removed. Nothing belonging to other programs is touched. */
static int tray_settings_cleanup(void)
{
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
        /* TraySense.exe, or a renamed/test copy like TraySense_test.exe */
        if (_wcsnicmp(file, L"TraySense", 9) || len < 13 || lstrcmpiW(file + len - 4, L".exe")) continue;
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
                          L"TraySense on.%s",
               hidden_menu_off()
                   ? L"Windows hides new tray icons, and the hidden icons menu (^ next to the clock) is turned "
                     L"off on this PC, so TraySense's icon can't be seen at all."
                   : L"Windows put TraySense's icon in the hidden icons menu (^ next to the clock) instead of on "
                     L"the taskbar.",
               L"");
    body[699] = 0;

    int choice = 0;
    typedef HRESULT (WINAPI *TDI)(const TASKDIALOGCONFIG *, int *, int *, BOOL *);
    HMODULE cc = GetModuleHandleW(L"comctl32.dll");
    if (!cc) cc = LoadLibraryW(L"comctl32.dll");   /* the manifest picks version 6, which has task dialogs */
    TDI tdi = cc ? (TDI)(void *)GetProcAddress(cc, "TaskDialogIndirect") : NULL;
    if (tdi) {
        TASKDIALOG_BUTTON b[2] = { { 100, L"Open taskbar settings" }, { 101, L"Switch back to big labels" } };
        TASKDIALOGCONFIG tc; ZeroMemory(&tc, sizeof tc);
        tc.cbSize = sizeof tc;
        tc.hwndParent = g_wnd;
        tc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
        tc.dwCommonButtons = TDCBF_CLOSE_BUTTON;
        tc.pszWindowTitle = APP;
        tc.pszMainIcon = TD_INFORMATION_ICON;
        tc.pszMainInstruction = L"Windows is hiding TraySense's tray icon";
        tc.pszContent = body;
        tc.cButtons = 2; tc.pButtons = b; tc.nDefaultButton = 100;
        if (FAILED(tdi(&tc, &choice, NULL, NULL))) choice = 0;
    } else {
        wchar_t mb[800];
        _snwprintf(mb, 800, L"Windows is hiding TraySense's tray icon.\n\n%s\n\nOpen taskbar settings now?", body);
        mb[799] = 0;
        choice = MessageBoxW(g_wnd, mb, APP, MB_YESNO | MB_ICONINFORMATION) == IDYES ? 100 : 0;
    }
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

static void menu(void)
{
    static const DWORD iv[3] = { 1000, 2000, 5000 };
    HMENU h = CreatePopupMenu(), sub = CreatePopupMenu();
    for (int o = 0; o < M_N; o++) {
        int m = ORDER[o];
        wchar_t lbl[80];
        lstrcpyW(lbl, NAMES[m]);
        if (m == M_CPUT && g_pioState == PIO_OK) lstrcatW(lbl, L"  (CPU sensor)");
        else if (m == M_CPUP && (g_mask >> m & 1) && g_pioState != PIO_OK && g_pioState != PIO_UNSUPPORTED)
            lstrcatW(lbl, L"  (needs CPU temperature support)");
        else if ((m == M_CPUT || m == M_GPUT || m == M_DISKT || m == M_CPUP || m == M_GPUP) && (g_mask >> m & 1) && !g_ok[m])
            lstrcatW(lbl, L"  (no sensor found)");
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
    AppendMenuW(sub, MF_STRING | (g_interval == 1000 ? MF_CHECKED : 0), 200, L"1 second");
    AppendMenuW(sub, MF_STRING | (g_interval == 2000 ? MF_CHECKED : 0), 201, L"2 seconds");
    AppendMenuW(sub, MF_STRING | (g_interval == 5000 ? MF_CHECKED : 0), 202, L"5 seconds (lowest power)");
    AppendMenuW(h, MF_POPUP, (UINT_PTR)sub, L"Refresh rate");
    AppendMenuW(h, MF_STRING | (autostart_on() ? MF_CHECKED : 0), 300, L"Start with Windows");
    AppendMenuW(h, MF_SEPARATOR, 0, NULL);
    if (TS_UPDATE_REPO[0]) {
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
    AppendMenuW(h, MF_STRING, 350, L"What's new (version " TS_VERSION L")");
    AppendMenuW(h, MF_STRING, 399, L"Exit");

    POINT pt; TPMPARAMS tp;
    UINT where = menu_pos(&pt, &tp);
    SetForegroundWindow(g_wnd);
    int cmd = TrackPopupMenuEx(h, where | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, g_wnd,
                               tp.cbSize ? &tp : NULL);
    PostMessageW(g_wnd, WM_NULL, 0, 0);
    DestroyMenu(h);

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
    } else if (cmd == 300) autostart_toggle();
    else if (cmd == 310) {
        g_mode = g_mode == MODE_BAR ? MODE_TRAY : MODE_BAR;
        refresh(1);
        save();
        /* Windows hides new tray icons; check shortly, once Windows has registered the icon. */
        if (g_mode == MODE_TRAY) SetTimer(g_wnd, TIMER_TRAYCHECK, 1500, NULL);
    }
    else if (cmd >= 360 && cmd < 360 + M_N) {
        g_trayShows = (DWORD)(cmd - 360);
        refresh(1);
        save();
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
            upd_cleanup();
            if (upd_auto_on() && !upd_start(UPD_AUTO)) upd_schedule(60 * 60 * 1000);
            return 0;
        }
        if (wp == TIMER_UPD_NOTICE) {
            KillTimer(w, wp);
            if (g_upd.ver[0]) upd_notice();
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

int WINAPI WinMain(HINSTANCE hi, HINSTANCE prev, LPSTR cmd, int show)
{
    (void)prev; (void)cmd; (void)show;
    {
        typedef BOOL (WINAPI *SDDD)(DWORD);
        SDDD f = (SDDD)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetDefaultDllDirectories");
        if (f) f(LOAD_LIBRARY_SEARCH_SYSTEM32);
        SetDllDirectoryW(L"");
    }
    if (is_admin()) lock_self();
    int restart = wcsstr(GetCommandLineW(), L"/restart") != NULL;
    g_showMenuMsg = RegisterWindowMessageW(L"TraySense.ShowMenu");
    for (int tries = 0;; tries++) {
#ifndef SINGLE_INSTANCE_NAME
#define SINGLE_INSTANCE_NAME L"Local\\TraySense.single"   /* test builds use their own name */
#endif
        HANDLE mx = CreateMutexW(NULL, TRUE, SINGLE_INSTANCE_NAME);
        if (GetLastError() != ERROR_ALREADY_EXISTS) break;
        CloseHandle(mx);
        if (!restart) {
            /* Already running (from the Start menu, a shortcut or the .exe): open its menu, so it can be
               reached even when its tray icons are hidden and the taskbar labels are off. */
            HWND other = FindWindowW(APP, APP);
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
    load();
    read_env();
    init_gpu();
    init_disk();
    if (wcsstr(GetCommandLineW(), L"/install-pawnio") && is_admin()) {
        HANDLE probe = CreateFileW(L"\\\\?\\GLOBALROOT\\Device\\PawnIO", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, 0, NULL);
        if (probe != INVALID_HANDLE_VALUE) CloseHandle(probe);
        else {
            const wchar_t *why = L"";
            if (!pawnio_install(&why)) MessageBoxW(NULL, why, APP L" \x2013 CPU temperature", MB_ICONWARNING);
        }
    }
    pio_init();
    autostart_migrate();

    WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = wndproc; wc.hInstance = hi; wc.lpszClassName = APP;
    RegisterClassW(&wc);
    wc.lpfnWndProc = barproc; wc.lpszClassName = L"TraySenseBar";
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    RegisterClassW(&wc);
    g_wnd = CreateWindowExW(WS_EX_TOOLWINDOW, APP, APP, WS_POPUP, 0, 0, 0, 0, NULL, NULL, hi, NULL);
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
    upd_cleanup();
    upd_schedule(60 * 1000);   /* first check a minute after starting, so it never slows down sign-in */

    MSG m;
    int trimmed = 0;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (g_notes && IsDialogMessageW(g_notes, &m)) continue;   /* Enter / Esc / Tab in the notes window */
        DispatchMessageW(&m);
        if (!trimmed && m.message == WM_SAMPLED) {
            trimmed = 1;
            if ((g_mask & (1u << M_CPUT)) && g_pioState != PIO_OK && g_pioState != PIO_UNSUPPORTED && !g_ok[M_CPUT])
                cpu_temp_setup(1);
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
