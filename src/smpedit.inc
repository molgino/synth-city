// Sample-Editor (SMPL, Regler EDIT auf der Seite QUELLE): Wellenform des Sample-Slots mit Zoom, Marken
// START / LOOP / ENDE und Einrasten auf Nulldurchgaenge. Die Marken schreiben die Spur-Parameter STRT/END (grob)
// + S.FIN/E.FIN (fein), LSTR/L.FIN und ZERO - also bleibt alles im Projekt/Preset und ist pro Step lockbar.
// Eingebunden in main.cpp nach wtedit.inc.
static bool sedOpen = false;
static int sedTrack = 0, sedMark = 0;   // 0 START, 1 LOOP, 2 ENDE
static int sedZoom = 0;                 // 0 = ganzes Sample, je Stufe Faktor 2
static double sedCenter = 0;            // Mitte der Ansicht (Sample)
static uint8_t sedOrig[8];              // fuer Abbruch: S3 S4 S5 X3 X4 X5 X6 X7

static const Sample* sedSmp() { return sampleGet(curP().p[sedTrack][P_S0]); }
static int sedParamC(int m) { return m == 0 ? P_S3 : (m == 1 ? P_X3 : P_S4); }
static int sedParamF(int m) { return m == 0 ? P_X4 : (m == 1 ? P_X6 : P_X5); }
// Sample-Position einer Marke (wie die Engine sie spielt, mit ZERO)
static int sedPos(int m) {
    const Sample* sm = sedSmp(); if (!sm) return 0;
    const uint8_t* p = curP().p[sedTrack];
    int a = smpPoint(sm, p[P_S3], p[P_X4]), e = smpPoint(sm, p[P_S4], p[P_X5]);
    bool z = p[P_X7] != 0;
    if (m == 0) return z ? smpZero(sm, a) : a;
    if (m == 2) return z ? smpZero(sm, e) : e;
    if (!p[P_X3] && !p[P_X6]) return z ? smpZero(sm, a) : a;   // LSTR 0 = ab START
    int l = smpPoint(sm, p[P_X3], p[P_X6]);
    return z ? smpZero(sm, l) : l;
}
// Marke auf eine Sample-Position setzen: Grob- und Feinwert so, dass smpPoint() moeglichst genau trifft
static void sedSet(int m, double pos) {
    const Sample* sm = sedSmp(); if (!sm || sm->len < 2) return;
    if (pos < 0) pos = 0;
    if (pos > sm->len - 1) pos = sm->len - 1;
    double f = pos / (sm->len - 1) * 127.0;
    int c = (int)f, fi = (int)((f - c) * 128.0 + 0.5);
    if (fi >= 128) { c++; fi = 0; }
    if (c >= 127) { c = 127; fi = 0; }
    if (m == 1 && c == 0 && fi == 0) fi = 1;   // LSTR 0/0 heisst "ab START"
    lockA(); curP().p[sedTrack][sedParamC(m)] = (uint8_t)c; curP().p[sedTrack][sedParamF(m)] = (uint8_t)fi; unlockA();
}
static double sedSpan() { const Sample* sm = sedSmp(); double n = sm ? sm->len : 1; double s = n / (double)(1 << sedZoom); return s < 64 ? 64 : s; }
static void sedFollow() {   // Ansicht folgt der aktiven Marke
    const Sample* sm = sedSmp(); if (!sm) return;
    double span = sedSpan(), p = sedPos(sedMark);
    if (p < sedCenter - span * 0.4 || p > sedCenter + span * 0.4) sedCenter = p;
    if (sedCenter < span / 2) sedCenter = span / 2;
    if (sedCenter > sm->len - span / 2) sedCenter = sm->len - span / 2;
    if (span >= sm->len) sedCenter = sm->len / 2.0;
}
static void openSmpEditor() {
    sedTrack = track;
    if (!sedSmp()) { toast(TR("ERST EIN SAMPLE WÄHLEN (X AUF SMPL)", "PICK A SAMPLE FIRST (X ON SMPL)")); return; }
    sedOpen = true; sedMark = 0; sedZoom = 0;
    static const int ps[8] = {P_S3, P_S4, P_S5, P_X3, P_X4, P_X5, P_X6, P_X7};
    for (int i = 0; i < 8; i++) sedOrig[i] = curP().p[sedTrack][ps[i]];
    sedCenter = sedSmp()->len / 2.0;
}
static void sedClose(bool keep) {
    if (!keep) {
        static const int ps[8] = {P_S3, P_S4, P_S5, P_X3, P_X4, P_X5, P_X6, P_X7};
        lockA(); for (int i = 0; i < 8; i++) curP().p[sedTrack][ps[i]] = sedOrig[i]; unlockA();
    }
    sedOpen = false;
}
static void sedInput(Input& I, Uint32 now, float dt) {
    const Sample* sm = sedSmp();
    if (!sm) { sedClose(false); return; }
    if (I.pressed(BT_R1)) sedMark = (sedMark + 1) % 3;
    if (I.pressed(BT_L1)) sedMark = (sedMark + 2) % 3;
    if (I.repeat(BT_UP, now, 250, 120) && sedZoom < 14) sedZoom++;
    if (I.repeat(BT_DOWN, now, 250, 120) && sedZoom > 0) sedZoom--;
    // links/rechts: um 1 Pixel der Ansicht (gehalten schneller), X/Y: fein (1/8 Pixel, mind. 1 Sample)
    double px = sedSpan() / 600.0;
    int dx = (I.repeat(BT_RIGHT, now, 220, 30) ? 1 : 0) - (I.repeat(BT_LEFT, now, 220, 30) ? 1 : 0);
    if (dx) { double st = px * (I.heldMs(I.down[BT_RIGHT] ? BT_RIGHT : BT_LEFT, now) > 900 ? 8 : 1); sedSet(sedMark, sedPos(sedMark) + dx * (st < 1 ? 1 : st)); }
    int fx = (I.repeat(BT_X, now, 220, 40) ? 1 : 0) - (I.repeat(BT_Y, now, 220, 40) ? 1 : 0);
    if (fx) { double st = px / 8; sedSet(sedMark, sedPos(sedMark) + fx * (st < 1 ? 1 : st)); }
    if (fabsf(I.rx) > 0.3f) sedSet(sedMark, sedPos(sedMark) + I.rx * px * 600.0 * dt);   // rechter Stick: schieben
    if (I.pressed(BT_SELECT)) { lockA(); uint8_t& z = curP().p[sedTrack][P_X7]; z = !z; unlockA(); toast(curP().p[sedTrack][P_X7] ? TR("ZERO: AN (NULLDURCHGÄNGE)", "ZERO: ON (ZERO CROSSINGS)") : TR("ZERO: AUS", "ZERO: OFF")); }
    if (I.pressed(BT_START)) { lockA(); uint8_t& l = curP().p[sedTrack][P_S5]; l = !l; unlockA(); toast(curP().p[sedTrack][P_S5] ? "LOOP: AN" : "LOOP: AUS"); }
    if (I.pressed(BT_A)) { lockA(); eng.preview(sedTrack, nullptr); unlockA(); }
    if (I.pressed(BT_B)) sedClose(true);
    sedFollow();
}
static void drawSmpEditor() {
    const Sample* sm = sedSmp();
    if (!sm) return;
    int x0 = 6, y0 = 34, w = 628, h = 412;
    rrect(x0, y0, w, h, 12, C_CYAN);
    rrect(x0 + 2, y0 + 2, w - 4, h - 4, 11, C_BG);
    char b[128];
    text(x0 + 14, y0 + 8, TR("SAMPLE-EDITOR", "SAMPLE EDITOR"), C_CYAN, 2);
    snprintf(b, sizeof b, "%03d %.40s  %s %.2fs", curP().p[sedTrack][P_S0] + 1, sampleName(curP().p[sedTrack][P_S0]), sm->dataR ? "ST" : "MO", sm->len / (float)(sm->rate > 0 ? sm->rate : 44100));
    text(x0 + w - 14 - textW(b), y0 + 14, b, C_DIM);
    // Wellenform
    int wx = x0 + 14, wy = y0 + 48, ww = 600, wh = 260, cy = wy + wh / 2;
    rrect(wx - 4, wy - 4, ww + 8, wh + 8, 6, C_PANEL);
    double span = sedSpan(), from = sedCenter - span / 2;
    if (from < 0) from = 0;
    int a = sedPos(0), l = sedPos(1), e = sedPos(2);
    bool loop = curP().p[sedTrack][P_S5] != 0;
    int lo = a < e ? a : e, hi = a < e ? e : a;
    for (int c = 0; c < ww; c++) {
        int s0 = (int)(from + span * c / ww), s1 = (int)(from + span * (c + 1) / ww);
        if (s1 <= s0) s1 = s0 + 1;
        if (s0 >= sm->len) break;
        if (s1 > sm->len) s1 = sm->len;
        float mn = 1, mx = -1; int stp = (s1 - s0) > 256 ? (s1 - s0) / 256 : 1;
        for (int i = s0; i < s1; i += stp) { float v = sm->data[i]; if (sm->dataR) v = 0.5f * (v + sm->dataR[i]); if (v < mn) mn = v; if (v > mx) mx = v; }
        bool in = s0 >= lo && s0 <= hi, inLoop = loop && s0 >= l && s0 <= e;
        Col cc = inLoop ? C_CYAN : (in ? C_ACC : C_OFF2);
        int y1 = cy - (int)(mx * (wh / 2 - 4)), y2 = cy - (int)(mn * (wh / 2 - 4));
        fill(wx + c, y1, 1, y2 - y1 + 1, cc);
    }
    fill(wx, cy, ww, 1, C_DIM);
    // Marken
    struct { int pos; const char* n; Col c; } M[3] = {{a, "S", C_GREEN}, {l, "L", C_CYAN}, {e, "E", C_TRIG}};
    for (int m = 0; m < 3; m++) {
        if (m == 1 && !loop) continue;
        double rel = (M[m].pos - from) / span;
        if (rel < 0 || rel > 1) continue;
        int mx = wx + (int)(rel * ww);
        fill(mx, wy - 4, m == sedMark ? 3 : 1, wh + 8, M[m].c);
        rrect(mx - 8, wy - 18, 16, 14, 3, m == sedMark ? M[m].c : C_PANEL);
        textC(mx, wy - 18, M[m].n, m == sedMark ? C_BG : M[m].c);
    }
    // Uebersicht: ganze Datei, Ausschnitt markiert
    int oy = wy + wh + 10;
    rrect(wx, oy, ww, 10, 3, C_PANEL);
    rrect(wx + (int)(from / sm->len * ww), oy, (int)(span / sm->len * ww) < 4 ? 4 : (int)(span / sm->len * ww), 10, 3, C_DIM);
    // Werte
    int ty = oy + 22;
    static const char* const MN_DE[3] = {"START", "LOOP", "ENDE"}; static const char* const MN_EN[3] = {"START", "LOOP", "END"};
    float rate = sm->rate > 0 ? sm->rate : 44100.f;
    for (int m = 0; m < 3; m++) {
        int p = m == 0 ? a : (m == 1 ? l : e);
        snprintf(b, sizeof b, "%s %.3fs", g_lang ? MN_EN[m] : MN_DE[m], p / rate);
        text(wx + m * 128, ty, b, m == sedMark ? M[m].c : C_DIM);
    }
    snprintf(b, sizeof b, "ZOOM x%d  ZERO %s  LOOP %s", 1 << sedZoom, curP().p[sedTrack][P_X7] ? TR("AN", "ON") : TR("AUS", "OFF"), loop ? TR("AN", "ON") : TR("AUS", "OFF"));
    text(wx + ww - textW(b), ty, b, C_TEXT);
    hint(x0 + 14, y0 + h - 26, TR("[L1/R1]MARKE [</>]SCHIEBEN [X/Y]FEIN [^v]ZOOM [A]HÖREN [SEL]ZERO [START]LOOP [B]FERTIG",
                                  "[L1/R1]MARKER [</>]MOVE [X/Y]FINE [^v]ZOOM [A]LISTEN [SEL]ZERO [START]LOOP [B]DONE"), C_ACC, C_DIM);
}
static void drawSmpEditorIfOpen() { if (sedOpen) drawSmpEditor(); }
