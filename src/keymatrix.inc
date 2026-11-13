// Tasten-Matrix: JEDE Taste und Tastenkombination auf JEDER Seite und in JEDEM Fenster einzeln druecken und die
// interne Wirkung als Text festhalten (Seite, Regler, Werte, Steps, Mutes, Menues, Meldung, Einstellungen ...).
// Vergleich mit tests/keymatrix.golden (abgenommener Sollzustand): jede Abweichung = Fehler.
//   groovebox --keymatrix tests/keymatrix.golden            pruefen
//   groovebox --keymatrix tests/keymatrix.golden --update   Sollzustand neu schreiben (nach gewollten Aenderungen, Diff ansehen!)
// Vor jedem Fall wird alles auf einen festen Grundzustand gesetzt (Projekt, Einstellungen, Oberflaeche, Ablagen),
// danach kommen die Regeln (Werte im Bereich, Cursor/Regler gueltig, nichts haengt). Eingebunden am Ende von selftest.inc.

static Project* kmBase = nullptr;   // Stand nach dem Kontext-Aufbau (Vergleichsbasis)
static Project* kmOrig = nullptr;   // unveraenderter Grundzustand
static Settings kmCfg;
static Pattern kmPat;   // Pattern unter dem Cursor vor dem Druck

static uint64_t kmOrigHash = 0;
// Projekte/Sounds im Temp-Ordner auf den festen Stand bringen (nur wenn ein Fall etwas geschrieben hat)
static time_t kmDirStamp = 0; static long kmDirNs = 0; static bool kmPresets = false;
static void kmFiles() {
    char d[700]; snprintf(d, sizeof d, "%sprojects", basePath);
    struct stat st; char pr[700]; snprintf(pr, sizeof pr, "%spresets", basePath);
    bool presets = stat(pr, &st) == 0;
    if (stat(d, &st) == 0 && st.st_mtim.tv_sec == kmDirStamp && st.st_mtim.tv_nsec == kmDirNs && !presets && kmPresets == presets) {
        char pp[700]; projectPath("Matrix", pp, sizeof pp); struct stat s2;
        if (stat(pp, &s2) == 0 && s2.st_mtim.tv_sec <= kmDirStamp) return;   // nichts geaendert
    }
    char cmd[1400]; snprintf(cmd, sizeof cmd, "rm -rf '%s' '%s'; mkdir -p '%s'", d, pr, d);
    if (system(cmd) != 0) {}
    static const char* const NAMES[2] = {"Matrix", "Zweites"};
    for (int k = 0; k < 2; k++) { char pp[700]; projectPath(NAMES[k], pp, sizeof pp); FILE* f = fopen(pp, "wb"); if (f) { projectWrite(f, *kmOrig, nullptr); fclose(f); } }
    sleep(0); if (stat(d, &st) == 0) { kmDirStamp = st.st_mtim.tv_sec; kmDirNs = st.st_mtim.tv_nsec; }
    kmPresets = false;
}
static void kmReset() {
    for (int b = 0; b < BT_COUNT; b++) simDown[b] = false;
    simLX = simLY = simRX = simRY = 0;
    lockA();
    if (eng.playing) eng.stop();
    memcpy(&eng.proj, kmOrig, sizeof(Project));
    eng.fill = false; eng.recording = false; eng.songOn = false; eng.curPat = 0; eng.nextPat = -1; eng.dj = 0;
    for (int t = 0; t < NTRACKS; t++) eng.solo[t] = false;
    unlockA();
    cfg = kmCfg; g_lang = 0;
    page = PG_TRG; track = 4; slot = 0; cursor = 0;
    menuOpen = false; menuSel = PG_TRG; pgmOpen = false; pgmSel = 0; pgmAll = false; fbOpen = false; kbOpen = false; confirmOpen = false;
    sedOpen = false; wteOpen = false; mrOpen = false; mrSel = 0; v2View = V2_NONE; v2Foc[0] = 1; v2Foc[1] = 2; v2Foc[2] = 1; sndClip.have = false; setIn = false; setSel = SI_PATTERN; prjSel = PI_PAT0; prjPat = -1; prjBank = -1; clipFrom = -1;
    songSel = 0; songCol = 0; songExp = false; expField = 2; mixRow = 0; lockLatch = false; aHeld = false;
    haveClip = false; haveTrigClip = false; pageClipStore = -1; pgClipState = 0; trkClipFrom = -1; mutRnd = 0x1234567u; mutAmt = 1; tmplSel = 0;
    smpSrc = 0; smpChops = 0; smpMode = 0; smpNorm = 1; smpFade = 5; smpDest = 0; smpState = 0; pvOn = false; smpLen = 0; smpStart = smpEnd = 0;
    expState = 0; expFrom = 0; expTo = -1; usbApplyAt = 0; audioApplyAt = 0; quitReq = false; funcUsed = false; startHeldPlaying = false; startFill = false;
    undoCnt = 0; undoShadowPat = -1; redoCnt = 0;
    kmFiles();   // fester Datei-Stand
    snprintf(cfg.project, sizeof cfg.project, "Matrix"); projectPath(cfg.project, projPath, sizeof projPath);
    lastSavedHash = kmOrigHash; dirtyAt = 0;   // Projekt gilt als gespeichert
    for (int i = 0; i < 3; i++) simFrame();
    msg[0] = 0;
}

// ---- Zustand vorher/nachher
struct KmSnap {
    int page, track, slot, cursor, menuOpen, menuSel, pgmOpen, pgmSel, pgmAll, fbOpen, fbMode, fbSel, kbOpen, confirmOpen, confirmAct, sedOpen, wteOpen;
    int setIn, setSel, prjSel, prjPat, songSel, songCol, songExp, mixRow, lockLatch, aHeld, playing, fill, recording, curPat, nextPat, songOn, solo, dj;
    int haveClip, haveTrigClip, pageClip, pgClip, trkClip, mutAmt, tmplSel, smp[9], expFrom, expTo, usbApply, audioApply, quit, v2View, mrOpen, mrSel, sndClip;
};
static KmSnap kmTake() {
    KmSnap s; memset(&s, 0, sizeof s);
    s.page = page; s.track = track; s.slot = slot; s.cursor = cursor; s.menuOpen = menuOpen; s.menuSel = menuSel; s.pgmOpen = pgmOpen; s.pgmSel = pgmSel; s.pgmAll = pgmAll;
    s.fbOpen = fbOpen; s.fbMode = fbOpen ? fbMode : 0; s.fbSel = fbOpen ? fbSel : 0; s.kbOpen = kbOpen; s.confirmOpen = confirmOpen; s.confirmAct = confirmOpen ? confirmAct : 0;
    s.sedOpen = sedOpen; s.wteOpen = wteOpen; s.setIn = setIn; s.setSel = setSel; s.prjSel = prjSel; s.prjPat = prjPat; s.songSel = songSel; s.songCol = songCol; s.songExp = songExp;
    s.mixRow = mixRow; s.lockLatch = lockLatch; s.aHeld = aHeld; s.playing = eng.playing; s.fill = eng.fill; s.recording = eng.recording; s.curPat = eng.curPat; s.nextPat = eng.nextPat;
    s.songOn = eng.songOn; for (int t = 0; t < NTRACKS; t++) s.solo |= eng.solo[t] << t; s.dj = (int)(eng.dj * 100);
    s.haveClip = haveClip; s.haveTrigClip = haveTrigClip; s.pageClip = pageClipStore; s.pgClip = pgClipState; s.trkClip = trkClipFrom; s.mutAmt = mutAmt; s.tmplSel = tmplSel;
    int sm[9] = {smpSrc, smpChops, smpMode, smpNorm, smpFade, smpDest, smpState, pvOn, cfg.recPages}; memcpy(s.smp, sm, sizeof sm);
    s.v2View = v2View; s.mrOpen = mrOpen; s.mrSel = mrSel; s.sndClip = sndClip.have;
    s.expFrom = expFrom; s.expTo = expTo; s.usbApply = usbApplyAt != 0; s.audioApply = audioApplyAt != 0; s.quit = quitReq;
    return s;
}

static char kmOut[1400]; static int kmN = 0, kmItems = 0;
static void kmAdd(const char* fmt, ...) {
    if (kmItems++ > 14) { if (kmItems == 17) { if (kmN < (int)sizeof kmOut - 8) kmN += snprintf(kmOut + kmN, sizeof kmOut - kmN, "%s...", kmN ? "; " : ""); } return; }
    char b[200]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    if (kmN < (int)sizeof kmOut - 210) kmN += snprintf(kmOut + kmN, sizeof kmOut - kmN, "%s%s", kmN ? "; " : "", b);
}
static const char* kmPgName(int p) { return p >= 0 && p < NUI_PAGES ? PAGE_NAMES[p] : "?"; }

static void kmDiff(const KmSnap& a, const KmSnap& b, int pi0) {
    kmN = 0; kmOut[0] = 0; kmItems = 0;
    if (a.v2View != b.v2View) { static const char* const VN[5] = {"-", "SEQ", "SOUND", "EFFEKTE", "MASTER"}; kmAdd("Ansicht %s>%s", VN[a.v2View + 1], VN[b.v2View + 1]); }
    if (a.mrOpen != b.mrOpen) kmAdd(b.mrOpen ? "MEHR auf" : "MEHR zu");
    if (b.mrOpen && a.mrSel != b.mrSel) kmAdd("MEHR-Eintrag %d>%d", a.mrSel, b.mrSel);
    if (a.sndClip != b.sndClip) kmAdd("Sound-Ablage voll");
    if (a.page != b.page) kmAdd("Seite %s>%s", kmPgName(a.page), kmPgName(b.page));
    if (a.track != b.track) kmAdd("Spur %d>%d", a.track + 1, b.track + 1);
    if (a.slot != b.slot) kmAdd("Regler %d>%d", a.slot + 1, b.slot + 1);
    if (a.cursor != b.cursor) kmAdd("Step %d>%d", a.cursor + 1, b.cursor + 1);
    if (a.menuOpen != b.menuOpen) kmAdd(b.menuOpen ? "Seitenkarte auf" : "Seitenkarte zu");
    if (b.menuOpen && a.menuSel != b.menuSel) kmAdd("Karte %s>%s", kmPgName(a.menuSel), kmPgName(b.menuSel));
    if (a.pgmOpen != b.pgmOpen) kmAdd(b.pgmOpen ? "PAGE-Menue auf" : "PAGE-Menue zu");
    if (a.pgmSel != b.pgmSel) kmAdd("PAGE-Eintrag %d>%d", a.pgmSel, b.pgmSel);
    if (a.pgmAll != b.pgmAll) kmAdd("PAGE-Bereich %s", b.pgmAll ? "alle" : "Spur");
    if (a.fbOpen != b.fbOpen || a.fbMode != b.fbMode) kmAdd(b.fbOpen ? "Liste %d auf" : "Liste zu", b.fbMode);
    else if (b.fbOpen && a.fbSel != b.fbSel) kmAdd("Listeneintrag %d>%d", a.fbSel, b.fbSel);
    if (a.kbOpen != b.kbOpen) kmAdd(b.kbOpen ? "Tastatur auf" : "Tastatur zu");
    if (a.confirmOpen != b.confirmOpen || a.confirmAct != b.confirmAct) kmAdd(b.confirmOpen ? "Abfrage %d" : "Abfrage zu", b.confirmAct);
    if (a.sedOpen != b.sedOpen) kmAdd(b.sedOpen ? "Sample-Editor auf" : "Sample-Editor zu");
    if (a.wteOpen != b.wteOpen) kmAdd(b.wteOpen ? "Wavetable-Editor auf" : "Wavetable-Editor zu");
    if (a.setIn != b.setIn) kmAdd(b.setIn ? "SET Eintraege" : "SET Bereiche");
    if (a.setSel != b.setSel) kmAdd("SET %d>%d", a.setSel, b.setSel);
    if (a.prjSel != b.prjSel) kmAdd("PRJ-Feld %d>%d", a.prjSel, b.prjSel);
    if (a.prjPat != b.prjPat) kmAdd("PRJ-Ziel %s", PN(b.prjPat));
    if (a.songSel != b.songSel || a.songCol != b.songCol) kmAdd("Song-Zelle %d/%d>%d/%d", a.songSel, a.songCol, b.songSel, b.songCol);
    if (a.songExp != b.songExp) kmAdd(b.songExp ? "Export-Leiste" : "Matrix");
    if (a.mixRow != b.mixRow) kmAdd("Mixer-Zeile %d>%d", a.mixRow, b.mixRow);
    if (a.lockLatch != b.lockLatch) kmAdd(b.lockLatch ? "Lock-Modus an" : "Lock-Modus aus");
    if (a.aHeld != b.aHeld) kmAdd(b.aHeld ? "Step gehalten" : "Step losgelassen");
    if (a.playing != b.playing) kmAdd(b.playing ? "PLAY" : "STOP");
    if (a.fill != b.fill) kmAdd(b.fill ? "FILL an" : "FILL aus");
    if (a.recording != b.recording) kmAdd(b.recording ? "REC an" : "REC aus");
    if (a.curPat != b.curPat) kmAdd("Pattern %s>%s", PN(a.curPat), PN(b.curPat));
    if (a.nextPat != b.nextPat) kmAdd("naechstes Pattern %s", PN(b.nextPat));
    if (a.songOn != b.songOn) kmAdd(b.songOn ? "Song an" : "Song aus");
    if (a.solo != b.solo) kmAdd("Solo %02x>%02x", a.solo, b.solo);
    if (a.dj != b.dj) kmAdd("DJ-Filter %d", b.dj);
    if (a.haveClip != b.haveClip) kmAdd("Pattern-Ablage voll");
    if (a.haveTrigClip != b.haveTrigClip) kmAdd("Trig-Ablage voll");
    if (a.pageClip != b.pageClip) kmAdd("Seiten-Ablage %s", kmPgName(b.pageClip));
    if (a.pgClip != b.pgClip) kmAdd("Page-Ablage %d", b.pgClip);
    if (a.trkClip != b.trkClip) kmAdd("Spur-Ablage T%d", b.trkClip + 1);
    if (a.mutAmt != b.mutAmt) kmAdd("Mutation %d", b.mutAmt);
    if (a.tmplSel != b.tmplSel) kmAdd("Vorlage %d", b.tmplSel);
    for (int i = 0; i < 9; i++) if (a.smp[i] != b.smp[i]) {
        static const char* const SN[9] = {"Quelle", "Chops", "Chop-Art", "Normal.", "Fade", "Ziel", "Aufnahme", "Vorhoeren", "Laenge"};
        kmAdd("Sampler %s %d>%d", SN[i], a.smp[i], b.smp[i]);
    }
    if (a.expFrom != b.expFrom || a.expTo != b.expTo) kmAdd("Export %d-%d", b.expFrom, b.expTo);
    if (b.usbApply) kmAdd("USB-Umbau geplant");
    if (b.audioApply) kmAdd("Audio neu oeffnen");
    if (b.quit) kmAdd("BEENDEN");
    // Pattern unter dem Cursor: Engine, Laengen, Parameter, Steps
    const Pattern& A = kmPat; const Pattern& B = eng.proj.pat[pi0];
    for (int t = 0; t < NTRACKS; t++) {
        if (A.machine[t] != B.machine[t]) kmAdd("T%d Engine %s>%s", t + 1, MACHINE_NAMES[A.machine[t] % M_COUNT], MACHINE_NAMES[B.machine[t] % M_COUNT]);
        if (A.len[t] != B.len[t]) kmAdd("T%d Laenge %d>%d", t + 1, A.len[t], B.len[t]);
        if (A.speed[t] != B.speed[t]) kmAdd("T%d Tempo %d>%d", t + 1, A.speed[t], B.speed[t]);
        if (A.machine[t] == B.machine[t])
            for (int i = 0; i < NPARAMS; i++) if (A.p[t][i] != B.p[t][i]) kmAdd("T%d.%s %d>%d", t + 1, paramDef(B.machine[t] % M_COUNT, i).name, A.p[t][i], B.p[t][i]);
        int ns = 0, first = -1;
        for (int s = 0; s < NSTEPS; s++) if (memcmp(&A.s[t][s], &B.s[t][s], sizeof(Step))) { ns++; if (first < 0) first = s; }
        if (ns == 1) {
            const Step& x = A.s[t][first]; const Step& y = B.s[t][first];
            if (x.on != y.on) kmAdd("T%d Step %d %d>%d", t + 1, first + 1, x.on, y.on);
            for (int i = 0; i < NPARAMS; i++) if (x.lock[i] != y.lock[i]) kmAdd("T%d Step %d Lock %s %d>%d", t + 1, first + 1, paramDef(B.machine[t] % M_COUNT, i).name, x.lock[i], y.lock[i]);
        } else if (ns) kmAdd("T%d %d Steps geaendert (ab %d)", t + 1, ns, first + 1);
    }
    if (A.chg != B.chg) kmAdd("Wechsel %d>%d", A.chg, B.chg);
    // andere Patterns und Projekt-Werte
    for (int pi = 0; pi < NPATTERNS; pi++) if (pi != pi0 && memcmp(&eng.proj.pat[pi], &kmBase->pat[pi], sizeof(Pattern))) kmAdd("Pattern %s geaendert", PN(pi));
    if (eng.proj.bpm != kmBase->bpm) kmAdd("BPM %d>%d", kmBase->bpm, eng.proj.bpm);
    for (int i = 0; i < 8; i++) if (eng.proj.mst[i] != kmBase->mst[i]) kmAdd("Master.%s %d>%d", MST_DEFS[i].name, kmBase->mst[i], eng.proj.mst[i]);
    for (int i = 0; i < 8; i++) if (eng.proj.cmp[i] != kmBase->cmp[i]) kmAdd("Kompr.%s %d>%d", CMP_DEFS[i].name, kmBase->cmp[i], eng.proj.cmp[i]);
    for (int bb = 0; bb < NBUS; bb++) for (int i = 0; i < 8; i++) if (eng.proj.bus[bb][i] != kmBase->bus[bb][i]) kmAdd("Bus %c.%d %d>%d", 'A' + bb, i, kmBase->bus[bb][i], eng.proj.bus[bb][i]);
    if (eng.proj.songLen != kmBase->songLen) kmAdd("Song-Zeilen %d>%d", kmBase->songLen, eng.proj.songLen);
    if (memcmp(eng.proj.row, kmBase->row, sizeof eng.proj.row)) kmAdd("Song-Zeilen geaendert");
    for (int pi = 0; pi < NPATTERNS; pi++) if (eng.proj.patMute[pi] != kmBase->patMute[pi]) kmAdd("Mutes %s %02x>%02x", PN(pi), kmBase->patMute[pi], eng.proj.patMute[pi]);
    // Einstellungen
    {
        const int* x = &kmCfg.buf; const int* y = &cfg.buf;   // int-Felder ab buf bis usbNetOn
        static const char* const CN[] = {"buf", "midiIn", "notesOut", "clk", "chm", "usb", "song", "dispOff", "usbAudio", "hwVol", "pcIn", "usbVol", "intUsb", "cores", "lang", "usbNet", "uac", "uacSendOnly", "usbCh", "hostMc", "usbBint", "usbNetOn"};
        for (int i = 0; i < (int)(sizeof CN / sizeof *CN); i++) if (x[i] != y[i]) kmAdd("cfg.%s %d>%d", CN[i], x[i], y[i]);
        if (memcmp(kmCfg.usbRoute, cfg.usbRoute, sizeof cfg.usbRoute)) kmAdd("cfg.usbRoute geaendert");
        if (strcmp(kmCfg.dev, cfg.dev)) kmAdd("cfg.dev");
        if (kmCfg.recPages != cfg.recPages) kmAdd("cfg.recPages %d>%d", kmCfg.recPages, cfg.recPages);
        if (kmCfg.newPage != cfg.newPage) kmAdd("cfg.newPage %d>%d", kmCfg.newPage, cfg.newPage);
        if (kmCfg.recThr != cfg.recThr) kmAdd("cfg.recThr %d>%d", kmCfg.recThr, cfg.recThr);
        if (kmCfg.listFold != cfg.listFold) kmAdd("cfg.listFold %d>%d", kmCfg.listFold, cfg.listFold);
        if (kmCfg.preview != cfg.preview) kmAdd("cfg.preview %d>%d", kmCfg.preview, cfg.preview);
        if (strcmp(kmCfg.project, cfg.project)) kmAdd("Projekt '%s'", cfg.project);
    }
    if (msg[0]) kmAdd("Meldung \"%s\"", msg);
    if (!kmN) snprintf(kmOut, sizeof kmOut, "-");
}

// ---- Regeln nach jedem Druck (gelten immer)
static int kmRuleFails = 0;
static void kmRules(const char* where) {
    char e[160] = "";
    const Pattern& P = curP();
    if (page < 0 || page >= NUI_PAGES || !pageAvail(page)) snprintf(e, sizeof e, "Seite %d ungueltig/nicht verfuegbar", page);
    else if (track < 0 || track >= NTRACKS) snprintf(e, sizeof e, "Spur %d", track);
    else if (slot < 0 || slot >= nSlots()) snprintf(e, sizeof e, "Regler %d von %d", slot, nSlots());
    else if (cursor < 0 || cursor >= NSTEPS) snprintf(e, sizeof e, "Cursor %d", cursor);
    else if ((int)menuOpen + (int)pgmOpen + (int)fbOpen + (int)kbOpen + (int)sedOpen + (int)wteOpen + (int)mrOpen > 1) snprintf(e, sizeof e, "mehrere Fenster gleichzeitig offen");
    else if (eng.curPat < 0 || eng.curPat >= NPATTERNS || eng.nextPat >= NPATTERNS) snprintf(e, sizeof e, "Pattern %d/%d", eng.curPat, eng.nextPat);
    else for (int t = 0; t < NTRACKS && !e[0]; t++) {
        if (P.len[t] < 1 || P.len[t] > NSTEPS) snprintf(e, sizeof e, "T%d Laenge %d", t + 1, P.len[t]);
        else if (P.speed[t] > 6) snprintf(e, sizeof e, "T%d Tempo %d", t + 1, P.speed[t]);
        else if (P.machine[t] >= M_COUNT) snprintf(e, sizeof e, "T%d Engine %d", t + 1, P.machine[t]);
        else for (int i = 0; i < NPARAMS && !e[0]; i++) {
            ParamDef d = paramDef(P.machine[t], i);
            int v = P.p[t][i];
            if (d.kind == K_NONE) continue;
            if (d.kind == K_SMPL && (v == SMP_NONE || v < MAX_SAMPLES)) continue;
            if (v > d.max) snprintf(e, sizeof e, "T%d.%s = %d > max %d", t + 1, d.name, v, d.max);
        }
    }
    if (e[0]) { if (kmRuleFails++ < 30) printf("  REGEL VERLETZT [%s]: %s\n", where, e); }
}

// ---- Eingaben
struct KmIn { const char* name; void (*run)(); };
static void kmTap(int b) { simDown[b] = true; simFrame(); simFrame(); simDown[b] = false; simFrame(); simFrame(); }
static void kmCombo(int m, int b) { simDown[m] = true; simFrame(); kmTap(b); simDown[m] = false; simFrame(); simFrame(); }
static void kmHold(int b, int ms) { simDown[b] = true; for (int t = 0; t < ms; t += 16) simFrame(); simDown[b] = false; simFrame(); simFrame(); }
#define KI(n, code) {n, [] { code; }}
static const KmIn KM_IN[] = {
    KI("HOCH", kmTap(BT_UP)), KI("RUNTER", kmTap(BT_DOWN)), KI("LINKS", kmTap(BT_LEFT)), KI("RECHTS", kmTap(BT_RIGHT)),
    KI("A", kmTap(BT_A)), KI("B", kmTap(BT_B)), KI("X", kmTap(BT_X)), KI("Y", kmTap(BT_Y)),
    KI("L1", kmTap(BT_L1)), KI("R1", kmTap(BT_R1)), KI("L2", kmTap(BT_L2)), KI("R2", kmTap(BT_R2)),
    KI("START", kmTap(BT_START)), KI("SELECT", kmTap(BT_SELECT)),
    KI("A lang", kmHold(BT_A, 700)), KI("X lang", kmHold(BT_X, 1500)), KI("Y lang", kmHold(BT_Y, 1500)), KI("START lang", kmHold(BT_START, 600)), KI("SELECT lang", kmHold(BT_SELECT, 900)),
    KI("SEL+HOCH", kmCombo(BT_SELECT, BT_UP)), KI("SEL+RUNTER", kmCombo(BT_SELECT, BT_DOWN)), KI("SEL+LINKS", kmCombo(BT_SELECT, BT_LEFT)), KI("SEL+RECHTS", kmCombo(BT_SELECT, BT_RIGHT)),
    KI("SEL+A", kmCombo(BT_SELECT, BT_A)), KI("SEL+B", kmCombo(BT_SELECT, BT_B)), KI("SEL+X", kmCombo(BT_SELECT, BT_X)), KI("SEL+Y", kmCombo(BT_SELECT, BT_Y)),
    KI("SEL+L1", kmCombo(BT_SELECT, BT_L1)), KI("SEL+R1", kmCombo(BT_SELECT, BT_R1)), KI("SEL+L2", kmCombo(BT_SELECT, BT_L2)), KI("SEL+R2", kmCombo(BT_SELECT, BT_R2)),
    KI("SEL+START", kmCombo(BT_SELECT, BT_START)),
    KI("A+B", kmCombo(BT_A, BT_B)), KI("A+X", kmCombo(BT_A, BT_X)), KI("A+Y", kmCombo(BT_A, BT_Y)), KI("A+HOCH", kmCombo(BT_A, BT_UP)), KI("A+RUNTER", kmCombo(BT_A, BT_DOWN)),
    KI("A+L1", kmCombo(BT_A, BT_L1)), KI("A+R1", kmCombo(BT_A, BT_R1)), KI("A+SEL+L1", { simDown[BT_A] = true; simFrame(); kmCombo(BT_SELECT, BT_L1); simDown[BT_A] = false; simFrame(); simFrame(); }),
    KI("A+SEL+R1", { simDown[BT_A] = true; simFrame(); kmCombo(BT_SELECT, BT_R1); simDown[BT_A] = false; simFrame(); simFrame(); }),
    KI("Stick R hoch", { simRY = -1.f; for (int i = 0; i < 20; i++) simFrame(); simRY = 0; simFrame(); simFrame(); }),
    KI("Stick L rechts", { simLX = 1.f; simFrame(); simFrame(); simLX = 0; for (int i = 0; i < 4; i++) simFrame(); }),
};
enum { KM_NIN = sizeof KM_IN / sizeof *KM_IN };
// kurze Eingabeliste fuer Wert-Felder (SET-Eintraege, PRJ-Felder, Song-Spalten, Mixer-Zeilen)
static const int KM_VALUE_IN[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

// ---- Zusammenhaenge (Kontexte): vor jedem Druck hergestellt
struct KmCtx { char name[64]; void (*setup)(int); int arg; bool shortList; };
static KmCtx kmCtx[900]; static int kmNCtx = 0;
static void kmCtxAdd(const char* name, void (*f)(int), int arg, bool shortList = false) {
    if (kmNCtx >= (int)(sizeof kmCtx / sizeof *kmCtx)) return;
    KmCtx& c = kmCtx[kmNCtx++]; snprintf(c.name, sizeof c.name, "%s", name); c.setup = f; c.arg = arg; c.shortList = shortList;
}
static void kmBuildContexts() {
    char n[64];
    for (int p = 0; p < NUI_PAGES; p++) {   // jede Seite (Spur 5 = SYNTH)
        if (p == PG_SYS) continue;   // SET eigene Kontexte
        snprintf(n, sizeof n, "Seite %s", PAGE_NAMES[p]);
        kmCtxAdd(n, [](int a) { page = a; if (!pageAvail(page)) page = PG_TRG; }, p);
    }
    static const int SRCP[5] = {PG_SRC, PG_SRC2, PG_SRC3, PG_SRC4, PG_SRC5};
    for (int m = 0; m < M_COUNT; m++) for (int p : SRCP) {   // Quell-Seiten je Engine
        bool av = p == PG_SRC || (p == PG_SRC2 ? machineHasSrc2(m) : machineHasSrc35(m));
        if (!av) continue;
        snprintf(n, sizeof n, "%s auf %s", PAGE_NAMES[p], MACHINE_NAMES[m]);
        kmCtxAdd(n, [](int a) { track = 0; lockA(); Engine::setMachine(curP(), 0, a / 64); unlockA(); page = a % 64; }, m * 64 + p);
    }
    kmCtxAdd("TRIG mit Trig auf Step 1", [](int) { curP().s[track][0].on = 1; }, 0);
    kmCtxAdd("TRIG Lock-Modus", [](int) { simDown[BT_A] = true; for (int i = 0; i < 40; i++) simFrame(); simDown[BT_A] = false; simFrame(); simFrame(); }, 0);
    kmCtxAdd("TRIG spielt", [](int) { lockA(); eng.start(); unlockA(); simFrame(); }, 0);
    kmCtxAdd("TRIG Spur 32 Steps, Page 2", [](int) { curP().len[track] = 32; cursor = 20; }, 0);
    kmCtxAdd("Seitenkarte offen", [](int) { menuOpen = true; menuSel = PG_TRG; }, 0);
    // Oberflaeche: jede Taste in SEQ, SOUND (Makros, Filter, Huellkurve), Lock-Modus, MEHR-Menue
    kmCtxAdd("NEU SEQ", [](int) { page = PG_TRG; slot = 0; simFrame(); }, 0);
    kmCtxAdd("NEU SEQ Trig auf Step 1", [](int) { curP().s[track][0].on = 1; page = PG_TRG; slot = 0; simFrame(); }, 0);
    kmCtxAdd("NEU SOUND Makros", [](int) { v2GoView(1); simFrame(); }, 0);
    kmCtxAdd("NEU SOUND Filter", [](int) { page = PG_FLT; slot = 0; simFrame(); }, 0);
    kmCtxAdd("NEU SOUND Huellkurve 1", [](int) { page = PG_ENV1; slot = 4; simFrame(); }, 0);
    kmCtxAdd("NEU SOUND Lock-Modus", [](int) { page = PG_FLT; slot = 0; simFrame(); simDown[BT_A] = true; for (int i = 0; i < 40; i++) simFrame(); simDown[BT_A] = false; simFrame(); simFrame(); }, 0);
    kmCtxAdd("NEU EFFEKTE Bus A", [](int) { page = PG_FX; slot = 0; simFrame(); }, 0);
    kmCtxAdd("NEU EFFEKTE Master", [](int) { page = PG_MST; slot = 0; simFrame(); }, 0);
    kmCtxAdd("NEU MIX", [](int) { page = PG_MIX; simFrame(); }, 0);
    kmCtxAdd("NEU MEHR offen", [](int) { mrOpen = true; mrSel = 0; }, 0);
    for (int i = 0; i < MR_COUNT; i++) { char nn[64]; snprintf(nn, sizeof nn, "NEU MEHR Eintrag %d", i); kmCtxAdd(nn, [](int a) { mrOpen = true; mrSel = a; }, i, true); }
    // Ablagen gefuellt: Einfuegen wirklich pruefen
    kmCtxAdd("TRIG mit Trig-Ablage", [](int) { memset(&trigClip, 0, sizeof trigClip); memset(trigClip.lock, NO_LOCK, NPARAMS); trigClip.on = 1; trigClip.lock[P_NOTE] = 48; haveTrigClip = true; cursor = 3; }, 0);
    kmCtxAdd("Seitenkarte mit Seiten-Ablage FILTER1 von T1", [](int) { menuOpen = true; menuSel = PG_FLT; pageClipStore = PG_FLT; pageClipTrack = 0; pageClipMachine = curP().machine[0];
        for (int i = 0; i < uiSlots(PG_FLT); i++) pageClip[i] = (uint8_t)(10 + i); }, 0);
    kmCtxAdd("PAGE-Menue mit Page-/Spur-Ablage (T1)", [](int) { Pattern& P = curP(); P.s[0][2].on = 1; P.s[0][2].lock[P_VEL] = 90;
        for (int i = 0; i < 16; i++) pgClip[0][i] = P.s[0][i]; pgClipState = 1; pgClipTrack = 0; pgClipPage = 0;
        trkClip.machine = P.machine[0]; trkClip.len = P.len[0]; trkClip.speed = P.speed[0]; memcpy(trkClip.p, P.p[0], NPARAMS); memcpy(trkClip.s, P.s[0], sizeof trkClip.s); trkClipFrom = 0; trkClipPat = 0;
        openPageMenu(); }, 0);
    for (int i = 0; i < PM_COUNT; i++) { char nn[64]; snprintf(nn, sizeof nn, "PAGE-Menue mit Ablage, Eintrag %d", i);
        kmCtxAdd(nn, [](int a) { Pattern& P = curP(); P.s[0][2].on = 1; for (int k = 0; k < 16; k++) pgClip[0][k] = P.s[0][k]; pgClipState = 1; pgClipTrack = 0;
            trkClip.machine = P.machine[0]; trkClip.len = 32; trkClip.speed = P.speed[0]; memcpy(trkClip.p, P.p[0], NPARAMS); memcpy(trkClip.s, P.s[0], sizeof trkClip.s); trkClipFrom = 0;
            openPageMenu(); pgmSel = a; }, i, true); }
    kmCtxAdd("PRJ mit Pattern-Ablage, Feld EINFUEGEN, Ziel A02", [](int) { page = PG_PRJ; clip = eng.proj.pat[0]; clip.s[0][0].on = 1; haveClip = true; clipFrom = 0; prjPat = 1; prjSel = PI_PASTE; }, 0);
    kmCtxAdd("TRIG mit Undo-Verlauf (Trig gesetzt)", [](int) { simFrame(); curP().s[track][5].on = 1; for (int i = 0; i < 70; i++) simFrame(); }, 0);
    kmCtxAdd("PAGE-Menue offen", [](int) { openPageMenu(); }, 0);
    for (int i = 0; i < PM_COUNT; i++) { snprintf(n, sizeof n, "PAGE-Menue Eintrag %d", i); kmCtxAdd(n, [](int a) { openPageMenu(); pgmSel = a; }, i, true); }
    kmCtxAdd("Abfrage offen (Spur leeren)", [](int) { askConfirm(ACT_CLEARTRK, track, "TEST"); }, 0);
    kmCtxAdd("Tastatur offen", [](int) { openKeyboard(KB_SAVEAS, "Test"); }, 0);
    kmCtxAdd("Liste Engine", [](int) { page = PG_TRG; slot = 7; openBrowser(FB_MACH); }, 0);
    kmCtxAdd("Liste Bedingung", [](int) { page = PG_TRG; fbParam = P_COND; openBrowser(FB_COND); }, 0);
    kmCtxAdd("Liste LFO-Ziel", [](int) { page = PG_LFO1; fbParam = P_L1 + LF_DEST; openBrowser(FB_LDEST); }, 0);
    kmCtxAdd("Liste Sound", [](int) { openBrowser(FB_PRESET); }, 0);
    kmCtxAdd("Liste Projekt", [](int) { openBrowser(FB_PROJECT); }, 0);
    kmCtxAdd("Liste Sample-Slots", [](int) { track = 7; page = PG_SRC; openBrowser(FB_SLOT); }, 0);
    kmCtxAdd("Liste Samples", [](int) { track = 7; page = PG_SRC; openBrowser(FB_SAMPLE); }, 0);
    kmCtxAdd("Sample-Editor", [](int) { track = 7; page = PG_SRC; lockA(); curP().p[7][P_S0] = 0; unlockA(); if (sampleGet(0)) openSmpEditor(); }, 0);
    kmCtxAdd("Wavetable-Editor", [](int) { track = 6; lockA(); Engine::setMachine(curP(), 6, M_WTBL); unlockA(); page = PG_SRC2; openWtEditor(); }, 0);
    kmCtxAdd("SET Bereiche", [](int) { page = PG_SYS; setIn = false; }, 0);
    for (int i = 0; i < SI_COUNT; i++) {
        if (setIsHeader(i) || setHidden(i)) continue;
        if (i == SI_BENCH || i == SI_DRIVE || i == SI_QUIT || i == SI_LEARN || i == SI_PCREC || i == SI_USBTONE || i == SI_DEMO) continue;   // Systemaktionen (eigene Tests)
        char l[64], v[64]; setLabel(i, l, sizeof l, v, sizeof v);
        snprintf(n, sizeof n, "SET %d %s", i, l);
        kmCtxAdd(n, [](int a) { page = PG_SYS; setIn = true; setSel = a; }, i, true);
    }
    for (int i = 0; i < PI_COUNT; i++) { snprintf(n, sizeof n, "PRJ Feld %d", i); kmCtxAdd(n, [](int a) { page = PG_PRJ; prjSel = a; }, i, true); }
    for (int c = 0; c < SC_N; c++) { snprintf(n, sizeof n, "SONG Spalte %d", c); kmCtxAdd(n, [](int a) { page = PG_SONG; lockA(); eng.proj.songLen = 2; Engine::rowFromPattern(eng.proj, eng.proj.row[0], 0); Engine::rowFromPattern(eng.proj, eng.proj.row[1], 1); eng.proj.row[0].rep = eng.proj.row[1].rep = 1; unlockA(); songCol = a; }, c, true); }
    for (int r = 0; r < MIX_ROWS; r++) { snprintf(n, sizeof n, "MIX Zeile %d", r); kmCtxAdd(n, [](int a) { page = PG_MIX; mixRow = a; }, r, true); }
    for (int k = 0; k < SK_COUNT; k++) { snprintf(n, sizeof n, "SAMPLER Regler %d", k + 1); kmCtxAdd(n, [](int a) { page = PG_SMPR; slot = a; }, k, true); }
}

// ---- Zusammenhaenge: zufaellige Bedienketten (feste Startwerte), Regeln nach jedem Schritt,
// zwischendurch Speichern+Laden (muss gleich bleiben) und Rueckgaengig/Wiederholen (muss genau zurueckfuehren).
static int kmChainFails = 0, kmSaveChecks = 0, kmUndoChecks = 0, kmUndoSteps = 0;
static void kmFail(const char* fmt, ...) {
    if (kmChainFails++ >= 30) return;
    char b[300]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    printf("  ZUSAMMENHANG FEHLER: %s\n", b);
}
static bool kmPatEq(const Pattern& a, const Pattern& b) { Pattern x = a, y = b; x.used = y.used = 0; return !memcmp(&x, &y, sizeof(Pattern)); }
static void kmCheckSaveLoad(int seed, int step) {
    kmSaveChecks++;
    if (!saveProject()) { kmFail("Kette %d Schritt %d: Speichern fehlgeschlagen", seed, step); return; }
    static Engine* chk = nullptr; if (!chk) chk = newEngine();
    if (!chk->load(projPath)) { kmFail("Kette %d Schritt %d: gespeichertes Projekt nicht ladbar", seed, step); return; }
    static Pattern fresh; static bool fr = false; if (!fr) { Engine::initPattern(fresh); fr = true; }
    for (int pi = 0; pi < NPATTERNS; pi++) {
        const Pattern& A = eng.proj.pat[pi]; const Pattern& B = chk->proj.pat[pi];
        if (patternUsed(A)) { if (!kmPatEq(A, B)) { kmFail("Kette %d Schritt %d: Pattern %s nach Speichern+Laden anders", seed, step, PN(pi)); return; } }
        else if (!kmPatEq(B, fresh)) { kmFail("Kette %d Schritt %d: unbelegtes Pattern %s kommt nicht frisch zurueck", seed, step, PN(pi)); return; }
    }
    if (memcmp((const uint8_t*)&eng.proj + 20 + (long)NPATTERNS * sizeof(Pattern), (const uint8_t*)&chk->proj + 20 + (long)NPATTERNS * sizeof(Pattern), PROJECT_TAIL_SIZE)
        || eng.proj.bpm != chk->proj.bpm || memcmp(eng.proj.mst, chk->proj.mst, 8))
        kmFail("Kette %d Schritt %d: Projekt-Werte (Tempo/Master/Busse/Song) nach Speichern+Laden anders", seed, step);
}
static void kmCheckUndo(uint32_t& r, int seed, int step) {
    // auf einer Spurseite 1-5 Aenderungen mit Pausen (= je ein Rueckgaengig-Schritt), dann alles zurueck und wieder vor
    v2View = V2_NONE; page = PG_TRG; slot = 0; mrOpen = false; menuOpen = pgmOpen = fbOpen = kbOpen = confirmOpen = sedOpen = wteOpen = false; lockLatch = aHeld = false;
    for (int i = 0; i < 60; i++) simFrame();   // vorherige Aenderungen abschliessen
    int pi = curPi(); Pattern before = eng.proj.pat[pi];
    int n = 1 + (int)(r % 5), done = 0;
    for (int k = 0; k < n; k++) {
        r ^= r << 13; r ^= r >> 17; r ^= r << 5;
        Pattern prev = eng.proj.pat[pi];
        switch (r % 4) {
        case 0: cursor = (int)((r >> 8) % 16); kmTap(BT_A); break;               // Trig an/aus
        case 1: kmTap(BT_X); break;                                              // Wert +
        case 2: cursor = (int)((r >> 8) % 16); kmCombo(BT_A, BT_Y); break;       // Lock
        default: kmTap(BT_Y); break;
        }
        for (int i = 0; i < 60; i++) simFrame();   // > 0,8 s Pause: eigener Schritt
        if (getenv("GB_KM_DEBUG")) printf("    Undo-Probe k=%d op=%u page=%d slot=%d view=%d fb=%d pi=%d/%d changed=%d\n", k, r % 4, page, slot, v2View, fbOpen, pi, curPi(), !kmPatEq(prev, eng.proj.pat[pi]));
        if (!kmPatEq(prev, eng.proj.pat[pi])) done++;
    }
    Pattern after = eng.proj.pat[pi]; kmUndoChecks++; kmUndoSteps += done;
    for (int k = 0; k < done; k++) { kmCombo(BT_SELECT, BT_B); for (int i = 0; i < 4; i++) simFrame(); }
    if (!kmPatEq(before, eng.proj.pat[pi])) kmFail("Kette %d Schritt %d: %d x Rueckgaengig fuehrt nicht zum Stand davor zurueck", seed, step, done);
    for (int k = 0; k < done; k++) { redoStep(); simFrame(); }
    if (done && !kmPatEq(after, eng.proj.pat[pi])) kmFail("Kette %d Schritt %d: Wiederholen fuehrt nicht zum Stand danach", seed, step);
}
static void kmChains(int seeds, int steps) {
    // gefaehrliche/langsame Eingaben gibt es hier nicht (selfTesting); Abfragen werden manchmal bestaetigt
    Uint32 t0 = SDL_GetTicks(); int total = 0;
    for (int sd = 1; sd <= seeds; sd++) {
        kmReset();
        uint32_t r = 0x9E3779B9u * (uint32_t)sd;
        for (int st = 0; st < steps; st++) {
            r ^= r << 13; r ^= r >> 17; r ^= r << 5;
            int k = (int)(r % KM_NIN);
            if (r % 53 == 0) { int p = (int)((r >> 7) % NUI_PAGES); if (pageAvail(p)) page = p; }   // Sprung auf eine beliebige Seite
            if (r % 41 == 0) track = (int)((r >> 9) % NTRACKS);
            KM_IN[k].run();
            quitReq = false; usbApplyAt = 0; audioApplyAt = 0; expState = expState == 1 ? 1 : 0;
            char where[96]; snprintf(where, sizeof where, "Kette %d Schritt %d (%s)", sd, st, KM_IN[k].name);
            int before = kmRuleFails; kmRules(where); if (kmRuleFails != before) kmChainFails++;
            if (st % 400 == 399) kmCheckSaveLoad(sd, st);
            if (st % 250 == 120) kmCheckUndo(r, sd, st);
            total++;
        }
    }
    printf("Zusammenhaenge: %d Ketten, %d Schritte, %d x Speichern+Laden, %d x Rueckgaengig/Wiederholen (%d Schritte), %d Fehler in %.1f s\n", seeds, total, kmSaveChecks, kmUndoChecks, kmUndoSteps, kmChainFails, (SDL_GetTicks() - t0) / 1000.0);
}

static int runKeymatrix(const char* golden, bool update) {
    selfTesting = true;
    char tmp[600]; snprintf(tmp, sizeof tmp, "/tmp/gb-keymatrix-%d/", (int)getpid());
    mkdir(tmp, 0755);
    char sub[700]; snprintf(sub, sizeof sub, "%sprojects", tmp); mkdir(sub, 0755);
    snprintf(basePath, sizeof basePath, "%s", tmp);
    {   // eigene, immer gleiche Test-Samples (die Matrix darf nicht von der Sample-Bibliothek des Rechners abhaengen)
        static const char* const NM[8] = {"drums/kick-boom", "drums/snare-tight", "drums/hat-closed", "fx/riser", "keys/pad-warm", "keys/stab-minor", "loops/beat-90", "vox/hey"};
        static float buf[22050 * 2];
        uint32_t r = 1;
        for (int k = 0; k < 8; k++) {
            int n = 4000 + k * 2500;
            for (int i = 0; i < n; i++) {
                r = r * 1664525u + 1013904223u;
                float v = 0.4f * sinf(i * (0.01f + 0.004f * k)) * (1.f - (float)i / n) + (k % 3 == 1 ? ((int)(r >> 9) % 2000 - 1000) / 5000.f : 0.f);
                buf[i * 2] = v; buf[i * 2 + 1] = k == 6 ? -v : v;   // ein Stereo-Sample
            }
            char d[800]; snprintf(d, sizeof d, "%ssamples/%.*s", tmp, (int)(strchr(NM[k], '/') - NM[k]), NM[k]);
            char sd[800]; snprintf(sd, sizeof sd, "%ssamples", tmp); mkdir(sd, 0755); mkdir(d, 0755);
            char f[900]; snprintf(f, sizeof f, "%ssamples/%s.wav", tmp, NM[k]); writeWav(f, buf, n, 44100);
        }
        char sd[800]; snprintf(sd, sizeof sd, "%ssamples", tmp);
        int n = samplesLoad(sd);
        for (int i = n; i < PREVIEW_SLOT; i++) if (sampleGet(i)) poolSetSlot(i, "");   // Rest des Pools leeren
    }
    snprintf(cfgPath, sizeof cfgPath, "%sinput.cfg", tmp);
    snprintf(setPath, sizeof setPath, "%ssettings.cfg", tmp);
    snprintf(cfg.project, sizeof cfg.project, "Matrix");
    projectPath(cfg.project, projPath, sizeof projPath);
    lockA(); eng.defaultProject(false); unlockA();
    kmBase = (Project*)malloc(sizeof(Project)); kmOrig = (Project*)malloc(sizeof(Project));
    memcpy(kmOrig, &eng.proj, sizeof(Project)); kmOrigHash = projectHash(*kmOrig);
    Settings baseCfg = cfg; baseCfg.newPage = 1; baseCfg.recPages = 0; baseCfg.recThr = -40; baseCfg.lang = 0;
    kmCfg = baseCfg;
    kmBuildContexts();
    const char* ext = getenv("GB_KM_FILTER");   // nur Kontexte mit diesem Text (Fehlersuche)
    // Soll laden
    static char* gold = nullptr; long gn = 0;
    if (!update) {
        FILE* f = fopen(golden, "rb");
        if (!f) { printf("FEHLER: Sollzustand %s fehlt (erst mit --update erzeugen)\n", golden); return 1; }
        fseek(f, 0, SEEK_END); gn = ftell(f); fseek(f, 0, SEEK_SET);
        gold = (char*)malloc(gn + 1); gn = (long)fread(gold, 1, gn, f); gold[gn] = 0; fclose(f);
    }
    FILE* out = update ? fopen(golden, "wb") : nullptr;
    if (update && !out) { printf("FEHLER: %s nicht schreibbar\n", golden); return 1; }
    if (out) fprintf(out, "# Tasten-Matrix SYNTH CITY: Kontext | Eingabe | Wirkung (erzeugt mit groovebox --keymatrix <datei> --update)\n");
    int cases = 0, diffs = 0, missing = 0;
    Uint32 t0 = SDL_GetTicks();
    for (int c = 0; c < kmNCtx; c++) {
        if (ext && !strstr(kmCtx[c].name, ext)) continue;
        int nin = kmCtx[c].shortList ? (int)(sizeof KM_VALUE_IN / sizeof *KM_VALUE_IN) : KM_NIN;
        for (int k = 0; k < nin; k++) {
            const KmIn& in = KM_IN[kmCtx[c].shortList ? KM_VALUE_IN[k] : k];
            kmReset();
            kmCtx[c].setup(kmCtx[c].arg);
            simFrame(); msg[0] = 0;
            int pi0 = curPi();
            memcpy(&kmPat, &eng.proj.pat[pi0], sizeof(Pattern));
            memcpy(kmBase, &eng.proj, sizeof(Project));   // Kontext-Aufbau zaehlt nicht als Wirkung
            KmSnap a = kmTake();
            Uint32 c0 = SDL_GetTicks();
            in.run();
            KmSnap b = kmTake();
            kmDiff(a, b, pi0);
            char where[160]; snprintf(where, sizeof where, "%s | %s", kmCtx[c].name, in.name);
            kmRules(where);
            if (SDL_GetTicks() - c0 > 1500 && kmRuleFails++ < 30) printf("  REGEL VERLETZT [%s]: dauerte %u ms (Einfrieren?)\n", where, SDL_GetTicks() - c0);
            char line[1700]; snprintf(line, sizeof line, "%s | %s", where, kmOut);
            cases++;
            if (out) fprintf(out, "%s\n", line);
            else {
                char key[200]; snprintf(key, sizeof key, "\n%s | ", where);
                const char* g = strstr(gold, key);
                if (!g) { if (missing++ < 40) printf("  NEU (nicht im Sollzustand): %s\n", line); continue; }
                g += strlen(key);
                const char* e = strchr(g, '\n'); int gl = e ? (int)(e - g) : (int)strlen(g);
                if ((int)strlen(kmOut) != gl || strncmp(g, kmOut, gl)) {
                    if (diffs++ < 60) printf("  ABWEICHUNG [%s]\n      Soll: %.*s\n      Ist:  %s\n", where, gl, g, kmOut);
                }
            }
        }
    }
    if (out) fclose(out);
    // Kontexte, die es nicht mehr gibt
    int gone = 0;
    if (gold && !ext) {
        for (char* l = strchr(gold, '\n'); l && l[1]; l = strchr(l + 1, '\n')) gone++;
        gone -= cases; if (gone < 0) gone = 0;
    }
    if (!update && !ext) kmChains(6, 1500);
    printf("Tasten-Matrix: %d Kontexte, %d Eingaben je Kontext (kurz %d), %d Faelle in %.1f s\n", kmNCtx, KM_NIN, (int)(sizeof KM_VALUE_IN / sizeof *KM_VALUE_IN), cases, (SDL_GetTicks() - t0) / 1000.0);
    if (update) { printf("Sollzustand geschrieben: %s (Regel-Verletzungen: %d)\n", golden, kmRuleFails); return kmRuleFails ? 1 : 0; }
    printf("%s: %d Abweichungen, %d neue Faelle, %d entfallene Zeilen, %d Regel-Verletzungen, %d Zusammenhang-Fehler\n", diffs || missing || gone || kmRuleFails || kmChainFails ? "FEHLGESCHLAGEN" : "OK", diffs, missing, gone, kmRuleFails, kmChainFails);
    char cmd[700]; snprintf(cmd, sizeof cmd, "rm -rf '%s'", tmp);
    if (system(cmd) != 0) {}
    return diffs || missing || gone || kmRuleFails || kmChainFails ? 1 : 0;
}
