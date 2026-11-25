/* TrayVitals' own look: themes, see-through windows, and the windows drawn with them
   (the modern menu panel, popups and the "What's new" window).

   Every window here is a layered window: TrayVitals draws it into a 32-bit image with an alpha
   channel and hands that to Windows (UpdateLayeredWindow). That gives smooth rounded corners,
   a soft shadow and a background that can be partly see-through, on Windows 10 and 11 alike.
   GDI can't draw text with alpha, so text is drawn white-on-black into a second image and that
   coverage is blended in with the wanted color (the same trick the taskbar labels use).
   Included by trayvitals.c after its helpers (mkfont, sysdll, REGKEY, APP). */

/* ---------- themes ---------- */
typedef struct {
    const wchar_t *name;
    int      dark;
    COLORREF bg, surface, text, sub, accent, onAccent, border;
} THEME;

enum { TH_SYSTEM, TH_LIGHT, TH_DARK, TH_MIDNIGHT, TH_GRAPHITE, TH_FOREST, TH_SAKURA, TH_N };
static const THEME THEMES[TH_N] = {
    { L"Follow Windows", 0, 0, 0, 0, 0, 0, 0, 0 },   /* resolved to Light or Dark */
    { L"Light",    0, RGB(249,249,249), RGB(235,235,235), RGB(27,27,27),    RGB(96,96,96),    RGB(0,95,184),    RGB(255,255,255), RGB(214,214,214) },
    { L"Dark",     1, RGB(40,40,40),    RGB(60,60,60),    RGB(255,255,255), RGB(166,166,166), RGB(96,205,255),  RGB(0,0,0),       RGB(72,72,72) },
    { L"Midnight", 1, RGB(17,22,38),    RGB(32,41,68),    RGB(228,234,255), RGB(140,152,190), RGB(122,162,255), RGB(9,13,28),     RGB(46,57,90) },
    { L"Graphite", 1, RGB(30,30,32),    RGB(54,54,58),    RGB(236,236,236), RGB(152,152,157), RGB(255,166,77),  RGB(32,18,4),     RGB(68,68,72) },
    { L"Forest",   1, RGB(16,31,25),    RGB(30,54,43),    RGB(224,243,231), RGB(138,174,153), RGB(92,214,145),  RGB(6,24,14),     RGB(40,68,54) },
    { L"Sakura",   0, RGB(255,242,247), RGB(250,224,235), RGB(64,32,47),    RGB(140,98,116),  RGB(214,64,128),  RGB(255,255,255), RGB(240,204,219) },
};
/* Transparency of the menu and popups (not the What's new window): Off, Low, Medium, High. */
static const int UI_ALPHA_PCT[4]    = { 100, 92, 82, 70 };
static const wchar_t *UI_ALPHA_NAME[4] = { L"Off", L"Low", L"Medium", L"High" };
/* g_theme, g_uiAlpha, g_labelAlpha and LABEL_ALPHA_PCT are declared in trayvitals.c (the labels use them too). */

static int apps_light(void)
{
    DWORD v = 1, sz = sizeof v;
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &v, &sz);
    return v != 0;
}
static const THEME *ui_theme(void)
{
    DWORD t = g_theme < TH_N ? g_theme : TH_SYSTEM;
    if (t == TH_SYSTEM) t = apps_light() ? TH_LIGHT : TH_DARK;
    return &THEMES[t];
}
static int ui_bg_alpha(void) { return UI_ALPHA_PCT[g_uiAlpha < 4 ? g_uiAlpha : 0] * 255 / 100; }

/* Windows' own menus and dialogs (the classic menu) follow light/dark through uxtheme's
   undocumented SetPreferredAppMode (ordinal 135, Windows 10 1903+) and FlushMenuThemes (136). */
static void ui_native_theme(void)
{
    typedef int (WINAPI *SPAM)(int);
    typedef void (WINAPI *FMT)(void);
    HMODULE ux = sysdll(L"uxtheme.dll");
    if (!ux) return;
    SPAM spam = (SPAM)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    FMT  fmt  = (FMT)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(136));
    if (!spam) return;
    int mode = g_theme == TH_SYSTEM ? 1 /* AllowDark */ : THEMES[g_theme].dark ? 2 /* ForceDark */ : 3 /* ForceLight */;
    spam(mode);
    if (fmt) fmt();
}

/* ---------- drawing surface ---------- */
typedef struct {
    int w, h;
    DWORD *px, *tx;            /* the image (premultiplied BGRA) and the text coverage scratch */
    HDC dc, tdc;
    HBITMAP bmp, tbmp;
    HGDIOBJ ob, otb;
    RECT clip;
} CANVAS;

static BYTE g_covLut[256];

static int cv_init(CANVAS *c, int w, int h)
{
    ZeroMemory(c, sizeof *c);
    if (w < 1 || h < 1) return 0;
    if (!g_covLut[255]) for (int i = 0; i < 256; i++) g_covLut[i] = (BYTE)(255.0 * pow(i / 255.0, 0.85) + 0.5);
    BITMAPINFO bi; ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    c->dc = CreateCompatibleDC(NULL);
    c->tdc = CreateCompatibleDC(NULL);
    c->bmp = c->dc ? CreateDIBSection(c->dc, &bi, DIB_RGB_COLORS, (void **)&c->px, NULL, 0) : NULL;
    c->tbmp = c->tdc ? CreateDIBSection(c->tdc, &bi, DIB_RGB_COLORS, (void **)&c->tx, NULL, 0) : NULL;
    if (!c->bmp || !c->tbmp) {
        if (c->bmp) DeleteObject(c->bmp);
        if (c->tbmp) DeleteObject(c->tbmp);
        if (c->dc) DeleteDC(c->dc);
        if (c->tdc) DeleteDC(c->tdc);
        ZeroMemory(c, sizeof *c);
        return 0;
    }
    c->ob = SelectObject(c->dc, c->bmp);
    c->otb = SelectObject(c->tdc, c->tbmp);
    SetBkMode(c->tdc, TRANSPARENT);
    SetTextColor(c->tdc, RGB(255, 255, 255));
    c->w = w; c->h = h;
    SetRect(&c->clip, 0, 0, w, h);
    return 1;
}
static void cv_free(CANVAS *c)
{
    if (!c->dc) return;
    SelectObject(c->dc, c->ob); SelectObject(c->tdc, c->otb);
    DeleteObject(c->bmp); DeleteObject(c->tbmp);
    DeleteDC(c->dc); DeleteDC(c->tdc);
    ZeroMemory(c, sizeof *c);
}
static void cv_clear(CANVAS *c) { ZeroMemory(c->px, (SIZE_T)c->w * c->h * 4); SetRect(&c->clip, 0, 0, c->w, c->h); }

/* Paints color col with opacity a (0-255) over one pixel. */
static void px_over(DWORD *d, COLORREF col, int a)
{
    if (a <= 0) return;
    if (a > 255) a = 255;
    DWORD v = *d;
    int ia = 255 - a;
    int r = (GetRValue(col) * a + (int)((v >> 16) & 255) * ia + 127) / 255;
    int g = (GetGValue(col) * a + (int)((v >> 8) & 255) * ia + 127) / 255;
    int b = (GetBValue(col) * a + (int)(v & 255) * ia + 127) / 255;
    int na = a + ((int)(v >> 24) * ia + 127) / 255;
    *d = ((DWORD)na << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
}

/* Signed distance from (px,py) to a rounded rectangle: < 0 inside, > 0 outside. */
static float rr_dist(float px, float py, float x0, float y0, float x1, float y1, float rad)
{
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, hw = (x1 - x0) * 0.5f, hh = (y1 - y0) * 0.5f;
    if (rad > hw) rad = hw;
    if (rad > hh) rad = hh;
    float qx = fabsf(px - cx) - (hw - rad), qy = fabsf(py - cy) - (hh - rad);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float in = qx > qy ? qx : qy;
    return sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - rad;
}
static void cv_bounds(CANVAS *c, float x0, float y0, float x1, float y1, float grow, int *ix0, int *iy0, int *ix1, int *iy1)
{
    *ix0 = (int)floorf(x0 - grow); *iy0 = (int)floorf(y0 - grow);
    *ix1 = (int)ceilf(x1 + grow);  *iy1 = (int)ceilf(y1 + grow);
    if (*ix0 < c->clip.left) *ix0 = c->clip.left;
    if (*iy0 < c->clip.top) *iy0 = c->clip.top;
    if (*ix1 > c->clip.right) *ix1 = c->clip.right;
    if (*iy1 > c->clip.bottom) *iy1 = c->clip.bottom;
}
/* Filled rounded rectangle (anti-aliased). */
static void cv_rrect(CANVAS *c, float x0, float y0, float x1, float y1, float rad, COLORREF col, int a)
{
    int X0, Y0, X1, Y1;
    if (a <= 0 || x1 <= x0 || y1 <= y0) return;
    cv_bounds(c, x0, y0, x1, y1, 1, &X0, &Y0, &X1, &Y1);
    for (int y = Y0; y < Y1; y++)
        for (int x = X0; x < X1; x++) {
            float d = rr_dist(x + 0.5f, y + 0.5f, x0, y0, x1, y1, rad), cov = 0.5f - d;
            if (cov <= 0) continue;
            px_over(&c->px[y * c->w + x], col, cov >= 1 ? a : (int)(a * cov + 0.5f));
        }
}
/* Rounded-rectangle outline of width wd, inside the edge. */
static void cv_ring(CANVAS *c, float x0, float y0, float x1, float y1, float rad, float wd, COLORREF col, int a)
{
    int X0, Y0, X1, Y1;
    if (a <= 0) return;
    cv_bounds(c, x0, y0, x1, y1, 1, &X0, &Y0, &X1, &Y1);
    for (int y = Y0; y < Y1; y++)
        for (int x = X0; x < X1; x++) {
            float d = rr_dist(x + 0.5f, y + 0.5f, x0, y0, x1, y1, rad);
            float o = 0.5f - d, i = 0.5f - (d + wd);
            o = o < 0 ? 0 : o > 1 ? 1 : o; i = i < 0 ? 0 : i > 1 ? 1 : i;
            float cov = o - i;
            if (cov > 0) px_over(&c->px[y * c->w + x], col, (int)(a * cov + 0.5f));
        }
}
static void cv_circle(CANVAS *c, float cx, float cy, float r, COLORREF col, int a)
{ cv_rrect(c, cx - r, cy - r, cx + r, cy + r, r, col, a); }
/* Soft shadow outside a rounded rectangle. */
static void cv_shadow(CANVAS *c, float x0, float y0, float x1, float y1, float rad, float size, int a)
{
    int X0, Y0, X1, Y1;
    cv_bounds(c, x0, y0, x1, y1, size, &X0, &Y0, &X1, &Y1);
    for (int y = Y0; y < Y1; y++)
        for (int x = X0; x < X1; x++) {
            float d = rr_dist(x + 0.5f, y + 0.5f, x0, y0 + size * 0.15f, x1, y1 + size * 0.15f, rad);
            if (d <= 0 || d >= size) continue;   /* only outside the card: a see-through card stays clear */
            float f = 1 - d / size;
            px_over(&c->px[y * c->w + x], RGB(0, 0, 0), (int)(a * f * f + 0.5f));
        }
}
/* Text in rectangle r (DrawText flags), color col, opacity a. */
static void cv_text(CANVAS *c, HFONT f, const wchar_t *t, RECT r, UINT fmt, COLORREF col, int a)
{
    RECT rr;
    if (!t || !*t || !IntersectRect(&rr, &r, &c->clip)) return;
    for (int y = rr.top; y < rr.bottom; y++) ZeroMemory(c->tx + y * c->w + rr.left, (SIZE_T)(rr.right - rr.left) * 4);
    HRGN rg = CreateRectRgnIndirect(&rr);
    SelectClipRgn(c->tdc, rg);
    HGDIOBJ of = SelectObject(c->tdc, f);
    RECT dr = r;
    DrawTextW(c->tdc, t, -1, &dr, fmt | DT_NOPREFIX);
    SelectObject(c->tdc, of);
    SelectClipRgn(c->tdc, NULL);
    DeleteObject(rg);
    GdiFlush();
    for (int y = rr.top; y < rr.bottom; y++)
        for (int x = rr.left; x < rr.right; x++) {
            DWORD cov = c->tx[y * c->w + x] & 0xFF;
            if (cov) px_over(&c->px[y * c->w + x], col, a * g_covLut[cov] / 255);
        }
}
/* Height of text wrapped to width w. */
static int cv_measure(CANVAS *c, HFONT f, const wchar_t *t, int w, UINT fmt)
{
    RECT r = { 0, 0, w, 0 };
    HGDIOBJ of = SelectObject(c->tdc, f);
    DrawTextW(c->tdc, t, -1, &r, fmt | DT_CALCRECT | DT_NOPREFIX);
    SelectObject(c->tdc, of);
    return r.bottom;
}
static int cv_textw(CANVAS *c, HFONT f, const wchar_t *t)
{
    SIZE s = {0, 0};
    HGDIOBJ of = SelectObject(c->tdc, f);
    GetTextExtentPoint32W(c->tdc, t, lstrlenW(t), &s);
    SelectObject(c->tdc, of);
    return s.cx;
}

/* ---------- windows drawn with the canvas ---------- */
enum { UK_PANEL, UK_DIALOG, UK_NOTES };
#define UI_MAXHIT 128
typedef struct UIW UIW;
struct UIW {
    HWND hwnd;
    int kind, x, y, W, H, fade, done, result;
    float s;                                   /* DPI scale */
    CANVAS cv;
    const THEME *t;
    HFONT fUi, fSmall, fHead, fTitle, fBig, fIcon, fIconBig;
    int hover, focus, press, nh;
    struct { RECT r; int id; } hit[UI_MAXHIT];
    void (*paint)(UIW *);
    void (*click)(UIW *, int id);
    int  (*key)(UIW *, UINT vk);               /* returns 1 if handled */
    int  (*nchit)(UIW *, int x, int y);        /* HTCAPTION for draggable spots */
    void (*wheel)(UIW *, int delta);
    void (*drag)(UIW *, int phase, int y);     /* scrollbar dragging: 0 start, 1 move, 2 end */
    void *data;
    int dragging;
    int mx, my;                                /* last mouse position (client) */
    int solid;                                 /* ignore the Transparency setting */
};

#define UI_CLASS L"TrayVitalsUI"
static int S_(UIW *u, float v) { return (int)(v * u->s + 0.5f); }

static HFONT ui_mkfont(int px, int weight)
{
    HDC dc = GetDC(NULL);
    HFONT f = mkfont(dc, L"Segoe UI Variable Text", px, weight);
    if (!f) f = mkfont(dc, L"Segoe UI", px, weight);
    if (!f) f = CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Tahoma");
    ReleaseDC(NULL, dc);
    return f;
}
static HFONT ui_iconfont(int px)
{
    HDC dc = GetDC(NULL);
    HFONT f = mkfont(dc, L"Segoe Fluent Icons", px, FW_NORMAL);
    if (!f) f = mkfont(dc, L"Segoe MDL2 Assets", px, FW_NORMAL);
    ReleaseDC(NULL, dc);
    return f;
}
static void ui_fonts(UIW *u)
{
    u->fUi = ui_mkfont(S_(u, 13.5f), FW_NORMAL);
    u->fSmall = ui_mkfont(S_(u, 12), FW_NORMAL);
    u->fHead = ui_mkfont(S_(u, 12), FW_SEMIBOLD);
    u->fBig = ui_mkfont(S_(u, 15.5f), FW_SEMIBOLD);
    u->fTitle = ui_mkfont(S_(u, 20), FW_SEMIBOLD);
    u->fIcon = ui_iconfont(S_(u, 14));
    u->fIconBig = ui_iconfont(S_(u, 22));
}
static void ui_fonts_free(UIW *u)
{
    HFONT *fs[] = { &u->fUi, &u->fSmall, &u->fHead, &u->fTitle, &u->fBig, &u->fIcon, &u->fIconBig };
    for (int i = 0; i < 7; i++) if (*fs[i]) { DeleteObject(*fs[i]); *fs[i] = NULL; }
}
/* An icon glyph (Segoe Fluent Icons / MDL2 Assets), centered in r. Nothing if the font is missing. */
static void ui_icon(UIW *u, HFONT f, wchar_t g, RECT r, COLORREF col, int a)
{
    wchar_t s[2] = { g, 0 };
    if (f && g) cv_text(&u->cv, f, s, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE, col, a);
}
static void ui_addhit(UIW *u, RECT r, int id)
{
    if (u->nh >= UI_MAXHIT) return;
    RECT v;
    if (!IntersectRect(&v, &r, &u->cv.clip)) return;   /* scrolled out of view */
    u->hit[u->nh].r = v; u->hit[u->nh].id = id; u->nh++;
}
static int ui_hitat(UIW *u, int x, int y)
{
    POINT p = { x, y };
    for (int i = 0; i < u->nh; i++) if (PtInRect(&u->hit[i].r, p)) return u->hit[i].id;
    return -1;
}
static int ui_focusidx(UIW *u)
{
    for (int i = 0; i < u->nh; i++) if (u->hit[i].id == u->focus) return i;
    return -1;
}
static void ui_focusring(UIW *u, RECT r, float rad)
{
    cv_ring(&u->cv, (float)r.left - 1, (float)r.top - 1, (float)r.right + 1, (float)r.bottom + 1, rad + 1, 2.0f * u->s, u->t->accent, 255);
}
static void ui_present(UIW *u)
{
    POINT src = { 0, 0 }, pos = { u->x, u->y };
    SIZE sz = { u->W, u->H };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)u->fade, AC_SRC_ALPHA };
    UpdateLayeredWindow(u->hwnd, NULL, &pos, &sz, u->cv.dc, &src, 0, &bf, ULW_ALPHA);
}
static void ui_redraw(UIW *u)
{
    if (!u->cv.dc) return;
    cv_clear(&u->cv);
    u->nh = 0;
    u->t = ui_theme();
    u->paint(u);
    ui_present(u);
}
/* Resizes (and moves) the window, keeping the drawing surface in step. */
static void ui_resize(UIW *u, int x, int y, int W, int H)
{
    u->x = x; u->y = y;
    if (W != u->W || H != u->H || !u->cv.dc) {
        cv_free(&u->cv);
        u->W = W; u->H = H;
        cv_init(&u->cv, W, H);
    }
}
static void ui_move_focus(UIW *u, int dir)
{
    if (!u->nh) return;
    int i = ui_focusidx(u);
    i = i < 0 ? (dir > 0 ? 0 : u->nh - 1) : (i + dir + u->nh) % u->nh;
    u->focus = u->hit[i].id;
    ui_redraw(u);
}

static LRESULT CALLBACK ui_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    UIW *u = (UIW *)GetWindowLongPtrW(w, GWLP_USERDATA);
    if (!u) return DefWindowProcW(w, msg, wp, lp);
    switch (msg) {
    case WM_NCHITTEST: {
        POINT p = { (short)LOWORD(lp), (short)HIWORD(lp) };
        ScreenToClient(w, &p);
        if (u->nchit && ui_hitat(u, p.x, p.y) < 0) { int h = u->nchit(u, p.x, p.y); if (h) return h; }
        return HTCLIENT;
    }
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        u->mx = x; u->my = y;
        if (u->dragging && u->drag) { u->drag(u, 1, y); return 0; }
        int h = ui_hitat(u, x, y);
        if (h != u->hover) { u->hover = h; ui_redraw(u); }
        TRACKMOUSEEVENT te = { sizeof te, TME_LEAVE, w, 0 };
        TrackMouseEvent(&te);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (u->hover != -1 && !u->dragging) { u->hover = -1; ui_redraw(u); }
        return 0;
    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        u->press = ui_hitat(u, x, y);
        SetCapture(w);
        if (u->drag && u->press == 9000) { u->dragging = 1; u->drag(u, 0, y); }
        if (u->press >= 0) ui_redraw(u);
        return 0;
    }
    case WM_LBUTTONUP: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp), p = u->press;
        ReleaseCapture();
        if (u->dragging) { u->dragging = 0; if (u->drag) u->drag(u, 2, y); }
        u->press = -1;
        if (p >= 0 && p != 9000 && ui_hitat(u, x, y) == p) { u->focus = -1; u->click(u, p); }
        else ui_redraw(u);
        return 0;
    }
    case WM_MOUSEWHEEL:
        if (u->wheel) u->wheel(u, GET_WHEEL_DELTA_WPARAM(wp));
        return 0;
    case WM_KEYDOWN:
        if (u->key && u->key(u, (UINT)wp)) return 0;
        switch (wp) {
        case VK_ESCAPE: u->result = 0; u->done = 1; if (u->kind == UK_NOTES) DestroyWindow(w); return 0;
        case VK_TAB: ui_move_focus(u, GetKeyState(VK_SHIFT) < 0 ? -1 : 1); return 0;
        case VK_DOWN: case VK_RIGHT: ui_move_focus(u, 1); return 0;
        case VK_UP: case VK_LEFT: ui_move_focus(u, -1); return 0;
        case VK_RETURN: case VK_SPACE:
            if (u->focus >= 0 && ui_focusidx(u) >= 0) u->click(u, u->focus);
            return 0;
        }
        break;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && u->kind == UK_PANEL) { u->result = 0; u->done = 1; }
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            u->fade += 64;
            if (u->fade >= 255) { u->fade = 255; KillTimer(w, 1); }
            ui_present(u);
        }
        return 0;
    case WM_CLOSE:
        u->result = 0; u->done = 1;
        if (u->kind == UK_NOTES) DestroyWindow(w);
        return 0;
    case WM_SETTINGCHANGE:
        ui_redraw(u);   /* Windows switched light/dark */
        break;
    case WM_WINDOWPOSCHANGED: {
        /* Dragged somewhere else: remember it, so redrawing (scrolling, hovering) keeps it there. */
        const WINDOWPOS *p = (const WINDOWPOS *)lp;
        if (!(p->flags & SWP_NOMOVE)) { u->x = p->x; u->y = p->y; }
        break;
    }
    }
    return DefWindowProcW(w, msg, wp, lp);
}

static UIW *ui_create(int kind, DWORD exStyle)
{
    static int registered;
    HINSTANCE hi = GetModuleHandleW(NULL);
    if (!registered) {
        WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
        wc.lpfnWndProc = ui_proc; wc.hInstance = hi; wc.lpszClassName = UI_CLASS;
        wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1));   /* TrayVitals' icon, for the What's new window's taskbar button */
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        registered = RegisterClassW(&wc) != 0;
    }
    UIW *u = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *u);
    if (!u) return NULL;
    HDC dc = GetDC(NULL);
    u->s = (dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96) / 96.0f;
    if (dc) ReleaseDC(NULL, dc);
    if (u->s < 1) u->s = 1;
    u->kind = kind; u->hover = u->focus = u->press = -1;
    u->t = ui_theme();
    ui_fonts(u);
    u->hwnd = CreateWindowExW(WS_EX_LAYERED | exStyle, UI_CLASS, APP, WS_POPUP, 0, 0, 1, 1, NULL, NULL, hi, NULL);
    if (!u->hwnd) { ui_fonts_free(u); HeapFree(GetProcessHeap(), 0, u); return NULL; }
    SetWindowLongPtrW(u->hwnd, GWLP_USERDATA, (LONG_PTR)u);
    return u;
}
static void ui_destroy(UIW *u)
{
    if (!u) return;
    if (u->hwnd && IsWindow(u->hwnd)) { SetWindowLongPtrW(u->hwnd, GWLP_USERDATA, 0); DestroyWindow(u->hwnd); }
    cv_free(&u->cv);
    ui_fonts_free(u);
    HeapFree(GetProcessHeap(), 0, u);
}
/* First frame: draw, then fade in. */
static void ui_show(UIW *u, int activate)
{
    u->fade = 0;
    ui_redraw(u);
    ShowWindow(u->hwnd, activate ? SW_SHOW : SW_SHOWNOACTIVATE);
    if (activate) { SetForegroundWindow(u->hwnd); SetFocus(u->hwnd); }
    SetTimer(u->hwnd, 1, 12, NULL);
}
/* Modal loop until the window is done. Passes a WM_QUIT on to the main loop. */
static int ui_run(UIW *u)
{
    MSG m;
    while (!u->done) {
        int r = GetMessageW(&m, NULL, 0, 0);
        if (r <= 0) { PostQuitMessage(r == 0 ? (int)m.wParam : 0); break; }
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return u->result;
}
/* The work area of the monitor at p. */
static RECT ui_work(POINT p)
{
    MONITORINFO mi = { sizeof mi };
    GetMonitorInfoW(MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST), &mi);
    return mi.rcWork;
}
/* Background card: shadow, see-through fill, hairline border. Returns the card's rect. */
static void ui_card(UIW *u, RECT r, float rad, int shadow)
{
    if (shadow) cv_shadow(&u->cv, (float)r.left, (float)r.top, (float)r.right, (float)r.bottom, rad, (float)S_(u, 14), u->t->dark ? 110 : 60);
    /* The What's new window is for reading, so it's always solid; Transparency is for the menu and popups. */
    int a = (u->kind == UK_NOTES || u->solid) ? 255 : ui_bg_alpha();
    cv_rrect(&u->cv, (float)r.left, (float)r.top, (float)r.right, (float)r.bottom, rad, u->t->bg, a);
    cv_ring(&u->cv, (float)r.left, (float)r.top, (float)r.right, (float)r.bottom, rad, 1, u->t->border, 255);
}

/* ---------- popups (replace MessageBox / TaskDialog) ---------- */
enum { UI_INFO, UI_WARN, UI_QUESTION, UI_UPDATE };
#define UI_SOLID 0x100   /* add to the icon: always solid, whatever the Transparency setting (update popups) */
typedef struct { int id; const wchar_t *text; } UIBTN;
typedef struct { const wchar_t *head, *body; const UIBTN *b; int nb, icon, def; RECT card; } DLGDATA;
#define DLG_CLOSE 8000

static void dlg_paint(UIW *u)
{
    DLGDATA *d = (DLGDATA *)u->data;
    const THEME *t = u->t;
    int sh = S_(u, 16), pad = S_(u, 24), cw = u->W - 2 * sh;
    RECT card = { sh, sh, u->W - sh, u->H - sh };
    d->card = card;
    ui_card(u, card, (float)S_(u, 10), 1);
    /* icon */
    static const wchar_t IC[4] = { 0xE946, 0xE7BA, 0xE9CE, 0xE896 };
    COLORREF ic = (d->icon & 3) == UI_WARN ? RGB(240, 168, 40) : t->accent;
    int ix = card.left + pad, iy = card.top + pad, isz = S_(u, 36);
    cv_circle(&u->cv, ix + isz / 2.0f, iy + isz / 2.0f, isz / 2.0f, ic, 50);
    RECT ir = { ix, iy, ix + isz, iy + isz };
    ui_icon(u, u->fIconBig, IC[d->icon & 3], ir, ic, 255);
    /* close X */
    int cs = S_(u, 30);
    RECT xr = { card.right - S_(u, 8) - cs, card.top + S_(u, 8), card.right - S_(u, 8), card.top + S_(u, 8) + cs };
    if (u->hover == DLG_CLOSE) cv_rrect(&u->cv, (float)xr.left, (float)xr.top, (float)xr.right, (float)xr.bottom, (float)S_(u, 5), t->surface, 255);
    ui_icon(u, u->fIcon, 0xE711, xr, t->sub, 255);
    if (u->focus == DLG_CLOSE) ui_focusring(u, xr, (float)S_(u, 5));
    ui_addhit(u, xr, DLG_CLOSE);
    /* text */
    int tx = ix + isz + S_(u, 14), tw = card.right - pad - tx - S_(u, 10);
    int hh = cv_measure(&u->cv, u->fBig, d->head, tw, DT_WORDBREAK);
    RECT hr = { tx, iy + S_(u, 6), tx + tw, iy + S_(u, 6) + hh };
    cv_text(&u->cv, u->fBig, d->head, hr, DT_WORDBREAK, t->text, 255);
    if (d->body && *d->body) {
        int bh = cv_measure(&u->cv, u->fUi, d->body, tw, DT_WORDBREAK);
        RECT br = { tx, hr.bottom + S_(u, 8), tx + tw, hr.bottom + S_(u, 8) + bh };
        cv_text(&u->cv, u->fUi, d->body, br, DT_WORDBREAK, t->sub, 255);
    }
    /* buttons, right to left from the card's bottom-right; the first one is the main one */
    int bh = S_(u, 34), by = card.bottom - pad - bh, bx = card.right - pad;
    for (int i = d->nb - 1; i >= 0; i--) {
        int bw = cv_textw(&u->cv, u->fUi, d->b[i].text) + S_(u, 32);
        if (bw < S_(u, 96)) bw = S_(u, 96);
        RECT r = { bx - bw, by, bx, by + bh };
        int main = i == 0, hov = u->hover == d->b[i].id, prs = u->press == d->b[i].id;
        if (main) {
            cv_rrect(&u->cv, (float)r.left, (float)r.top, (float)r.right, (float)r.bottom, (float)S_(u, 6), t->accent, prs ? 200 : hov ? 230 : 255);
            cv_text(&u->cv, u->fUi, d->b[i].text, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE, t->onAccent, 255);
        } else {
            cv_rrect(&u->cv, (float)r.left, (float)r.top, (float)r.right, (float)r.bottom, (float)S_(u, 6), t->surface, prs ? 255 : hov ? 230 : 150);
            cv_ring(&u->cv, (float)r.left, (float)r.top, (float)r.right, (float)r.bottom, (float)S_(u, 6), 1, t->border, 255);
            cv_text(&u->cv, u->fUi, d->b[i].text, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE, t->text, 255);
        }
        if (u->focus == d->b[i].id) ui_focusring(u, r, (float)S_(u, 6));
        ui_addhit(u, r, d->b[i].id);
        bx = r.left - S_(u, 8);
    }
    (void)cw;
}
static void dlg_click(UIW *u, int id) { u->result = id == DLG_CLOSE ? 0 : id; u->done = 1; }
static int dlg_key(UIW *u, UINT vk)
{
    DLGDATA *d = (DLGDATA *)u->data;
    if (vk == VK_RETURN && (u->focus < 0 || ui_focusidx(u) < 0)) { dlg_click(u, d->def); return 1; }
    return 0;
}
static int dlg_nchit(UIW *u, int x, int y)
{
    DLGDATA *d = (DLGDATA *)u->data;
    POINT p = { x, y };
    return PtInRect(&d->card, p) ? HTCAPTION : 0;
}
static HWND g_uiDialog;   /* the popup waiting for an answer, if any */

/* Shows a themed popup and waits. Returns the clicked button's id, or 0 for Esc / close. */
static int ui_dialog(const wchar_t *head, const wchar_t *body, const UIBTN *b, int nb, int icon)
{
    DLGDATA d = { head, body, b, nb, icon, nb ? b[0].id : 0, { 0, 0, 0, 0 } };
    if (g_uiDialog) return 0;   /* one question at a time */
    UIW *u = ui_create(UK_DIALOG, WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
    if (!u) return 0;
    g_uiDialog = u->hwnd;
    u->solid = (icon & UI_SOLID) != 0;
    u->data = &d; u->paint = dlg_paint; u->click = dlg_click; u->key = dlg_key; u->nchit = dlg_nchit;
    /* size: fixed width, height from the text */
    int sh = S_(u, 16), pad = S_(u, 24), W = S_(u, 460) + 2 * sh;
    CANVAS m;
    int H = 0;
    if (cv_init(&m, 8, 8)) {
        int tw = W - 2 * sh - 2 * pad - S_(u, 36) - S_(u, 14) - S_(u, 10);
        int hh = cv_measure(&m, u->fBig, head, tw, DT_WORDBREAK);
        int bh = body && *body ? cv_measure(&m, u->fUi, body, tw, DT_WORDBREAK) + S_(u, 8) : 0;
        int text = S_(u, 6) + hh + bh;
        if (text < S_(u, 36)) text = S_(u, 36);
        H = 2 * sh + pad + text + S_(u, 24) + S_(u, 34) + pad;
        cv_free(&m);
    }
    POINT p; GetCursorPos(&p);
    RECT wa = ui_work(p);
    ui_resize(u, wa.left + (wa.right - wa.left - W) / 2, wa.top + (wa.bottom - wa.top - H) / 2 - S_(u, 40), W, H);
    MessageBeep((icon & 3) == UI_WARN ? MB_ICONWARNING : MB_ICONINFORMATION);
    ui_show(u, 1);
    int r = ui_run(u);
    g_uiDialog = NULL;
    ui_destroy(u);
    return r;
}
/* While a popup waits for an answer, the menu doesn't open; the popup comes to the front instead. */
static int ui_busy(void)
{
    if (!g_uiDialog) return 0;
    SetForegroundWindow(g_uiDialog);
    FlashWindow(g_uiDialog, TRUE);
    return 1;
}
static void ui_msg(const wchar_t *head, const wchar_t *body, int icon)
{
    UIBTN ok = { 1, L"OK" };
    ui_dialog(head, body, &ok, 1, icon);
}
static int ui_ask(const wchar_t *head, const wchar_t *body, const wchar_t *yes, const wchar_t *no, int icon)
{
    UIBTN b[2] = { { 1, yes }, { 2, no } };
    return ui_dialog(head, body, b, 2, icon) == 1;
}

/* ---------- modern menu panel ---------- */
enum { PI_HEAD, PI_TOGGLE, PI_GRID, PI_CHIPS, PI_SWATCH, PI_ACTION, PI_FOOTER, PI_SEP, PI_NOTE,
       PI_DROPDOWN,   /* label + a box showing the current choice; on = open */
       PI_LIST };     /* the open drop-down's choices (chips), right under it */
typedef struct { wchar_t t[28]; int cmd, on, warn; wchar_t icon; } PCHIP;
typedef struct {
    int type, cmd, on, dis, stay, accent;
    wchar_t icon;
    wchar_t text[400], right[48];
    int nch;
    PCHIP ch[16];   /* the Readings grid has one per reading (11) */
} PITEM;
typedef struct {
    PITEM it[40];
    int n;
    int (*build)(PITEM *it, int max);
    void (*exec)(int cmd);
    int contentH, scroll, closeCmd;
    UINT flags; POINT pt;            /* where it opens, as for TrackPopupMenu */
    RECT card;
    int listScroll, listSeen;        /* the open drop-down list scrolls on its own */
    RECT listRect;
} PANEL;
#define LIST_ROWS 5                  /* a drop-down shows this many choices at a time */
#define PANEL_W 330

static int pi_height(UIW *u, CANVAS *c, PITEM *it, int w)
{
    switch (it->type) {
    case PI_HEAD:   return S_(u, 28);
    case PI_TOGGLE: return S_(u, 36);
    case PI_GRID:   return ((it->nch + 1) / 2) * S_(u, 36) + S_(u, 2);
    case PI_CHIPS:  return S_(u, 38);
    case PI_SWATCH: return S_(u, 68);
    case PI_ACTION: return S_(u, 34);
    case PI_FOOTER: return S_(u, 44);
    case PI_SEP:    return S_(u, 9);
    case PI_NOTE:   return cv_measure(c, u->fSmall, it->text, w - S_(u, 24), DT_WORDBREAK) + S_(u, 8);
    case PI_DROPDOWN: return S_(u, 38);
    case PI_LIST:   return (it->nch < LIST_ROWS ? it->nch : LIST_ROWS) * S_(u, 30) + S_(u, 10);
    }
    return 0;
}
static void panel_layout(UIW *u, PANEL *p)
{
    CANVAS m;
    int sh = S_(u, 16), w = S_(u, PANEL_W), h = S_(u, 6) * 2;
    if (!cv_init(&m, 8, 8)) return;
    for (int i = 0; i < p->n; i++) h += pi_height(u, &m, &p->it[i], w - S_(u, 12));
    cv_free(&m);
    p->contentH = h;
    /* Fit on the screen; scroll if it doesn't. */
    RECT wa = ui_work(p->pt);
    int maxH = wa.bottom - wa.top - S_(u, 16);
    int vh = h < maxH ? h : maxH;
    if (p->scroll > h - vh) p->scroll = h - vh;
    if (p->scroll < 0) p->scroll = 0;
    int x = p->flags & TPM_CENTERALIGN ? p->pt.x - w / 2 : p->flags & TPM_RIGHTALIGN ? p->pt.x - w : p->pt.x;
    int y = p->flags & TPM_BOTTOMALIGN ? p->pt.y - vh : p->flags & TPM_VCENTERALIGN ? p->pt.y - vh / 2 : p->pt.y;
    int gap = S_(u, 8);
    if ((p->flags & TPM_VERTICAL) && (p->flags & TPM_BOTTOMALIGN)) y -= gap;
    else if (p->flags & TPM_VERTICAL) y += gap;
    else if (p->flags & TPM_RIGHTALIGN) x -= gap;
    else x += gap;
    if (x + w > wa.right - gap) x = wa.right - gap - w;
    if (x < wa.left + gap) x = wa.left + gap;
    if (y + vh > wa.bottom - gap) y = wa.bottom - gap - vh;
    if (y < wa.top + gap) y = wa.top + gap;
    ui_resize(u, x - sh, y - sh, w + 2 * sh, vh + 2 * sh);
}
static void panel_switch(UIW *u, RECT r, int on, int hov)
{
    const THEME *t = u->t;
    float h = (float)S_(u, 20), w = (float)S_(u, 40), x1 = (float)r.right - S_(u, 12), x0 = x1 - w, y0 = (r.top + r.bottom - h) / 2.0f;
    if (on) cv_rrect(&u->cv, x0, y0, x1, y0 + h, h / 2, t->accent, hov ? 225 : 255);
    else {
        cv_rrect(&u->cv, x0, y0, x1, y0 + h, h / 2, t->surface, hov ? 255 : 120);
        cv_ring(&u->cv, x0, y0, x1, y0 + h, h / 2, 1, t->sub, 255);
    }
    float kr = h / 2 - S_(u, 4) - (on ? 0 : 1);
    float kx = on ? x1 - h / 2 : x0 + h / 2;
    cv_circle(&u->cv, kx, y0 + h / 2, kr, on ? t->onAccent : t->sub, 255);
}
static void panel_paint(UIW *u)
{
    PANEL *p = (PANEL *)u->data;
    const THEME *t = u->t;
    int sh = S_(u, 16);
    RECT card = { sh, sh, u->W - sh, u->H - sh };
    p->card = card;
    ui_card(u, card, (float)S_(u, 8), 1);
    RECT view = card; InflateRect(&view, -1, -S_(u, 4));
    u->cv.clip = view;
    int x0 = card.left + S_(u, 6), x1 = card.right - S_(u, 6), y = card.top + S_(u, 6) - p->scroll;
    float rad = (float)S_(u, 5);
    int sawList = 0;
    for (int i = 0; i < p->n; i++) {
        PITEM *it = &p->it[i];
        int h = pi_height(u, &u->cv, it, x1 - x0);
        RECT r = { x0, y, x1, y + h };
        y += h;
        if (r.bottom < view.top || r.top > view.bottom) continue;
        int id = i * 16, hov = u->hover == id, foc = u->focus == id;
        COLORREF tc = it->dis ? t->sub : it->accent ? t->accent : t->text;
        int tx = r.left + S_(u, 12);
        switch (it->type) {
        case PI_HEAD: {
            RECT tr = { tx, r.top + S_(u, 8), r.right - S_(u, 12), r.bottom };
            cv_text(&u->cv, u->fHead, it->text, tr, DT_SINGLELINE | DT_VCENTER, t->sub, 255);
            break;
        }
        case PI_SEP:
            cv_rrect(&u->cv, (float)tx, r.top + h / 2.0f - 0.5f, (float)r.right - S_(u, 12), r.top + h / 2.0f + 0.5f, 0, t->border, 255);
            break;
        case PI_NOTE: {
            RECT tr = { tx, r.top + S_(u, 2), r.right - S_(u, 12), r.bottom };
            cv_text(&u->cv, u->fSmall, it->text, tr, DT_WORDBREAK, t->sub, 255);
            break;
        }
        case PI_TOGGLE:
        case PI_ACTION: {
            if (hov && !it->dis) cv_rrect(&u->cv, (float)r.left, (float)r.top + 1, (float)r.right, (float)r.bottom - 1, rad, t->surface, u->press == id ? 255 : 200);
            if (it->icon) {
                RECT ir = { tx, r.top, tx + S_(u, 18), r.bottom };
                ui_icon(u, u->fIcon, it->icon, ir, it->accent ? t->accent : t->text, it->dis ? 140 : 255);
                tx += S_(u, 28);
            }
            int rw = it->type == PI_TOGGLE ? S_(u, 56) : S_(u, 12);
            if (it->right[0]) {
                int w = cv_textw(&u->cv, u->fSmall, it->right);
                RECT rr = { r.right - rw - w, r.top, r.right - rw, r.bottom };
                cv_text(&u->cv, u->fSmall, it->right, rr, DT_SINGLELINE | DT_VCENTER | DT_RIGHT, t->sub, 255);
                rw += w + S_(u, 8);
            }
            RECT tr = { tx, r.top, r.right - rw, r.bottom };
            cv_text(&u->cv, u->fUi, it->text, tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS, tc, 255);
            if (it->type == PI_TOGGLE) panel_switch(u, r, it->on, hov);
            if (foc) ui_focusring(u, r, rad);
            if (!it->dis) ui_addhit(u, r, id);
            break;
        }
        case PI_GRID: {
            int gap = S_(u, 6), cw = (r.right - r.left - S_(u, 12) * 2 + S_(u, 12) - gap) / 2, ph = S_(u, 30);
            for (int k = 0; k < it->nch; k++) {
                PCHIP *c = &it->ch[k];
                int cx = r.left + S_(u, 6) + (k % 2) * (cw + gap), cy = r.top + (k / 2) * S_(u, 36) + S_(u, 3);
                RECT cr = { cx, cy, cx + cw, cy + ph };
                int kid = id + k, kh = u->hover == kid;
                if (c->on) {
                    cv_rrect(&u->cv, (float)cr.left, (float)cr.top, (float)cr.right, (float)cr.bottom, rad, t->accent, kh ? 70 : 45);
                    cv_ring(&u->cv, (float)cr.left, (float)cr.top, (float)cr.right, (float)cr.bottom, rad, 1, t->accent, 170);
                } else {
                    cv_rrect(&u->cv, (float)cr.left, (float)cr.top, (float)cr.right, (float)cr.bottom, rad, t->surface, kh ? 230 : 90);
                    cv_ring(&u->cv, (float)cr.left, (float)cr.top, (float)cr.right, (float)cr.bottom, rad, 1, t->border, 255);
                }
                RECT ir = { cr.left + S_(u, 6), cr.top, cr.left + S_(u, 24), cr.bottom };
                ui_icon(u, u->fIcon, c->on ? 0xE73E : 0xE739, ir, c->on ? t->accent : t->sub, 255);
                RECT tr = { cr.left + S_(u, 28), cr.top, cr.right - S_(u, c->warn ? 20 : 6), cr.bottom };
                cv_text(&u->cv, u->fSmall, c->t, tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS, t->text, c->on ? 255 : 200);
                if (c->warn) cv_circle(&u->cv, cr.right - S_(u, 11), (cr.top + cr.bottom) / 2.0f, S_(u, 3.5f), RGB(240, 168, 40), 255);
                if (u->focus == kid) ui_focusring(u, cr, rad);
                ui_addhit(u, cr, kid);
            }
            break;
        }
        case PI_CHIPS: {
            int lw = it->text[0] ? S_(u, 104) : 0;
            if (lw) {
                RECT tr = { tx, r.top, tx + lw, r.bottom };
                cv_text(&u->cv, u->fUi, it->text, tr, DT_SINGLELINE | DT_VCENTER, t->text, 255);
            }
            int sx0 = tx + lw, sx1 = r.right - S_(u, 10), sh2 = S_(u, 28), sy = (r.top + r.bottom - sh2) / 2;
            cv_rrect(&u->cv, (float)sx0, (float)sy, (float)sx1, (float)sy + sh2, rad, t->surface, 170);
            float segw = (float)(sx1 - sx0) / (it->nch ? it->nch : 1);
            for (int k = 0; k < it->nch; k++) {
                PCHIP *c = &it->ch[k];
                RECT cr = { sx0 + (int)(k * segw), sy, sx0 + (int)((k + 1) * segw), sy + sh2 };
                int kid = id + k, kh = u->hover == kid;
                if (c->on) cv_rrect(&u->cv, cr.left + 2.0f, cr.top + 2.0f, cr.right - 2.0f, cr.bottom - 2.0f, rad - 1, t->accent, 255);
                else if (kh) cv_rrect(&u->cv, cr.left + 2.0f, cr.top + 2.0f, cr.right - 2.0f, cr.bottom - 2.0f, rad - 1, t->text, u->press == kid ? 40 : 22);
                if (c->icon) ui_icon(u, u->fIcon, c->icon, cr, c->on ? t->onAccent : t->text, 255);
                else cv_text(&u->cv, u->fSmall, c->t, cr, DT_SINGLELINE | DT_VCENTER | DT_CENTER, c->on ? t->onAccent : t->text, 255);
                if (u->focus == kid) ui_focusring(u, cr, rad);
                ui_addhit(u, cr, kid);
            }
            break;
        }
        case PI_DROPDOWN: {
            int lw = S_(u, 104), bx0 = tx + lw, bx1 = r.right - S_(u, 10), bh = S_(u, 28), by = (r.top + r.bottom - bh) / 2;
            RECT tr = { tx, r.top, bx0, r.bottom };
            cv_text(&u->cv, u->fUi, it->text, tr, DT_SINGLELINE | DT_VCENTER, t->text, 255);
            RECT br = { bx0, by, bx1, by + bh };
            cv_rrect(&u->cv, (float)br.left, (float)br.top, (float)br.right, (float)br.bottom, rad, t->surface, hov ? 255 : 170);
            cv_ring(&u->cv, (float)br.left, (float)br.top, (float)br.right, (float)br.bottom, rad, 1, it->on ? t->accent : t->border, 255);
            RECT vr = { br.left + S_(u, 10), br.top, br.right - S_(u, 30), br.bottom };
            cv_text(&u->cv, u->fSmall, it->right, vr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS, t->text, 255);
            RECT cr = { br.right - S_(u, 28), br.top, br.right - S_(u, 6), br.bottom };
            ui_icon(u, u->fIcon, it->on ? 0xE70E : 0xE70D, cr, t->sub, 255);   /* chevron up / down */
            if (foc) ui_focusring(u, br, rad);
            ui_addhit(u, br, id);
            break;
        }
        case PI_LIST: {
            int lx0 = tx + S_(u, 104), lx1 = r.right - S_(u, 10), oh = S_(u, 30);
            int vis = it->nch < LIST_ROWS ? it->nch : LIST_ROWS, maxS = (it->nch - vis) * oh;
            if (!p->listSeen) {   /* just opened: start with the current choice in view */
                int sel = 0;
                for (int k = 0; k < it->nch; k++) if (it->ch[k].on) sel = k;
                p->listScroll = (sel - vis / 2) * oh;
                p->listSeen = 1;
            }
            if (p->listScroll > maxS) p->listScroll = maxS;
            if (p->listScroll < 0) p->listScroll = 0;
            sawList = 1;
            RECT lr = { lx0, r.top + S_(u, 2), lx1, r.bottom - S_(u, 6) };
            p->listRect = lr;
            cv_rrect(&u->cv, (float)lr.left, (float)lr.top, (float)lr.right, (float)lr.bottom, rad, t->surface, 200);
            cv_ring(&u->cv, (float)lr.left, (float)lr.top, (float)lr.right, (float)lr.bottom, rad, 1, t->border, 255);
            RECT keepClip = u->cv.clip, inner = lr;
            InflateRect(&inner, -1, -S_(u, 2));
            IntersectRect(&u->cv.clip, &keepClip, &inner);
            if (maxS > 0) {   /* slim scrollbar */
                float th = (float)(inner.bottom - inner.top) * vis / it->nch;
                float ty = inner.top + (inner.bottom - inner.top - th) * p->listScroll / (float)maxS;
                cv_rrect(&u->cv, (float)inner.right - S_(u, 4), ty, (float)inner.right - S_(u, 1), ty + th, (float)S_(u, 1.5f), t->sub, 150);
            }
            for (int k = 0; k < it->nch; k++) {
                PCHIP *c = &it->ch[k];
                int oy = lr.top + S_(u, 2) + k * oh - p->listScroll;
                RECT orr = { lr.left + S_(u, 3), oy, lr.right - S_(u, 7), oy + oh };
                int kid = id + k, kh = u->hover == kid;
                if (kh) cv_rrect(&u->cv, (float)orr.left, (float)orr.top, (float)orr.right, (float)orr.bottom, rad - 1, t->text, u->press == kid ? 40 : 22);
                if (c->on) cv_rrect(&u->cv, (float)orr.left + S_(u, 2), orr.top + S_(u, 8.0f), (float)orr.left + S_(u, 5), orr.bottom - S_(u, 8.0f),
                                    (float)S_(u, 1.5f), t->accent, 255);
                RECT ot = { orr.left + S_(u, 12), orr.top, orr.right - S_(u, 26), orr.bottom };
                cv_text(&u->cv, u->fSmall, c->t, ot, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS, c->on ? t->accent : t->text, 255);
                if (c->on) { RECT ci = { orr.right - S_(u, 24), orr.top, orr.right - S_(u, 6), orr.bottom }; ui_icon(u, u->fIcon, 0xE73E, ci, t->accent, 255); }
                if (u->focus == kid) ui_focusring(u, orr, rad - 1);
                ui_addhit(u, orr, kid);
            }
            u->cv.clip = keepClip;
            break;
        }
        case PI_SWATCH: {
            /* "Theme   Midnight" on the first line, the swatches spread across the second */
            RECT tr = { tx, r.top, r.right, r.top + S_(u, 30) };
            cv_text(&u->cv, u->fUi, it->text, tr, DT_SINGLELINE | DT_VCENTER, t->text, 255);
            RECT nr = { tx, r.top, r.right - S_(u, 12), r.top + S_(u, 30) };
            cv_text(&u->cv, u->fSmall, it->right, nr, DT_SINGLELINE | DT_VCENTER | DT_RIGHT, t->sub, 255);
            float d = (float)S_(u, 24), cy = r.top + S_(u, 30) + S_(u, 18);
            float span = (float)(r.right - S_(u, 12) - tx) - d - S_(u, 8);
            float g = it->nch > 1 ? span / (it->nch - 1) - d : 0;
            float cx = tx + S_(u, 4) + d / 2;
            for (int k = 0; k < it->nch; k++, cx += d + g) {
                PCHIP *c = &it->ch[k];
                int kid = id + k, kh = u->hover == kid;
                float rr = d / 2;
                if (c->on) cv_circle(&u->cv, cx, cy, rr + S_(u, 3.5f), t->text, 255), cv_circle(&u->cv, cx, cy, rr + S_(u, 1.5f), t->bg, 255);
                else if (kh) cv_circle(&u->cv, cx, cy, rr + S_(u, 3), t->sub, 160), cv_circle(&u->cv, cx, cy, rr + S_(u, 1.5f), t->bg, 255);
                if (k == 0) {   /* Follow Windows: half light, half dark */
                    RECT keep = u->cv.clip, l = { (int)(cx - rr - 2), (int)(cy - rr - 2), (int)cx, (int)(cy + rr + 2) };
                    cv_circle(&u->cv, cx, cy, rr, THEMES[TH_DARK].bg, 255);
                    IntersectRect(&u->cv.clip, &keep, &l);
                    cv_circle(&u->cv, cx, cy, rr, THEMES[TH_LIGHT].bg, 255);
                    u->cv.clip = keep;
                } else {
                    const THEME *s = &THEMES[k];
                    cv_circle(&u->cv, cx, cy, rr, s->bg, 255);
                    cv_circle(&u->cv, cx, cy, rr * 0.45f, s->accent, 255);
                }
                cv_ring(&u->cv, cx - rr, cy - rr, cx + rr, cy + rr, rr, 1, t->border, 255);
                RECT cr = { (int)(cx - rr - 3), (int)(cy - rr - 3), (int)(cx + rr + 3), (int)(cy + rr + 3) };
                if (u->focus == kid) ui_focusring(u, cr, rr + 3);
                ui_addhit(u, cr, kid);
            }
            break;
        }
        case PI_FOOTER: {
            int gap = S_(u, 6), n = it->nch ? it->nch : 1, bw = (r.right - r.left - S_(u, 12) - (n - 1) * gap) / n;
            for (int k = 0; k < it->nch; k++) {
                PCHIP *c = &it->ch[k];
                RECT cr = { r.left + S_(u, 6) + k * (bw + gap), r.top + S_(u, 6), r.left + S_(u, 6) + k * (bw + gap) + bw, r.bottom - S_(u, 4) };
                int kid = id + k, kh = u->hover == kid;
                cv_rrect(&u->cv, (float)cr.left, (float)cr.top, (float)cr.right, (float)cr.bottom, rad, t->surface, u->press == kid ? 255 : kh ? 230 : 130);
                int tw = cv_textw(&u->cv, u->fUi, c->t), iw = c->icon ? S_(u, 22) : 0, sx = (cr.left + cr.right - tw - iw) / 2;
                if (c->icon) { RECT ir = { sx, cr.top, sx + S_(u, 16), cr.bottom }; ui_icon(u, u->fIcon, c->icon, ir, t->text, 255); }
                RECT tr = { sx + iw, cr.top, cr.right, cr.bottom };
                cv_text(&u->cv, u->fUi, c->t, tr, DT_SINGLELINE | DT_VCENTER, t->text, 255);
                if (u->focus == kid) ui_focusring(u, cr, rad);
                ui_addhit(u, cr, kid);
            }
            break;
        }
        }
    }
    /* scroll hint: a thin bar when the panel doesn't fit */
    int vh = view.bottom - view.top;
    if (p->contentH > vh + S_(u, 12)) {
        float th = (float)vh * vh / p->contentH, ty = view.top + (float)(vh - th) * p->scroll / (p->contentH - vh + S_(u, 12));
        cv_rrect(&u->cv, (float)view.right - S_(u, 5), ty, (float)view.right - S_(u, 2), ty + th, (float)S_(u, 2), t->sub, 150);
    }
    if (!sawList) p->listSeen = 0;
    u->cv.clip.left = 0; u->cv.clip.top = 0; u->cv.clip.right = u->W; u->cv.clip.bottom = u->H;
}
static PCHIP *panel_chip(PANEL *p, int id, PITEM **pit)
{
    int i = id / 16, k = id % 16;
    if (i < 0 || i >= p->n) return NULL;
    *pit = &p->it[i];
    int t = (*pit)->type;
    return t == PI_GRID || t == PI_CHIPS || t == PI_SWATCH || t == PI_FOOTER || t == PI_LIST
           ? (k < (*pit)->nch ? &(*pit)->ch[k] : NULL) : NULL;
}
static void panel_click(UIW *u, int id)
{
    PANEL *p = (PANEL *)u->data;
    PITEM *it = NULL;
    PCHIP *c = panel_chip(p, id, &it);
    if (!it || it->dis) return;
    int cmd = c ? c->cmd : it->cmd;
    if (!cmd) return;
    if (!it->stay) { p->closeCmd = cmd; u->done = 1; return; }
    int keepFocus = u->focus;
    p->exec(cmd);
    if (u->done) return;   /* the command opened something that took the focus away */
    p->n = p->build(p->it, 40);
    u->t = ui_theme();
    panel_layout(u, p);
    u->focus = keepFocus;
    ui_redraw(u);
}
static void panel_wheel(UIW *u, int delta)
{
    PANEL *p = (PANEL *)u->data;
    POINT m = { u->mx, u->my };
    if (p->listSeen && PtInRect(&p->listRect, m)) {   /* over the drop-down list: scroll the list */
        p->listScroll -= delta * S_(u, 30) / WHEEL_DELTA;
        ui_redraw(u);
        return;
    }
    p->scroll -= delta * S_(u, 48) / WHEEL_DELTA;
    panel_layout(u, p);
    ui_redraw(u);
}
static int panel_key(UIW *u, UINT vk)
{
    if (vk == VK_APPS) { u->done = 1; return 1; }
    return 0;
}
/* Shows the panel and waits; stay-open items run right away through exec. Returns the command
   of an item that closes the panel (or 0). */
static int ui_panel(PANEL *p, int (*build)(PITEM *, int), void (*exec)(int), POINT pt, UINT flags)
{
    UIW *u = ui_create(UK_PANEL, WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
    if (!u) return 0;
    p->build = build; p->exec = exec; p->pt = pt; p->flags = flags; p->scroll = 0; p->closeCmd = 0;
    p->n = build(p->it, 40);
    u->data = p; u->paint = panel_paint; u->click = panel_click; u->wheel = panel_wheel; u->key = panel_key;
    panel_layout(u, p);
    ui_show(u, 1);
    ui_run(u);
    int cmd = p->closeCmd;
    ShowWindow(u->hwnd, SW_HIDE);
    ui_destroy(u);
    return cmd;
}

/* ---------- "What's new" window ---------- */
#define NOTES_CLOSE  8001
#define NOTES_OK     8002
#define NOTES_THUMB  9000
typedef struct { int head; const wchar_t *s; int len; } NLINE;
typedef struct {
    NLINE l[160]; int n;
    int contentH, scroll, viewTop, viewH, dragY0, dragS0;
    RECT thumb, title;
} NOTES;
static UIW  *g_notesW;
static NOTES g_notesD;

static void notes_parse(NOTES *d)
{
    const wchar_t *s = PATCH_NOTES;
    d->n = 0;
    while (*s && d->n < 160) {
        const wchar_t *e = wcsstr(s, L"\r\n");
        int ln = e ? (int)(e - s) : lstrlenW(s);
        if (ln > 0) {
            int bullet = ln >= 2 && s[0] == L'\x2022' && s[1] == L' ';
            d->l[d->n].head = !bullet;
            d->l[d->n].s = bullet ? s + 2 : s;
            d->l[d->n].len = bullet ? ln - 2 : ln;
            d->n++;
        }
        if (!e) break;
        s = e + 2;
    }
}
/* Lays out (and, when draw is set, paints) the notes: one card per version. Returns the height. */
static int notes_flow(UIW *u, NOTES *d, int x0, int x1, int y0, int draw)
{
    const THEME *t = u->t;
    wchar_t buf[1024];
    int y = y0, pad = S_(u, 16), first = 1;
    for (int i = 0; i < d->n;) {
        /* measure this version's card */
        int j = i + 1, h = pad + S_(u, 24) + S_(u, 6);
        for (; j < d->n && !d->l[j].head; j++) {
            lstrcpynW(buf, d->l[j].s, d->l[j].len + 1 < 1024 ? d->l[j].len + 1 : 1024);
            h += cv_measure(&u->cv, u->fUi, buf, x1 - x0 - 2 * pad - S_(u, 18), DT_WORDBREAK) + S_(u, 8);
        }
        h += pad - S_(u, 8);
        if (draw && y + h >= u->cv.clip.top && y <= u->cv.clip.bottom) {
            cv_rrect(&u->cv, (float)x0, (float)y, (float)x1, (float)y + h, (float)S_(u, 8), t->surface, first ? 150 : 90);
            if (first) cv_ring(&u->cv, (float)x0, (float)y, (float)x1, (float)y + h, (float)S_(u, 8), 1, t->accent, 120);
            lstrcpynW(buf, d->l[i].s, d->l[i].len + 1 < 64 ? d->l[i].len + 1 : 64);
            RECT hr = { x0 + pad, y + pad, x1 - pad, y + pad + S_(u, 24) };
            cv_text(&u->cv, u->fBig, buf, hr, DT_SINGLELINE | DT_VCENTER, t->text, 255);
            if (first) {
                int vw = cv_textw(&u->cv, u->fBig, buf);
                const wchar_t *badge = !lstrcmpW(buf, TV_VERSION) ? L"This version" : L"Latest";
                int bw = cv_textw(&u->cv, u->fSmall, badge) + S_(u, 16);
                RECT br = { hr.left + vw + S_(u, 10), hr.top + S_(u, 2), hr.left + vw + S_(u, 10) + bw, hr.bottom - S_(u, 2) };
                cv_rrect(&u->cv, (float)br.left, (float)br.top, (float)br.right, (float)br.bottom, (br.bottom - br.top) / 2.0f, t->accent, 255);
                cv_text(&u->cv, u->fSmall, badge, br, DT_CENTER | DT_VCENTER | DT_SINGLELINE, t->onAccent, 255);
            }
            int ly = hr.bottom + S_(u, 6);
            for (int k = i + 1; k < j; k++) {
                lstrcpynW(buf, d->l[k].s, d->l[k].len + 1 < 1024 ? d->l[k].len + 1 : 1024);
                int tw = x1 - x0 - 2 * pad - S_(u, 18), lh = cv_measure(&u->cv, u->fUi, buf, tw, DT_WORDBREAK);
                cv_circle(&u->cv, x0 + pad + S_(u, 4.0f), ly + S_(u, 10.0f), S_(u, 2.6f), t->accent, 255);
                RECT lr = { x0 + pad + S_(u, 18), ly, x0 + pad + S_(u, 18) + tw, ly + lh };
                cv_text(&u->cv, u->fUi, buf, lr, DT_WORDBREAK, t->text, 230);
                ly += lh + S_(u, 8);
            }
        }
        y += h + S_(u, 12);
        first = 0;
        i = j;
    }
    return y - y0;
}
static void notes_paint(UIW *u)
{
    NOTES *d = (NOTES *)u->data;
    const THEME *t = u->t;
    int sh = S_(u, 16), pad = S_(u, 20);
    RECT card = { sh, sh, u->W - sh, u->H - sh };
    ui_card(u, card, (float)S_(u, 10), 1);
    /* title */
    RECT tr = { card.left + pad, card.top + S_(u, 14), card.right - S_(u, 60), card.top + S_(u, 44) };
    cv_text(&u->cv, u->fTitle, L"What's new", tr, DT_SINGLELINE | DT_VCENTER, t->text, 255);
    RECT sr = { card.left + pad, tr.bottom, card.right - pad, tr.bottom + S_(u, 20) };
    cv_text(&u->cv, u->fSmall, APP L" " TV_VERSION L"  \x00B7  newest first", sr, DT_SINGLELINE | DT_VCENTER, t->sub, 255);
    d->title = card; d->title.bottom = sr.bottom + S_(u, 6);
    int cs = S_(u, 32);
    RECT xr = { card.right - S_(u, 10) - cs, card.top + S_(u, 10), card.right - S_(u, 10), card.top + S_(u, 10) + cs };
    if (u->hover == NOTES_CLOSE) cv_rrect(&u->cv, (float)xr.left, (float)xr.top, (float)xr.right, (float)xr.bottom, (float)S_(u, 5), t->surface, 255);
    ui_icon(u, u->fIcon, 0xE711, xr, t->sub, 255);
    if (u->focus == NOTES_CLOSE) ui_focusring(u, xr, (float)S_(u, 5));
    ui_addhit(u, xr, NOTES_CLOSE);
    /* footer button */
    int bh = S_(u, 34), bw = S_(u, 110);
    RECT ok = { card.right - pad - bw, card.bottom - pad - bh + S_(u, 4), card.right - pad, card.bottom - pad + S_(u, 4) };
    cv_rrect(&u->cv, (float)ok.left, (float)ok.top, (float)ok.right, (float)ok.bottom, (float)S_(u, 6), t->accent,
             u->press == NOTES_OK ? 200 : u->hover == NOTES_OK ? 230 : 255);
    cv_text(&u->cv, u->fUi, L"Close", ok, DT_CENTER | DT_VCENTER | DT_SINGLELINE, t->onAccent, 255);
    if (u->focus == NOTES_OK) ui_focusring(u, ok, (float)S_(u, 6));
    ui_addhit(u, ok, NOTES_OK);
    /* scrolling content */
    RECT view = { card.left + S_(u, 12), d->title.bottom + S_(u, 4), card.right - S_(u, 12), ok.top - S_(u, 14) };
    d->viewTop = view.top; d->viewH = view.bottom - view.top;
    RECT keep = u->cv.clip;
    u->cv.clip = view;
    d->contentH = notes_flow(u, d, view.left, view.right - S_(u, 10), view.top - d->scroll, 1);
    u->cv.clip = keep;
    /* scrollbar thumb, only when the notes don't fit */
    if (d->contentH > d->viewH) {
        float th = (float)d->viewH * d->viewH / d->contentH;
        if (th < S_(u, 30)) th = (float)S_(u, 30);
        float ty = view.top + (d->viewH - th) * d->scroll / (float)(d->contentH - d->viewH);
        SetRect(&d->thumb, view.right - S_(u, 8), (int)ty, view.right - S_(u, 2), (int)(ty + th));
        int active = u->dragging || u->hover == NOTES_THUMB;
        cv_rrect(&u->cv, (float)d->thumb.left + (active ? 0 : S_(u, 2)), (float)d->thumb.top, (float)d->thumb.right, (float)d->thumb.bottom,
                 (float)S_(u, 3), t->sub, active ? 220 : 140);
        RECT hr = d->thumb; InflateRect(&hr, S_(u, 4), 0);
        ui_addhit(u, hr, NOTES_THUMB);
    } else SetRectEmpty(&d->thumb);
}
static void notes_scrollto(UIW *u, int s)
{
    NOTES *d = (NOTES *)u->data;
    int mx = d->contentH - d->viewH;
    d->scroll = s < 0 ? 0 : s > mx ? (mx > 0 ? mx : 0) : s;
    ui_redraw(u);
}
static void notes_click(UIW *u, int id) { if (id == NOTES_CLOSE || id == NOTES_OK) DestroyWindow(u->hwnd); }
static void notes_wheel(UIW *u, int delta) { notes_scrollto(u, ((NOTES *)u->data)->scroll - delta * S_(u, 60) / WHEEL_DELTA); }
static int notes_key(UIW *u, UINT vk)
{
    NOTES *d = (NOTES *)u->data;
    switch (vk) {
    case VK_DOWN:  notes_scrollto(u, d->scroll + S_(u, 40)); return 1;
    case VK_UP:    notes_scrollto(u, d->scroll - S_(u, 40)); return 1;
    case VK_NEXT:  notes_scrollto(u, d->scroll + d->viewH - S_(u, 40)); return 1;
    case VK_PRIOR: notes_scrollto(u, d->scroll - d->viewH + S_(u, 40)); return 1;
    case VK_HOME:  notes_scrollto(u, 0); return 1;
    case VK_END:   notes_scrollto(u, d->contentH); return 1;
    case VK_RETURN: if (u->focus < 0) { DestroyWindow(u->hwnd); return 1; } return 0;
    }
    return 0;
}
static int notes_nchit(UIW *u, int x, int y)
{
    NOTES *d = (NOTES *)u->data;
    POINT p = { x, y };
    return PtInRect(&d->title, p) ? HTCAPTION : 0;
}
static void notes_drag(UIW *u, int phase, int y)
{
    NOTES *d = (NOTES *)u->data;
    if (phase == 0) { d->dragY0 = y; d->dragS0 = d->scroll; return; }
    int track = d->viewH - (d->thumb.bottom - d->thumb.top);
    if (track > 0) notes_scrollto(u, d->dragS0 + (y - d->dragY0) * (d->contentH - d->viewH) / track);
    else ui_redraw(u);
}
static LRESULT CALLBACK notes_sub(HWND w, UINT msg, WPARAM wp, LPARAM lp);
static WNDPROC g_notesBase;
static LRESULT CALLBACK notes_sub(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCDESTROY && g_notesW && g_notesW->hwnd == w) {
        UIW *u = g_notesW;
        g_notesW = NULL;
        LRESULT r = CallWindowProcW(g_notesBase, w, msg, wp, lp);
        u->hwnd = NULL;
        ui_destroy(u);
        return r;
    }
    return CallWindowProcW(g_notesBase, w, msg, wp, lp);
}
static void ui_notes(void)
{
    if (g_notesW) { ShowWindow(g_notesW->hwnd, SW_SHOWNORMAL); SetForegroundWindow(g_notesW->hwnd); return; }
    UIW *u = ui_create(UK_NOTES, WS_EX_APPWINDOW);
    if (!u) return;
    ZeroMemory(&g_notesD, sizeof g_notesD);
    notes_parse(&g_notesD);
    u->data = &g_notesD; u->paint = notes_paint; u->click = notes_click; u->wheel = notes_wheel;
    u->key = notes_key; u->nchit = notes_nchit; u->drag = notes_drag;
    SetWindowTextW(u->hwnd, APP L" \x2013 What's new");
    int sh = S_(u, 16), W = S_(u, 600) + 2 * sh, H = S_(u, 620) + 2 * sh;
    POINT p; GetCursorPos(&p);
    RECT wa = ui_work(p);
    if (H > wa.bottom - wa.top) H = wa.bottom - wa.top;
    if (W > wa.right - wa.left) W = wa.right - wa.left;
    ui_resize(u, wa.left + (wa.right - wa.left - W) / 2, wa.top + (wa.bottom - wa.top - H) / 2, W, H);
    g_notesW = u;
    g_notesBase = (WNDPROC)SetWindowLongPtrW(u->hwnd, GWLP_WNDPROC, (LONG_PTR)notes_sub);
    ui_show(u, 1);
}
/* After a theme or transparency change: redraw what's open. */
static void ui_retheme(void)
{
    if (g_notesW) ui_redraw(g_notesW);
}
