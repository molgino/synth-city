// PAGE-Menue (TRIG-Seite, SELECT + L1/R1): Step-Page (16 Steps) der Spur oder aller Spuren kopieren, einfuegen,
// leeren, eine Page anhaengen/entfernen und festlegen, ob eine neue Page leer oder eine Kopie der vorigen ist
// (cfg.newPage, gilt auch beim Verlaengern ueber LEN). Eingebunden in main.cpp nach smpedit.inc.
enum { PM_COPY, PM_PASTE, PM_CLEAR, PM_ADD, PM_DEL, PM_TCOPY, PM_TPASTE, PM_TCLEAR, PM_ALL, PM_NEW, PM_COUNT };
static int pgmSel = 0;
static bool pgmAll = false;                // Bereich: nur Spur bzw. alle 8 Spuren
static Step pgClip[NTRACKS][16];
static int pgClipState = 0;                // 0 leer, 1 eine Spur, 2 alle Spuren
static int pgClipTrack = 0, pgClipPage = 0;
// Spur-Ablage: ganze Spur (Engine, Sound, Laenge, Tempo, alle Steps)
static struct { uint8_t machine, len, speed; uint8_t p[NPARAMS]; Step s[NSTEPS]; } trkClip;
static int trkClipFrom = -1, trkClipPat = 0;

static void openPageMenu() { pgmOpen = true; pgmSel = 0; }
static int pgmPage() { return cursor / 16; }
static void pgmRun(int it) {
    Pattern& P = curP();
    int base = pgmPage() * 16, t0 = pgmAll ? 0 : track, t1 = pgmAll ? NTRACKS : track + 1;
    switch (it) {
    case PM_COPY:
        for (int t = t0; t < t1; t++) for (int i = 0; i < 16; i++) pgClip[t][i] = P.s[t][base + i];
        pgClipState = pgmAll ? 2 : 1; pgClipTrack = track; pgClipPage = pgmPage();
        if (pgmAll) toast(TR("PAGE %d ALLER SPUREN KOPIERT", "PAGE %d OF ALL TRACKS COPIED"), pgmPage() + 1);
        else toast(TR("PAGE %d VON SPUR %d KOPIERT", "PAGE %d OF TRACK %d COPIED"), pgmPage() + 1, track + 1);
        break;
    case PM_PASTE:
        if (!pgClipState) { toast(TR("PAGE-ABLAGE LEER (ERST KOPIEREN)", "PAGE CLIPBOARD EMPTY (COPY FIRST)")); return; }
        lockA();
        if (pgClipState == 2) for (int t = 0; t < NTRACKS; t++) for (int i = 0; i < 16; i++) P.s[t][base + i] = pgClip[t][i];
        else for (int i = 0; i < 16; i++) P.s[track][base + i] = pgClip[pgClipTrack][i];
        unlockA();
        if (pgClipState == 2) toast(TR("PAGE %d -> PAGE %d (ALLE SPUREN)", "PAGE %d -> PAGE %d (ALL TRACKS)"), pgClipPage + 1, pgmPage() + 1);
        else toast(TR("PAGE %d (T%d) -> PAGE %d (T%d)", "PAGE %d (T%d) -> PAGE %d (T%d)"), pgClipPage + 1, pgClipTrack + 1, pgmPage() + 1, track + 1);
        break;
    case PM_CLEAR:
        lockA();
        for (int t = t0; t < t1; t++) for (int i = 0; i < 16; i++) { Step& s = P.s[t][base + i]; s.on = 0; memset(s.lock, NO_LOCK, NPARAMS); }
        unlockA();
        toast(pgmAll ? TR("PAGE %d ALLER SPUREN GELEERT", "PAGE %d OF ALL TRACKS CLEARED") : TR("PAGE %d GELEERT", "PAGE %d CLEARED"), pgmPage() + 1);
        break;
    case PM_ADD: {
        int n = 0;
        lockA();
        for (int t = t0; t < t1; t++) {
            int l = P.len[t], nl = (l + 15) / 16 * 16 + 16;   // auf volle Pages, dann eine dazu
            if (nl > NSTEPS) continue;
            setTrackLen(P, t, nl, true); n++;
        }
        unlockA();
        if (!n) toast(TR("MAXIMAL %d STEPS", "MAXIMUM %d STEPS"), NSTEPS);
        else { cursor = (P.len[track] - 1) / 16 * 16 + cursor % 16; if (cursor >= P.len[track]) cursor = P.len[track] - 1;
               toast(cfg.newPage ? TR("PAGE ANGEHÄNGT (KOPIE) - %d STEPS", "PAGE ADDED (COPY) - %d STEPS") : TR("LEERE PAGE ANGEHÄNGT - %d STEPS", "EMPTY PAGE ADDED - %d STEPS"), P.len[track]); }
    } break;
    case PM_DEL: {
        lockA();
        for (int t = t0; t < t1; t++) { int l = P.len[t]; if (l > 16) P.len[t] = (uint8_t)((l - 1) / 16 * 16); }
        unlockA();
        if (cursor >= P.len[track]) cursor = P.len[track] - 1;
        toast(TR("LETZTE PAGE ENTFERNT - %d STEPS", "LAST PAGE REMOVED - %d STEPS"), P.len[track]);
    } break;
    case PM_TCOPY:
        trkClip.machine = P.machine[track]; trkClip.len = P.len[track]; trkClip.speed = P.speed[track];
        memcpy(trkClip.p, P.p[track], NPARAMS); memcpy(trkClip.s, P.s[track], sizeof trkClip.s);
        trkClipFrom = track; trkClipPat = curPi();
        toast(TR("SPUR %d KOPIERT (SOUND + STEPS)", "TRACK %d COPIED (SOUND + STEPS)"), track + 1);
        break;
    case PM_TPASTE:
        if (trkClipFrom < 0) { toast(TR("SPUR-ABLAGE LEER (ERST KOPIEREN)", "TRACK CLIPBOARD EMPTY (COPY FIRST)")); return; }
        lockA();
        P.machine[track] = trkClip.machine; P.len[track] = trkClip.len; P.speed[track] = trkClip.speed;
        memcpy(P.p[track], trkClip.p, NPARAMS); memcpy(P.s[track], trkClip.s, sizeof trkClip.s);
        unlockA();
        if (cursor >= P.len[track]) cursor = P.len[track] - 1;
        toast(TR("%s T%d -> %s T%d EINGEFÜGT", "%s T%d -> %s T%d PASTED"), PN(trkClipPat), trkClipFrom + 1, PN(curPi()), track + 1);
        break;
    case PM_TCLEAR:
        lockA();
        for (int i = 0; i < NSTEPS; i++) { Step& st = P.s[track][i]; st.on = 0; memset(st.lock, NO_LOCK, NPARAMS); }
        unlockA();
        toast(TR("ALLE STEPS VON SPUR %d GELEERT (SOUND BLEIBT)", "ALL STEPS OF TRACK %d CLEARED (SOUND STAYS)"), track + 1);
        break;
    case PM_ALL: pgmAll = !pgmAll; return;
    case PM_NEW: cfg.newPage = !cfg.newPage; saveSettings(); return;
    }
    pgmOpen = false;
}
static void pgmInput(Input& I, Uint32 now) {
    int d = (I.repeat(BT_DOWN, now, 250, 110) ? 1 : 0) - (I.repeat(BT_UP, now, 250, 110) ? 1 : 0);
    if (d) pgmSel = (pgmSel + d + PM_COUNT) % PM_COUNT;
    if (I.pressed(BT_A)) pgmRun(pgmSel);
    else if ((I.pressed(BT_X) || I.pressed(BT_Y) || I.pressed(BT_LEFT) || I.pressed(BT_RIGHT)) && (pgmSel == PM_ALL || pgmSel == PM_NEW)) pgmRun(pgmSel);
    else if (I.pressed(BT_B) || I.pressed(BT_SELECT) || I.pressed(BT_L1) || I.pressed(BT_R1)) pgmOpen = false;
}
static void drawPageMenu() {
    if (!pgmOpen) return;
    hitModal();   // Touch: nur das Menue (daneben = schliessen)
    const Pattern& P = curP();
    int w = 400, rh = 26, h = 64 + PM_COUNT * rh + 30, x0 = (640 - w) / 2, y0 = (480 - h) / 2 - 10;
    rrect(x0, y0, w, h, 12, C_ACC);
    rrect(x0 + 2, y0 + 2, w - 4, h - 4, 11, C_BG);
    char b[96];
    snprintf(b, sizeof b, "PAGE %d/%d", pgmPage() + 1, (P.len[track] + 15) / 16);
    text(x0 + 16, y0 + 10, b, C_ACC, 2);
    snprintf(b, sizeof b, pgmAll ? TR("ALLE SPUREN", "ALL TRACKS") : TR("SPUR %d", "TRACK %d"), track + 1);
    text(x0 + w - 16 - textW(b), y0 + 18, b, C_DIM);
    for (int i = 0; i < PM_COUNT; i++) {
        int y = y0 + 50 + i * rh;
        bool sel = i == pgmSel;
        if (sel) rrect(x0 + 10, y - 3, w - 20, rh - 4, 6, C_SEL);
        hitAdd(x0 + 10, y - 3, w - 20, rh - 2, HK_PGM, i);
        if (i == PM_TCOPY || i == PM_ALL) fill(x0 + 16, y - 5, w - 32, 1, C_OFF2);
        const char* l = "";
        b[0] = 0;
        switch (i) {
        case PM_COPY: l = TR("PAGE KOPIEREN", "COPY PAGE"); break;
        case PM_PASTE: l = TR("PAGE EINFÜGEN", "PASTE PAGE");
            if (pgClipState == 2) snprintf(b, sizeof b, TR("P%d ALLE", "P%d ALL"), pgClipPage + 1);
            else if (pgClipState == 1) snprintf(b, sizeof b, "P%d T%d", pgClipPage + 1, pgClipTrack + 1);
            else snprintf(b, sizeof b, TR("(LEER)", "(EMPTY)"));
            break;
        case PM_CLEAR: l = TR("PAGE LEEREN", "CLEAR PAGE"); break;
        case PM_ADD: l = TR("PAGE ANHÄNGEN", "ADD PAGE"); snprintf(b, sizeof b, "%d -> %d", P.len[track], (P.len[track] + 15) / 16 * 16 + 16 > NSTEPS ? P.len[track] : (P.len[track] + 15) / 16 * 16 + 16); break;
        case PM_DEL: l = TR("LETZTE PAGE ENTFERNEN", "REMOVE LAST PAGE"); break;
        case PM_TCOPY: l = TR("SPUR KOPIEREN", "COPY TRACK"); snprintf(b, sizeof b, TR("SOUND + STEPS", "SOUND + STEPS")); break;
        case PM_TPASTE: l = TR("SPUR EINFÜGEN", "PASTE TRACK");
            if (trkClipFrom >= 0) snprintf(b, sizeof b, "%s T%d", PN(trkClipPat), trkClipFrom + 1); else snprintf(b, sizeof b, TR("(LEER)", "(EMPTY)"));
            break;
        case PM_TCLEAR: l = TR("SPUR LEEREN", "CLEAR TRACK"); snprintf(b, sizeof b, TR("NUR STEPS", "STEPS ONLY")); break;
        case PM_ALL: l = TR("BEREICH", "SCOPE"); snprintf(b, sizeof b, "%s", pgmAll ? TR("ALLE SPUREN", "ALL TRACKS") : TR("NUR SPUR", "TRACK ONLY")); break;
        case PM_NEW: l = TR("NEUE PAGE", "NEW PAGE"); snprintf(b, sizeof b, "%s", cfg.newPage ? TR("KOPIE DER VORIGEN", "COPY OF PREVIOUS") : TR("LEER", "EMPTY")); break;
        }
        text(x0 + 20, y + 2, l, sel ? C_ACC : C_TEXT);
        if (b[0]) text(x0 + w - 20 - textW(b), y + 2, b, sel ? C_PLAY : C_DIM);
    }
    hint(x0 + 16, y0 + h - 26, TR("[^v]WÄHLEN [A]AUSFÜHREN [X/Y]UMSCHALTEN [B]ZU  (ANTIPPEN GEHT AUCH)", "[^v]SELECT [A]RUN [X/Y]TOGGLE [B]CLOSE  (OR TAP)"), C_ACC, C_DIM);
}
