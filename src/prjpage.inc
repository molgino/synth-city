// PRJ-Seite: drei Bereiche untereinander - PROJEKT (Laden/Speichern ...), PATTERN (Bank A-H + 16 Felder der Bank, Kopieren/Einfuegen/
// Leeren/Mutieren, Laenge aller Spuren, Wechsel-Laenge CHG) und SOUND (Klang der gewaehlten Spur).
// Bedienung: Steuerkreuz waehlt das Feld (2D), A fuehrt aus, X/Y aendern Werte (halten = 16er-Schritte), L1/R1 = +-16.
// Pattern kopieren: A auf ein Pattern-Feld (waehlt + spielt es) -> KOPIEREN, A auf das Ziel-Feld -> EINFUEGEN.
// Das mit A gewaehlte Pattern ist das Ziel fuer Kopieren/Einfuegen/Leeren/Mutieren/Laenge/Wechsel (nicht das Feld unter dem Cursor).
enum { PI_LOAD, PI_SAVE, PI_SAVEAS, PI_NEW, PI_DEMO, PI_BANK0, PI_PAT0 = PI_BANK0 + NBANKS, PI_COPY = PI_PAT0 + 16, PI_PASTE, PI_CLEAR, PI_MUTATE,
       PI_LEN, PI_CHG, PI_TRACK, PI_SLOAD, PI_SSAVE, PI_SINIT, PI_COUNT };
struct PrjBox { int x, y, w, h; };
static int prjSel = PI_PAT0, prjPat = -1, clipFrom = -1;
static int prjBankNow() { if (prjBank < 0 || prjBank >= NBANKS) prjBank = curPi() / 16; return prjBank; }

static PrjBox prjBox(int i) {
    if (i <= PI_DEMO) return {10 + i * 124, 84, 116, 34};
    if (i < PI_PAT0) return {10 + (i - PI_BANK0) * 78, 158, 72, 22};
    if (i < PI_COPY) { int k = i - PI_PAT0; return {10 + (k % 8) * 78, 184 + (k / 8) * 32, 72, 28}; }
    if (i <= PI_MUTATE) return {10 + (i - PI_COPY) * 155, 250, 150, 28};
    if (i == PI_LEN) return {10, 284, 305, 44};
    if (i == PI_CHG) return {325, 284, 305, 44};
    if (i == PI_TRACK) return {10, 376, 210, 52};
    return {230 + (i - PI_SLOAD) * 135, 376, 128, 52};
}
static bool prjIsValue(int i) { return i == PI_LEN || i == PI_CHG || i == PI_TRACK || i == PI_MUTATE; }
static int prjTarget() { if (prjPat < 0 || prjPat >= NPATTERNS) prjPat = curPi(); return prjPat; }
static bool patEmpty(const Pattern& P) {
    for (int t = 0; t < NTRACKS; t++) for (int s = 0; s < NSTEPS; s++) if (P.s[t][s].on) return false;
    return true;
}
static int patMaxLen(const Pattern& P) { int m = 1; for (int t = 0; t < NTRACKS; t++) if (P.len[t] > m) m = P.len[t]; return m; }

// Aenderung eines Wertfelds um d (Laenge/Wechsel: in 16er-Schritten auf Vielfache von 16 einrasten)
static void prjValue(int i, int d) {
    if (!d) return;
    Pattern& P = eng.proj.pat[prjTarget()];
    auto step16 = [](int v, int d) { return d > 0 ? (v / 16 + 1) * 16 : ((v - 1) / 16) * 16; };
    if (i == PI_LEN) {
        int m = patMaxLen(P);
        int n = abs(d) >= 16 ? step16(m, d) : m + d;
        n = clampi(n, 1, NSTEPS);
        lockA(); for (int t = 0; t < NTRACKS; t++) setTrackLen(P, t, n); unlockA();
    } else if (i == PI_CHG) {
        int c = P.chg;
        int n = abs(d) >= 16 ? step16(c, d) : c + d;
        P.chg = (uint8_t)clampi(n, 0, NSTEPS);
    } else if (i == PI_TRACK) {
        track = (track + NTRACKS + (d > 0 ? 1 : -1)) % NTRACKS;
    } else if (i == PI_MUTATE) {
        mutAmt = clampi(mutAmt + (d > 0 ? 1 : -1), 0, 2);
    }
}
static void prjActivate(int i) {
    int tp = prjTarget();
    if (i <= PI_NEW) { prjTrigger(i); return; }
    if (i == PI_DEMO) { prjTrigger(7); return; }
    if (i >= PI_BANK0 && i < PI_PAT0) { prjBank = i - PI_BANK0; toast(TR("BANK %c", "BANK %c"), 'A' + prjBank); return; }
    if (i >= PI_PAT0 && i < PI_COPY) {
        prjPat = prjBankNow() * 16 + i - PI_PAT0;
        bool later = eng.playing && prjPat != curPi();
        switchPattern(prjPat);
        toast(later ? TR("PATTERN %s KOMMT AM PATTERN-ENDE", "PATTERN %s COMES AT THE PATTERN END") : "PATTERN %s", PN(prjPat));
        return;
    }
    switch (i) {
    case PI_COPY: clip = eng.proj.pat[tp]; haveClip = true; clipFrom = tp;
        toast(TR("PATTERN %s KOPIERT - ZIEL WÄHLEN, EINFÜGEN", "PATTERN %s COPIED - PICK TARGET, PASTE"), PN(tp)); break;
    case PI_PASTE:
        if (!haveClip) { toast(TR("ERST EIN PATTERN KOPIEREN", "COPY A PATTERN FIRST")); break; }
        if (patEmpty(eng.proj.pat[tp])) { lockA(); eng.proj.pat[tp] = clip; eng.proj.pat[tp].used = 1; unlockA(); toast(TR("PATTERN %s EINGEFÜGT", "PATTERN %s PASTED"), PN(tp)); }
        else askConfirm(ACT_PASTE, tp, TR("PATTERN %s ÜBERSCHREIBEN?", "OVERWRITE PATTERN %s?"), PN(tp));
        break;
    case PI_CLEAR: askConfirm(ACT_CLEARPAT, tp, TR("ALLE STEPS VON PATTERN %s LÖSCHEN?", "CLEAR ALL STEPS OF PATTERN %s?"), PN(tp)); break;
    case PI_MUTATE: mutatePattern(tp); break;
    case PI_SLOAD: prjTrigger(4); break;
    case PI_SSAVE: prjTrigger(5); break;
    case PI_SINIT: prjTrigger(6); break;
    }
}
// Steuerkreuz: links/rechts innerhalb der Zeile (im Kreis), hoch/runter zum naechstgelegenen Feld der Nachbarzeile
static void prjMove(int dx, int dy) {
    PrjBox c = prjBox(prjSel);
    if (dx) {
        int best = -1, bestX = dx > 0 ? 99999 : -99999, wrap = -1, wrapX = dx > 0 ? 99999 : -99999;
        for (int i = 0; i < PI_COUNT; i++) {
            PrjBox b = prjBox(i);
            if (b.y != c.y || i == prjSel) continue;
            if (dx > 0 ? (b.x > c.x && b.x < bestX) : (b.x < c.x && b.x > bestX)) { best = i; bestX = b.x; }
            if (dx > 0 ? b.x < wrapX : b.x > wrapX) { wrap = i; wrapX = b.x; }
        }
        prjSel = best >= 0 ? best : (wrap >= 0 ? wrap : prjSel);
    }
    if (dy) {
        int rowY = -1;   // naechste Zeile in Richtung dy
        for (int i = 0; i < PI_COUNT; i++) { int y = prjBox(i).y; if (dy > 0 ? (y > c.y && (rowY < 0 || y < rowY)) : (y < c.y && y > rowY)) rowY = y; }
        if (rowY < 0) { rowY = dy > 0 ? 99999 : -1; for (int i = 0; i < PI_COUNT; i++) { int y = prjBox(i).y; if (dy > 0 ? y < rowY : y > rowY) rowY = y; } }   // im Kreis
        int best = prjSel, bd = 1 << 30, cx = c.x + c.w / 2;
        for (int i = 0; i < PI_COUNT; i++) { PrjBox b = prjBox(i); if (b.y != rowY) continue; int dd = abs(b.x + b.w / 2 - cx); if (dd < bd) { bd = dd; best = i; } }
        prjSel = best;
    }
}
static void prjInput(Input& I, Uint32 now) {
    int dx = (I.repeat(BT_RIGHT, now, 250, 90) ? 1 : 0) - (I.repeat(BT_LEFT, now, 250, 90) ? 1 : 0);
    int dy = (I.repeat(BT_DOWN, now, 250, 110) ? 1 : 0) - (I.repeat(BT_UP, now, 250, 110) ? 1 : 0);
    if (dx || dy) prjMove(dx, dy);
    if (I.pressed(BT_A)) prjActivate(prjSel);
    if (prjIsValue(prjSel)) {
        bool big = prjSel == PI_LEN || prjSel == PI_CHG;
        if (I.repeat(BT_X, now, 350, 110)) prjValue(prjSel, big && I.heldMs(BT_X, now) > 340 ? 16 : 1);
        if (I.repeat(BT_Y, now, 350, 110)) prjValue(prjSel, big && I.heldMs(BT_Y, now) > 340 ? -16 : -1);
        if (big && I.repeat(BT_R1, now, 300, 120)) prjValue(prjSel, 16);
        if (big && I.repeat(BT_L1, now, 300, 120)) prjValue(prjSel, -16);
    }
    if (I.pressed(BT_B) && (prjSel != PI_PAT0 + prjTarget() % 16 || prjBankNow() != prjTarget() / 16)) { prjBank = prjTarget() / 16; prjSel = PI_PAT0 + prjTarget() % 16; }   // B: zurueck zum Pattern
    if (I.pressed(BT_L2)) gotoPage(-1);
    if (I.pressed(BT_R2)) gotoPage(1);
    if (startTap(I, now)) startStop();
}

static void prjButton(int i, const char* label, const char* sub) {
    PrjBox b = prjBox(i);
    bool sel = i == prjSel;
    float p = sel ? 0.5f + 0.5f * sinf(animTime * 4.f) : 0.f;
    rrect(b.x, b.y, b.w, b.h, 8, sel ? mix(C_ACC, C_PLAY, p * 0.25f) : C_OFF2);
    int ty = sub && sub[0] ? b.y + b.h / 2 - 13 : b.y + b.h / 2 - 7;
    textC(b.x + b.w / 2, ty, label, sel ? C_BG : C_TEXT);
    if (sub && sub[0]) textC(b.x + b.w / 2, ty + 15, sub, sel ? C_BG : C_DIM);
    hitAdd(b.x, b.y, b.w, b.h, HK_PRJ, i, 0);
}
static void prjValueBox(int i, const char* label, const char* val, const char* sub) {
    PrjBox b = prjBox(i);
    bool sel = i == prjSel;
    rrect(b.x, b.y, b.w, b.h, 8, sel ? C_SEL : C_BG);
    if (sel) rrect(b.x, b.y + 6, 3, b.h - 12, 1, C_ACC);
    text(b.x + 12, b.y + 6, label, sel ? C_TEXT : C_DIM);
    if (sub && sub[0]) text(b.x + 12, b.y + b.h - 18, sub, C_DIM);
    int vw = textW(val, 2);
    text(b.x + b.w - 44 - vw, b.y + b.h / 2 - 9, val, sel ? C_ACC : C_TEXT, 2);
    // Touch: -/+ an der rechten Kante
    rrect(b.x + b.w - 38, b.y + 4, 16, b.h - 8, 4, C_OFF2); textC(b.x + b.w - 30, b.y + b.h / 2 - 7, "-", C_TEXT);
    rrect(b.x + b.w - 20, b.y + 4, 16, b.h - 8, 4, C_OFF2); textC(b.x + b.w - 12, b.y + b.h / 2 - 7, "+", C_TEXT);
    hitAdd(b.x, b.y, b.w - 40, b.h, HK_PRJ, i, 0);
    hitAdd(b.x + b.w - 40, b.y, 20, b.h, HK_PRJ, i, -1);
    hitAdd(b.x + b.w - 20, b.y, 20, b.h, HK_PRJ, i, 1);
}
static void prjCard(int y, int h, const char* title) {
    rrect(4, y, 632, h, 10, C_PANEL);
    text(14, y + 6, title, C_ACC);
}

static void drawPrj() {
    char b[96], s2[48];
    int tp = prjTarget();
    // ---- Projekt
    prjCard(46, 84, TR("PROJEKT", "PROJECT"));
    bool dirty = projDirtyCached();
    snprintf(b, sizeof b, "%.28s", cfg.project);
    text(110, 50, b, C_TEXT, 2);
    const char* st = dirty ? TR("UNGESPEICHERT", "UNSAVED") : TR("GESPEICHERT", "SAVED");
    text(626 - textW(st), 56, st, dirty ? C_TRIG : C_GREEN);
    prjButton(PI_LOAD, TR("LADEN", "LOAD"), nullptr);
    prjButton(PI_SAVE, TR("SPEICHERN", "SAVE"), nullptr);
    prjButton(PI_SAVEAS, TR("SPEICH. ALS", "SAVE AS"), nullptr);
    prjButton(PI_NEW, TR("NEU", "NEW"), nullptr);
    prjButton(PI_DEMO, "DEMO", nullptr);
    // ---- Pattern
    prjCard(134, 200, "PATTERN");
    int playing = eng.songActive() ? -1 : curPi();
    snprintf(b, sizeof b, TR("ZIEL %s", "TARGET %s"), PN(tp));
    text(110, 140, b, C_CYAN);
    if (eng.nextPat >= 0) { snprintf(b, sizeof b, TR("NÄCHSTES: %s", "NEXT: %s"), PN(eng.nextPat)); text(250, 140, b, (SDL_GetTicks() / 300) % 2 ? C_PLAY : C_DIM); }
    if (haveClip && clipFrom >= 0) { snprintf(b, sizeof b, TR("ABLAGE: %s", "CLIPBOARD: %s"), PN(clipFrom)); text(626 - textW(b), 140, b, C_DIM); }
    int bank = prjBankNow();
    for (int k = 0; k < NBANKS; k++) {   // Bank-Reihe A-H: Punkt = Bank hat belegte Patterns
        int i = PI_BANK0 + k; PrjBox bx = prjBox(i);
        bool sel = i == prjSel, cur = k == bank, any = false;
        for (int j = 0; j < 16 && !any; j++) any = eng.proj.pat[k * 16 + j].used || !patEmpty(eng.proj.pat[k * 16 + j]);
        rrect(bx.x, bx.y, bx.w, bx.h, 6, sel ? C_SEL : (cur ? C_OFF2 : C_BG));
        if (cur) rrect(bx.x, bx.y + bx.h - 2, bx.w, 2, 1, C_ACC);
        snprintf(b, sizeof b, TR("BANK %c", "BANK %c"), 'A' + k);
        text(bx.x + 8, bx.y + 3, b, sel ? C_ACC : (cur ? C_TEXT : C_DIM));
        if (any) fill(bx.x + bx.w - 12, bx.y + 9, 5, 5, playing >= 0 && playing / 16 == k ? C_PLAY : C_TEXT);
        hitAdd(bx.x, bx.y, bx.w, bx.h, HK_PRJ, i, 0);
    }
    for (int kk = 0; kk < 16; kk++) {
        int k = bank * 16 + kk;
        int i = PI_PAT0 + kk; PrjBox bx = prjBox(i);
        const Pattern& P = eng.proj.pat[k];
        bool sel = i == prjSel, isTgt = k == tp, isPlay = k == playing, isNext = k == eng.nextPat, empty = patEmpty(P) && !P.used;
        Col bg = sel ? C_SEL : C_BG;
        rrect(bx.x, bx.y, bx.w, bx.h, 6, bg);
        if (isTgt) rrect(bx.x, bx.y + bx.h - 3, bx.w, 3, 1, C_CYAN);
        if (isPlay) { rrect(bx.x, bx.y, 4, bx.h, 1, eng.playing ? C_PLAY : C_GREEN); }
        if (isNext && (SDL_GetTicks() / 300) % 2) rrect(bx.x, bx.y, 4, bx.h, 1, C_PLAY);
        snprintf(b, sizeof b, "%s", PN(k));
        text(bx.x + 9, bx.y + 6, b, empty ? C_DIM : (sel ? C_ACC : C_TEXT));
        for (int t = 0; t < NTRACKS; t++) {   // je Spur ein Punkt, wenn sie Trigs hat
            bool any = false; for (int s = 0; s < P.len[t] && !any; s++) any = P.s[t][s].on == 1;
            fill(bx.x + 44 + (t % 4) * 6, bx.y + 7 + (t / 4) * 8, 4, 4, any ? TCOL[t] : C_OFF2);
        }
        hitAdd(bx.x, bx.y, bx.w, bx.h, HK_PRJ, i, 0);
    }
    prjButton(PI_COPY, TR("KOPIEREN", "COPY"), nullptr);
    prjButton(PI_PASTE, TR("EINFÜGEN", "PASTE"), nullptr);
    prjButton(PI_CLEAR, TR("LEEREN", "CLEAR"), nullptr);
    { const char* const AM[3] = {TR("LEICHT", "LIGHT"), TR("MITTEL", "MEDIUM"), TR("STARK", "STRONG")};
      snprintf(b, sizeof b, TR("MUTIEREN: %s", "MUTATE: %s"), AM[mutAmt % 3]); prjButton(PI_MUTATE, b, nullptr); }
    const Pattern& TP = eng.proj.pat[tp];
    {   // Laenge aller Spuren
        int mn = 255, mx = 0; for (int t = 0; t < NTRACKS; t++) { if (TP.len[t] < mn) mn = TP.len[t]; if (TP.len[t] > mx) mx = TP.len[t]; }
        if (mn == mx) snprintf(b, sizeof b, "%d", mx); else snprintf(b, sizeof b, "%d-%d", mn, mx);
        prjValueBox(PI_LEN, TR("LÄNGE (ALLE SPUREN)", "LENGTH (ALL TRACKS)"), b, TR("X/Y +-1, halten +-16", "X/Y +-1, hold +-16"));
    }
    {   // Wechsel-Laenge
        long auto16 = 0; for (int t = 0; t < NTRACKS; t++) { long l = (long)(TP.len[t] < 1 ? 1 : TP.len[t]) * SPEED_TPS[TP.speed[t] % 7] / 12; if (l > auto16) auto16 = l; }
        if (TP.chg) snprintf(b, sizeof b, "%d", TP.chg); else snprintf(b, sizeof b, "AUTO");
        if (TP.chg) snprintf(s2, sizeof s2, TR("Steps (0 = AUTO)", "steps (0 = AUTO)"));
        else snprintf(s2, sizeof s2, TR("längste Spur: %ld", "longest track: %ld"), auto16);
        prjValueBox(PI_CHG, TR("WECHSEL NACH", "CHANGE AFTER"), b, s2);
    }
    // ---- Sound der Spur
    prjCard(342, 98, TR("SOUND DER SPUR", "TRACK SOUND"));
    snprintf(b, sizeof b, "%d", track + 1);
    snprintf(s2, sizeof s2, "%s", MACHINE_LONG[curP().machine[track] % M_COUNT]);
    prjValueBox(PI_TRACK, TR("SPUR", "TRACK"), b, s2);
    prjButton(PI_SLOAD, TR("LADEN", "LOAD"), TR("Sound/Werksklang", "sound/factory"));
    prjButton(PI_SSAVE, TR("SPEICHERN", "SAVE"), TR("als Sound", "as sound"));
    prjButton(PI_SINIT, "INIT", TR("zurücksetzen", "reset"));
    // ---- Hinweis
    const char* h;
    if (prjSel >= PI_BANK0 && prjSel < PI_PAT0) h = TR("[A]BANK ZEIGEN (A01-H16, JE 16 PATTERNS) [<>^v]FELD [L2/R2]SEITE", "[A]SHOW BANK (A01-H16, 16 PATTERNS EACH) [<>^v]FIELD [L2/R2]PAGE");
    else if (prjSel >= PI_PAT0 && prjSel < PI_COPY) h = TR("[A]PATTERN WÄHLEN (SPIELT + ZIEL FÜR KOPIEREN/EINFÜGEN) [L2/R2]SEITE", "[A]PICK PATTERN (PLAYS + TARGET FOR COPY/PASTE) [L2/R2]PAGE");
    else if (prjSel == PI_LEN || prjSel == PI_CHG) h = TR("[X/Y]+-1 (HALTEN +-16) [L1/R1]+-16 [<>^v]FELD [B]ZUM PATTERN", "[X/Y]+-1 (HOLD +-16) [L1/R1]+-16 [<>^v]FIELD [B]TO PATTERN");
    else if (prjSel == PI_TRACK) h = TR("[X/Y]SPUR [<>^v]FELD [L2/R2]SEITE [SEL]KARTE", "[X/Y]TRACK [<>^v]FIELD [L2/R2]PAGE [SEL]MAP");
    else if (prjSel == PI_MUTATE) h = TR("[A]MUTIEREN [X/Y]STÄRKE [<>^v]FELD [B]ZUM PATTERN", "[A]MUTATE [X/Y]AMOUNT [<>^v]FIELD [B]TO PATTERN");
    else h = TR("[A]AUSFÜHREN [<>^v]FELD [B]ZUM PATTERN [L2/R2]SEITE [SEL]KARTE", "[A]RUN [<>^v]FIELD [B]TO PATTERN [L2/R2]PAGE [SEL]MAP");
    hint(6, 452, h, C_ACC, C_DIM);
}
