/* mtv.h - all-windows preview panel (close / pin / close-all), tray grid, cascading Programs menu, tile shuffle */
static HWND g_tv, g_tg; static int g_tvN, g_tvCw, g_tvCh, g_tvRows, g_tvHdr, g_tvTb, g_tvHotK, g_tvHotI = -1;
static HWND g_tvWin[16]; static HBITMAP g_tvBmp[16]; static RECT g_tvCloseAll;

/* ---- all-windows preview ---- */
static void TvFree(void) { int i; for (i = 0; i < 16; i++) if (g_tvBmp[i]) { DeleteObject(g_tvBmp[i]); g_tvBmp[i] = NULL; } g_tvN = 0; }
static void TvCard(int i, RECT *r) {
    int col = i / g_tvRows, row = i % g_tvRows, x = 8 + col * (g_tvCw + 8), y = g_tvTb + 8 + row * (g_tvCh + 8);
    SetRect(r, x, y, x + g_tvCw, y + g_tvCh);
}
static void TvBtn(int i, int which, RECT *r) {          /* 0 = close, 1 = pin */
    RECT c; int b = g_tvHdr - 6; TvCard(i, &c);
    SetRect(r, c.right - 3 - b - which * (b + 3), c.top + 3, c.right - 3 - which * (b + 3), c.top + 3 + b);
}
static int TvHit(int x, int y, int *idx) {
    POINT p; int i; p.x = x; p.y = y; *idx = -1;
    if (PtInRect(&g_tvCloseAll, p)) return 4;
    for (i = 0; i < g_tvN; i++) { RECT r; TvBtn(i, 0, &r); if (PtInRect(&r, p)) { *idx = i; return 2; } TvBtn(i, 1, &r); if (PtInRect(&r, p)) { *idx = i; return 3; } TvCard(i, &r); if (PtInRect(&r, p)) { *idx = i; return 1; } }
    return 0;
}
static void TvPlace(void) {
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN), avail, maxRows, cols, W, H, x, y;
    g_tvCw = g.pvW * 80 / 100; g_tvHdr = ScaleI(28); g_tvCh = g_tvHdr + g.pvH * 80 / 100 + 6; g_tvTb = ScaleI(42);
    avail = sh - g_tbThick - 40 - g_tvTb - 16; maxRows = avail / (g_tvCh + 8); if (maxRows < 1) maxRows = 1;
    g_tvRows = g_tvN < maxRows ? (g_tvN ? g_tvN : 1) : maxRows; cols = (g_tvN + g_tvRows - 1) / g_tvRows; if (cols < 1) cols = 1;
    W = cols * (g_tvCw + 8) + 8; if (W < ScaleI(240)) W = ScaleI(240); H = g_tvTb + g_tvRows * (g_tvCh + 8) + 8;
    if (g.tbPos == 3) x = g_tbRect.left - W - 8; else x = sw - W - 8;                 /* right side of the taskbar */
    if (g.tbPos == 0) y = g_tbRect.top - H - 8; else if (g.tbPos == 1) y = g_tbRect.bottom + 8; else y = sh - H - 8;
    if (x < 4) x = 4; if (y < 4) y = 4;
    SetRect(&g_tvCloseAll, W - 8 - ScaleI(110), 6, W - 8, g_tvTb - 6);
    SetWindowPos(g_tv, HWND_TOPMOST, x, y, W, H, SWP_NOACTIVATE);
}
static void TvHide(void) { if (g_tv && IsWindowVisible(g_tv)) ShowWindow(g_tv, SW_HIDE); TvFree(); }
static void TvShow(void) {
    int i; if (!g_tv) return;
    TvFree(); RefreshRun();
    for (i = 0; i < g_nrun && i < 16; i++) { g_tvWin[i] = g_run[i].h; g_tvBmp[i] = CapThumb(g_run[i].h, g_tvCw ? g_tvCw - 6 : 160, g.pvH * 80 / 100); g_tvN++; }
    TvPlace();
    if (g_tvN > 0 && g_tvBmp[0] == NULL) {}
    ShowWindow(g_tv, SW_SHOWNOACTIVATE); InvalidateRect(g_tv, NULL, FALSE);
}
static void TvToggle(void) { if (g_tv && IsWindowVisible(g_tv)) TvHide(); else { g_tvCw = g.pvW * 80 / 100; TvShow(); } }
static void TvRemove(int i) { int k; if (g_tvBmp[i]) DeleteObject(g_tvBmp[i]); for (k = i; k < g_tvN - 1; k++) { g_tvWin[k] = g_tvWin[k + 1]; g_tvBmp[k] = g_tvBmp[k + 1]; } g_tvBmp[g_tvN - 1] = NULL; g_tvN--; if (g_tvN == 0) TvHide(); else TvPlace(); }
static void TvPaint(HDC dc, int w, int h) {
    RECT all, r, q; int i; wchar_t buf[48];
    SetRect(&all, 0, 0, w, h); PaintSurface(dc, &all, P.panel, Blend(P.panel, K_, 10), g.effPv); if (g.pShadow) EdgeShadow(dc, &all);
    SelectObject(dc, g_fTbB); _snwprintf(buf, 47, L"Open windows (%d)", g_tvN); buf[47] = 0;
    SetRect(&q, 14, 0, w - ScaleI(130), g_tvTb); Txt(dc, buf, &q, DT_SINGLELINE | DT_VCENTER, P.panelTxt); SelectObject(dc, g_fTb);
    RRect(dc, &g_tvCloseAll, ScaleI(8), (g_tvHotK == 4) ? RGB(225,70,70) : RGB(190,55,55), RGB(190,55,55)); q = g_tvCloseAll; Txt(dc, L"Close all", &q, DT_SINGLELINE | DT_VCENTER | DT_CENTER, W_);
    for (i = 0; i < g_tvN; i++) {
        BOOL hot = (g_tvHotK >= 1 && g_tvHotK <= 3 && g_tvHotI == i), pinned;
        TvCard(i, &r); RRect(dc, &r, ScaleI(8), Blend(P.panel, P.panelTxt, hot ? 18 : 9), Blend(P.panel, P.panelTxt, 22));
        DrawIconEx(dc, r.left + 6, r.top + (g_tvHdr - 16) / 2, MWinIcon(g_tvWin[i]), 16, 16, 0, NULL, DI_NORMAL);
        { wchar_t t[80]; GetWindowTextW(g_tvWin[i], t, 80); q = r; q.left += 28; q.right -= 2 * (g_tvHdr - 3) + 4; q.bottom = r.top + g_tvHdr; Txt(dc, t, &q, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS, P.panelTxt); }
        TvBtn(i, 0, &q); RRect(dc, &q, ScaleI(6), (g_tvHotK == 2 && g_tvHotI == i) ? RGB(225,70,70) : Blend(P.panel, P.panelTxt, 14), Blend(P.panel, P.panelTxt, 14)); DrawUiIcon(dc, UI_ADD, &q, W_);
        { HPEN pn = CreatePen(PS_SOLID, 2, (g_tvHotK == 2 && g_tvHotI == i) ? W_ : P.panelTxt), op; HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH), ob; int cx = (q.left + q.right) / 2, cy = (q.top + q.bottom) / 2;
          FillRectC(dc, q.left + 1, q.top + 1, q.right - 1, q.bottom - 1, (g_tvHotK == 2 && g_tvHotI == i) ? RGB(225,70,70) : Blend(P.panel, P.panelTxt, 14));
          op = (HPEN)SelectObject(dc, pn); ob = (HBRUSH)SelectObject(dc, nb);
          MoveToEx(dc, cx - 4, cy - 4, NULL); LineTo(dc, cx + 5, cy + 5); MoveToEx(dc, cx + 4, cy - 4, NULL); LineTo(dc, cx - 5, cy + 5);
          SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pn); }
        pinned = IsPinned(g_tvWin[i]);
        TvBtn(i, 1, &q); RRect(dc, &q, ScaleI(6), pinned ? P.acc : Blend(P.panel, P.panelTxt, 14), pinned ? P.acc : Blend(P.panel, P.panelTxt, 14));
        { PState s; memset(&s, 0, sizeof s); s.pinned = pinned; DrawPresetGlyph(dc, BT_PIN, &q, 1, pinned ? P.accTxt : P.panelTxt, &s, CurTheme()->pin); }
        if (g_tvBmp[i]) { HDC m = CreateCompatibleDC(dc); HBITMAP o = (HBITMAP)SelectObject(m, g_tvBmp[i]); BitBlt(dc, r.left + 3, r.top + g_tvHdr, g_tvCw - 6, g_tvCh - g_tvHdr - 3, m, 0, 0, SRCCOPY); SelectObject(m, o); DeleteDC(m); }
    }
}
static LRESULT CALLBACK TvProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: {
        PAINTSTRUCT ps; RECT rc; HDC dc = BeginPaint(h, &ps), mem; HBITMAP bm, ob; GetClientRect(h, &rc);
        mem = CreateCompatibleDC(dc); bm = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        if (mem && bm) { ob = (HBITMAP)SelectObject(mem, bm); TvPaint(mem, rc.right, rc.bottom); BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, ob); }
        if (bm) DeleteObject(bm); if (mem) DeleteDC(mem); EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: {
        int idx, k = TvHit((short)LOWORD(l), (short)HIWORD(l), &idx);
        if (k != g_tvHotK || idx != g_tvHotI) { TRACKMOUSEEVENT te; te.cbSize = sizeof te; te.dwFlags = TME_LEAVE; te.hwndTrack = h; te.dwHoverTime = 0; TrackMouseEvent(&te); g_tvHotK = k; g_tvHotI = idx; InvalidateRect(h, NULL, FALSE); }
        return 0; }
    case WM_MOUSELEAVE: g_tvHotK = 0; g_tvHotI = -1; InvalidateRect(h, NULL, FALSE); return 0;
    case WM_LBUTTONUP: {
        int idx, k = TvHit((short)LOWORD(l), (short)HIWORD(l), &idx);
        if (k == 4) { int i; for (i = 0; i < g_tvN; i++) PostMessageW(g_tvWin[i], WM_CLOSE, 0, 0); TvHide(); }
        else if (k == 2) { PostMessageW(g_tvWin[idx], WM_CLOSE, 0, 0); TvRemove(idx); InvalidateRect(h, NULL, FALSE); }
        else if (k == 3) { SetPin(g_tvWin[idx], !IsPinned(g_tvWin[idx])); InvalidateRect(h, NULL, FALSE); }
        else if (k == 1) { HWND t = g_tvWin[idx]; TvHide(); ActivateRun(t); }
        return 0; }
    }
    return DefWindowProcW(h, m, w, l);
}

/* ---- tray grid (hidden-icons style) ---- */
static int TgCell(void) { return ScaleI(46); }
static void TgPlace(void) {
    int n = g_nL[L_TRAY], cols = n < 4 ? (n ? n : 1) : 4, rows = (n + cols - 1) / cols, W = cols * TgCell() + 16, H = rows * TgCell() + 16, x, y, i, sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN); RECT ir;
    SetRect(&ir, g_tbRect.left, g_tbRect.top, g_tbRect.right, g_tbRect.bottom);
    for (i = 0; i < g_nit; i++) if (g_it[i].type == IT_TRAY) { ItemRect(i, &ir); OffsetRect(&ir, g_tbRect.left, g_tbRect.top); break; }
    if (g.tbPos == 0) { x = ir.left; y = g_tbRect.top - H - 6; } else if (g.tbPos == 1) { x = ir.left; y = g_tbRect.bottom + 6; }
    else if (g.tbPos == 2) { x = g_tbRect.right + 6; y = ir.top; } else { x = g_tbRect.left - W - 6; y = ir.top; }
    if (x + W > sw) x = sw - W - 4; if (y + H > sh) y = sh - H - 4; if (x < 0) x = 0; if (y < 0) y = 0;
    SetWindowPos(g_tg, HWND_TOPMOST, x, y, W, H, SWP_NOACTIVATE);
}
static void TgToggle(void) {
    if (!g_tg) return;
    if (IsWindowVisible(g_tg)) { ShowWindow(g_tg, SW_HIDE); return; }
    if (!g_nL[L_TRAY]) return;
    TgPlace(); ShowWindow(g_tg, SW_SHOWNOACTIVATE); InvalidateRect(g_tg, NULL, FALSE);
}
static LRESULT CALLBACK TgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: {
        PAINTSTRUCT ps; RECT rc, c; HDC dc = BeginPaint(h, &ps); int i, n = g_nL[L_TRAY], cols; GetClientRect(h, &rc);
        PaintSurface(dc, &rc, P.panel, Blend(P.panel, K_, 10), g.effPv); if (g.pShadow) EdgeShadow(dc, &rc);
        cols = n < 4 ? (n ? n : 1) : 4;
        for (i = 0; i < n; i++) { SetRect(&c, 8 + (i % cols) * TgCell(), 8 + (i / cols) * TgCell(), 8 + (i % cols + 1) * TgCell(), 8 + (i / cols + 1) * TgCell()); DrawIc(dc, PathIcon(g_L[L_TRAY][i].p), (c.left + c.right) / 2, (c.top + c.bottom) / 2, ScaleI(26)); }
        EndPaint(h, &ps); return 0; }
    case WM_LBUTTONUP: case WM_RBUTTONUP: {
        int n = g_nL[L_TRAY], cols = n < 4 ? (n ? n : 1) : 4, x = (short)LOWORD(l) - 8, y = (short)HIWORD(l) - 8, i;
        if (x < 0 || y < 0) return 0; i = (y / TgCell()) * cols + x / TgCell(); if (x / TgCell() >= cols || i >= n) return 0;
        if (m == WM_LBUTTONUP) { LaunchPath(g_L[L_TRAY][i].p); ShowWindow(h, SW_HIDE); } else { ListRemove(L_TRAY, i); ApplyAll(); if (g_nL[L_TRAY]) TgPlace(); else ShowWindow(h, SW_HIDE); InvalidateRect(h, NULL, FALSE); }
        return 0; }
    }
    return DefWindowProcW(h, m, w, l);
}

/* ---- cascading Programs menu (Start Menu folders become sub-menus, like Windows 10) ---- */
static wchar_t g_pmPath[480][MAX_PATH]; static int g_npm;
static void BuildProg(HMENU m, const wchar_t *dir, int depth) {
    WIN32_FIND_DATAW fd; wchar_t pat[MAX_PATH], p[MAX_PATH]; HANDLE h; int pass;
    if (depth > 4) return;
    for (pass = 0; pass < 2; pass++) {
        _snwprintf(pat, MAX_PATH - 1, L"%ls\\*", dir); pat[MAX_PATH - 1] = 0; h = FindFirstFileW(pat, &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do {
            BOOL isdir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; size_t n = wcslen(fd.cFileName);
            if (fd.cFileName[0] == L'.') continue;
            _snwprintf(p, MAX_PATH - 1, L"%ls\\%ls", dir, fd.cFileName); p[MAX_PATH - 1] = 0;
            if (pass == 0 && isdir) {
                HMENU sub = CreatePopupMenu();
                if (sub) { BuildProg(sub, p, depth + 1); if (GetMenuItemCount(sub) > 0) AppendMenuW(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, fd.cFileName); else DestroyMenu(sub); }
            } else if (pass == 1 && !isdir && g_npm < 480 && n > 4 && (!_wcsicmp(fd.cFileName + n - 4, L".lnk") || !_wcsicmp(fd.cFileName + n - 4, L".exe")) && ListFind(L_HIDE, p) < 0) {
                wchar_t nm[64]; BaseName(p, nm, 64); wcscpy(g_pmPath[g_npm], p); AppendMenuW(m, MF_STRING, (UINT_PTR)(1000 + g_npm), nm); g_npm++;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
}
static void ProgramsMenu(HWND owner, int x, int y) {
    HMENU m = CreatePopupMenu(); wchar_t d[MAX_PATH]; DWORD n; int cmd;
    if (!m) return;
    g_npm = 0;
    n = GetEnvironmentVariableW(L"ProgramData", d, MAX_PATH); if (n && n < MAX_PATH - 40) { wcscat(d, L"\\Microsoft\\Windows\\Start Menu\\Programs"); BuildProg(m, d, 0); }
    n = GetEnvironmentVariableW(L"APPDATA", d, MAX_PATH); if (n && n < MAX_PATH - 40) { wcscat(d, L"\\Microsoft\\Windows\\Start Menu\\Programs"); BuildProg(m, d, 0); }
    if (GetMenuItemCount(m) == 0) AppendMenuW(m, MF_STRING | MF_GRAYED, 1, L"(no programs found)");
    cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, x, y, 0, owner, NULL);
    DestroyMenu(m);
    if (cmd >= 1000 && cmd < 1000 + g_npm) LaunchPath(g_pmPath[cmd - 1000]);
}

/* ---- shuffle: random size, colour and order of the pinned tiles ---- */
static void ShuffleTiles(void) {
    static const int W2[5] = { 1, 2, 2, 4, 2 }, H2[5] = { 1, 1, 2, 2, 2 }; int i, n = g_nL[L_TILE];
    srand((unsigned)time(NULL) ^ GetTickCount());
    for (i = n - 1; i > 0; i--) { int j = rand() % (i + 1); LItem t = g_L[L_TILE][i]; g_L[L_TILE][i] = g_L[L_TILE][j]; g_L[L_TILE][j] = t; }
    for (i = 0; i < n; i++) {
        int k = rand() % 5; g_L[L_TILE][i].w = W2[k]; g_L[L_TILE][i].h = H2[k];
        g_L[L_TILE][i].col = RGB(40 + rand() % 170, 40 + rand() % 170, 40 + rand() % 170);
    }
    SaveList(L_TILE);
}
