// NEUE OBERFLAECHE (Branch ui-neu, SET -> "Oberflaeche": NEU / KLASSISCH, cfg.ui2)
// Ziele: feste Zonen statt wechselnder Seiten, eine Tasten-Grammatik fuer alles, Listen statt Kachel-Seiten.
//   Kopf      : Pattern, Tempo, Play + Reiter der Ansichten (L2/R2)
//   links     : 8 Spuren (immer sichtbar, Pegel, Mute)        Mitte: Inhalt der Ansicht (eine Liste)
//   unten     : 16 Steps der Spur (immer sichtbar) mit Lock-Spur des gewaehlten Werts
//   ganz unten: Hilfeleiste = was die Tasten JETZT tun (auch mit gehaltenem SELECT / A)
// Ansichten (L2/R2 im Kreis): SEQ, SOUND, MIX, FX A-C, KOMPRESSOR, LIVE, PROJEKT, SONG, SAMPLER, MASTER, SET.
// SEQ und SOUND sind neue Listen-Ansichten; der Fokus zeigt immer auf einen echten Regler (page/slot), dadurch
// funktionieren Wert aendern, Locks (A halten), Listen (X auf Ziel/Engine/Sample ...) und Touch wie bisher.
// Tasten: ^v Wert waehlen, <> Step, L1/R1 Spur, A Trig (halten = Lock), B Vorhoeren (halten = Hilfe), X/Y Wert,
//         SELECT tippen = MEHR-Menue, SELECT + ... = Kurzbefehle (siehe Hilfeleiste).

enum { V2_NONE = -1, V2_SEQ = 0, V2_SOUND = 1, V2_FX = 2, V2_VIEWS = 3 };
static int v2View = V2_NONE;
static int v2Foc[V2_VIEWS] = {1, 2, 1};   // SOUND: erstes Klang-Makro (nicht ENGINE)
static float v2Scroll[V2_VIEWS] = {0, 0, 0};
// Ansichten: SEQ, SOUND, FX (Busse + Kompressor + Master in einer Liste), dann die eigenstaendigen Werkzeuge
static const int V2_RING[] = {-10, -11, -12, PG_MIX, PG_LIVE, PG_PRJ, PG_SONG, PG_SMPR, PG_SYS};   // -10 SEQ, -11 SOUND, -12 FX
static const int V2_FX_P[] = {PG_FX, PG_FXB, PG_FXC, PG_CMP, PG_MST};
static const int V2_SEQ_P[] = {PG_TRG, PG_SEQ, PG_ARP};
enum { V2_RING_N = sizeof V2_RING / sizeof *V2_RING };
bool v2Active() {   // Listen-Ansicht (auch fuer die globalen Effekt-Seiten)
    if (trackPage(page)) return true;
    for (int p : V2_FX_P) if (p == page) return true;
    return false;
}
static int v2RingIndex();
// Ansicht (Index in V2_RING), in der eine Seite liegt: SEQ, SOUND, EFFEKTE oder eine eigene Ansicht
static int v2RingOfPage(int p) {
    for (int i = 3; i < V2_RING_N; i++) if (V2_RING[i] == p) return i;
    for (int q : V2_FX_P) if (q == p) return 2;
    for (int q : V2_SEQ_P) if (q == p) return 0;
    return 1;   // alles Uebrige ist eine Klangseite -> SOUND
}
int v2TabN() { return V2_RING_N; }
const char* v2TabName(int i) { int p = V2_RING[(i % V2_RING_N + V2_RING_N) % V2_RING_N]; return p == -10 ? "SEQ" : p == -11 ? "SOUND" : p == -12 ? TR("EFFEKTE", "FX") : PAGE_NAMES[p]; }
int v2TabPage(int i) { int p = V2_RING[(i % V2_RING_N + V2_RING_N) % V2_RING_N]; return p < 0 ? -1 : p; }
int v2TabCur() { return v2RingIndex(); }

// ---- Listen-Modell: Gruppen (= Bedienseiten) mit ihren Reglern; SOUND beginnt mit 8 Makros
struct V2It { bool hdr; int pg, k; const char* title; bool mac; int grp; };   // mac = Makro (Abkuerzung), grp = Gruppennummer
static V2It v2It[200]; static int v2N = 0, v2Groups = 0;
static const int V2_SND_P[] = {PG_SRC, PG_SRC2, PG_SRC3, PG_SRC4, PG_SRC5, PG_FLT, PG_FLT2, PG_AMP, PG_INS, PG_SEND, PG_ENV1, PG_ENV2, PG_ENV3, PG_LFO1, PG_LFO2, PG_LFO3};
static bool v2Valid(int p, int k) {
    if (!pageAvail(p)) return false;
    if (!trackPage(p)) {   // globale Seiten (Busse, Kompressor, Master): Regler ueber knobInfo
        int save = page; page = p;
        ParamDef d; int val, base; bool lk; char lb[24];
        bool ok = knobInfo(k, d, val, lk, base, lb, sizeof lb) && d.kind != K_NONE;
        page = save;
        return ok;
    }
    int idx = uiParam(p, k);
    if (idx < 0) return false;
    return paramDef(curP().machine[track] % M_COUNT, idx).kind != K_NONE;
}
// Listeneintrag zu (Seite, Regler): bevorzugt der echte Eintrag, sonst das Makro (ENGINE gibt es nur als Makro)
static int v2FindAny(int pg, int k) {
    for (int i = 0; i < v2N; i++) if (!v2It[i].hdr && !v2It[i].mac && v2It[i].pg == pg && v2It[i].k == k) return i;
    for (int i = 0; i < v2N; i++) if (!v2It[i].hdr && v2It[i].pg == pg && v2It[i].k == k) return i;
    return -1;
}
static bool v2HasPage(int p) { for (int i = 0; i < v2N; i++) if (!v2It[i].hdr && v2It[i].pg == p) return true; return false; }
static int v2FindItem2(int pg, int k) { for (int i = 0; i < v2N; i++) if (!v2It[i].hdr && !v2It[i].mac && v2It[i].pg == pg && v2It[i].k == k) return i; return 0; }
static bool v2Find(int param, int& pg, int& k) {   // Regler zu einem Parameter (fuer die Makros)
    for (int p : V2_SND_P) { if (!pageAvail(p)) continue; for (int i = 0; i < uiSlots(p); i++) if (uiParam(p, i) == param) { pg = p; k = i; return true; } }
    return false;
}
static int v2Key = -1;   // Liste nur neu bauen, wenn sich Ansicht, Spur oder Engine aendert (sonst 2x je Bild)
static void v2BuildNow(int view) {
    v2N = 0; v2Groups = 0;
    bool mac = view == V2_SOUND;
    auto add = [&](bool hdr, int pg, int k, const char* t) { if (v2N < 200) v2It[v2N++] = V2It{hdr, pg, k, t, mac && !hdr, v2Groups ? v2Groups - 1 : 0}; };
    if (view == V2_SOUND) {
        v2Groups++; add(true, -1, -1, TR("MAKROS", "MACROS"));
        for (int k = 0; k < uiSlots(PG_TRG); k++) { int idx = uiParam(PG_TRG, k); if (idx >= 0 && paramDef(curP().machine[track] % M_COUNT, idx).kind == K_MACH) { add(false, PG_TRG, k, nullptr); break; } }
        int mp[8] = {uiParam(PG_SRC, 0), uiParam(PG_SRC, 1), P_FREQ, P_RESO, ENV_ATK[0], ENV_DEC[0], P_DLY, P_REV};
        for (int i = 0; i < 8; i++) { int pg, k; if (mp[i] >= 0 && v2Find(mp[i], pg, k) && v2Valid(pg, k)) add(false, pg, k, nullptr); }
    }
    const int* pl = view == V2_SEQ ? V2_SEQ_P : (view == V2_FX ? V2_FX_P : V2_SND_P);
    int pn = view == V2_SEQ ? 3 : (view == V2_FX ? (int)(sizeof V2_FX_P / sizeof *V2_FX_P) : (int)(sizeof V2_SND_P / sizeof *V2_SND_P));
    for (int i = 0; i < pn; i++) {
        int p = pl[i];
        if (!pageAvail(p)) continue;
        mac = false;
        int first = v2N; v2Groups++; add(true, p, -1, nullptr);
        int ns = trackPage(p) ? uiSlots(p) : 8;
        for (int k = 0; k < ns; k++) if (v2Valid(p, k)) add(false, p, k, nullptr);
        if (v2N == first + 1) { v2N = first; v2Groups--; }   // leere Gruppe weglassen
    }
}
static void v2Build(int view) {
    int key = view * 4096 + track * 64 + (curP().machine[track] % M_COUNT);
    if (key == v2Key && v2N > 0) return;
    v2Key = key;
    v2BuildNow(view);
}
static void v2Dirty() { v2Key = -1; }
static int v2Clamp(int f, int dir) {   // Fokus auf den naechsten Regler (keine Ueberschrift)
    if (v2N == 0) return 0;
    if (f < 0) f = 0;
    if (f >= v2N) f = v2N - 1;
    for (int g = 0; g < v2N && v2It[f].hdr; g++) f = (f + (dir >= 0 ? 1 : v2N - 1)) % v2N;
    return f;
}
static void v2Apply() {   // Fokus -> page/slot (darauf arbeiten editValue, Locks, Listen, Touch)
    int f = v2Foc[v2View];
    if (f >= 0 && f < v2N && !v2It[f].hdr) { page = v2It[f].pg; slot = v2It[f].k; }
}
// Ansicht einstellen; aus page/slot uebernehmen, wenn die alte Oberflaeche/Touch/Listen die Seite gesetzt haben
static void v2Sync() {
    bool fxPage = false;
    for (int p : V2_FX_P) if (p == page) fxPage = true;
    if (!trackPage(page) && !fxPage) { v2View = V2_NONE; return; }
    if (v2View != V2_NONE) {   // passt der Fokus der aktuellen Ansicht noch zu page/slot? (ENGINE steht in SOUND, liegt aber auf TRIG)
        v2Build(v2View);
        int f = v2Foc[v2View];
        if (f >= 0 && f < v2N && !v2It[f].hdr && v2It[f].pg == page && v2It[f].k == slot) return;
    }
    bool seqPage = page == PG_TRG || page == PG_SEQ || page == PG_ARP;
    int want = fxPage ? V2_FX : (seqPage ? V2_SEQ : V2_SOUND);
    if (v2View != want) v2View = want;
    v2Build(v2View);
    int f = v2Foc[v2View];
    if (f < 0 || f >= v2N || v2It[f].hdr || v2It[f].pg != page || v2It[f].k != slot) {   // Fokus passt nicht (Seite von aussen gewaehlt)
        int found = -1;
        for (int i = 0; i < v2N; i++) if (!v2It[i].hdr && v2It[i].pg == page && v2It[i].k == slot) { found = i; if (!(v2View == V2_SOUND && i < 10)) break; }
        if (found < 0) for (int i = 0; i < v2N; i++) if (!v2It[i].hdr && v2It[i].pg == page) { found = i; break; }
        v2Foc[v2View] = v2Clamp(found < 0 ? 0 : found, 1);
    }
    v2Apply();
}
static void v2GoView(int i) {   // Index in V2_RING
    int v = V2_RING[(i % V2_RING_N + V2_RING_N) % V2_RING_N];
    if (v <= -10) {
        v2View = v == -10 ? V2_SEQ : (v == -11 ? V2_SOUND : V2_FX);
        v2Build(v2View);
        v2Foc[v2View] = v2Clamp(v2Foc[v2View], 1);
        v2Apply();
    } else { v2View = V2_NONE; page = v; }
}
static int v2RingIndex() {
    if (v2View >= 0 && v2View < 3) return v2View;
    for (int i = 3; i < V2_RING_N; i++) if (V2_RING[i] == page) return i;
    return trackPage(page) ? 1 : 0;
}
static void v2Step(int dir) { v2GoView(v2RingIndex() + dir); }

// ---- MEHR-Menue (SELECT tippen): Ansicht, Steps/Spur kopieren..., Sound, Aufnahme/Tap
enum { MR_V0 = 0, MR_VN = V2_RING_N, MR_PCOPY = MR_VN, MR_PPASTE, MR_PCLEAR, MR_PADD, MR_PDEL, MR_PNEW, MR_TCOPY, MR_TPASTE, MR_TCLEAR,
       MR_ENGINE, MR_SLOAD, MR_SSAVE, MR_SINIT, MR_REC, MR_TAP, MR_UNDO, MR_REDO, MR_SAVE, MR_COUNT };
static bool mrOpen = false; static int mrSel = 0; static float mrScroll = 0;
static bool mrIsHdr(int i) { (void)i; return false; }
static const char* mrSection(int i) {
    if (i == MR_V0) return TR("ANSICHT", "VIEW");
    if (i == MR_PCOPY) return TR("STEPS (PAGE UNTER DEM CURSOR)", "STEPS (PAGE UNDER CURSOR)");
    if (i == MR_TCOPY) return TR("SPUR", "TRACK");
    if (i == MR_ENGINE) return TR("SOUND DER SPUR", "TRACK SOUND");
    if (i == MR_REC) return TR("SONST", "MORE");
    return nullptr;
}
static void mrLabel(int i, char* l, int ln, char* v, int vn) {
    v[0] = 0;
    if (i < MR_VN) {
        int p = V2_RING[i];
        snprintf(l, ln, "%s", p == -10 ? "SEQ" : p == -11 ? "SOUND" : PAGE_LONG[p]);
        if (i == v2RingIndex()) snprintf(v, vn, "<");
        return;
    }
    switch (i) {
    case MR_PCOPY: snprintf(l, ln, TR("Page kopieren", "Copy page")); break;
    case MR_PPASTE: snprintf(l, ln, TR("Page einfügen", "Paste page")); snprintf(v, vn, "%s", pgClipState ? "" : TR("(leer)", "(empty)")); break;
    case MR_PCLEAR: snprintf(l, ln, TR("Page leeren", "Clear page")); break;
    case MR_PADD: snprintf(l, ln, TR("Page anhängen", "Add page")); snprintf(v, vn, "%d Steps", curP().len[track]); break;
    case MR_PDEL: snprintf(l, ln, TR("Letzte Page entfernen", "Remove last page")); break;
    case MR_PNEW: snprintf(l, ln, TR("Neue Page", "New page")); snprintf(v, vn, "%s", cfg.newPage ? TR("KOPIE", "COPY") : TR("LEER", "EMPTY")); break;
    case MR_TCOPY: snprintf(l, ln, TR("Spur kopieren (Sound + Steps)", "Copy track (sound + steps)")); break;
    case MR_TPASTE: snprintf(l, ln, TR("Spur einfügen", "Paste track")); snprintf(v, vn, "%s", trkClipFrom >= 0 ? "" : TR("(leer)", "(empty)")); break;
    case MR_TCLEAR: snprintf(l, ln, TR("Spur leeren (nur Steps)", "Clear track (steps only)")); break;
    case MR_ENGINE: snprintf(l, ln, TR("Engine wählen ...", "Choose engine ...")); snprintf(v, vn, "%s", MACHINE_NAMES[curP().machine[track] % M_COUNT]); break;
    case MR_SLOAD: snprintf(l, ln, TR("Sound laden ...", "Load sound ...")); break;
    case MR_SSAVE: snprintf(l, ln, TR("Sound speichern ...", "Save sound ...")); break;
    case MR_SINIT: snprintf(l, ln, TR("Sound zurücksetzen", "Reset sound")); break;
    case MR_REC: snprintf(l, ln, TR("Live-Aufnahme", "Live record")); snprintf(v, vn, "%s", eng.recording ? TR("AN", "ON") : TR("AUS", "OFF")); break;
    case MR_TAP: snprintf(l, ln, TR("Tap-Tempo (mehrmals A)", "Tap tempo (press A repeatedly)")); snprintf(v, vn, "%d BPM", eng.proj.bpm); break;
    case MR_UNDO: snprintf(l, ln, TR("Rückgängig", "Undo")); break;
    case MR_REDO: snprintf(l, ln, TR("Wiederholen", "Redo")); break;
    case MR_SAVE: snprintf(l, ln, TR("Projekt speichern", "Save project")); snprintf(v, vn, "%.18s", cfg.project); break;
    default: l[0] = 0;
    }
}
static Uint32 mrTaps[4]; static int mrTapN = 0;
static void mrRun(int i) {
    if (i < MR_VN) { mrOpen = false; v2GoView(i); return; }
    switch (i) {
    case MR_PCOPY: case MR_PPASTE: case MR_PCLEAR: case MR_PADD: case MR_PDEL: {
        static const int M[5] = {PM_COPY, PM_PASTE, PM_CLEAR, PM_ADD, PM_DEL};
        pgmAll = false; pgmRun(M[i - MR_PCOPY]); pgmOpen = false; mrOpen = false; } break;
    case MR_PNEW: cfg.newPage = !cfg.newPage; saveSettings(); break;
    case MR_TCOPY: case MR_TPASTE: case MR_TCLEAR: {
        static const int M[3] = {PM_TCOPY, PM_TPASTE, PM_TCLEAR};
        pgmRun(M[i - MR_TCOPY]); pgmOpen = false; mrOpen = false; } break;
    case MR_ENGINE: mrOpen = false; openBrowser(FB_MACH); break;
    case MR_SLOAD: mrOpen = false; prjTrigger(4); break;
    case MR_SSAVE: mrOpen = false; prjTrigger(5); break;
    case MR_SINIT: mrOpen = false; prjTrigger(6); break;
    case MR_REC: eng.recording = !eng.recording; toast(eng.recording ? TR("AUFNAHME AN", "RECORD ON") : TR("AUFNAHME AUS", "RECORD OFF")); break;
    case MR_TAP: {
        Uint32 now = SDL_GetTicks();
        if (mrTapN && now - mrTaps[(mrTapN - 1) % 4] > 2000) mrTapN = 0;
        mrTaps[mrTapN % 4] = now; mrTapN++;
        int n = mrTapN < 4 ? mrTapN : 4;
        if (n >= 2) { float iv = (mrTaps[(mrTapN - 1) % 4] - mrTaps[(mrTapN - n) % 4]) / (float)(n - 1); if (iv > 0) eng.proj.bpm = (uint16_t)clampi((int)(60000.f / iv + 0.5f), 30, 300); }
    } break;
    case MR_UNDO: undoStep(); break;
    case MR_REDO: redoStep(); break;
    case MR_SAVE: mrOpen = false; prjTrigger(1); break;
    }
}
static void mrInput(Input& I, Uint32 now) {
    int d = (I.repeat(BT_DOWN, now, 250, 90) ? 1 : 0) - (I.repeat(BT_UP, now, 250, 90) ? 1 : 0);
    if (d) mrSel = (mrSel + d + MR_COUNT) % MR_COUNT;
    if (I.pressed(BT_L1) || I.pressed(BT_R1)) {   // Abschnitt springen
        int dir = I.pressed(BT_R1) ? 1 : -1, s = mrSel;
        for (int g = 0; g < MR_COUNT; g++) { s = (s + dir + MR_COUNT) % MR_COUNT; if (mrSection(s)) break; }
        mrSel = s;
    }
    if (I.pressed(BT_A)) mrRun(mrSel);
    else if ((I.pressed(BT_X) || I.pressed(BT_Y) || I.pressed(BT_LEFT) || I.pressed(BT_RIGHT)) && mrSel == MR_PNEW) mrRun(mrSel);
    else if (I.pressed(BT_B) || I.pressed(BT_SELECT)) mrOpen = false;
    if (I.pressed(BT_START)) { mrOpen = false; startStop(); }
}
static void drawMore() {
    if (!mrOpen) return;
    hitModal();
    const int x0 = 120, y0 = 50, w = 400, h = 392, rh = 22, vis = 14;
    rrect(x0, y0, w, h, 12, C_ACC); rrect(x0 + 2, y0 + 2, w - 4, h - 4, 11, C_BG);
    text(x0 + 16, y0 + 8, TR("MEHR", "MORE"), C_ACC, 2);
    char b[64]; snprintf(b, sizeof b, "%s  T%d %s", PN(curPi()), track + 1, MACHINE_NAMES[curP().machine[track] % M_COUNT]);
    text(x0 + w - 16 - textW(b), y0 + 16, b, TCOL[track]);
    // Zeilen inkl. Abschnitts-Ueberschriften
    int rows[MR_COUNT * 2], rk[MR_COUNT * 2], nr = 0, selRow = 0;
    for (int i = 0; i < MR_COUNT; i++) { if (mrSection(i)) { rows[nr] = -1; rk[nr++] = i; } if (i == mrSel) selRow = nr; rows[nr] = i; rk[nr++] = i; }
    float target = selRow - vis / 2.f; if (target > nr - vis) target = (float)(nr - vis); if (target < 0) target = 0;
    if (fabsf(target - mrScroll) > vis) mrScroll = target; else mrScroll += (target - mrScroll) * (1.f - expf(-animDt * 14.f));
    int top = y0 + 44;
    for (int r = 0; r < nr; r++) {
        int y = top + (int)((r - mrScroll) * rh);
        if (y < top - 2 || y > top + (vis - 1) * rh) continue;
        if (rows[r] < 0) { text(x0 + 16, y + 3, mrSection(rk[r]), C_CYAN); fill(x0 + 16, y + rh - 3, w - 32, 1, C_OFF2); continue; }
        int i = rows[r]; bool sel = i == mrSel;
        char l[64], v[48]; mrLabel(i, l, sizeof l, v, sizeof v);
        if (sel) rrect(x0 + 10, y, w - 20, rh - 2, 6, C_SEL);
        text(x0 + 24, y + 3, l, sel ? C_ACC : C_TEXT);
        if (v[0]) text(x0 + w - 24 - textW(v), y + 3, v, sel ? C_PLAY : C_DIM);
        hitAdd(x0 + 10, y, w - 20, rh - 2, HK_MORE, i);
    }
    hint(x0 + 14, y0 + h - 24, TR("[^v]WÄHLEN [A]AUSFÜHREN [L1/R1]ABSCHNITT [B]ZU", "[^v]SELECT [A]RUN [L1/R1]SECTION [B]CLOSE"), C_ACC, C_DIM);
}

// ---- Kopieren/Einfuegen per SELECT + X / Y: was betroffen ist, ergibt sich aus der Lage
static struct { bool have; uint8_t machine; uint8_t p[NPARAMS]; int from; } sndClip = {false, 0, {0}, 0};
static void v2Copy() {
    Pattern& P = curP();
    if (aHeld || lockLatch) {
        Step& hs = holdS(); trigClip = hs; if (aPrevOn == 0) { trigClip.on = 0; memset(trigClip.lock, NO_LOCK, NPARAMS); }
        haveTrigClip = true; aEdited = true; toast(TR("STEP %d KOPIERT", "STEP %d COPIED"), holdStep + 1);
    } else if (v2View == V2_SOUND) {
        sndClip.have = true; sndClip.machine = P.machine[track]; memcpy(sndClip.p, P.p[track], NPARAMS); sndClip.from = track;
        toast(TR("SOUND VON SPUR %d KOPIERT", "SOUND OF TRACK %d COPIED"), track + 1);
    } else { pgmAll = false; pgmRun(PM_COPY); pgmOpen = false; }
}
static void v2Paste() {
    Pattern& P = curP();
    if (aHeld || lockLatch) {
        if (!haveTrigClip) { toast(TR("STEP-ABLAGE LEER", "STEP CLIPBOARD EMPTY")); return; }
        lockA(); holdS() = trigClip; unlockA(); aEdited = true; toast(TR("STEP %d EINGEFÜGT", "STEP %d PASTED"), holdStep + 1);
    } else if (v2View == V2_SOUND) {
        if (!sndClip.have) { toast(TR("SOUND-ABLAGE LEER (SELECT + X)", "SOUND CLIPBOARD EMPTY (SELECT + X)")); return; }
        lockA(); P.machine[track] = sndClip.machine; memcpy(P.p[track], sndClip.p, NPARAMS); unlockA();
        toast(TR("SOUND T%d -> T%d", "SOUND T%d -> T%d"), sndClip.from + 1, track + 1);
    } else { pgmAll = false; pgmRun(PM_PASTE); pgmOpen = false; }
}

static void v2Group(int dir) {   // vorige/naechste Gruppe: erster Regler darin
    int f = v2Foc[v2View];
    if (dir < 0) {   // erst an den Anfang der eigenen Gruppe, dann eine Gruppe davor
        int h = f; while (h > 0 && !v2It[h].hdr) h--;
        f = h;
        for (int g = 0; g < v2N; g++) { f = (f - 1 + v2N) % v2N; if (v2It[f].hdr) break; }
    } else for (int g = 0; g < v2N; g++) { f = (f + 1) % v2N; if (v2It[f].hdr) break; }
    v2Foc[v2View] = v2Clamp(f, 1); v2Apply();
}
// ---- Eingabe der Listen-Ansichten. true = Bild erledigt; sonst laeuft die bisherige Spurseiten-Eingabe weiter
// (Wert, Trig, Locks, Vorhoeren ...); die hier verbrauchten Tasten sind in I.mask gesperrt.
static bool v2Input(Input& I, Uint32 now, bool func) {
    v2Sync();
    if (v2View == V2_NONE) return false;
    unsigned used = 0;
    auto take = [&](int b) { used |= 1u << b; };
    if (func) {
        if (I.repeat(BT_UP, now, 280, 120) || I.repeat(BT_DOWN, now, 280, 120)) { funcUsed = true; v2Group(I.down[BT_DOWN] ? 1 : -1); }   // SELECT + ^v: Gruppe
        if (!aHeld && !lockLatch && (I.repeat(BT_L1, now) || I.repeat(BT_R1, now))) {   // SELECT + L1/R1: 4 Steps (ein Schlag)
            funcUsed = true; int cl = curP().len[track] < 1 ? 1 : curP().len[track];
            cursor = ((cursor + (I.down[BT_R1] ? 4 : -4)) % cl + cl) % cl;
        }
        if (!aHeld && !lockLatch) { take(BT_L1); take(BT_R1); }
        if (I.pressed(BT_X)) { funcUsed = true; v2Copy(); }
        if (I.pressed(BT_Y)) { funcUsed = true; v2Paste(); }
        take(BT_UP); take(BT_DOWN); take(BT_X); take(BT_Y);
    } else {
        int d = (I.repeat(BT_DOWN, now, 260, 70) ? 1 : 0) - (I.repeat(BT_UP, now, 260, 70) ? 1 : 0);
        int f = flick(I.ly, flLY, flLYt, now); if (f) d -= f;   // linker Stick hoch = Liste hoch
        if (d) { v2Foc[v2View] = v2Clamp((v2Foc[v2View] + d + v2N) % v2N, d); v2Apply(); }
        if (v2View != V2_SEQ && !aHeld && !lockLatch) {   // SOUND/EFFEKTE: <> = Gruppe (Steps waehlt man in SEQ bzw. im Lock-Modus)
            int g = (I.repeat(BT_RIGHT, now, 300, 120) ? 1 : 0) - (I.repeat(BT_LEFT, now, 300, 120) ? 1 : 0);
            if (g) v2Group(g);
            take(BT_LEFT); take(BT_RIGHT);
        }
        if (!aHeld && !lockLatch) {
            int t = (I.repeat(BT_R1, now, 300, 120) ? 1 : 0) - (I.repeat(BT_L1, now, 300, 120) ? 1 : 0);
            if (t) { track = (track + t + NTRACKS) % NTRACKS; v2Build(v2View); v2Foc[v2View] = v2Clamp(v2Foc[v2View], 1); v2Apply(); }
        }
        take(BT_UP); take(BT_DOWN); take(BT_L1); take(BT_R1);
    }
    {   // ENGINE nie per Wert/Stick durchschalten (loescht den Klang): X oder Y oeffnet die Engine-Liste
        int idx = slotParam(slot);
        if (!func && idx >= 0 && paramDef(curP().machine[track] % M_COUNT, idx).kind == K_MACH && !aHeld && !lockLatch) {
            if (I.pressed(BT_X) || I.pressed(BT_Y)) openBrowser(FB_MACH);
            take(BT_X); take(BT_Y); I.rx = 0; I.ry = 0;
        }
    }
    I.mask |= used;
    I.ly = 0; I.lx = 0;   // linker Stick gehoert hier der Liste
    return false;
}

// ---- Zeichnen
static void v2Tracks(int x, int y0, int w, int h) {
    const Pattern& P = curP();
    int rh = h / NTRACKS;
    for (int t = 0; t < NTRACKS; t++) {
        int y = y0 + t * rh; bool sel = t == track, mu = eng.muted(t);
        Col c = mu ? dimC(TCOL[t], 0.35f) : TCOL[t];
        rrect(x, y, w, rh - 3, 5, sel ? C_SEL : C_PANEL);
        if (sel) { rrect(x, y, 3, rh - 3, 1, c); }
        drawIcon(P.machine[t] % M_COUNT, x + 7, y + 5, 14, mix(c, C_PLAY, flash[t] * 0.6f));
        char b[24]; snprintf(b, sizeof b, "%d %.6s", t + 1, MACHINE_NAMES[P.machine[t] % M_COUNT]);
        text(x + 25, y + 3, b, mu ? C_DIM : (sel ? c : C_TEXT));
        int mw = (int)((w - 32) * (meterV[t] > 1.f ? 1.f : meterV[t]));
        fill(x + 25, y + rh - 11, w - 32, 3, C_OFF2); if (mw > 0) fill(x + 25, y + rh - 11, mw, 3, c);
        if (mu) text(x + w - 14, y + 3, "M", C_TRIG);
        hitAdd(x, y, w, rh - 3, HK_TRACK, t);
    }
}
static const char* v2GrpName(int i) {   // Name der Gruppe, in der Eintrag i liegt
    for (int j = i; j >= 0; j--) if (v2It[j].hdr) return v2It[j].title ? v2It[j].title : pageLong(v2It[j].pg);
    return "";
}
static void v2List(int x, int y0, int w, int h) {
    const int rh = 24; int vis = (h - 18) / rh;
    int f = v2Foc[v2View];
    float target = f - vis / 2.f; if (target > v2N - vis) target = (float)(v2N - vis); if (target < 0) target = 0;
    float& sc = v2Scroll[v2View];
    if (fabsf(target - sc) > vis) sc = target;   // weiter Sprung: sofort
    else sc += (target - sc) * (1.f - expf(-animDt * 16.f));
    Col tc = v2View == V2_FX ? C_CYAN : TCOL[track];
    const Pattern& P = curP();
    int savePage = page, saveSlot = slot;
    const int hy = y0, ly = y0 + 18;   // feste Kopfzeile der Gruppe, darunter die Liste
    h -= 18;
    for (int i = 0; i < v2N; i++) {
        int y = ly + (int)((i - sc) * rh);
        if (y < ly - 2 || y > ly + h - rh + 2) continue;
        const V2It& it = v2It[i];
        if (it.hdr) {
            const char* t = it.title ? it.title : pageLong(it.pg);
            text(x + 4, y + 6, t, C_CYAN);
            fill(x + 8 + textW(t), y + 13, w - 12 - textW(t), 1, C_OFF2);
            continue;
        }
        bool sel = i == f;
        page = it.pg;   // knobInfo/knobText arbeiten auf page/slot-Index
        ParamDef d; int val, baseVal; bool locked; char label[24], vb[40];
        bool ok = knobInfo(it.k, d, val, locked, baseVal, label, sizeof label);
        page = savePage;
        if (!ok) continue;
        knobText(d, val, vb, sizeof vb, it.k);
        if (sel) { rrect(x, y + 1, w, rh - 2, 6, C_SEL); rrect(x, y + 4, 3, rh - 8, 1, locked ? C_LOCK : tc); }
        text(x + 12, y + 4, d.name, sel ? C_TEXT : C_DIM);   // immer der Reglername (Sample-/Zielname steht rechts)
        if (it.mac) {   // Makro: aus welcher Gruppe der Regler stammt (derselbe Regler steht dort nochmal)
            int src = v2FindItem2(it.pg, it.k);
            if (src > 0) { char gn[20]; snprintf(gn, sizeof gn, "%.9s", v2GrpName(src)); text(x + 12 + textW(d.name) + 8, y + 5, gn, dimC(C_CYAN, 0.55f)); }
        }
        if (label[0]) { char lb[28]; snprintf(lb, sizeof lb, "%.16s", label); strcpy(vb, lb); }   // Sample-/Dateiname statt Nummer
        // Wertbalken (bipolar ab der Mitte), Wert rechts
        int bx = x + w / 2 - 20, bw = w / 2 - 110;
        float fr = knobFrac(d, val);
        fill(bx, y + 11, bw, 2, C_OFF2);
        Col bc = locked ? C_LOCK : (sel ? tc : dimC(tc, 0.7f));
        if (d.kind == K_BIP || d.kind == K_OCT) { int mid = bx + bw / 2, e = bx + (int)(bw * fr); fill(e < mid ? e : mid, y + 10, abs(e - mid) + 1, 4, bc); }
        else fill(bx, y + 10, (int)(bw * fr), 4, bc);
        text(x + w - 10 - textW(vb), y + 4, vb, locked ? C_LOCK : (sel ? C_PLAY : C_TEXT));
        hitAdd(x, y, w, rh, HK_V2ROW, i);
    }
    page = savePage; slot = saveSlot;
    {   // feste Kopfzeile: Gruppe der obersten sichtbaren Zeile (nur wenn ihr eigener Kopf weggescrollt ist) + Position des Fokus
        fill(x, hy, w, 18, C_BG);
        int top = (int)(sc + 0.999f); if (top >= v2N) top = v2N - 1; if (top < 0) top = 0;
        bool headVisible = false;
        for (int j = top; j < v2N && j <= top + 1; j++) if (v2It[j].hdr) headVisible = true;
        if (!headVisible) text(x + 4, hy + 2, v2GrpName(top), C_CYAN);
        int gi = 0; for (int j = 0; j <= f && j < v2N; j++) if (v2It[j].hdr) gi++;
        char pb[48]; int npg = (curP().len[track] + 15) / 16; if (npg < 1) npg = 1;
        if (npg > 1) snprintf(pb, sizeof pb, TR("PAGE %d/%d   GRUPPE %d/%d", "PAGE %d/%d   GROUP %d/%d"), cursor / 16 + 1, npg, gi, v2Groups);
        else snprintf(pb, sizeof pb, TR("GRUPPE %d/%d", "GROUP %d/%d"), gi, v2Groups);
        text(x + w - 8 - textW(pb), hy + 2, pb, C_DIM);
        fill(x, hy + 16, w, 1, C_OFF2);
    }
    // Bildlaufleiste
    if (v2N > vis) { int bh = h * vis / v2N; if (bh < 8) bh = 8; int by = ly + (int)((h - bh) * sc / (v2N - vis)); fill(x + w + 2, ly, 3, h, C_OFF2); fill(x + w + 2, by, 3, bh, C_CYAN); }
    (void)P;
}
static void v2Steps(int x, int y, int w) {   // 16 Steps + Lock-Spur des gewaehlten Werts
    const Pattern& P = curP();
    int len = P.len[track] < 1 ? 1 : P.len[track], base = (cursor / 16) * 16, sw = w / 16;
    int param = slotParam(slot);
    ParamDef d = param >= 0 ? paramDef(P.machine[track] % M_COUNT, param) : PDEF_NONE;
    Col tc = TCOL[track];
    for (int s0 = 0; s0 < 16; s0++) {
        int s = base + s0, sx = x + s0 * sw;
        if (s >= len) { rrect(sx, y, sw - 3, 40, 5, C_DARK); continue; }
        const Step& st = P.s[track][s];
        bool cur = s == cursor, pl = eng.playing && eng.playedStep[track] == s;
        Col bg = st.on == 1 ? tc : (st.on == 2 ? C_PANEL : C_OFF2);
        rrect(sx, y, sw - 3, 40, 5, bg);
        if (st.on == 2) { rrect(sx, y, sw - 3, 2, 1, tc); rrect(sx, y + 38, sw - 3, 2, 1, tc); }
        if (s0 % 4 == 0) fill(sx, y + 42, sw - 3, 2, C_DIM);
        // Lock-Spur: Balken in der Hoehe des Lock-Werts des gewaehlten Reglers
        if (param >= 0 && st.lock[param] != NO_LOCK && d.max > 0) {
            int lh = 4 + (int)(26 * (st.lock[param] / (float)d.max));
            fill(sx + 3, y + 36 - lh, sw - 9, lh, C_LOCK);
        } else { bool any = false; for (int i = 0; i < NPARAMS && !any; i++) any = st.lock[i] != NO_LOCK; if (any) fill(sx + sw / 2 - 3, y + 30, 4, 4, C_LOCK); }
        char nb[8]; snprintf(nb, sizeof nb, "%d", s + 1);
        text(sx + 4, y + 2, nb, st.on == 1 ? C_BG : C_DIM);
        if (pl) rrect(sx, y + 40, sw - 3, 3, 1, C_PLAY);
        if (cur) { Col cc = aHeld || lockLatch ? C_LOCK : C_TEXT; rrect(sx - 1, y - 2, sw - 1, 2, 1, cc); rrect(sx - 1, y + 40, sw - 1, 2, 1, cc); rrect(sx - 1, y - 2, 2, 44, 1, cc); rrect(sx + sw - 4, y - 2, 2, 44, 1, cc); }
        hitAdd(sx, y - 2, sw - 2, 46, HK_STEP, s, track);
    }
}
static void v2Help() {   // was die Tasten JETZT tun
    const char* h;
    if (lockLatch) h = TR("LOCK  [X/Y]WERT  [^v]REGLER  [<>]STEP  [B]LOCKS WEG  [SEL+X/Y]STEP KOPIE  [A]FERTIG", "LOCK  [X/Y]VALUE  [^v]CONTROL  [<>]STEP  [B]CLEAR LOCKS  [SEL+X/Y]STEP COPY  [A]DONE");
    else if (aHeld) h = TR("[X/Y]LOCK FÜR DIESEN STEP  [^v]REGLER  [B]LOCKS WEG  LANG HALTEN = LOCK-MODUS", "[X/Y]LOCK FOR THIS STEP  [^v]CONTROL  [B]CLEAR LOCKS  HOLD LONG = LOCK MODE");
    else if (in.down[BT_SELECT]) h = v2View == V2_SOUND
        ? TR("[^v]GRUPPE [<>]PATTERN [L1/R1]4 STEPS [X]SOUND KOPIE [Y]EINF. [A]MUTE [B]UNDO [START]SPEICHERN", "[^v]GROUP [<>]PATTERN [L1/R1]4 STEPS [X]COPY SOUND [Y]PASTE [A]MUTE [B]UNDO [START]SAVE")
        : TR("[^v]GRUPPE [<>]PATTERN [L1/R1]4 STEPS [X]PAGE KOPIE [Y]EINF. [A]MUTE [B]UNDO [START]SPEICHERN", "[^v]GROUP [<>]PATTERN [L1/R1]4 STEPS [X]COPY PAGE [Y]PASTE [A]MUTE [B]UNDO [START]SAVE");
    else if (v2View != V2_SEQ) h = TR("[<>]GRUPPE [^v]WERT [X/Y]ÄNDERN [L1/R1]SPUR [A]TRIG [B]HÖREN [L2/R2]ANSICHT [SEL]MEHR", "[<>]GROUP [^v]VALUE [X/Y]CHANGE [L1/R1]TRACK [A]TRIG [B]LISTEN [L2/R2]VIEW [SEL]MORE");
    else h = TR("[<>]STEP [^v]WERT [X/Y]ÄNDERN [L1/R1]SPUR [A]TRIG [B]HÖREN [L2/R2]ANSICHT [SEL]MEHR", "[<>]STEP [^v]VALUE [X/Y]CHANGE [L1/R1]TRACK [A]TRIG [B]LISTEN [L2/R2]VIEW [SEL]MORE");
    hint(6, 456, h, C_ACC, C_DIM);
    {   // antippbarer MEHR-Knopf: am Handy gibt es kein SELECT unter dem Daumen
        const char* m = TR("MEHR", "MORE");
        int w = textW(m) + 14, x = 634 - w, y = 452;
        rrect(x, y, w, 20, 5, mrOpen ? C_SEL : C_PANEL);
        text(x + 7, y + 4, m, mrOpen ? C_TEXT : C_ACC);
        hitAdd(x - 2, y - 2, w + 4, 24, HK_MORE, -1);   // a = -1: Menue oeffnen (sonst Zeile ausfuehren)
    }
}
static void drawV2() {
    v2Sync();
    if (v2View == V2_NONE) return;
    v2Tracks(4, 48, 108, 342);   // Kopfzeile (mit den Ansichten-Reitern) und Hintergrund kommen aus drawAll
    {   // Inhalt: Gruppen-Kopfzeile, Liste, Erklaerung des gewaehlten Reglers
        int x = 118, y = 48, w = 506, h = 342;
        rrect(x - 4, y - 2, w + 14, h + 4, 8, C_PANEL);
        v2List(x, y, w, h - 20);
        int p = slotParam(slot);
        if (p >= 0) {
            ParamDef d = paramDef(curP().machine[track] % M_COUNT, p);
            const char* hp = paramHelp(d.name);
            if (hp && *hp) { char hb[90]; snprintf(hb, sizeof hb, "%.75s", hp); text(x + 4, y + h - 16, hb, C_DIM); }
        }
    }
    v2Steps(6, 396, 628);
    v2Help();
}
