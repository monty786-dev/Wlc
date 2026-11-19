/* mtb2.h - taskbar window, hover preview, reveal bar, floating icon, full-screen logic, animation */
static void StartToggle(void);
static void StartHide(void);
static void OpenSettings(void);
static void ApplyAll(void);
static void DesktopToggle(void);
static void QuitShell(void);
static void TvToggle(void);
static void TgToggle(void);
enum { MT_RUN = 11, MT_CLOCK, MT_LOGIC, MT_KBD, MT_HIDEW, MT_ANIM, MT_PV, MT_PVREF, MT_SYNC, MT_SCAN };
static int AnimMs(int base) { return g.animOn ? base * 100 / g.animSpeed : 0; }      /* 0 = animations off */
static HWND TbZ(void) { return g.tbZ == 0 ? HWND_TOPMOST : (g.tbZ == 1 ? HWND_NOTOPMOST : HWND_BOTTOM); }   /* taskbar layer */

static int  g_tbShown = 1, g_tbAnimOn, g_tbAnimIn, g_revealed, g_fsActive, g_wantHide;
static DWORD g_tbAnimT0, g_revUntil;
static RECT g_wa0; static int g_waSaved, g_waKey = -1;
static HWND g_deskList[64]; static int g_nDeskList, g_showingDesk;

static void MWinPath(HWND h, wchar_t *out, int n) {
    DWORD pid = 0; HANDLE ph; out[0] = 0; GetWindowThreadProcessId(h, &pid);
    ph = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (ph) { DWORD sz = n; if (!QueryFullProcessImageNameW(ph, 0, out, &sz)) out[0] = 0; CloseHandle(ph); }
}
static void ShowDesktopToggle(void) {
    int i;
    if (g_showingDesk) { for (i = 0; i < g_nDeskList; i++) if (IsWindow(g_deskList[i])) ShowWindow(g_deskList[i], SW_RESTORE); g_showingDesk = 0; return; }
    g_nDeskList = 0;
    for (i = 0; i < g_nrun; i++) if (!IsIconic(g_run[i].h)) { g_deskList[g_nDeskList++] = g_run[i].h; ShowWindow(g_run[i].h, SW_MINIMIZE); }
    g_showingDesk = g_nDeskList > 0;
}
static void ActivateRun(HWND h) {
    if (IsIconic(h)) ShowWindow(h, SW_RESTORE);
    else if (h == GetForegroundWindow()) { ShowWindow(h, SW_MINIMIZE); return; }
    BringWindowToTop(h); SetForegroundWindow(h);
}

/* ---- work area (maximized windows must not hide behind the bar) ---- */
static void UpdateWorkArea(void) {
    RECT wa; int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN), key, reserve;
    if (!g_waSaved) { SystemParametersInfoW(SPI_GETWORKAREA, 0, &g_wa0, 0); g_waSaved = 1; }
    reserve = !g.rvAlways && !(g_fsActive && g.fsMode >= 1) && g_tbShown;
    SetRect(&wa, 0, 0, sw, sh);
    if (reserve) {
        int t = g_tbThick + g.tbDetach;
        if (g.tbPos == 0) wa.bottom = sh - t; else if (g.tbPos == 1) wa.top = t; else if (g.tbPos == 2) wa.left = t; else wa.right = sw - t;
    }
    key = wa.left * 3 + wa.top * 5 + wa.right * 7 + wa.bottom * 11;
    if (key == g_waKey) return;
    g_waKey = key; SystemParametersInfoW(SPI_SETWORKAREA, 0, &wa, SPIF_SENDCHANGE);
}
static void RestoreWorkArea(void) { if (g_waSaved) SystemParametersInfoW(SPI_SETWORKAREA, 0, &g_wa0, SPIF_SENDCHANGE); }
static BOOL CALLBACK WineTbEnum(HWND h, LPARAM lp) {
    wchar_t cls[64], t[64]; DWORD pid = 0; int hide = (int)lp; cls[0] = t[0] = 0;
    GetWindowThreadProcessId(h, &pid); if (pid == GetCurrentProcessId()) return TRUE;
    GetClassNameW(h, cls, 64); GetWindowTextW(h, t, 64);
    if (!wcscmp(cls, L"Shell_TrayWnd") || !wcscmp(cls, L"Shell_SecondaryTrayWnd") || !wcscmp(cls, L"WineTaskbar") || !wcscmp(cls, L"ExplorerTray") ||
        !_wcsicmp(t, L"Wine Taskbar") || !_wcsicmp(t, L"Taskbar")) {
        if (hide) { if (IsWindowVisible(h)) ShowWindow(h, SW_HIDE); } else if (!IsWindowVisible(h)) ShowWindow(h, SW_SHOWNOACTIVATE);
    }
    return TRUE;
}
static void WineHide(void) { if (g.hideWine) EnumWindows(WineTbEnum, 1); }
static void WineShow(void) { EnumWindows(WineTbEnum, 0); }

/* ---- hover preview ---- */
static HBITMAP g_pvBmp; static HWND g_pvTarget; static int g_pvItem = -1;
static HBITMAP CapThumb(HWND t, int tw, int th) {
    RECT r; int w, h, fw, fh; HDC sdc, mdc, tdc; HBITMAP big, nb, ob1, ob2, res = NULL; BOOL ok = FALSE;
    if (!GetWindowRect(t, &r)) return NULL;
    w = r.right - r.left; h = r.bottom - r.top; if (w < 1 || h < 1 || tw < 8 || th < 8) return NULL; if (w > 1920) w = 1920; if (h > 1080) h = 1080;
    sdc = GetDC(NULL); mdc = CreateCompatibleDC(sdc); tdc = CreateCompatibleDC(sdc);
    big = CreateCompatibleBitmap(sdc, w, h); nb = CreateCompatibleBitmap(sdc, tw, th);
    if (mdc && tdc && big && nb) {
        ob1 = (HBITMAP)SelectObject(mdc, big); ob2 = (HBITMAP)SelectObject(tdc, nb);
        FillRectC(tdc, 0, 0, tw, th, Blend(P.panel, K_, 20)); FillRectC(mdc, 0, 0, w, h, K_);
        ok = PrintWindow(t, mdc, 0);
        if (ok) {
            fw = tw; fh = fw * h / w; if (fh > th) { fh = th; fw = fh * w / h; }
            SetStretchBltMode(tdc, HALFTONE); SetBrushOrgEx(tdc, 0, 0, NULL);
            StretchBlt(tdc, (tw - fw) / 2, (th - fh) / 2, fw, fh, mdc, 0, 0, w, h, SRCCOPY);
        } else DrawIconEx(tdc, tw / 2 - 16, th / 2 - 16, MWinIcon(t), 32, 32, 0, NULL, DI_NORMAL);
        SelectObject(mdc, ob1); SelectObject(tdc, ob2); res = nb; nb = NULL;
    }
    if (nb) DeleteObject(nb); if (big) DeleteObject(big); if (mdc) DeleteDC(mdc); if (tdc) DeleteDC(tdc); ReleaseDC(NULL, sdc);
    return res;
}
static void PvCapture(HWND t) { HBITMAP b = CapThumb(t, g.pvW, g.pvH); if (b) { if (g_pvBmp) DeleteObject(g_pvBmp); g_pvBmp = b; } }
static void PvHide(void) { if (g_pv) ShowWindow(g_pv, SW_HIDE); g_pvItem = -1; g_pvTarget = NULL; }
static void PvShow(int item) {
    RECT ir, sr; int W = g.pvW + 12, H = g.pvH + 34, x, y, sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND t; if (!g_pv || item < 0 || g_it[item].type != IT_RUN) return;
    t = g_run[g_it[item].idx].h; if (!IsWindow(t)) return;
    g_pvTarget = t; g_pvItem = item; PvCapture(t);
    ItemRect(item, &ir); OffsetRect(&ir, g_tbRect.left, g_tbRect.top); sr = ir;
    if (g.tbPos == 0) { x = (sr.left + sr.right - W) / 2; y = g_tbRect.top - H - 8; }
    else if (g.tbPos == 1) { x = (sr.left + sr.right - W) / 2; y = g_tbRect.bottom + 8; }
    else if (g.tbPos == 2) { x = g_tbRect.right + 8; y = (sr.top + sr.bottom - H) / 2; }
    else { x = g_tbRect.left - W - 8; y = (sr.top + sr.bottom - H) / 2; }
    if (x < 4) x = 4; if (x + W > sw - 4) x = sw - 4 - W; if (y < 4) y = 4; if (y + H > sh - 4) y = sh - 4 - H;
    SetWindowPos(g_pv, HWND_TOPMOST, x, y, W, H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_pv, NULL, FALSE);
}
static LRESULT CALLBACK PvProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: {
        PAINTSTRUCT ps; RECT rc, hd; HDC dc = BeginPaint(h, &ps); int idx;
        GetClientRect(h, &rc);
        PaintSurface(dc, &rc, P.panel, Blend(P.panel, K_, 12), g.effPv);
        if (g.pShadow) EdgeShadow(dc, &rc);
        idx = g_pvItem;
        if (g_pvTarget && IsWindow(g_pvTarget)) {
            wchar_t t[96]; GetWindowTextW(g_pvTarget, t, 96);
            DrawIconEx(dc, 8, 5, MWinIcon(g_pvTarget), 16, 16, 0, NULL, DI_NORMAL);
            hd = rc; hd.left = 30; hd.right -= 8; hd.bottom = 26; SelectObject(dc, g_fTb); Txt(dc, t, &hd, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS, P.panelTxt);
        }
        if (g_pvBmp) { HDC m2 = CreateCompatibleDC(dc); HBITMAP o = (HBITMAP)SelectObject(m2, g_pvBmp); BitBlt(dc, 6, 28, g.pvW, g.pvH, m2, 0, 0, SRCCOPY); SelectObject(m2, o); DeleteDC(m2); }
        (void)idx; EndPaint(h, &ps); return 0; }
    case WM_LBUTTONUP: if (g_pvTarget) { ActivateRun(g_pvTarget); } PvHide(); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ---- reveal bar + floating icon ---- */
static RECT g_rvRect;
static void RvPlace(void) {
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN), horiz = g.rvPos < 2, th = g.rvH, len = g.rvW, arm = g.rvArm, W, H, x, y, cx, cy;
    HRGN rg = NULL, a, b;
    if (!g_rv) return;
    if (horiz) { cy = (g.rvPos == 0); cx = (g.rvAlign == 2); } else { cx = (g.rvPos == 3); cy = (g.rvAlign == 2); }
    if (g.rvL) {            /* L at the screen corner: one arm along the width, one along the height */
        int hl = horiz ? len : arm, vl = horiz ? arm : len, x0, y0, x1, y1;
        if (hl > sw) hl = sw; if (vl > sh) vl = sh;
        W = hl > th ? hl : th; H = vl > th ? vl : th; x = cx ? sw - W : 0; y = cy ? sh - H : 0;
        x0 = cx ? W - hl : 0; y0 = cy ? H - th : 0; x1 = cx ? W - th : 0; y1 = cy ? H - vl : 0;
        rg = CreateRectRgn(x0, y0, x0 + hl, y0 + th); a = CreateRectRgn(x1, y1, x1 + th, y1 + vl);
        if (rg && a) CombineRgn(rg, rg, a, RGN_OR); if (a) DeleteObject(a);
    } else if (horiz) {
        W = len > sw ? sw : len; H = th; x = g.rvAlign == 0 ? 0 : (g.rvAlign == 1 ? (sw - W) / 2 : sw - W); y = cy ? sh - H : 0;
    } else {
        W = th; H = len > sh ? sh : len; x = cx ? sw - W : 0; y = g.rvAlign == 0 ? 0 : (g.rvAlign == 1 ? (sh - H) / 2 : sh - H);
    }
    (void)b;
    SetRect(&g_rvRect, x, y, x + W, y + H);
    SetWindowPos(g_rv, HWND_TOPMOST, x, y, W, H, SWP_NOACTIVATE);
    if (rg) { if (!SetWindowRgn(g_rv, rg, TRUE)) DeleteObject(rg); } else SetWindowRgn(g_rv, NULL, TRUE);
    SetLayeredWindowAttributes(g_rv, 0, (BYTE)(g.rvOpacity * 255 / 100), LWA_ALPHA);
    InvalidateRect(g_rv, NULL, FALSE);
}
static LRESULT CALLBACK RvProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) { PAINTSTRUCT ps; RECT rc; HDC dc = BeginPaint(h, &ps); GetClientRect(h, &rc); FillRectC(dc, 0, 0, rc.right, rc.bottom, (COLORREF)g.rvCol); EndPaint(h, &ps); return 0; }
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    return DefWindowProcW(h, m, w, l);
}
static void Reveal(void);
static LRESULT CALLBACK FlProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; RECT rc; HDC dc = BeginPaint(h, &ps); GetClientRect(h, &rc);
        FillRectC(dc, 0, 0, rc.right, rc.bottom, P.acc); DrawStartBtn(dc, &rc, P.accTxt, P.acc); EndPaint(h, &ps); return 0;
    }
    if (m == WM_LBUTTONUP) { Reveal(); return 0; }
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    return DefWindowProcW(h, m, w, l);
}
static void FlPlace(void) {
    int s = ScaleI(44), sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN), x = 8, y = sh - s - 8; HRGN rg;
    if (!g_fl) return;
    if (g.tbPos == 1) y = 8; if (g.tbPos == 3) x = sw - s - 8;
    if (g.tbAlign == 2 && g.tbPos < 2) x = sw - s - 8;
    SetWindowPos(g_fl, HWND_TOPMOST, x, y, s, s, SWP_NOACTIVATE);
    rg = CreateEllipticRgn(0, 0, s + 1, s + 1); if (rg && !SetWindowRgn(g_fl, rg, TRUE)) DeleteObject(rg);
}
static void RvShow(int on) {
    if (!g_rv) return;
    if (on) { if (!IsWindowVisible(g_rv)) { RvPlace(); ShowWindow(g_rv, SW_SHOWNOACTIVATE); } } else if (IsWindowVisible(g_rv)) ShowWindow(g_rv, SW_HIDE);
}
static void FlShow(int on) {
    if (!g_fl) return;
    if (on) { if (!IsWindowVisible(g_fl)) { FlPlace(); ShowWindow(g_fl, SW_SHOWNOACTIVATE); } } else if (IsWindowVisible(g_fl)) ShowWindow(g_fl, SW_HIDE);
}

/* ---- show / hide animation: 0 blink, 1 pop (fade), 2 slide from edge, 3 slide from left ---- */
static void TbPutRect(const RECT *r) { SetWindowPos(g_tb, TbZ(), r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOACTIVATE); }
static void TbSetAlpha(int a) { SetLayeredWindowAttributes(g_tb, 0, (BYTE)a, LWA_ALPHA); }
static void TbSetShown(int show) {
    if (!g_tb || show == g_tbShown) return;
    g_tbShown = show;
    if (AnimMs(200) <= 0) {                                   /* animations off: switch instantly */
        g_tbAnimOn = 0; KillTimer(g_msg, MT_ANIM); TbSetAlpha(g.tbOpacity * 255 / 100);
        if (show) { TbPutRect(&g_tbRect); ShowWindow(g_tb, SW_SHOWNOACTIVATE); } else ShowWindow(g_tb, SW_HIDE);
        UpdateWorkArea(); return;
    }
    g_tbAnimIn = show; g_tbAnimOn = 1; g_tbAnimT0 = GetTickCount();
    if (show) { TbPutRect(&g_tbRect); ShowWindow(g_tb, SW_SHOWNOACTIVATE); }
    SetTimer(g_msg, MT_ANIM, 16, NULL);
    UpdateWorkArea();
}
static void TbAnimStep(void) {
    int dur = AnimMs(200), base = g.tbOpacity * 255 / 100, sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN), t, p, mode = g.rvAnim, off, th;
    DWORD el = GetTickCount() - g_tbAnimT0; RECT r = g_tbRect;
    if (dur < 1) dur = 1;
    t = el >= (DWORD)dur ? 100 : (int)(el * 100 / dur); p = g_tbAnimIn ? t : 100 - t; off = 100 - p; th = g_tbThick;
    if (mode == 0) { int ph = (int)(el / 45) % 2; TbSetAlpha(g_tbAnimIn ? (ph ? base : base / 5) : (ph ? base / 5 : base)); }
    else if (mode == 1) TbSetAlpha(base * p / 100);
    else {
        if (mode == 3) OffsetRect(&r, -(r.right) * off / 100, 0);
        else if (mode == 4) OffsetRect(&r, (sw - r.left) * off / 100, 0);
        else if (mode == 5) {                                  /* grow out of the screen edge */
            int cur = th * p / 100; if (cur < 1) cur = 1;
            if (g.tbPos == 0) r.top = r.bottom - cur; else if (g.tbPos == 1) r.bottom = r.top + cur; else if (g.tbPos == 2) r.right = r.left + cur; else r.left = r.right - cur;
        }
        else if (g.tbPos == 0) OffsetRect(&r, 0, (sh - r.top) * off / 100);
        else if (g.tbPos == 1) OffsetRect(&r, 0, -(r.bottom) * off / 100);
        else if (g.tbPos == 2) OffsetRect(&r, -(r.right) * off / 100, 0);
        else OffsetRect(&r, (sw - r.left) * off / 100, 0);
        TbPutRect(&r); TbSetAlpha(base);
    }
    if (t >= 100) {
        KillTimer(g_msg, MT_ANIM); g_tbAnimOn = 0;
        if (g_tbAnimIn) { TbPutRect(&g_tbRect); TbSetAlpha(base); } else { ShowWindow(g_tb, SW_HIDE); TbSetAlpha(base); }
    }
}
static void Reveal(void) {
    g_revealed = 1; g_revUntil = GetTickCount() + g.rvSecs * 1000; RvShow(0); FlShow(0); TbSetShown(1);
}

/* ---- full-screen / maximized detection and reveal logic ---- */
static BOOL FsDetect(void) {
    HWND fg = GetForegroundWindow(); RECT r, wa; wchar_t cls[48], exe[40]; DWORD pid = 0; int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN), full, maxed;
    if (!fg || !IsWindowVisible(fg) || IsIconic(fg)) return FALSE;
    GetWindowThreadProcessId(fg, &pid); if (pid == GetCurrentProcessId()) return FALSE;
    cls[0] = 0; GetClassNameW(fg, cls, 48);
    if (!wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW") || !wcscmp(cls, L"Shell_TrayWnd")) return FALSE;
    MWinExe(fg, exe, 40); if (ListHasExe(L_RVX, exe)) return FALSE;
    if (!GetWindowRect(fg, &r)) return FALSE;
    full = r.left <= 0 && r.top <= 0 && r.right >= sw && r.bottom >= sh;
    GetWork(&wa);
    maxed = IsZoomed(fg) || (r.left <= wa.left + 2 && r.right >= wa.right - 2 && r.bottom >= wa.bottom - 2 && r.top <= wa.top + 60 && (r.right - r.left) > sw / 2);
    return g.fsApplies == 0 ? full : (g.fsApplies == 1 ? maxed : (full || maxed));
}
static void TbLogic(void) {
    POINT pt; RECT tr; BOOL inRv = FALSE, inTb; DWORD now = GetTickCount();
    g_fsActive = FsDetect();
    g_wantHide = (g_fsActive && g.fsMode >= 2) || g.rvAlways;
    GetCursorPos(&pt);
    if (!g_wantHide) {
        if (!g_tbShown) { g_revealed = 0; TbSetShown(1); }
        RvShow(0); FlShow(0); g_revealed = 0; UpdateWorkArea(); return;
    }
    if (!g_revealed) {
        if (g_tbShown && !g_tbAnimOn) TbSetShown(0);
        RvShow(g.rvOn); FlShow(g_fsActive && g.fsMode == 2);
        if (g.rvOn) { RECT q = g_rvRect; InflateRect(&q, 2, 2); inRv = PtInRect(&q, pt); if (inRv && g.rvL) inRv = (WindowFromPoint(pt) == g_rv); }
        if (inRv) Reveal();
    } else {
        tr = g_tbRect; InflateRect(&tr, 6, 6); inTb = PtInRect(&tr, pt);
        if (inTb || g_stOpen || g_pvItem >= 0) g_revUntil = now + g.rvSecs * 1000;
        else if ((long)(now - g_revUntil) > 0) { g_revealed = 0; TbSetShown(0); RvShow(g.rvOn); }
    }
    UpdateWorkArea();
}

/* ---- taskbar window ---- */
static HBITMAP g_tbBmp; static int g_tbBw, g_tbBh;
static void TbRelayout(void) {
    MakeFonts(); BuildPal(); TbLayout();
    if (g_tb) {
        int w = g_tbRect.right - g_tbRect.left, h = g_tbRect.bottom - g_tbRect.top;
        if (g_tbShown && !g_tbAnimOn) TbPutRect(&g_tbRect);
        if (g.tbRound) RoundRgn(g_tb, w, h, ScaleI(10)); else SetWindowRgn(g_tb, NULL, TRUE);
        TbSetAlpha(g.tbOpacity * 255 / 100);
        InvalidateRect(g_tb, NULL, FALSE);
    }
    RvPlace(); FlPlace(); UpdateWorkArea();
}
static void TbContext(int it, int sub) {
    HMENU mn = CreatePopupMenu(); POINT pt; int cmd; GetCursorPos(&pt);
    if (!mn) return;
    if (it >= 0 && g_it[it].type == IT_RUN) {
        AppendMenuW(mn, MF_STRING, 1, L"Restore"); AppendMenuW(mn, MF_STRING, 2, L"Minimize"); AppendMenuW(mn, MF_STRING, 3, L"Close window");
        AppendMenuW(mn, MF_STRING, 4, L"Pin to taskbar"); AppendMenuW(mn, MF_SEPARATOR, 0, NULL);
    } else if (it >= 0 && g_it[it].type == IT_PIN) {
        AppendMenuW(mn, MF_STRING, 5, L"Unpin from taskbar"); AppendMenuW(mn, MF_STRING, 6, L"Move earlier"); AppendMenuW(mn, MF_STRING, 7, L"Move later"); AppendMenuW(mn, MF_SEPARATOR, 0, NULL);
    } else if (it >= 0 && g_it[it].type == IT_TRAY) { AppendMenuW(mn, MF_STRING, 8, L"Remove tray icon"); AppendMenuW(mn, MF_SEPARATOR, 0, NULL); }
    AppendMenuW(mn, MF_STRING, 20, L"Taskbar settings"); AppendMenuW(mn, MF_STRING, 21, L"Show desktop");
    AppendMenuW(mn, MF_STRING | (g.dkOn ? MF_CHECKED : 0), 22, L"Desktop mode"); AppendMenuW(mn, MF_STRING, 24, L"Task manager"); AppendMenuW(mn, MF_STRING, 23, L"Exit Monty shell");
    SetForegroundWindow(g_tb);
    cmd = TrackPopupMenu(mn, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_tb, NULL);
    DestroyMenu(mn);
    if (cmd >= 1 && cmd <= 4 && it >= 0) {
        RunItem *r = &g_run[g_it[it].idx];
        if (cmd == 1) ShowWindow(r->h, SW_RESTORE); else if (cmd == 2) ShowWindow(r->h, SW_MINIMIZE); else if (cmd == 3) PostMessageW(r->h, WM_CLOSE, 0, 0);
        else { wchar_t p[MAX_PATH]; MWinPath(r->h, p, MAX_PATH); if (p[0]) ListAdd(L_TB, p); ApplyAll(); }
    } else if (cmd >= 5 && cmd <= 7 && it >= 0) {
        int i = g_it[it].idx;
        if (cmd == 5) ListRemove(L_TB, i);
        else { int j = cmd == 6 ? i - 1 : i + 1; if (j >= 0 && j < g_nL[L_TB]) { LItem t = g_L[L_TB][i]; g_L[L_TB][i] = g_L[L_TB][j]; g_L[L_TB][j] = t; SaveList(L_TB); } }
        ApplyAll();
    } else if (cmd == 8 && it >= 0) { ListRemove(L_TRAY, sub < g_nL[L_TRAY] ? sub : 0); ApplyAll(); }
    else if (cmd == 20) OpenSettings();
    else if (cmd == 21) ShowDesktopToggle();
    else if (cmd == 22) { g.dkOn = !g.dkOn; MWr(L"cfg", L"dkOn", g.dkOn); DesktopToggle(); }
    else if (cmd == 23) QuitShell();
    else if (cmd == 24) ShellExecuteW(NULL, L"open", L"taskmgr.exe", NULL, NULL, SW_SHOWNORMAL);
}
static void TbClick(int it, int sub) {
    Item *t; if (it < 0) return;
    t = &g_it[it];
    switch (t->type) {
    case IT_START: StartToggle(); break;
    case IT_PIN: LaunchPath(g_L[L_TB][t->idx].p); break;
    case IT_RUN: PvHide(); ActivateRun(g_run[t->idx].h); break;
    case IT_DESK: ShowDesktopToggle(); break;
    case IT_VIEW: TvToggle(); break;
    case IT_TRAY: if (g.trayGrid) TgToggle(); else if (sub >= 0 && sub < g_nL[L_TRAY]) LaunchPath(g_L[L_TRAY][sub].p); break;
    }
    g_tbDown = -1;
}
static LRESULT CALLBACK TbProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: {
        PAINTSTRUCT ps; RECT rc; HDC dc = BeginPaint(h, &ps), mem; HBITMAP ob; GetClientRect(h, &rc);
        mem = CreateCompatibleDC(dc);
        if (mem) {
            if (!g_tbBmp || g_tbBw != rc.right || g_tbBh != rc.bottom) { if (g_tbBmp) DeleteObject(g_tbBmp); g_tbBmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); g_tbBw = rc.right; g_tbBh = rc.bottom; }
            ob = (HBITMAP)SelectObject(mem, g_tbBmp); TbPaint(mem, rc.right, rc.bottom);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, ob); DeleteDC(mem);
        }
        EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: {
        int sub, i = TbHit((short)LOWORD(l), (short)HIWORD(l), &sub);
        if (i != g_tbHot) {
            TRACKMOUSEEVENT te; te.cbSize = sizeof te; te.dwFlags = TME_LEAVE; te.hwndTrack = h; te.dwHoverTime = 0; TrackMouseEvent(&te);
            g_tbHot = i; InvalidateRect(h, NULL, FALSE);
            KillTimer(g_msg, MT_PV);
            if (i >= 0 && g_it[i].type == IT_RUN && g.tbPreview) SetTimer(g_msg, MT_PV, g_pvItem >= 0 ? 60 : 450, NULL); else if (g_pvItem >= 0 && !(i >= 0 && g_it[i].type == IT_RUN)) PvHide();
        }
        return 0; }
    case WM_MOUSELEAVE: g_tbHot = -1; InvalidateRect(h, NULL, FALSE); KillTimer(g_msg, MT_PV); SetTimer(g_msg, MT_PVREF, 250, NULL); return 0;
    case WM_LBUTTONDOWN: g_tbDown = g_tbHot; if (g.tbZ == 1) SetWindowPos(h, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); InvalidateRect(h, NULL, FALSE); return 0;
    case WM_LBUTTONUP: { int sub, i = TbHit((short)LOWORD(l), (short)HIWORD(l), &sub); if (i >= 0 && i == g_tbDown) TbClick(i, sub); g_tbDown = -1; InvalidateRect(h, NULL, FALSE); return 0; }
    case WM_RBUTTONUP: { int sub, i = TbHit((short)LOWORD(l), (short)HIWORD(l), &sub); TbContext(i, sub); return 0; }
    }
    return DefWindowProcW(h, m, w, l);
}
