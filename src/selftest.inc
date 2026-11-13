// Leistungstest, Stresstest und UI-Tests (wird am Ende von main.cpp eingebunden, nutzt dessen Zustand).
//   groovebox --bench [datei]      Audio-Last je Engine + Worst Case, Zeichenzeit je Seite, Speicher
//   groovebox --selftest [sek]     UI-Szenarien, Bedienweg-Report, UI-Monkey, Engine-Stress (Rueckgabe 1 bei Fehler)
// Auf dem Geraet: SET -> "Leistungstest" (schreibt logs/bench.txt, im Browser unter Logs).

#include <sys/resource.h>

static FILE* benchOut = nullptr;
static void bout(const char* fmt, ...) {
    char b[400];
    va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    fputs(b, stdout);
    if (benchOut) fputs(b, benchOut);
}
static double nowMs() { return SDL_GetPerformanceCounter() * 1000.0 / SDL_GetPerformanceFrequency(); }
static void memInfo(long& rssKb, long& peakKb) {
    rssKb = peakKb = -1;
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return;
    char l[200];
    while (fgets(l, sizeof l, f)) {
        if (!strncmp(l, "VmRSS:", 6)) rssKb = atol(l + 6);
        if (!strncmp(l, "VmHWM:", 6)) peakKb = atol(l + 6);
    }
    fclose(f);
}
static void cpuInfo(char* b, int n) {
    snprintf(b, n, "unbekannt");
    FILE* f = fopen("/proc/cpuinfo", "r");
    int cores = 0; char model[96] = "";
    if (f) {
        char l[256];
        while (fgets(l, sizeof l, f)) {
            if (!strncmp(l, "processor", 9)) cores++;
            if (!model[0] && (!strncmp(l, "model name", 10) || !strncmp(l, "CPU part", 8) || !strncmp(l, "Hardware", 8))) {
                char* c = strchr(l, ':'); if (c) { snprintf(model, sizeof model, "%s", c + 2); model[strcspn(model, "\n")] = 0; }
            }
        }
        fclose(f);
    }
    long mhz = 0;
    FILE* q = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (q) { if (fscanf(q, "%ld", &mhz) != 1) mhz = 0; fclose(q); }
    snprintf(b, n, "%d Kerne, %s%s%ld MHz", cores, model[0] ? model : "?", mhz ? ", " : "", mhz / 1000);
}

// ---------------------------------------------------------------- Audio-Last

static Engine* benchEngP = nullptr;   // eigene Engine: die laufende Wiedergabe bleibt unberuehrt
struct BenchRes { double load, peak; float lvl; bool bad; double voices; };
static BenchRes benchRender(Engine& E, double secs) {
    static float blk[256 * 2];
    const int B = 256;
    int blocks = (int)(secs * E.sr / B);
    double budget = B * 1000.0 / E.sr, tot = 0, worst = 0;
    float pk = 0; bool bad = false; double vsum = 0;
    E.start();
    for (int i = 0; i < blocks; i++) {
        double t0 = nowMs();
        E.render(blk, B);
        double el = nowMs() - t0;
        tot += el; if (el > worst) worst = el;
        for (int t = 0; t < NTRACKS; t++) { if (E.v[t].active) vsum += 1; for (int k = 0; k < NCHORDV; k++) if (E.cv[t][k].active) vsum += 1; }
        for (int k = 0; k < B * 2; k++) { float x = blk[k]; if (!(x == x)) bad = true; else if (fabsf(x) > pk) pk = fabsf(x); }
    }
    E.stop();
    BenchRes r = {tot / (blocks * budget) * 100.0, worst / budget * 100.0, pk, bad, vsum / blocks};
    return r;
}
// alle 8 Spuren mit Engine m, jeder Step ein Trig, typische Einstellungen
static void benchSetupEngine(Engine& E, int m) {
    E.defaultProject(false);
    E.songOn = false; E.curPat = 0; E.nextPat = -1;
    E.proj.bpm = 128;
    Pattern& P = E.proj.pat[0];
    for (int t = 0; t < NTRACKS; t++) {
        Engine::initTrack(P, t, m);
        if (m == M_SAMPLE || m == M_CHOP || m == M_GRAN) P.p[t][P_S0] = (uint8_t)(t % (sampleCount() ? sampleCount() : 1));
        for (int s = 0; s < 16; s++) P.s[t][s].on = 1;
        P.p[t][P_LEN] = 60; P.p[t][P_DLY] = 40; P.p[t][P_REV] = 40;
        P.p[t][P_NOTE] = (uint8_t)(48 + t * 3);
    }
}
static void benchSetupWorst(Engine& E) {
    static const int ms[NTRACKS] = {M_SYNTH, M_WTBL, M_FM, M_GRAN, M_CHOP, M_SAMPLE, M_SYNTH, M_SNARE};
    E.defaultProject(false);
    E.songOn = false; E.curPat = 0; E.nextPat = -1;
    E.proj.bpm = 174;
    Pattern& P = E.proj.pat[0];
    for (int t = 0; t < NTRACKS; t++) {
        int m = ms[t];
        Engine::initTrack(P, t, m);
        if (m == M_SAMPLE || m == M_CHOP || m == M_GRAN) P.p[t][P_S0] = (uint8_t)(t % (sampleCount() ? sampleCount() : 1));
        if (m == M_GRAN) { P.p[t][P_S4] = 127; P.p[t][P_S3] = 40; }   // maximale Kornrate
        for (int s = 0; s < 16; s++) P.s[t][s].on = 1;
        P.p[t][P_LEN] = 100; P.p[t][P_DLY] = 90; P.p[t][P_REV] = 90; P.p[t][P_RESO] = 100;
        P.p[t][P_FTYP] = (uint8_t)(t == 0 ? FT_LADR : (t == 6 ? FT_CMBP : (t == 1 ? FT_VOWL : FT_LP24)));
        P.p[t][P_RTRG] = 6;
        P.p[t][P_ITYP] = (uint8_t)(1 + t % (INS_COUNT - 1)); P.p[t][P_IMIX] = 100; P.p[t][P_CRSH] = 40; P.p[t][P_SRR] = 30; P.p[t][P_DRIV] = 60;
        P.p[t][P_BASE] = 20; P.p[t][P_WDTH] = 90;
        static const int lb[3] = {P_L1, P_L2, P_L3};
        for (int k = 0; k < 3; k++) { P.p[t][lb[k] + LF_DEST] = (uint8_t)(k == 0 ? P_FREQ + 1 : (k == 1 ? P_PAN + 1 : P_S2 + 1)); P.p[t][lb[k] + LF_DEPTH] = 110; P.p[t][lb[k] + LF_RATE] = 100; P.p[t][lb[k] + LF_WAVE] = LW_SH; }
        if (!machineIsDrum(m)) { P.p[t][P_AMOD] = (t == 0 || t == 1 || t == 2) ? ARP_CHORD : 1; P.p[t][P_ACHD] = 5; P.p[t][P_AOCT] = 2; P.p[t][P_ARAT] = 0; }   // 3 Spuren mit 4-stimmigen Akkorden
    }
    E.proj.cmp[CM_THR] = 30; E.proj.cmp[CM_SRC] = 1; E.proj.cmp[CM_MIX] = 127;
    E.proj.mst[MS_SWNG] = 10;
}
static void benchAudio() {
    if (!benchEngP) benchEngP = newEngine();
    Engine& E = *benchEngP;
    E.init(44100);
    bout("\nAUDIO (44,1 kHz, Bloecke zu 256 Frames = %.1f ms Budget)\n", 256000.0 / 44100);
    bout("  %-26s %8s %10s %7s %8s %10s\n", "Szenario", "Last", "Spitze", "Pegel", "Stimmen", "je Stimme");
    E.defaultProject(false);
    BenchRes r = benchRender(E, 2.0);
    bout("  %-26s %7.1f%% %9.1f%% %7.2f\n", "Leerlauf (leeres Pattern)", r.load, r.peak, r.lvl);
    double idle = r.load;
    double worstEng = 0; int worstM = 0;
    E.audioThreads = 1;   // Engine-Tabelle mit einem Kern (vergleichbar mit frueheren Messungen)
    for (int i = 0; i < M_COUNT; i++) {
        int m = MACHINE_ORDER[i];
        benchSetupEngine(E, m);
        r = benchRender(E, 3.0);
        char nm[40]; snprintf(nm, sizeof nm, "8x %s", MACHINE_NAMES[m]);
        double per = r.voices > 0.1 ? (r.load - idle) / r.voices : 0;
        bout("  %-26s %7.1f%% %9.1f%% %7.2f %8.1f %9.2f%%%s\n", nm, r.load, r.peak, r.lvl, r.voices, per, r.bad ? "  NAN!" : "");
        if (per > worstEng) { worstEng = per; worstM = m; }
    }
    int thr = E.helperReady() ? 2 : 1;
    for (int k = 1; k <= thr; k++) {   // realistischer Song und Worst Case mit 1 und (falls vorhanden) 2 Kernen
        E.audioThreads = k;
        char nm[48];
        buildDemo(E); E.songOn = true; E.songStart(0);
        r = benchRender(E, 6.0);
        E.songOn = false;
        snprintf(nm, sizeof nm, "DEMO-SONG, %d KERN%s", k, k > 1 ? "E" : "");
        bout("  %-26s %7.1f%% %9.1f%% %7.2f %8.1f\n", nm, r.load, r.peak, r.lvl, r.voices);
        benchSetupWorst(E);
        r = benchRender(E, 4.0);
        snprintf(nm, sizeof nm, "WORST CASE, %d KERN%s", k, k > 1 ? "E" : "");
        bout("  %-26s %7.1f%% %9.1f%% %7.2f %8.1f%s\n", nm, r.load, r.peak, r.lvl, r.voices, r.bad ? "  NAN!" : "");
    }
    E.audioThreads = thr;
    bout("  -> teuerste Engine je Stimme: %s (%.2f%%). Reserve im Worst Case: %.0f%% eines Kerns\n", MACHINE_NAMES[worstM], worstEng, 100.0 - r.load);
    if (r.peak > 80) bout("  !! einzelne Bloecke brauchen >80%% des Budgets: Knackser bei kleinem Puffer moeglich\n");
}

// ---------------------------------------------------------------- Zeichenzeit

static void benchUi(int frames, bool withPresent) {
    int sp0 = page, st0 = track, ss0 = slot; bool mo0 = menuOpen;
    bout("\nBILDAUFBAU (%d Bilder je Seite%s, Ziel < 16,7 ms = 60 fps)\n", frames, withPresent ? ", inkl. Ausgabe aufs Display" : "");
    bout("  %-22s %8s %8s %9s\n", "Seite", "Mittel", "Max", withPresent ? "Ausgabe" : "");
    double sum = 0, worst = 0; int worstP = 0, n = 0;
    for (int p = 0; p <= NUI_PAGES + 1; p++) {
        menuOpen = false;
        if (p < NUI_PAGES) { page = p; if (!pageAvail(p)) continue; }
        else if (p == NUI_PAGES) { page = PG_FLT; menuOpen = true; menuSel = PG_FLT; }
        track = 4; slot = 0;
        double tot = 0, mx = 0, pres = 0;
        for (int i = 0; i < frames; i++) {
            double t0 = nowMs();
            drawAll(p == NUI_PAGES + 1);
            double t1 = nowMs();
            if (withPresent) present();
            double el = nowMs() - t0;
            pres += nowMs() - t1;
            tot += el; if (el > mx) mx = el;
        }
        const char* nm = p < NUI_PAGES ? PAGE_LONG[p] : (p == NUI_PAGES ? "SEITENMENUE" : "HILFE");
        if (withPresent) bout("  %-22s %6.2fms %6.2fms %7.2fms\n", nm, tot / frames, mx, pres / frames);
        else bout("  %-22s %6.2fms %6.2fms\n", nm, tot / frames, mx);
        sum += tot / frames; n++;
        if (tot / frames > worst) { worst = tot / frames; worstP = p; }
    }
    bout("  -> Mittel über alle Seiten %.2f ms, langsamste: %s (%.2f ms)\n", sum / n,
         worstP < NUI_PAGES ? PAGE_LONG[worstP] : (worstP == NUI_PAGES ? "SEITENMENUE" : "HILFE"), worst);
    page = sp0; track = st0; slot = ss0; menuOpen = mo0;
}

static void runBench(const char* outPath, bool onDevice) {
    benchOut = outPath ? fopen(outPath, "w") : nullptr;
    char ci[200]; cpuInfo(ci, sizeof ci);
    time_t tt = time(nullptr); char ts[40]; strftime(ts, sizeof ts, "%Y-%m-%d %H:%M", localtime(&tt));
    bout("SYNTH CITY Leistungstest %s  Version %s\n", ts, versionStr[0] ? versionStr : "dev");
    bout("CPU: %s\nSamples: %d   Zeichnen: %s\n", ci, sampleCount(), fbPix ? "eigene Pixel-Routinen" : "SDL-Software-Renderer");
    if (onDevice) bout("Hinweis: die laufende Wiedergabe rechnet nebenher weiter (CPU Audio jetzt %d%%)\n", (int)(cpuLoad * 100));
    benchAudio();
    benchUi(onDevice ? 30 : 60, onDevice && drmSurf);
    long rss, peak; memInfo(rss, peak);
    bout("\nSPEICHER: %ld MB belegt, Spitze %ld MB (Projekt %d KB, Engine %d KB)\n", rss / 1024, peak / 1024,
         (int)(sizeof(Project) / 1024), (int)(sizeof(Engine) / 1024));
    if (benchOut) { fclose(benchOut); benchOut = nullptr; printf("Bericht: %s\n", outPath); }
}

// ---------------------------------------------------------------- UI-Simulation

static Uint32 simNow = 1000000;
static bool simDown[BT_COUNT];
static float simLX = 0, simLY = 0, simRX = 0, simRY = 0;
static int simKeys = 0;   // gezaehlte Bedienhandlungen (Tastendruck, Stick-Antippen, Stick-Geste)
static bool simDraw = false;
static void simFrame() {
    for (int b = 0; b < BT_COUNT; b++) {
        in.prev[b] = in.down[b];
        in.down[b] = simDown[b];
        if (in.down[b] && !in.prev[b]) { in.downSince[b] = simNow; in.lastRep[b] = simNow; }
    }
    in.lx = simLX; in.ly = simLY; in.rx = simRX; in.ry = simRY;
    handleInput(simNow, 0.016f);
    undoTrack(simNow);
    if (simDraw) drawAll(false);
    simNow += 16;
}
static void simWait(int ms) { for (int t = 0; t < ms; t += 16) simFrame(); }
static void tap(int b) { simKeys++; simDown[b] = true; simFrame(); simFrame(); simDown[b] = false; simFrame(); simFrame(); }
static void holdDown(int b) { simKeys++; simDown[b] = true; simFrame(); }
static void release(int b) { simDown[b] = false; simFrame(); simFrame(); }
static void tapAt(int x, int y) {   // Finger kurz aufs Bild (zeichnet vorher, damit die Touch-Flaechen stimmen)
    bool d = simDraw; simDraw = true; simFrame(); simKeys++; remoteTouchInject('t', 1, x, y); simFrame(); remoteTouchInject('e', 1, x, y); simFrame(); simFrame(); simDraw = d;
}
static void combo(int mod, int b) { holdDown(mod); tap(b); release(mod); }   // z.B. SELECT + A
static void flickL(int dx, int dy) { simKeys++; simLX = (float)dx; simLY = (float)dy; simFrame(); simFrame(); simLX = simLY = 0; simFrame(); simFrame(); simFrame(); }
static void stickR(float ry, int ms) { simKeys++; simRY = ry; simWait(ms); simRY = 0; simFrame(); simFrame(); }
static void simReset() {
    for (int b = 0; b < BT_COUNT; b++) simDown[b] = false;
    simLX = simLY = simRX = simRY = 0;
    for (int i = 0; i < 6; i++) simFrame();
    page = PG_TRG; track = 0; slot = 0; cursor = 0; menuOpen = false; confirmOpen = false; aHeld = false; lockLatch = false;
    mrOpen = false; pgmOpen = false;   // Fenster der Oberflaeche: ein offenes MEHR-Menue faengt sonst alle Tasten ab
    eng.curPat = 0; eng.nextPat = -1; eng.songOn = false;
    for (int i = 0; i < 4; i++) simFrame();
    simKeys = 0;
}

static int uiPass = 0, uiFail = 0;
static const char* uiCase = "";
// Szenario-Liste fuer den Testbericht (tools/test-all.sh): je Fall Anzahl Pruefungen und Fehler
enum { MAX_CASES = 160 };
static struct { const char* name; int pass, fail; } uiCases[MAX_CASES]; static int nCases = 0;
static void caseCount(bool ok) {
    int k = -1; for (int i = 0; i < nCases; i++) if (uiCases[i].name == uiCase || !strcmp(uiCases[i].name, uiCase)) k = i;
    if (k < 0 && nCases < MAX_CASES) { k = nCases++; uiCases[k].name = uiCase; uiCases[k].pass = uiCases[k].fail = 0; }
    if (k >= 0) { if (ok) uiCases[k].pass++; else uiCases[k].fail++; }
}
#define UCHECK(c, ...) do { if (c) { uiPass++; caseCount(true); } else { uiFail++; caseCount(false); printf("  FEHLER [%s] Zeile %d: ", uiCase, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// Weg zur Seite: in derselben Gruppe mit L2/R2, sonst ueber die Seitenkarte (SELECT, Steuerkreuz, A)
// Die Oberflaeche: Ansicht per L2/R2 (Ring V2_RING), darin die Liste per Steuerkreuz hoch/runter.
// Die Liste beginnt mit Makros, die auf dieselbe Seite zeigen - deshalb ueber v2FindItem2 den
// echten Gruppeneintrag ansteuern und nicht einfach bis "page == target" tippen.
static void goView(int target) {   // Ansicht, in der die Zielseite liegt
    simFrame();
    int want = v2RingOfPage(target);   // 0 SEQ, 1 SOUND, 2 EFFEKTE, ab 3 eigene Ansicht
    for (int g = 0; g < V2_RING_N * 2; g++) {
        if (want >= 3 ? page == target : v2View == want) return;
        tap(BT_R2);
    }
}
static void goItem(int pg, int k) {   // Listeneintrag (Seite, Regler) ansteuern
    if (!v2Active()) return;
    for (int g = 0; g < 240; g++) {
        int want = v2FindAny(pg, k);
        if (want < 0) return;           // Regler gibt es in dieser Liste nicht
        int cur = v2Foc[v2View];
        if (cur == want) return;        // Eintrag erreicht (v2Apply setzt page/slot)
        tap(cur < want ? BT_DOWN : BT_UP);
    }
}
static void goPage(int target) {
    goView(target);
    if (!v2Active()) { if (page != target) { printf("  goPage(%d) haengt in '%s' (Seite %d)\n", target, uiCase, page); fflush(stdout); } return; }
    goItem(target, 0);
    for (int k = 1; k < 12 && page != target; k++) goItem(target, k);   // Regler 0 kann leer sein
    if (page != target) { printf("  goPage(%d) haengt in '%s' (Seite %d)\n", target, uiCase, page); fflush(stdout); }
}
static void goSlot(int target) {
    if (!v2Active()) return;
    int pg = page;
    goItem(pg, target);
    if (slot != target) { printf("  goSlot(%d) haengt in '%s' (Seite %d, Regler %d)\n", target, uiCase, page, slot); fflush(stdout); }
}
// Touch-Flaeche aus der zuletzt gezeichneten Registry holen (statt Koordinaten zu raten)
static bool hitFind(int kind, int a, int& hx, int& hy) {
    for (int i = nHits - 1; i >= 0; i--)
        if (hits[i].k == kind && hits[i].a == a) { hx = hits[i].x + hits[i].w / 2; hy = hits[i].y + hits[i].h / 2; return true; }
    return false;
}
static int visibleRow() {   // irgendeine gerade sichtbare Listenzeile (die Liste scrollt im Test nicht animiert)
    for (int i = 0; i < 14; i++) simFrame();
    for (int i = nHits - 1; i >= 0; i--) if (hits[i].k == HK_V2ROW) return hits[i].a;
    return -1;
}
static bool tapHit(int kind, int a) {   // Flaeche antippen; false = gibt es nicht
    int hx, hy;
    for (int i = 0; i < 14; i++) simFrame();   // die Liste scrollt animiert zum Fokus: auslaufen lassen
    if (!hitFind(kind, a, hx, hy)) return false;
    tapAt(hx, hy); return true;
}
static bool dragHit(int kind, int a, int dx, int dy, int steps) {   // Flaeche greifen und ziehen
    int hx, hy;
    for (int i = 0; i < 14; i++) simFrame();   // Scroll-Animation auslaufen lassen, sonst liegt die Zeile woanders
    if (!hitFind(kind, a, hx, hy)) return false;
    remoteTouchInject('t', 1, hx, hy); simFrame();
    for (int i = 1; i <= steps; i++) { remoteTouchInject('m', 1, hx + dx * i / steps, hy + dy * i / steps); simFrame(); }
    remoteTouchInject('e', 1, hx + dx, hy + dy); simFrame();
    return true;
}
static void moreRun(int entry) {   // MEHR-Menue (SELECT): Eintrag waehlen und ausfuehren
    simFrame();
    if (!mrOpen) tap(BT_SELECT);
    mrSel = entry; simFrame();
    tap(BT_A);
}
static void goPrj(int target) {   // PRJ-Seite: Steuerkreuz bis zum Feld
    for (int guard = 0; guard < 60 && prjSel != target; guard++) {
        PrjBox a = prjBox(prjSel), b = prjBox(target);
        if (a.y != b.y) tap(a.y < b.y ? BT_DOWN : BT_UP); else tap(a.x < b.x ? BT_RIGHT : BT_LEFT);
    }
    if (prjSel != target) { printf("  goPrj(%d) haengt in '%s' (bei %d)\n", target, uiCase, prjSel); fflush(stdout); }
}
static void goTrack(int t) {   // Spurseiten: Spur mit L1/R1, sonst Steuerkreuz   // Grenzen: ein Test darf nie haengen
    simFrame();
    bool v2 = trackPage(page);
    for (int g = 0; track != t && g < 20; g++) tap(track < t ? (v2 ? BT_R1 : BT_DOWN) : (v2 ? BT_L1 : BT_UP));
}
static void goStep(int s) { for (int g = 0; cursor != s && g < 300; g++) tap(cursor < s ? BT_RIGHT : BT_LEFT); }

static void uiScenarios() {
    Pattern& P = curP();
    uiCase = "Trig setzen/löschen";
    simReset(); P.s[0][0].on = 0;
    tap(BT_A); UCHECK(curP().s[0][0].on == 1, "A setzt keinen Trig");
    tap(BT_A); UCHECK(curP().s[0][0].on == 0, "A loescht den Trig nicht");

    uiCase = "Navigation";
    simReset(); simFrame();
    tap(BT_RIGHT); tap(BT_RIGHT);        // SEQ: Steuerkreuz links/rechts = Step
    UCHECK(cursor == 2, "Cursor %d statt 2", cursor);
    tap(BT_R1); UCHECK(track == 1, "R1 waehlt die Spur nicht (%d)", track);
    tap(BT_L1); UCHECK(track == 0, "L1 kehrt nicht zur Spur 1 zurueck (%d)", track);
    tap(BT_DOWN); UCHECK(slot == 1 && page == PG_TRG, "RUNTER: naechster Regler erwartet (Seite %d, Regler %d)", page, slot + 1);
    tap(BT_UP); UCHECK(slot == 0, "HOCH: erster Regler erwartet (Regler %d)", slot + 1);
    tap(BT_R2); UCHECK(v2View == V2_SOUND, "R2: Ansicht SOUND erwartet (%d)", v2View);
    tap(BT_R2); UCHECK(v2View == V2_FX, "R2: Ansicht EFFEKTE erwartet (%d)", v2View);
    tap(BT_L2); tap(BT_L2); UCHECK(v2View == V2_SEQ, "2x L2: zurueck zu SEQ (%d)", v2View);
    tap(BT_SELECT); UCHECK(mrOpen, "SELECT oeffnet das MEHR-Menue nicht");
    tap(BT_B); UCHECK(!mrOpen, "B schliesst das MEHR-Menue nicht");
    goPage(PG_FLT); UCHECK(page == PG_FLT, "FILTER-Gruppe nicht erreichbar (Seite %d)", page);
    goPage(PG_TRG); UCHECK(page == PG_TRG, "zurueck zu TRIG nicht moeglich (Seite %d)", page);

    uiCase = "Wert aendern";
    simReset(); goPage(PG_FLT); goSlot(0);
    int f0 = curP().p[0][P_FREQ];
    tap(BT_Y); UCHECK(curP().p[0][P_FREQ] == f0 - 1, "Y: FREQ %d statt %d", curP().p[0][P_FREQ], f0 - 1);
    stickR(1.f, 400); UCHECK(curP().p[0][P_FREQ] > f0 - 1, "rechter Stick hebt FREQ nicht");

    uiCase = "P-Lock";
    simReset(); simFrame();
    goStep(4);                 // Step in SEQ waehlen (dort liegt das Steuerkreuz auf den Steps)
    goPage(PG_FLT); goSlot(0);
    curP().s[0][4].on = 0; memset(curP().s[0][4].lock, NO_LOCK, NPARAMS);
    holdDown(BT_A); tap(BT_Y); release(BT_A);
    UCHECK(curP().s[0][4].on == 1 && curP().s[0][4].lock[P_FREQ] != NO_LOCK, "A halten + Y: kein Lock (on=%d)", curP().s[0][4].on);
    tap(BT_A);
    UCHECK(curP().s[0][4].on == 2, "Trig mit Lock antippen -> trigless erwartet (on=%d)", curP().s[0][4].on);
    tap(BT_A);
    UCHECK(curP().s[0][4].on == 1, "trigless antippen -> Trig erwartet (on=%d)", curP().s[0][4].on);

    uiCase = "Lock-Modus";
    simReset(); goPage(PG_FLT);
    holdDown(BT_A); simWait(600); release(BT_A);
    UCHECK(lockLatch, "A lang halten rastet den Lock-Modus nicht ein");
    tap(BT_X); tap(BT_RIGHT);
    UCHECK(cursor == 1 && lockLatch, "im Lock-Modus: Rechts -> Step %d", cursor);
    tap(BT_DOWN); UCHECK(lockLatch && slot == 1, "im Lock-Modus: RUNTER waehlt keinen Regler (Regler %d)", slot + 1);
    tap(BT_R2); UCHECK(lockLatch && page != PG_FLT, "im Lock-Modus: R2 wechselt die Seite nicht");
    goPage(PG_SRC); UCHECK(lockLatch && page == PG_SRC, "im Lock-Modus: Klangseite nicht erreichbar (Seite %d)", page);
    tap(BT_X); UCHECK(holdS().lock[P_S0 + slot] != NO_LOCK || slot > 7, "im Lock-Modus: Lock auf der Klangseite fehlt");
    tap(BT_A); UCHECK(!lockLatch, "A beendet den Lock-Modus nicht");

    uiCase = "FILL mit START";
    simReset(); tap(BT_START); UCHECK(eng.playing, "START spielt nicht");
    holdDown(BT_START); simWait(400); UCHECK(eng.fill, "START halten beim Spielen: kein FILL");
    release(BT_START); simWait(2); UCHECK(!eng.fill && eng.playing, "START loslassen nach FILL: FILL aus und weiter spielen erwartet");
    tap(BT_START); UCHECK(!eng.playing, "START kurz stoppt nicht");

    uiCase = "Trig kopieren";
    simReset();
    curP().s[0][0].on = 1; curP().s[0][0].lock[P_VEL] = 33;
    holdDown(BT_A); combo(BT_SELECT, BT_L1); release(BT_A);
    goStep(3);
    holdDown(BT_A); combo(BT_SELECT, BT_R1); release(BT_A);
    UCHECK(curP().s[0][3].on == 1 && curP().s[0][3].lock[P_VEL] == 33, "eingefuegter Trig: on=%d VEL-Lock=%d", curP().s[0][3].on, curP().s[0][3].lock[P_VEL]);

    uiCase = "Mute/Play";
    simReset();
    bool m0 = eng.muted(0);
    combo(BT_SELECT, BT_A); UCHECK(eng.muted(0) != m0, "SELECT+A schaltet nicht stumm");
    combo(BT_SELECT, BT_A);
    tap(BT_START); UCHECK(eng.playing, "START spielt nicht");
    tap(BT_START); UCHECK(!eng.playing, "START stoppt nicht");

    uiCase = "MEHR-Menue";
    simReset(); simFrame();
    tap(BT_SELECT); UCHECK(mrOpen, "SELECT antippen oeffnet kein Menue");
    tap(BT_B); tap(BT_SELECT);
    if (mrOpen) { tap(BT_B); }
    UCHECK(!mrOpen, "Menue laesst sich nicht schliessen");

    uiCase = "Song-Seite verlassen";
    simReset(); page = PG_SONG; simWait(50);
    tap(BT_R2); UCHECK(page != PG_SONG, "R2 verlaesst die Song-Seite nicht");
    page = PG_SONG; simWait(50);
    tap(BT_L2); UCHECK(page != PG_SONG, "L2 verlaesst die Song-Seite nicht");

    uiCase = "FM-Operatoren";
    {
        simReset(); goTrack(5);
        lockA(); applyFactory(curP(), 5, factoryByName("DX E-PIANO")); unlockA();
        goPage(PG_SRC3);
        const char* pl = pageLong(page);
        UCHECK(page == PG_SRC3 && !strncmp(pl, "OPERATOR 2", 10), "FM: Operator-2-Seite nicht erreichbar (Seite %d, '%s')", page, pl);
        UCHECK(strlen(pl) > 10, "FM: die Rolle des Operators steht nicht in der Gruppe ('%s')", pl);
        goSlot(OP_LVL); simFrame();   // wie ein Mensch durch die Liste, nicht slot von Hand setzen
        UCHECK(slot == OP_LVL, "FM: O.LVL von Operator 2 nicht erreichbar (Regler %d)", slot);
        int l0 = curP().p[5][FM_OPB[1] + OP_LVL];
        tap(BT_X); UCHECK(curP().p[5][FM_OPB[1] + OP_LVL] == l0 + 1, "FM: X aendert O.LVL nicht");
        tap(BT_DOWN); UCHECK(slot == OP_ATK, "FM: RUNTER -> O.ATK erwartet (Regler %d)", slot + 1);
        goPage(PG_SRC5); UCHECK(page == PG_SRC5, "FM: Operator 4 nicht erreichbar");
        lockA(); Engine::setMachine(curP(), 5, M_KICK); unlockA(); simWait(2);
        simFrame(); UCHECK(!pageAvail(PG_SRC3) && !v2HasPage(PG_SRC3), "Operator-Seiten bei KICK noch in der Liste");
        lockA(); Engine::setMachine(curP(), 5, M_FM); unlockA();
    }

    uiCase = "FM: Rollen und Übersicht";
    {   // gemeldet: "der FM-Synth kommt komisch daher, ich habe immer nur einen Pluck-Sound"
        // Derselbe Regler O.LVL ist beim Traeger Lautstaerke und beim Modulator Helligkeit -
        // das muss dranstehen, sonst ist FM nicht nachvollziehbar.
        simReset(); goTrack(5);
        lockA(); Engine::setMachine(curP(), 5, M_FM); unlockA();
        curP().p[5][P_S0] = 0;   // Algorithmus 0: OP1 Traeger, OP2-4 Modulatoren
        simFrame(); v2Dirty();
        goPage(PG_SRC2); goSlot(OP_LVL); simFrame();
        ParamDef d; int val, bv; bool lk; char lab[24];
        UCHECK(knobInfo(slot, d, val, lk, bv, lab, sizeof lab) && !strcmp(d.name, "O.LVL"),
               "Traeger muesste O.LVL heissen (%s)", d.name);
        goPage(PG_SRC3); goSlot(OP_LVL); simFrame();
        UCHECK(knobInfo(slot, d, val, lk, bv, lab, sizeof lab) && !strcmp(d.name, "INDEX"),
               "Modulator muesste INDEX heissen (%s)", d.name);
        UCHECK(strstr(pageLong(PG_SRC2), TR("TRÄGER", "CARRIER")) != nullptr, "Traeger steht nicht in der Gruppe (%s)", pageLong(PG_SRC2));
        UCHECK(strstr(pageLong(PG_SRC3), "OP1") != nullptr, "Das Ziel des Modulators fehlt (%s)", pageLong(PG_SRC3));
        // Algorithmus 7: alle vier sind Traeger, die Namen muessen umschalten
        curP().p[5][P_S0] = 7; v2Dirty(); simFrame();
        goPage(PG_SRC3); goSlot(OP_LVL); simFrame();
        UCHECK(knobInfo(slot, d, val, lk, bv, lab, sizeof lab) && !strcmp(d.name, "O.LVL"),
               "Bei Algorithmus 7 ist OP2 Traeger, muesste also O.LVL heissen (%s)", d.name);
        UCHECK(strstr(pageLong(PG_SRC3), TR("TRÄGER", "CARRIER")) != nullptr, "Rolle folgt dem Algorithmus nicht (%s)", pageLong(PG_SRC3));
        // Uebersicht: alle vier Operatoren in einer Gruppe, Namen sagen die Rolle
        v2Build(V2_SOUND);
        int ov = -1; for (int i = 0; i < v2N; i++) if (v2It[i].hdr && v2It[i].title && strstr(v2It[i].title, TR("ÜBERSICHT", "OVERVIEW"))) ov = i;
        UCHECK(ov >= 0, "Die FM-Übersicht fehlt in der Liste");
        if (ov >= 0) {
            int n = 0; for (int i = ov + 1; i < v2N && !v2It[i].hdr; i++) n++;
            UCHECK(n == 8, "Die Übersicht muesste 8 Zeilen haben (%d)", n);
            bool rat = false, lvl = false;
            for (int i = ov + 1; i < v2N && !v2It[i].hdr; i++) {
                if (v2It[i].title && !strcmp(v2It[i].title, "OP3 RAT")) rat = true;
                if (v2It[i].title && !strcmp(v2It[i].title, "OP3 LVL")) lvl = true;
            }
            UCHECK(rat && lvl, "OP3 fehlt in der Übersicht (RAT %d, LVL %d)", rat, lvl);
        }
        // bei einer anderen Engine darf es die Übersicht nicht geben
        lockA(); Engine::setMachine(curP(), 5, M_SYNTH); unlockA(); v2Dirty();
        simFrame(); v2Build(V2_SOUND);
        int ov2 = 0; for (int i = 0; i < v2N; i++) if (v2It[i].hdr && v2It[i].title && strstr(v2It[i].title, TR("ÜBERSICHT", "OVERVIEW"))) ov2++;
        UCHECK(ov2 == 0, "Die FM-Übersicht steht auch bei SYNTH in der Liste");
        lockA(); Engine::setMachine(curP(), 5, M_FM); unlockA();
        simReset(); simFrame();
    }

    uiCase = "SYNTH-Oszillatoren";
    {
        simReset(); goTrack(4);
        lockA(); Engine::setMachine(curP(), 4, M_SYNTH); unlockA();
        goPage(PG_SRC2);
        UCHECK(page == PG_SRC2 && curP().p[4][P_X0] == 3 && curP().p[4][P_X2] == 127, "SYNTH-Seite 2 fehlt oder falsche Grundwerte");
        slot = 1; tap(BT_X);
        UCHECK(curP().p[4][P_X1] == 4, "OCT2 +1 nicht gesetzt (%d)", curP().p[4][P_X1]);
    }

    uiCase = "Effekt-Bus";
    {
        simReset(); goPage(PG_FXB);
        UCHECK(page == PG_FXB, "Bus B nicht erreichbar (Seite %d)", page);
        int t0 = eng.proj.bus[1][BX_TYPE];
        slot = BX_TYPE; tap(BT_X);
        UCHECK(eng.proj.bus[1][BX_TYPE] == (t0 + 1 < BF_COUNT ? t0 + 1 : t0), "Bus-Typ wechselt nicht");
        UCHECK(eng.proj.bus[1][BX_P1] == busDef(1, eng.proj.bus[1][BX_TYPE], BX_P1).def, "Typwechsel setzt die Regler nicht auf Grundwerte");
        lockA(); Engine::setBusType(eng.proj, 1, BF_REVERB); unlockA();
    }

    uiCase = "Wavetable-Editor";
    {
        simReset(); goTrack(6);
        lockA(); Engine::setMachine(curP(), 6, M_WTBL); unlockA();
        goPage(PG_SRC2); slot = 4;   // DRAW
        tap(BT_X);
        UCHECK(wteOpen && curP().p[6][P_S0] == 4 && curP().p[6][P_X3] == PREVIEW_SLOT && sampleGet(PREVIEW_SLOT), "X auf DRAW oeffnet keinen Editor mit Vorhoeren");
        tap(BT_A); for (int i = 0; i < 3; i++) tap(BT_DOWN); tap(BT_A);   // Werkzeug RECHTECK
        UCHECK(wtePts[0][0] > 0.8f && wtePts[0][40] < -0.8f, "Werkzeug Rechteck wirkt nicht");
        tap(BT_R1); tap(BT_RIGHT); tap(BT_UP); tap(BT_UP);
        UCHECK(wteKey == 1 && wteCur == 1, "Welle/Punkt-Wahl falsch (%d/%d)", wteKey, wteCur);
        tap(BT_START);
        int sl = curP().p[6][P_X3];
        UCHECK(!wteOpen && curP().p[6][P_S0] == 4 && sl != PREVIEW_SLOT && sampleGet(sl) && !strncmp(sampleName(sl), "wavetables/draw-", 16),
               "START speichert/weist nicht zu (Platz %d, '%s')", sl, sampleName(sl));
        if (sampleGet(sl)) UCHECK(sampleGet(sl)->len == WTE_FR * WTE_F, "Wavetable-Laenge %d", sampleGet(sl)->len);
        char wp[800]; snprintf(wp, sizeof wp, "%ssamples/%s.wav", basePath, sampleName(sl));
        lockA(); eng.preview(6, nullptr); unlockA();
        float tmp[512 * 2]; float pk = 0; for (int k = 0; k < 20; k++) { lockA(); eng.render(tmp, 512); unlockA(); for (int i = 0; i < 1024; i++) pk = fmaxf(pk, fabsf(tmp[i])); }
        UCHECK(pk > 0.01f, "WTBL mit eigener Wavetable bleibt still (%.4f)", pk);
        tap(BT_X); tap(BT_B);   // Editor erneut, Abbruch stellt die gespeicherte Tabelle wieder her
        UCHECK(!wteOpen && curP().p[6][P_X3] == sl, "Abbruch stellt UWAV nicht wieder her");
        remove(wp); reloadSamples();
    }

    uiCase = "Song-Export";
    {
        static Project* keepP = (Project*)malloc(sizeof(Project)); Project& keep = *keepP; memcpy(&keep, &eng.proj, sizeof(Project));
        simReset();
        lockA(); buildDemo(eng); unlockA();
        expFrom = 1; expTo = 1;   // nur Zeile 2 (Groove, 4 Durchlaeufe)
        int a, b; expRange(a, b);
        UCHECK(a == 1 && b == 1, "Export-Bereich %d-%d statt 2-2", a + 1, b + 1);
        startExport();
        UCHECK(expState == 1, "Export startet nicht (Zustand %d)", expState);
        for (int k = 0; k < 1200 && expState == 1; k++) SDL_Delay(50);
        UCHECK(expState == 2, "Export nicht fertig (Zustand %d)", expState);
        struct stat st;
        long want = expFrames;   // 4 Takte bei 112 BPM
        long wantSec = (long)(4 * 4 * 60.0 / 112 * 44100);
        UCHECK(labs(want - wantSec) < 64, "Export-Laenge %ld Frames statt %ld", want, wantSec);
        if (stat(expPath, &st) == 0) {
            long frames = (st.st_size - 44) / 6;   // 24 Bit Stereo
            UCHECK(frames >= want && frames <= want + 8 * eng.sr + 2048, "WAV hat %ld Frames (Song %ld + Ausklang)", frames, want);
            FILE* f = fopen(expPath, "rb"); float pk = 0;
            uint8_t wh[44] = {0};
            if (f) { static uint8_t buf[44100 * 6]; size_t n = fread(wh, 1, 44, f) == 44 ? fread(buf, 3, 44100 * 2, f) : 0; for (size_t i = 0; i < n; i++) pk = fmaxf(pk, fabsf(get24(buf + i * 3) / 8388608.f)); fclose(f); }
            UCHECK(wh[34] == 24 && wh[22] == 2, "Export nicht 24 Bit Stereo (%d Bit, %d Kanaele)", wh[34], wh[22]);
            UCHECK(pk > 0.05f, "Export ist still (Spitze %.3f)", pk);
            remove(expPath);
        } else UCHECK(false, "Export-Datei fehlt: %s", expPath);
        expFrom = 0; expTo = -1; expState = 0;
        lockA(); memcpy(&eng.proj, &keep, sizeof(Project)); eng.songOn = false; unlockA();
    }

    uiCase = "Clip-Matrix";
    {
        Project& J = eng.proj;
        simReset(); J.songLen = 0; memset(J.patMute, 0, sizeof J.patMute);
        eng.toggleMute(2);   // Pattern-Mute: Spur 3 in Pattern 1
        UCHECK(J.patMute[0] == 4 && eng.muted(2), "Pattern-Mute nicht gesetzt");
        page = PG_SONG; simWait(10); songCol = SC_ALL;
        tap(BT_L1);
        UCHECK(J.songLen == 1 && J.row[0].pat[5] == 0 && J.row[0].mute == 4, "neue Zeile: Pattern %d, Mutes %02x (erwartet 1, 04)", J.row[0].pat[5] + 1, J.row[0].mute);
        tap(BT_RIGHT); tap(BT_RIGHT);   // Spalte Spur 2
        tap(BT_X); tap(BT_X);
        UCHECK(J.row[0].pat[1] == 2 && rowAllPat(J.row[0]) < 0, "Spur 2 nicht auf Pattern 3 (%d)", J.row[0].pat[1] + 1);
        tap(BT_A);
        UCHECK(J.row[0].mute & 2, "A schaltet die Zelle nicht stumm");
        for (int i = 0; i < 4; i++) tap(BT_Y);
        UCHECK(J.row[0].pat[1] == SONG_EMPTY, "Y bis unter 01 ergibt keine leere Zelle (%d)", J.row[0].pat[1]);
        tap(BT_L1);
        UCHECK(J.songLen == 2 && songSel == 1 && !memcmp(J.row[1].pat, J.row[0].pat, NTRACKS), "L1 kopiert die Zeile nicht");
        J.patMute[1] = 0x80;
        tap(BT_LEFT); tap(BT_LEFT); tap(BT_LEFT);
        UCHECK(songCol == SC_ALL, "Spalte ALLE nicht erreichbar (%d)", songCol);
        tap(BT_X);   // MIX -> vereinheitlichen
        UCHECK(rowAllPat(J.row[1]) == 0, "ALLE vereinheitlicht nicht (%d)", rowAllPat(J.row[1]));
        tap(BT_X);
        UCHECK(rowAllPat(J.row[1]) == 1 && J.row[1].mute == 0x80, "ALLE: Pattern %d, Mutes %02x (erwartet 2, 80)", rowAllPat(J.row[1]) + 1, J.row[1].mute);
        tap(BT_R1);
        UCHECK(eng.songActive() && eng.songPos == 1 && curPi() == 1 && eng.muted(7) && !eng.muted(2), "Song an: Zeile %d, Pattern %d", eng.songPos + 1, curPi() + 1);
        songSel = 0; simWait(2);
        UCHECK(eng.patOf(1) == 1, "Zeile 2 laeuft nicht");
        eng.songStart(0); simWait(2);
        UCHECK(eng.patOf(0) == 0 && eng.muted(1) && eng.muted(2) && !eng.muted(0), "Zeile 1: leere Zelle / Mute falsch");
        eng.toggleMute(0);   // Mute im Song-Modus aendert die Zeile, nicht das Pattern
        UCHECK((J.row[0].mute & 1) && !(J.patMute[0] & 1), "Mute im Song landet nicht in der Zeile");
        tap(BT_B);
        UCHECK(J.songLen == 1, "B loescht die Zeile nicht");
        page = PG_TRG; simWait(2);
        combo(BT_SELECT, BT_RIGHT);
        UCHECK(!eng.songOn, "Pattern-Wechsel beendet den Song-Modus nicht");
        J.songLen = 0; memset(J.patMute, 0, sizeof J.patMute); eng.songOn = false;
    }

    uiCase = "SET: jeden Eintrag ändern, dann Seite wechseln (kein Einfrieren)";
    {
        Settings keepCfg = cfg; int keepUsb = usbMode, keepLang = g_lang, keepThr = eng.audioThreads, keepPat = eng.curPat;
        simReset(); goPage(PG_SYS);
        int worst = 0, worstItem = -1, tested = 0;
        for (int i = 0; i < SI_COUNT; i++) {
            if (setIsHeader(i) || setHidden(i) || setIsAction(i)) continue;
            if (i == SI_DISP) continue;   // Bild-Versatz: nur Anzeige, wuerde Screenshots verschieben
            page = PG_SYS; setSel = i; setIn = true;
            Uint32 t0 = SDL_GetTicks();
            setChange(i, 1); simFrame(); setChange(i, -1); simFrame();
            tap(BT_R2); UCHECK(page != PG_SYS, "R2 verlaesst SET nicht (Eintrag %d)", i);
            tap(BT_L2); UCHECK(page == PG_SYS, "L2 kommt nicht zu SET zurueck (Eintrag %d, Seite %d)", i, page);
            int dt = (int)(SDL_GetTicks() - t0);
            if (dt > worst) { worst = dt; worstItem = i; }
            tested++;
            usbApplyAt = 0; audioApplyAt = 0;   // Geraete-Umbau gibt es im Test nicht
        }
        UCHECK(tested > 20, "zu wenige SET-Eintraege geprueft (%d)", tested);
        UCHECK(worst < 1500, "SET-Eintrag %d brauchte %d ms (Einfrieren?)", worstItem, worst);
        cfg = keepCfg; usbMode = keepUsb; g_lang = keepLang; eng.audioThreads = keepThr; eng.curPat = keepPat; eng.nextPat = -1; saveSettings();
    }

    uiCase = "Einstellungen";
    simReset(); page = PG_SYS; setIn = false; setSel = SI_PATTERN + 1; simWait(50);
    { int c0 = setCatOf(setSel); tap(BT_DOWN); UCHECK(setCatOf(setSel) != c0 && !setIn, "SET: runter waehlt nicht den naechsten Bereich");
      int c1 = setCatOf(setSel); tap(BT_A); UCHECK(setIn && setCatOf(setSel) == c1, "SET: A oeffnet den Bereich nicht");
      tap(BT_DOWN); UCHECK(setCatOf(setSel) == c1, "SET: runter verlaesst im Bereich den Bereich");
      tap(BT_B); UCHECK(!setIn && page == PG_SYS, "SET: B fuehrt nicht zurueck zu den Bereichen"); }
    tap(BT_B); UCHECK(page != PG_SYS, "B verlaesst SET nicht");

    uiCase = "Rückgängig";
    simReset(); curP().s[0][0].on = 0; simWait(900);
    tap(BT_A); simWait(900);
    UCHECK(curP().s[0][0].on == 1, "Trig nicht gesetzt");
    combo(BT_SELECT, BT_B);
    UCHECK(curP().s[0][0].on == 0, "SELECT+B macht den Trig nicht rückgängig");
    redoStep(); UCHECK(curP().s[0][0].on == 1, "Wiederholen stellt den Trig nicht her");
    curP().s[0][0].on = 0; simWait(900);

    uiCase = "Beat-Sprung";
    simReset(); goPage(PG_SEQ); combo(BT_SELECT, BT_R1); UCHECK(cursor == 4, "SELECT+R1: Step %d statt 5", cursor + 1);
    combo(BT_SELECT, BT_L1); UCHECK(cursor == 0, "SELECT+L1: Step %d statt 1", cursor + 1);

    uiCase = "PAGE-Menue (im MEHR-Menue)";
    {
        simReset(); goPage(PG_TRG);
        int keepNew = cfg.newPage; cfg.newPage = 1;
        Pattern& P = curP(); P.len[0] = 16; P.s[0][3].on = 1; P.s[0][3].lock[P_NOTE] = 50;
        tap(BT_SELECT); UCHECK(mrOpen, "SELECT oeffnet das MEHR-Menue nicht");
        mrSel = MR_PCOPY; tap(BT_A); UCHECK(!mrOpen && pgClipState == 1, "Page kopieren: Zustand %d", pgClipState);
        moreRun(MR_PADD);
        UCHECK(P.len[0] == 32 && P.s[0][19].on == 1 && P.s[0][19].lock[P_NOTE] == 50 && cursor >= 16, "Page anhaengen (Kopie): Laenge %d, Step 20 %d, Cursor %d", P.len[0], P.s[0][19].on, cursor);
        moreRun(MR_PCLEAR);
        UCHECK(P.s[0][19].on == 0 && P.s[0][3].on == 1, "Page leeren falsch");
        moreRun(MR_PPASTE);
        UCHECK(P.s[0][19].on == 1 && P.s[0][19].lock[P_NOTE] == 50, "Page einfuegen falsch");
        moreRun(MR_PNEW); UCHECK(cfg.newPage == 0, "NEUE PAGE nicht umschaltbar");
        moreRun(MR_PADD); UCHECK(P.len[0] == 48 && P.s[0][35].on == 0, "Page anhaengen (leer) falsch");
        moreRun(MR_PDEL); UCHECK(P.len[0] == 32, "Letzte Page entfernen: %d", P.len[0]);
        tap(BT_SELECT); tap(BT_B); UCHECK(!mrOpen, "B schliesst das MEHR-Menue nicht");
        // Handy-App/Touch: MEHR antippen oeffnet das Menue, Zeile antippen fuehrt aus
        simFrame(); tapAt(610, 462); UCHECK(mrOpen, "Antippen von MEHR oeffnet das Menue nicht");
        tap(BT_B);
        // SET: PAGE-Menue und NEUE PAGE auch in den Einstellungen (Bereich SPUR)
        { int nw = cfg.newPage; setChange(SI_NEWPAGE, 1); UCHECK(cfg.newPage == !nw, "SET: Neue Page nicht umschaltbar"); cfg.newPage = nw;
          page = PG_SYS; setAction(SI_PAGEMENU); UCHECK(pgmOpen && page == PG_TRG, "SET: PAGE-Menue oeffnen geht nicht"); tap(BT_B); }
        moreRun(MR_TCOPY);
        goTrack(2); goPage(PG_TRG); moreRun(MR_TPASTE);
        UCHECK(P.machine[2] == P.machine[0] && P.len[2] == P.len[0] && P.s[2][19].on == 1 && P.s[2][19].lock[P_NOTE] == 50, "Spur einfuegen falsch");
        moreRun(MR_TCLEAR);
        UCHECK(P.s[2][19].on == 0 && P.s[2][3].on == 0 && P.machine[2] == P.machine[0], "Spur leeren falsch");
        lockA(); Engine::initTrack(P, 2, M_HAT); P.machine[2] = M_HAT; P.len[2] = 16; unlockA();
        cfg.newPage = 1; P.len[1] = 16; P.s[1][2].on = 1; P.s[1][18].on = 0;   // Verlaengern ueber LEN: Kopie
        lockA(); setTrackLen(P, 1, 17); unlockA(); UCHECK(P.s[1][16].on == 0, "LEN: Step 17 nicht von Step 1");
        lockA(); setTrackLen(P, 1, 19); unlockA(); UCHECK(P.s[1][18].on == 1, "LEN +: Step 19 keine Kopie von Step 3");
        cfg.newPage = keepNew; P.len[0] = P.len[1] = 16;
        for (int t = 0; t < 2; t++) for (int st = 0; st < NSTEPS; st++) { P.s[t][st].on = 0; memset(P.s[t][st].lock, NO_LOCK, NPARAMS); }
    }

    uiCase = "Rhythmus-Vorlage";
    simReset(); track = 2; applyRhythm(0);
    UCHECK(curP().s[2][0].on == 1 && curP().s[2][4].on == 1 && curP().s[2][1].on == 0, "Four on the Floor falsch");
    applyRhythm(RHY_N - 1); simWait(900);

    uiCase = "LIVE-Seite";
    simReset(); goPage(PG_LIVE);
    UCHECK(page == PG_LIVE, "LIVE nicht erreichbar (Seite %d)", page);
    bool m5 = eng.muted(4);
    tap(BT_UP); UCHECK(eng.muted(0), "Steuerkreuz hoch schaltet Spur 1 nicht stumm");
    tap(BT_X); UCHECK(eng.muted(4) != m5, "X schaltet Spur 5 nicht");
    tap(BT_UP); tap(BT_X);
    holdDown(BT_L1); UCHECK(eng.fill, "L1 halten = FILL"); release(BT_L1); UCHECK(!eng.fill, "FILL bleibt nach Loslassen");
    eng.start();
    tap(BT_R1); UCHECK(recOn, "R1 startet keine Aufnahme");
    { static float tmpb[44100 * 2]; recPush(tmpb, 44100); }
    tap(BT_R1); UCHECK(!recOn, "R1 stoppt die Aufnahme nicht");
    for (int i = 0; i < 200 && recSaving; i++) SDL_Delay(10);
    UCHECK(recDoneNo > 0, "Aufnahme nicht gespeichert (%d)", recDoneNo);
    recDoneNo = 0; eng.stop();

    uiCase = "Speichern";
    simReset();
    curP().s[6][9].on = 1; curP().s[6][9].lock[P_VEL] = 77;
    combo(BT_SELECT, BT_START);
    {
        char tmp[720]; snprintf(tmp, sizeof tmp, "%s.tmp", projPath);
        struct stat st;
        UCHECK(stat(projPath, &st) == 0 && st.st_size > 20 + PROJECT_TAIL_SIZE + 8 && st.st_size < 2000000, "Projektdatei fehlt oder hat falsche Groesse (%ld)", (long)st.st_size);
        { static char nm[MAX_SAMPLES][96]; UCHECK(readPoolTrailer(projPath, nm), "Sample-Liste hinter den Projektdaten fehlt"); }
        UCHECK(stat(tmp, &st) != 0, "Temp-Datei bleibt liegen");
        static Engine* chkP = newEngine(); Engine& chk = *chkP; chk.audioThreads = 1;
        UCHECK(chk.load(projPath) && chk.proj.pat[eng.curPat].s[6][9].lock[P_VEL] == 77, "gespeichertes Projekt nicht korrekt ladbar");
    }
    curP().s[6][9].on = 0; curP().s[6][9].lock[P_VEL] = NO_LOCK;

    uiCase = "Projekt speichern als";
    simReset(); goPage(PG_PRJ); goPrj(PI_SAVEAS); tap(BT_A);
    UCHECK(kbOpen, "SPEICH. ALS oeffnet keine Tastatur");
    for (int i = 0; i < 40; i++) tap(BT_B);
    tap(BT_A); tap(BT_RIGHT); tap(BT_A); tap(BT_DOWN); tap(BT_DOWN); tap(BT_A);   // "A", "b", dann Zeile 3 Spalte 2 = "1"
    UCHECK(!strcmp(kbText, "Ab1"), "Tastatur-Text '%s' statt 'Ab1'", kbText);
    tap(BT_START);
    UCHECK(!kbOpen && projectExists("Ab1") && !strcmp(cfg.project, "Ab1"), "Projekt 'Ab1' nicht gespeichert (aktiv: %s)", cfg.project);

    uiCase = "Projekt laden";
    curP().s[5][5].on = 1; curP().s[5][5].lock[P_VEL] = 11;   // ungespeicherte Aenderung
    simWait(900);
    goPrj(PI_LOAD); tap(BT_A);
    UCHECK(fbOpen && fbMode == FB_PROJECT, "LADEN oeffnet keinen Projekt-Explorer");
    for (int i = 0; i < fbN; i++) if (!strcmp(fbEnt[i].name, "Ab1")) fbSel = i;
    tap(BT_A);
    UCHECK(confirmOpen, "keine Rueckfrage trotz ungespeicherter Aenderung");
    tap(BT_A);
    UCHECK(curP().s[5][5].lock[P_VEL] == NO_LOCK && !strcmp(cfg.project, "Ab1"), "Projekt nicht neu geladen");

    uiCase = "Sound speichern/laden";
    simReset(); goTrack(4); goPage(PG_PRJ); goPrj(PI_SSAVE); tap(BT_A);
    UCHECK(kbOpen && kbMode == KB_PSAVE, "SOUND SPEICH. oeffnet keine Tastatur");
    for (int i = 0; i < 40; i++) tap(BT_B);
    tap(BT_DOWN); tap(BT_LEFT); tap(BT_LEFT); tap(BT_LEFT); tap(BT_A);   // Zeile 2: von N dreimal links (Z, Y, X)
    int freq0 = curP().p[4][P_FREQ];
    tap(BT_START);
    { char pp[700]; presetFile(curP().machine[4], "X", pp, sizeof pp); UCHECK(fileExists(pp), "Sound-Datei X fehlt"); }
    curP().p[4][P_FREQ] = (uint8_t)(freq0 ^ 1);
    goPrj(PI_SLOAD); tap(BT_A);
    UCHECK(fbOpen && fbMode == FB_PRESET, "SOUND LADEN oeffnet keinen Explorer");
    for (int i = 0; i < fbN; i++) if (fbEnt[i].dir && fbEnt[i].idx == curP().machine[4]) fbSel = i;
    tap(BT_A);
    for (int i = 0; i < fbN; i++) if (!strcmp(fbEnt[i].name, "X")) fbSel = i;
    tap(BT_A);
    UCHECK(!fbOpen && curP().p[4][P_FREQ] == freq0, "Sound nicht geladen (FREQ %d statt %d)", curP().p[4][P_FREQ], freq0);

    uiCase = "Pattern kopieren (PRJ)";
    {
        simReset(); goPage(PG_PRJ);
        int src = curPi(), dst = (src + 5) % NPATTERNS;
        eng.proj.pat[src].s[3][7].on = 1; eng.proj.pat[src].s[3][7].lock[P_VEL] = 99; eng.proj.pat[src].len[3] = 48;
        goPrj(PI_PAT0 + src); tap(BT_A); goPrj(PI_COPY); tap(BT_A);
        UCHECK(haveClip && clipFrom == src, "KOPIEREN hat nichts kopiert");
        goPrj(PI_PAT0 + dst); tap(BT_A); goPrj(PI_PASTE); tap(BT_A);
        if (confirmOpen) tap(BT_A);
        const Pattern& D = eng.proj.pat[dst];
        UCHECK(D.s[3][7].on == 1 && D.s[3][7].lock[P_VEL] == 99 && D.len[3] == 48, "EINFUEGEN: Ziel P%02d nicht gleich der Quelle", dst + 1);
        goPrj(PI_CHG); tap(BT_X); tap(BT_X);
        UCHECK(eng.proj.pat[dst].chg == 2, "WECHSEL: X erhoeht nicht (%d)", eng.proj.pat[dst].chg);
        tap(BT_R1);
        UCHECK(eng.proj.pat[dst].chg == 16, "WECHSEL: R1 springt nicht auf 16 (%d)", eng.proj.pat[dst].chg);
        goPrj(PI_LEN); tap(BT_R1);
        UCHECK(eng.proj.pat[dst].len[0] == 64 && eng.proj.pat[dst].len[3] == 64, "LAENGE: R1 nicht auf 64 (%d/%d)", eng.proj.pat[dst].len[0], eng.proj.pat[dst].len[3]);
        tap(BT_Y);
        UCHECK(eng.proj.pat[dst].len[5] == 63, "LAENGE: Y nicht -1 (%d)", eng.proj.pat[dst].len[5]);
        eng.proj.pat[dst].chg = 0; eng.proj.pat[src].s[3][7].on = 0; eng.proj.pat[src].len[3] = 16;
    }
    uiCase = "Spurlaenge +-1/+-16";
    {
        simReset(); goTrack(3); goPage(PG_SEQ); goSlot(1);
        curP().len[3] = 16;
        tap(BT_X);
        UCHECK(curP().len[3] == 17, "X tippen: Laenge %d statt 17", curP().len[3]);
        holdDown(BT_X); simWait(420); release(BT_X);
        UCHECK(curP().len[3] == 32, "X halten: Laenge %d statt 32", curP().len[3]);
        holdDown(BT_Y); simWait(420); release(BT_Y);
        UCHECK(curP().len[3] == 16, "Y halten: Laenge %d statt 16", curP().len[3]);
    }
    uiCase = "Bedingung per Liste";
    {
        simReset(); goTrack(2); goPage(PG_TRG); goSlot(7); tap(BT_X);
        UCHECK(fbOpen && fbMode == FB_COND, "X auf COND oeffnet keine Liste");
        for (int i = 0; i < fbN; i++) if (!fbEnt[i].dir && fbEnt[i].idx == CND_LAST) fbSel = i;
        tap(BT_A);
        UCHECK(!fbOpen && curP().p[2][P_COND] == CND_LAST, "LAST nicht gewaehlt (%d)", curP().p[2][P_COND]);
        curP().p[2][P_COND] = 0;
    }

    uiCase = "Sample-Explorer";
    if (libCount() > 1) {
        simReset(); goTrack(7); Engine::setMachine(curP(), 7, M_SAMPLE); goPage(PG_SRC); goSlot(0);
        UCHECK(curP().p[7][P_S0] == SMP_NONE, "neue SMPL-Spur hat schon ein Sample (%d)", curP().p[7][P_S0]);
        tap(BT_X);
        UCHECK(fbOpen && fbMode == FB_SLOT && fbEnt[0].idx == -2, "X auf SMPL oeffnet keine Slot-Liste");
        fbSel = 0; tap(BT_A);   // "+ aus dem Speicher" -> Bibliothek
        UCHECK(fbOpen && fbMode == FB_SAMPLE, "Slot-Liste: Bibliothek oeffnet nicht");
        for (int g = 0; fbDir[0] && g < 12; g++) tap(BT_B);   // zur Wurzel
        int dirs = 0; for (int i = 0; i < fbN; i++) dirs += fbEnt[i].dir;
        UCHECK(dirs > 0, "keine Unterordner sichtbar");
        fbSel = 0; tap(BT_A);   // in den ersten Ordner
        int want = -1; for (int i = 0; i < fbN; i++) if (!fbEnt[i].dir) { fbSel = i; want = fbEnt[i].idx; break; }
        tap(BT_A);
        int got = curP().p[7][P_S0];
        UCHECK(!fbOpen && want >= 0 && sampleGet(got) && !strcmp(sampleName(got), libName(want)), "Sample nicht gewaehlt ('%s' statt '%s')", sampleName(got), libName(want));
        // leeren Slot gezielt belegen
        tap(BT_X);
        int empty = -1; for (int i = 0; i < fbN; i++) if (fbEnt[i].idx >= 0 && fbEnt[i].idx < PREVIEW_SLOT && !sampleGet(fbEnt[i].idx)) { fbSel = i; empty = fbEnt[i].idx; break; }
        tap(BT_A);
        UCHECK(fbOpen && fbMode == FB_SAMPLE && fbAssign == empty, "leerer Slot oeffnet keine Bibliothek fuer diesen Slot");
        for (int g = 0; fbDir[0] && g < 12; g++) tap(BT_B);
        fbSel = 0; tap(BT_A);
        int want2 = -1; for (int i = fbN - 1; i >= 0; i--) if (!fbEnt[i].dir) { fbSel = i; want2 = fbEnt[i].idx; break; }
        tap(BT_A);
        UCHECK(!fbOpen && curP().p[7][P_S0] == empty && want2 >= 0 && !strcmp(sampleName(empty), libName(want2)), "Slot %d nicht belegt ('%s')", empty + 1, sampleName(empty));
        // Vorhoeren + Abbruch stellt den Slot der Spur wieder her
        tap(BT_X);
        for (int i = 0; i < fbN; i++) if (fbEnt[i].idx == got) fbSel = i;
        tap(BT_X); tap(BT_SELECT);
        UCHECK(!fbOpen && curP().p[7][P_S0] == empty, "Abbruch nach Vorhoeren: Slot %d statt %d", curP().p[7][P_S0], empty);
        // kein Sample
        tap(BT_X); fbSel = 1; tap(BT_A);
        UCHECK(!fbOpen && curP().p[7][P_S0] == SMP_NONE, "KEIN SAMPLE nicht gesetzt (%d)", curP().p[7][P_S0]);
        poolSetSlot(empty, "");
    }

    uiCase = "Sample-Editor";
    if (libCount() > 0) {
        simReset(); goTrack(7); Engine::setMachine(curP(), 7, M_SAMPLE); poolSetSlot(0, libName(0)); curP().p[7][P_S0] = 0;
        goPage(PG_SRC); goSlot(7); tap(BT_X);
        UCHECK(sedOpen, "X auf EDIT oeffnet keinen Sample-Editor");
        int s0 = sedPos(0);
        tap(BT_UP); tap(BT_UP); tap(BT_RIGHT); tap(BT_RIGHT);
        UCHECK(sedPos(0) > s0, "START laesst sich nicht schieben (%d -> %d)", s0, sedPos(0));
        tap(BT_R1); tap(BT_START);   // Marke LOOP, Loop an
        UCHECK(sedMark == 1 && curP().p[7][P_S5] == 1, "Marke/Loop nicht umgeschaltet");
        tap(BT_SELECT);
        UCHECK(curP().p[7][P_X7] == 1, "ZERO nicht umgeschaltet");
        tap(BT_B);
        UCHECK(!sedOpen && curP().p[7][P_X4] + curP().p[7][P_S3] > 0, "Editor nicht geschlossen / START nicht uebernommen");
        curP().p[7][P_S3] = 0; curP().p[7][P_X4] = 0; curP().p[7][P_S5] = 0; curP().p[7][P_X7] = 0;
    }

    uiCase = "Engine-Liste";
    simReset(); goTrack(2); goPage(PG_TRG); slot = 6;   // ENGINE (TRG-Seite hat eigenes Raster)
    tap(BT_X);
    UCHECK(fbOpen && fbMode == FB_MACH && fbN == M_COUNT, "X auf ENGINE oeffnet keine Liste (%d Eintraege)", fbN);
    UCHECK(fbEnt[fbSel].idx == curP().machine[2], "Liste steht nicht auf der aktuellen Engine");
    for (int i = 0; i < fbN; i++) if (fbEnt[i].idx == M_FM) fbSel = i;
    tap(BT_A);
    UCHECK(!fbOpen && curP().machine[2] == M_FM, "Engine nicht gewechselt (%d)", curP().machine[2]);
    tap(BT_X); tap(BT_SELECT);
    UCHECK(!fbOpen && curP().machine[2] == M_FM, "Abbruch aendert die Engine");

    uiCase = "Sampler";
    {
        simReset(); goPage(PG_SMPR);
        UCHECK(page == PG_SMPR, "Sampler-Seite nicht erreichbar (Seite %d)", page);
        smpSrc = 1; smpState = 0; smpFromPc = false;
        slot = SK_SRC; tap(BT_X); UCHECK(smpSrc == 2, "Quelle SPUR 1 nicht waehlbar (%d)", smpSrc);
        tap(BT_A); simFrame(); UCHECK(smpState == 1, "Spur-Aufnahme wartet nicht (%d)", smpState);
        tap(BT_A); smpSrc = 1;
        tap(BT_A); UCHECK(smpState == 1, "A startet die Master-Aufnahme nicht (%d)", smpState);
        tap(BT_A); UCHECK(smpState == 0, "A bricht die wartende Aufnahme nicht ab (%d)", smpState);
        {   // feste Laenge: 2 Pages, Start am Page-Anfang, genau 32 Steps lang
            int keep = cfg.recPages; cfg.recPages = 0;
            slot = SK_LEN; tap(BT_X); tap(BT_X);
            UCHECK(cfg.recPages == 2, "LAENGE nicht einstellbar (%d)", cfg.recPages);
            tap(BT_A);
            int want = (int)(2 * 16.0 * 60.0 * eng.sr / (eng.proj.bpm * 4.0) + 0.5);
            UCHECK(smpState == 1 && smpTarget == want, "feste Laenge: Ziel %d statt %d", (int)smpTarget, want);
            lockA(); eng.start(); unlockA();
            static float blk[512 * 2];
            for (int k = 0; k < 4000 && smpState != 3; k++) { lockA(); eng.render(blk, 512); unlockA(); smpPush(blk, 512); }
            UCHECK(smpState == 3 && smpLen == want, "feste Laenge: %d Frames statt %d (Zustand %d)", (int)smpLen, want, (int)smpState);
            smpUpdate();
            UCHECK(smpEnd == want, "feste Laenge: Ende gekuerzt (%d)", smpEnd);
            lockA(); eng.stop(); unlockA();
            cfg.recPages = keep; smpTarget = 0;
        }
        {   // Start-Schwelle: -40 dB -> -37 dB, ganz runter = SOFORT
            int keep = cfg.recThr; cfg.recThr = -40;
            slot = SK_THR; tap(BT_X);
            UCHECK(cfg.recThr == -37 && fabsf(recThrLin() - 0.01413f) < 0.0005f, "SCHWELLE: %d dB", cfg.recThr);
            for (int k = 0; k < 30; k++) tap(BT_Y);
            UCHECK(cfg.recThr == -99 && recThrLin() < 0.f, "SCHWELLE SOFORT nicht erreichbar (%d)", cfg.recThr);
            tap(BT_X); UCHECK(cfg.recThr == -60, "SCHWELLE nach SOFORT: %d", cfg.recThr);
            cfg.recThr = keep;
        }
        smpDemo(); smpDest = 2; track = 7; smpSavedNo = 0;
        UCHECK(smpNCut == 8, "Transienten-Chops: %d statt 8", smpNCut);
        tap(BT_RIGHT); UCHECK(pvOn && smpChopPv == 0, "Chop-Vorhoeren startet nicht");
        tap(BT_UP);
        int no = smpSavedNo;
        UCHECK(no > 0 && curP().machine[7] == M_CHOP && curP().p[7][P_S1] == smpNCut - 1 && sampleGet(curP().p[7][P_S0]),
               "Speichern in CHOP-Spur: Nr %d, Engine %d, SLCS %d", no, curP().machine[7], curP().p[7][P_S1]);
        char f[800]; struct stat st2;
        snprintf(f, sizeof f, "%ssamples/sampler/smp-%02d/08.wav", basePath, no);
        UCHECK(stat(f, &st2) == 0, "Chop-Datei fehlt: %s", f);
        for (int i = 1; i <= 64; i++) { snprintf(f, sizeof f, "%ssamples/sampler/smp-%02d/%02d.wav", basePath, no, i); unlink(f); }   // aufraeumen
        snprintf(f, sizeof f, "%ssamples/sampler/smp-%02d", basePath, no); rmdir(f);
        snprintf(f, sizeof f, "%ssamples/sampler/smp-%02d.wav", basePath, no); unlink(f);
        snprintf(f, sizeof f, "%ssamples/sampler", basePath); rmdir(f);
        pvOn = false; reloadSamples();
    }

    uiCase = "LFO-Ziel-Liste";
    simReset(); goTrack(4); goPage(PG_LFO1); slot = LF_DEST;
    curP().p[4][P_L1 + LF_DEST] = 0;
    tap(BT_X);
    UCHECK(fbOpen && fbMode == FB_LDEST && !fbDir[0] && fbN > 3 && !fbEnt[fbSel].dir && fbEnt[fbSel].idx == 0, "X auf DEST oeffnet keine Ziel-Liste");
    for (int i = 0; i < fbN; i++) if (fbEnt[i].dir && fbEnt[i].idx == PG_FLT) fbSel = i;   // Ordner = Bedienseite FLT
    tap(BT_A);
    UCHECK(fbDir[0] && fbN > 0, "Seitenordner FLT leer");
    for (int i = 0; i < fbN; i++) if (fbEnt[i].idx == P_FREQ + 1) fbSel = i;
    tap(BT_A);
    UCHECK(!fbOpen && curP().p[4][P_L1 + LF_DEST] == P_FREQ + 1, "LFO-Ziel nicht gesetzt (%d)", curP().p[4][P_L1 + LF_DEST]);
    tap(BT_X);
    UCHECK(fbOpen && fbDir[0] && fbEnt[fbSel].idx == P_FREQ + 1, "Liste oeffnet nicht beim aktuellen Ziel");
    tap(BT_B); tap(BT_B);
    UCHECK(!fbOpen && curP().p[4][P_L1 + LF_DEST] == P_FREQ + 1, "Zurueck/Abbruch aendert das Ziel");
    { bool ok = true; int m = curP().machine[4];   // Liste enthaelt nur gueltige Ziele
      for (int pi = 0; pi < NPARAMS; pi++) if (ldestValid(m, pi) && ldestUiPage(pi) < 0) { ok = false; printf("  Parameter %d (%s) auf keiner Seite\n", pi, paramDef(m, pi).name); }
      UCHECK(ok, "Zielseite ohne Bedienseite"); }

    uiCase = "Sample-Pool";
    if (libCount() > 1) {
        simReset(); Engine::setMachine(curP(), 7, M_SAMPLE);
        char want[96]; snprintf(want, sizeof want, "%s", libName(libCount() - 1));
        int sl = poolAssign(want);
        UCHECK(sl >= 0 && !strcmp(sampleName(sl), want), "poolAssign laedt '%s' nicht", want);
        if (sl >= 0) {
            curP().p[7][P_S0] = (uint8_t)sl;
            doSaveProjectAs("PoolTest");
            poolSetSlot(sl, "");   // Pool leeren: Laden muss das Sample per Name wiederfinden
            doLoadProject("PoolTest");
            int s2 = curP().p[7][P_S0];
            UCHECK(sampleGet(s2) && !strcmp(sampleName(s2), want), "nach Projekt-Laden '%s' statt '%s'", sampleName(s2), want);
            {   // dasselbe Projekt im Format v11 (kuerzer, Namensliste direkt danach) muss sein Sample ebenso finden
                char pp[700]; projectPath("PoolTest", pp, sizeof pp);
                FILE* f = fopen(pp, "rb"); long n = 0; uint8_t* buf = nullptr;
                if (f) { fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET); buf = (uint8_t*)malloc(n); if (fread(buf, 1, n, f) != (size_t)n) n = 0; fclose(f); }
                long de = -1; { FILE* g = fopen(pp, "rb"); if (g) { de = projectDataEnd(g); fclose(g); } }
                if (buf && de > 0 && n > de) {   // altes Layout (96 Parameter) nachbauen, Namensliste von der aktuellen Datei
                    uint8_t* ob = (uint8_t*)calloc(1, PROJECT_V11_SIZE);
                    memcpy(ob, buf, 20); uint32_t v11 = 11; memcpy(ob + 4, &v11, 4);
                    for (int pi = 0; pi < OLD_NPAT; pi++) {
                        const Pattern& Q = eng.proj.pat[pi];
                        uint8_t* q = ob + 20 + (long)pi * OLD_PAT_SIZE;
                        memcpy(q, Q.machine, 8); memcpy(q + 8, Q.len, 8); memcpy(q + 16, Q.speed, 8);
                        for (int t = 0; t < NTRACKS; t++) {
                            int m = Q.machine[t] % M_COUNT;
                            for (int i = 0; i < OLD_NPARAMS; i++) { int j = Engine::mapOldParam(m, i); q[24 + t * OLD_NPARAMS + i] = j >= 0 ? Q.p[t][j] : 0; }
                            for (int st = 0; st < NSTEPS; st++) memset(q + 24 + 8 * OLD_NPARAMS + (t * NSTEPS + st) * (1 + OLD_NPARAMS) + 1, NO_LOCK, OLD_NPARAMS);
                        }
                    }
                    f = fopen(pp, "wb");
                    fwrite(ob, 1, PROJECT_V11_SIZE, f); fwrite(buf + de, 1, n - de, f); fclose(f);
                    free(ob);
                    poolSetSlot(s2, "");
                    doLoadProject("PoolTest");
                    int s4 = curP().p[7][P_S0];
                    UCHECK(sampleGet(s4) && !strcmp(sampleName(s4), want), "v11-Projekt: Sample '%s' statt '%s'", sampleName(s4), want);
                    s2 = s4;
                }
                free(buf);
            }
            char pp[700]; presetFile(M_SAMPLE, "PoolSound", pp, sizeof pp);
            UCHECK(savePresetFile(7, pp), "Sound mit Sample nicht gespeichert");
            poolSetSlot(s2, "");
            UCHECK(loadPresetPath(7, pp) == 1, "Sound mit Sample nicht ladbar");
            int s3 = curP().p[7][P_S0];
            UCHECK(sampleGet(s3) && !strcmp(sampleName(s3), want), "Sound: Sample '%s' statt '%s'", sampleName(s3), want);
            remove(pp);
        }
        static char l1[128][96], l2[250][96];   // altes Projekt ohne Namensliste: Nummer = Position in der frueheren Liste
        int n1 = libLegacyList(l1, 128, 1, 256), n2 = libLegacyList(l2, 250, SAMPLE_DEPTH, 500);
        if (n1 > 1 && n2 > 1) {
            for (int p = 0; p < NPATTERNS; p++) for (int t = 0; t < NTRACKS; t++) {
                if (machineUsesSample(eng.proj.pat[p].machine[t] % M_COUNT)) eng.proj.pat[p].p[t][P_S0] = 1;
                for (int s = 0; s < NSTEPS; s++) eng.proj.pat[p].s[t][s].lock[P_S0] = NO_LOCK;
            }
            for (int k = 0; k < 2; k++) {   // songNN = Zeit der 1-Ebenen-Liste, benannte Projekte = 250er-Liste
                const char* nm = k ? "PoolAlt" : "song99";
                char lp[700]; projectPath(nm, lp, sizeof lp);
                FILE* f = fopen(lp, "wb"); if (f) { projectWrite(f, eng.proj, nullptr); fclose(f); }   // ohne Namensliste
                doLoadProject(nm);
                const char* want = k ? l2[1] : l1[1];
                int s4 = curP().p[7][P_S0];
                UCHECK(poolLegacy == k && sampleGet(s4) && !strcmp(sampleName(s4), want), "altes Projekt %s: Liste %d, '%s' statt '%s'", nm, poolLegacy, sampleName(s4), want);
                setChange(SI_OLDSMP, 1);   // SET: andere Reihenfolge
                const char* other = k ? l1[1] : l2[1];
                UCHECK(poolLegacy == 1 - k && !strcmp(sampleName(curP().p[7][P_S0]), other), "SET Alte Sample-Nummern: '%s' statt '%s'", sampleName(curP().p[7][P_S0]), other);
                remove(lp);
            }
        }
        char p2[700]; projectPath("PoolTest", p2, sizeof p2); remove(p2);
        doLoadProject("Ab1");
    }

    uiCase = "SET USB-Klassen";   // Netzwerk NCM/RNDIS und Audio UAC2/UAC1 landen als Umgebung bei pc-modus.sh
    {
        int n0 = cfg.usbNet, u0 = cfg.uac;
        cfg.usbNet = 0; cfg.uac = 2;
        UCHECK(strstr(usbEnv(), "GB_USB_NET=ncm") && strstr(usbEnv(), "GB_UAC=2"), "Standard: '%s'", usbEnv());
        setChange(SI_USBNET, 1); setChange(SI_UACVER, 1);
        UCHECK(strstr(usbEnv(), "GB_USB_NET=rndis") && strstr(usbEnv(), "GB_UAC=1"), "umgeschaltet: '%s'", usbEnv());
        setChange(SI_USBNET, 1); setChange(SI_UACVER, 1);
        UCHECK(cfg.usbNet == 0 && cfg.uac == 2, "zurueck: Netz %d, UAC %d", cfg.usbNet, cfg.uac);
        cfg.usbNet = n0; cfg.uac = u0;
        char l[96], v[96]; l[0] = v[0] = 0; setLabel(SI_MIDIINFO, l, sizeof l, v, sizeof v);
        UCHECK(l[0] && v[0], "MIDI-Geraete-Zeile leer");
    }

    uiCase = "Pattern wechseln";
    simReset();
    combo(BT_SELECT, BT_RIGHT); UCHECK(eng.curPat == 1, "SELECT+Rechts: Pattern %d statt 2", eng.curPat + 1);
    combo(BT_SELECT, BT_LEFT);
    for (int t = 0; t < NTRACKS; t++) for (int s = 0; s < NSTEPS; s++) curP().s[t][s].on = 0;
}

// Bedienweg: Tastenzahl je typischer Aufgabe (je kleiner, desto schneller). Startpunkt: TRIG-Seite, Spur 1, Step 1.
struct Task { const char* name; int keys; int steps; };

// ---- Oberflaeche: Szenarien + Bedienweg
static int v2FindItem(int pg, int k) { for (int i = 0; i < v2N; i++) if (!v2It[i].hdr && v2It[i].pg == pg && v2It[i].k == k) return i; return -1; }
static int v2GroupOf(int i) { while (i > 0 && !v2It[i].hdr) i--; return i; }
static void v2GoTo(int view, int pg, int k) {   // wie ein Mensch: L2/R2 zur Ansicht, Gruppe springen (SOUND <>, SEQ SELECT+^v), dann ^v
    for (int g = 0; g < V2_RING_N && v2View != view; g++) tap(view == V2_SOUND ? BT_R2 : BT_L2);
    v2Build(v2View);
    int t = -1; for (int i = (view == V2_SOUND ? 10 : 0); i < v2N && t < 0; i++) if (!v2It[i].hdr && v2It[i].pg == pg && v2It[i].k == k) t = i;   // Gruppe, nicht Makro
    if (t < 0) t = v2FindItem(pg, k);
    if (t < 0) return;
    int tg = v2GroupOf(t);
    int mac = view == V2_SOUND ? v2FindItem(pg, k) : -1;   // Makro oben ist evtl. naeher
    if (mac >= 0 && mac < 10 && mac != t) {
        int dm = abs(mac - v2Foc[v2View]);
        if (dm <= 4 && v2GroupOf(v2Foc[v2View]) == 0) { t = mac; tg = 0; }
    }
    for (int g = 0; g < 30 && v2GroupOf(v2Foc[v2View]) != tg; g++) {
        int cg = v2GroupOf(v2Foc[v2View]);
        bool fwd = (tg > cg && tg - cg < v2N / 2) || (tg < cg && cg - tg > v2N / 2);
        if (view == V2_SOUND) tap(fwd ? BT_RIGHT : BT_LEFT); else combo(BT_SELECT, fwd ? BT_DOWN : BT_UP);
    }
    for (int g = 0; g < 60 && v2Foc[v2View] != t; g++) tap(v2Foc[v2View] < t ? BT_DOWN : BT_UP);
}
static void uiScenariosV2() {
    
    uiCase = "Neue Oberfläche: Ansichten und Liste";
    simReset(); simFrame();
    UCHECK(v2View == V2_SEQ && page == PG_TRG, "Start nicht in SEQ (Ansicht %d, Seite %d)", v2View, page);
    int f0 = v2Foc[V2_SEQ]; tap(BT_DOWN);
    UCHECK(v2Foc[V2_SEQ] != f0 && !v2It[v2Foc[V2_SEQ]].hdr, "RUNTER waehlt keinen anderen Wert");
    UCHECK(track == 0, "RUNTER hat die Spur gewechselt");
    tap(BT_R1); UCHECK(track == 1, "R1 wechselt die Spur nicht (%d)", track);
    tap(BT_L1); UCHECK(track == 0, "L1 wechselt die Spur nicht zurueck");
    tap(BT_R2); UCHECK(v2View == V2_SOUND && trackPage(page), "R2: SOUND erwartet (Ansicht %d)", v2View);
    tap(BT_R2); UCHECK(v2View == V2_FX && v2HasPage(page), "R2: EFFEKTE erwartet (Ansicht %d, Seite %d)", v2View, page);
    tap(BT_R2); UCHECK(v2View == V2_MST && v2HasPage(page), "R2: MASTER erwartet (Ansicht %d, Seite %d)", v2View, page);
    tap(BT_R2); UCHECK(v2View == V2_NONE && page == PG_MIX, "R2: MIX erwartet (Seite %d)", page);
    tap(BT_L2); tap(BT_L2); tap(BT_L2); tap(BT_L2); UCHECK(v2View == V2_SEQ, "4x L2: zurueck zu SEQ");
    for (int i = 0; i < V2_RING_N; i++) tap(BT_R2);
    UCHECK(v2View == V2_SEQ, "R2 im Kreis kommt nicht zu SEQ zurueck (Seite %d)", page);
    {   // SELECT + RUNTER: naechste Gruppe
        v2Build(V2_SEQ); int g0 = v2It[v2Foc[V2_SEQ]].pg; combo(BT_SELECT, BT_DOWN);
        UCHECK(v2It[v2Foc[V2_SEQ]].pg != g0, "SELECT+RUNTER springt nicht zur naechsten Gruppe");
        combo(BT_SELECT, BT_R1); UCHECK(cursor == 4, "SELECT+R1: 4 Steps erwartet (%d)", cursor);
        cursor = 0;
    }
    uiCase = "Neue Oberfläche: Wert, Lock, Trig";
    simReset(); simFrame(); goTrack(4);
    goStep(5); tap(BT_A); UCHECK(curP().s[4][5].on == 1, "SEQ: A setzt keinen Trig auf Step 6");
    v2GoTo(V2_SOUND, PG_FLT, 0);
    UCHECK(page == PG_FLT && slot == 0 && cursor == 5, "Cutoff nicht gewaehlt (Seite %d Regler %d Step %d)", page, slot, cursor);
    int fq = curP().p[4][P_FREQ]; tap(BT_X); UCHECK(curP().p[4][P_FREQ] == fq + 1, "X aendert Cutoff nicht");
    tap(BT_Y);
    {   // SOUND: <> = Gruppe, der Step bleibt
        int g0 = v2GroupOf(v2Foc[V2_SOUND]); tap(BT_RIGHT);
        UCHECK(v2GroupOf(v2Foc[V2_SOUND]) != g0 && cursor == 5, "SOUND: RECHTS springt nicht zur naechsten Gruppe");
        tap(BT_LEFT); UCHECK(v2GroupOf(v2Foc[V2_SOUND]) == g0, "SOUND: LINKS kommt nicht zur vorigen Gruppe zurueck");
        v2GoTo(V2_SOUND, PG_FLT, 0);
    }
    holdDown(BT_A); tap(BT_X); release(BT_A);
    UCHECK(curP().s[4][5].lock[P_FREQ] == fq + 1, "A halten + X: kein Cutoff-Lock (%d)", curP().s[4][5].lock[P_FREQ]);
    curP().s[4][5].on = 0; memset(curP().s[4][5].lock, NO_LOCK, NPARAMS);
    uiCase = "Neue Oberfläche: Kopieren per SELECT + X/Y";
    simReset(); simFrame(); goTrack(4); v2GoTo(V2_SOUND, PG_FLT, 0);
    curP().p[4][P_FREQ] = 33; combo(BT_SELECT, BT_X);
    tap(BT_R1); combo(BT_SELECT, BT_Y);
    UCHECK(curP().p[5][P_FREQ] == 33 && curP().machine[5] == curP().machine[4], "SOUND einfuegen: Spur 6 hat nicht den Sound von Spur 5");
    tap(BT_L2);   // SEQ: Page kopieren/einfuegen
    curP().s[5][1].on = 1; combo(BT_SELECT, BT_X); tap(BT_R1); combo(BT_SELECT, BT_Y);
    UCHECK(curP().s[6][1].on == 1, "SEQ: Page von Spur 6 nicht auf Spur 7 eingefuegt");
    uiCase = "Neue Oberfläche: MEHR-Menü";
    simReset(); simFrame();
    tap(BT_SELECT); UCHECK(mrOpen && !menuOpen, "SELECT oeffnet nicht MEHR");
    mrSel = 0; for (int i = 0; i < V2_RING_N; i++) if (V2_RING[i] == PG_SONG) mrSel = i;
    tap(BT_A); UCHECK(!mrOpen && page == PG_SONG, "MEHR: Ansicht SONG nicht geoeffnet (Seite %d)", page);
    tap(BT_SELECT); mrSel = MR_ENGINE; tap(BT_A); UCHECK(fbOpen && fbMode == FB_MACH, "MEHR: Engine-Liste nicht geoeffnet");
    tap(BT_B);
    tap(BT_SELECT); tap(BT_B); UCHECK(!mrOpen, "B schliesst MEHR nicht");
    tap(BT_SELECT); combo(BT_SELECT, BT_X); UCHECK(!mrOpen && curP().machine[0] == M_KICK, "SELECT+X nach Schliessen von MEHR wirkte weiter");
    uiCase = "Neue Oberfläche: Touch";
    simReset(); simFrame(); simDraw = true; simFrame();
    { int f = v2Foc[V2_SEQ], want = v2Clamp(f + 3, 1);
      int y = 72 + (int)((want - v2Scroll[V2_SEQ]) * 24) + 10;
      tapAt(300, y); UCHECK(v2Foc[V2_SEQ] == want, "Zeile antippen waehlt nicht (%d statt %d)", v2Foc[V2_SEQ], want); }
    tapAt(360, 12); UCHECK(v2View == V2_SOUND || v2View == V2_SEQ, "Reiter in der Kopfzeile antippen");

    // --- volle Touch-Bedienung: alles ohne Tasten erreichbar
    // Gefunden vom Monkey unter AddressSanitizer: die TRIG-Seite hat 12 Regler, MASTER nur 8.
    // Beim Wechsel auf eine Seite mit weniger Reglern muss der Index mitgehen, sonst liest
    // knobInfo hinter MST_DEFS (global-buffer-overflow).
    uiCase = "Regler-Index beim Seitenwechsel";
    simReset(); simFrame();
    goPage(PG_TRG); goSlot(11);
    UCHECK(slot == 11 && page == PG_TRG, "TRIG: Regler 12 nicht erreichbar (Seite %d, Regler %d)", page, slot);
    goPage(PG_MST); simFrame();
    UCHECK(page == PG_MST, "MASTER nicht erreichbar (Seite %d)", page);
    UCHECK(slot < nSlots(), "Regler %d auf MASTER (nur %d Regler)", slot + 1, nSlots());
    { ParamDef d; int v, bv; bool lk; char lb[24];
      for (int k = 0; k < 16; k++) { int o = slot; slot = k; knobInfo(k, d, v, lk, bv, lb, sizeof lb); slot = o; }
      UCHECK(true, "knobInfo mit zu grossem Index"); }   // darf nicht abstuerzen (Sanitizer)
    simReset(); simFrame();

    uiCase = "Liste: Gruppen einklappen";
    {
        simReset(); simFrame();
        int fold0 = cfg.listFold;
        cfg.listFold = 1;
        v2GoTo(V2_SOUND, PG_FLT, 0); simFrame();
        v2Build(v2View);
        int all = v2N, rowsFold = v2Rows(v2Foc[v2View]);
        UCHECK(all > 40, "SOUND hat nur %d Zeilen - Test taugt nicht", all);
        UCHECK(rowsFold < all / 2, "eingeklappt sind es %d von %d Zeilen (zu wenig gespart)", rowsFold, all);
        UCHECK(v2RowOf[v2Foc[v2View]] >= 0, "der Fokus liegt in einer eingeklappten Gruppe");
        int grpHdr = 0; for (int i = 0; i < v2N; i++) if (v2It[i].hdr && v2RowOf[i] >= 0) grpHdr++;
        UCHECK(grpHdr == v2Groups, "nicht alle Gruppenkoepfe sichtbar (%d von %d)", grpHdr, v2Groups);
        // runter durch die Gruppe: der Fokus bleibt immer in einer offenen Zeile
        for (int i = 0; i < 20; i++) {
            tap(BT_DOWN);
            v2Rows(v2Foc[v2View]);
            if (v2RowOf[v2Foc[v2View]] < 0) { UCHECK(false, "Schritt %d: Fokus in einer zugeklappten Gruppe", i); break; }
        }
        cfg.listFold = 0;
        int rowsOpen = v2Rows(v2Foc[v2View]);
        UCHECK(rowsOpen == all, "ausgeschaltet muessen alle %d Zeilen sichtbar sein (%d)", all, rowsOpen);
        cfg.listFold = fold0;
        simReset(); simFrame();
    }

    uiCase = "Grafikspalte: Bilder bleiben im Kasten";
    {   // gemeldet: "die Bilder sind abgeschnitten oder nur halb angezeigt, vor allem beim Wave"
        // Die Spalte ist 150 px breit; Bilder, die fuer den alten breiten Streifen gebaut waren,
        // zeichneten daneben. Die Wache zaehlt jeden Strich, der aus dem Kasten faellt.
        struct Fall { int mach; int pg; const char* nm; };
        static const Fall F[] = {
            {M_WTBL,  PG_SRC,  "WTBL Welle"},      {M_WTBL,  PG_SRC2, "WTBL Stapel"},
            {M_SYNTH, PG_SRC2, "SYNTH Oszillatoren"}, {M_SYNTH, PG_TRG, "Piano-Roll"},
            {M_SYNTH, PG_FLT,  "Filterkurve"},     {M_SYNTH, PG_FLT2, "Base/Width"},
            {M_SYNTH, PG_ENV1, "Huellkurve"},      {M_FM,    PG_SRC2, "FM-Operator"},
            {M_KICK,  PG_SRC,  "Drum"},
        };
        int fehler = 0;
        for (const Fall& f : F) {
            simReset(); simFrame();
            lockA(); Engine::setMachine(curP(), track, f.mach); unlockA();
            goPage(f.pg); simFrame();
            gdWant = true; gdOut = 0; v2ArtKind = 0;
            simFrame();
            gdWant = false; gdClear();
            if (v2ArtKind == 0) continue;   // dieser Fall zeigt kein Gruppenbild
            if (gdOut) { fehler++; UCHECK(false, "%s (Bild %d): %d Striche ausserhalb des Kastens", f.nm, v2ArtKind, gdOut); }
        }
        UCHECK(fehler == 0, "%d von %d Bildern ragen aus der Spalte", fehler, (int)(sizeof F / sizeof *F));
        simReset(); simFrame();
    }

    uiCase = "NT2: von AUS geht es bei Unison los";
    {   // gemeldet: der erste Schritt landete bei -24 statt bei +-0
        simReset(); simFrame();
        goPage(PG_TRG); goSlot(8); simFrame();
        UCHECK(slotParam(slot) == P_NT2, "Regler 8 der TRIG-Seite ist nicht NT2 (%d)", slotParam(slot));
        curP().p[track][P_NT2] = 0;
        tap(BT_X);
        UCHECK(curP().p[track][P_NT2] == NOTEX_ZERO, "aus AUS muss +0 (uni) werden, nicht %+d Halbtoene",
               curP().p[track][P_NT2] - NOTEX_ZERO);
        tap(BT_Y);
        UCHECK(curP().p[track][P_NT2] == NOTEX_ZERO - 1, "von Unison nach unten muss -1 kommen (%+d)",
               curP().p[track][P_NT2] - NOTEX_ZERO);
        tap(BT_X); tap(BT_X);
        UCHECK(curP().p[track][P_NT2] == NOTEX_ZERO + 1, "von Unison nach oben muss +1 kommen (%+d)",
               curP().p[track][P_NT2] - NOTEX_ZERO);
        curP().p[track][P_NT2] = 1;   // -24: eine Stufe tiefer ist AUS
        tap(BT_Y);
        UCHECK(curP().p[track][P_NT2] == 0, "unter -24 muss AUS liegen (%d)", curP().p[track][P_NT2]);
        simReset(); simFrame();
    }

    uiCase = "Vorhoeren beim Aendern bleibt aus";
    {   // gemeldet: jede Notenaenderung spielte den Klang an
        simReset(); simFrame();
        int pv0 = cfg.preview;
        cfg.preview = 0;
        goPage(PG_TRG); goSlot(0); simFrame();
        UCHECK(slotParam(slot) == P_NOTE, "Regler 0 der TRIG-Seite ist nicht NOTE (%d)", slotParam(slot));
        for (int t = 0; t < NTRACKS; t++) eng.v[t].active = false;
        tap(BT_X); tap(BT_X);
        int klingt = 0; for (int t = 0; t < NTRACKS; t++) if (eng.v[t].active) klingt++;
        UCHECK(klingt == 0, "Note geaendert und %d Stimmen klingen trotz 'Vorhoeren aus'", klingt);
        cfg.preview = 1;   // eingeschaltet soll es weiter vorhoeren
        tap(BT_X);
        klingt = 0; for (int t = 0; t < NTRACKS; t++) if (eng.v[t].active) klingt++;
        UCHECK(klingt > 0, "mit 'Vorhoeren an' muesste die Note zu hoeren sein");
        cfg.preview = pv0;
        simReset(); simFrame();
    }

    uiCase = "Kompressor-Routing (MIX-Zeile CMP)";
    {
        simReset(); simFrame();
        goView(PG_MIX); simFrame();
        UCHECK(page == PG_MIX, "MIX nicht erreichbar (Seite %d)", page);
        for (int i = 0; i < MIX_ROWS && MIX_P[mixRow] != P_CMPS; i++) tap(BT_DOWN);
        UCHECK(MIX_P[mixRow] == P_CMPS, "Zeile CMP nicht erreichbar (Zeile %d)", mixRow);
        for (int i = 0; i < NTRACKS && track != 2; i++) tap(BT_RIGHT);   // im Mixer waehlt links/rechts den Kanal
        UCHECK(track == 2, "Kanal 3 nicht erreichbar (%d)", track);
        UCHECK(curP().p[2][P_CMPS] == 1, "COMP steht nicht auf AN (%d)", curP().p[2][P_CMPS]);
        tap(BT_Y);
        UCHECK(curP().p[2][P_CMPS] == 0, "Y schaltet COMP nicht aus (%d)", curP().p[2][P_CMPS]);
        tap(BT_X);
        UCHECK(curP().p[2][P_CMPS] == 1, "X schaltet COMP nicht wieder an (%d)", curP().p[2][P_CMPS]);
        v2GoTo(V2_SOUND, PG_AMP, 2);   // derselbe Wert auf der AMP-Seite
        ParamDef d; int v, bv; bool lk; char lb[24];
        knobInfo(slot, d, v, lk, bv, lb, sizeof lb);
        UCHECK(!strcmp(d.name, "COMP"), "AMP Regler 3 ist nicht COMP (%s)", d.name);
        simReset(); simFrame();
    }

    // Gemeldet: nach "Neues Projekt" wirkte das Steuerkreuz links/rechts auf die Gruppe
    // (Makros, LFO3 ...) statt auf die Steps - die Ansicht blieb auf SOUND stehen.
    uiCase = "Neues Projekt: Ansicht und Cursor";
    {
        simReset(); simFrame();
        goPage(PG_FLT);                      // irgendwo in SOUND, Gruppe weit unten
        goTrack(5); goStep(9);
        UCHECK(v2View == V2_SOUND, "Vorbereitung: nicht in SOUND (%d)", v2View);
        doNewProject("TestNeu");
        simFrame();
        UCHECK(v2View == V2_SEQ, "nach Neues Projekt: Ansicht SEQ erwartet (%d)", v2View);
        UCHECK(page == PG_TRG && slot == 0, "nach Neues Projekt: TRIG/Regler 1 erwartet (Seite %d, Regler %d)", page, slot);
        UCHECK(track == 0 && cursor == 0, "nach Neues Projekt: Spur 1/Step 1 erwartet (%d/%d)", track + 1, cursor + 1);
        int c0 = cursor, p0 = page;
        tap(BT_RIGHT); tap(BT_RIGHT);
        UCHECK(cursor == c0 + 2 && page == p0, "links/rechts muss die Steps bewegen (Step %d, Seite %d)", cursor + 1, page);
        // und das neue Projekt ist leer, nicht der Demo-Song
        int trigs = 0;
        for (int t = 0; t < NTRACKS; t++) for (int st = 0; st < NSTEPS; st++) if (eng.proj.pat[0].s[t][st].on) trigs++;
        UCHECK(trigs == 0, "neues Projekt ist nicht leer (%d Trigs)", trigs);
        UCHECK(!eng.songOn, "neues Projekt startet im Song-Modus");
    }
    simReset(); simFrame();

    uiCase = "Touch: alles mit dem Finger";
    simReset(); simFrame(); simDraw = true; simFrame();
    { // Spur waehlen und stummschalten (M am rechten Rand der Spurzeile)
      UCHECK(tapHit(HK_TRACK, 2), "Spur 3 ist nicht antippbar");
      UCHECK(track == 2, "Spur antippen waehlt sie nicht (%d)", track + 1);
      bool mu0 = eng.muted(2);
      UCHECK(tapHit(HK_MUTE, 2), "kein M-Feld an der Spur");
      UCHECK(eng.muted(2) != mu0, "M am Spurrand schaltet nicht stumm");
      tapHit(HK_MUTE, 2); UCHECK(eng.muted(2) == mu0, "M schaltet nicht zurueck"); }
    { // Play/Stop in der Kopfzeile
      bool p0 = eng.playing;
      UCHECK(tapHit(HK_PLAY, 0), "PLAY/STOP ist nicht antippbar");
      UCHECK(eng.playing != p0, "PLAY/STOP in der Kopfzeile reagiert nicht");
      tapHit(HK_PLAY, 0); UCHECK(eng.playing == p0, "PLAY/STOP schaltet nicht zurueck"); }
    { // MEHR-Knopf unten rechts
      UCHECK(tapHit(HK_MORE, -1), "kein MEHR-Knopf");
      UCHECK(mrOpen, "MEHR-Knopf oeffnet das Menue nicht");
      tapAt(10, 200); UCHECK(!mrOpen, "daneben tippen schliesst das MEHR-Menue nicht"); }
    { // Gruppe per Pfeil in der Listen-Kopfzeile (SOUND hat viele Gruppen)
      v2GoView(1); simFrame();
      int g0 = v2GroupOf(v2Foc[V2_SOUND]);
      UCHECK(tapHit(HK_V2GRP, 1), "kein Pfeil > in der Listen-Kopfzeile");
      UCHECK(v2GroupOf(v2Foc[V2_SOUND]) != g0, "Pfeil > wechselt die Gruppe nicht");
      UCHECK(tapHit(HK_V2GRP, -1), "kein Pfeil < in der Listen-Kopfzeile");
      UCHECK(v2GroupOf(v2Foc[V2_SOUND]) == g0, "Pfeil < kommt nicht zurueck"); }
    { // senkrecht wischen: der Fokus wandert mit
      v2GoView(1); simFrame();
      int row = visibleRow();
      UCHECK(row >= 0, "keine sichtbare Listenzeile");
      tapHit(HK_V2ROW, row);
      int f0 = v2Foc[V2_SOUND];
      UCHECK(dragHit(HK_V2ROW, row, 0, -140, 7), "Listenzeile fuers Wischen nicht gefunden");
      UCHECK(v2Foc[V2_SOUND] > f0, "Wischen nach oben scrollt die Liste nicht (%d -> %d)", f0, v2Foc[V2_SOUND]); }
    { // waagerecht ziehen aendert den Wert, nicht die Auswahl
      v2GoView(1); simFrame();
      int row = visibleRow();
      tapHit(HK_V2ROW, row);
      int f0 = v2Foc[V2_SOUND], idx = uiParam(page, slot);
      int v0 = idx >= 0 ? curP().p[track][idx] : -1;
      UCHECK(idx >= 0, "Listenzeile zeigt auf keinen Spur-Parameter (Seite %d, Regler %d)", page, slot);
      UCHECK(dragHit(HK_V2ROW, row, v0 > 60 ? -150 : 150, 0, 9), "Listenzeile %d nicht als Flaeche gefunden", row);
      UCHECK(idx < 0 || curP().p[track][idx] != v0, "waagerecht ziehen aendert den Wert nicht (%d, Seite %d, Regler %d)", v0, page, slot);
      UCHECK(v2Foc[V2_SOUND] == f0, "waagerecht ziehen hat die Auswahl verschoben"); }
    simDraw = false;
}
static void v2ListInfo() {
    int keepTr = track, keepPg = page;
    bout("\nLISTEN-LAENGEN (neue Oberflaeche)\n  Engine        SEQ   SOUND  Gruppen  ^v von oben nach unten\n");
    for (int m = 0; m < M_COUNT; m++) {
        track = 0; lockA(); Engine::setMachine(curP(), 0, m); unlockA();
        page = PG_TRG; v2View = V2_SEQ; v2Build(V2_SEQ); int nSeq = v2N;
        page = PG_SRC; v2View = V2_SOUND; v2Build(V2_SOUND); int nSnd = v2N, gr = 0;
        for (int i = 0; i < v2N; i++) if (v2It[i].hdr) gr++;
        bout("  %-12s %5d %6d %7d %6d\n", MACHINE_NAMES[m], nSeq, nSnd, gr, nSnd - 1);
    }
    track = keepTr; page = keepPg;
}
static void uiTasks() {   // Bedienweg-Report: wie viele Tasten kostet eine typische Aufgabe
    
    Task T[16]; int n = 0;
    auto rec = [&](const char* nm, int steps) { T[n].name = nm; T[n].keys = simKeys; T[n].steps = steps; n++; };
    simReset(); simFrame(); for (int s = 0; s < 16; s += 4) { if (s) combo(BT_SELECT, BT_R1); tap(BT_A); } rec("Kick: 4 Trigs (1/5/9/13)", 4);
    simReset(); simFrame(); goTrack(4); v2GoTo(V2_SOUND, PG_FLT, 0); stickR(-1.f, 300); rec("Spur 5: Filter-Cutoff aendern", 3);
    simReset(); simFrame(); goTrack(4); goStep(6); v2GoTo(V2_SOUND, PG_FLT, 0); holdDown(BT_A); stickR(-1.f, 300); release(BT_A); rec("Spur 5: Cutoff-Lock auf Step 7", 4);
    UCHECK(curP().s[4][6].lock[P_FREQ] != NO_LOCK, "Aufgabe Cutoff-Lock (neu)");
    simReset(); simFrame(); goTrack(2); goView(PG_SRC); goItem(PG_TRG, 6);
    tap(BT_X);
    tap(BT_DOWN); tap(BT_A); rec("Spur 3: Engine wechseln", 3);
    UCHECK(curP().machine[2] != M_HAT, "Aufgabe Engine (neu)");
    simReset(); simFrame(); combo(BT_SELECT, BT_RIGHT); rec("Naechstes Pattern", 1);
    simReset(); simFrame(); goTrack(3); combo(BT_SELECT, BT_A); rec("Spur 4 stumm", 2);
    combo(BT_SELECT, BT_A);
    simReset(); simFrame(); tap(BT_R2); tap(BT_R2); tap(BT_R2);   // SOUND, EFFEKTE, MASTER
    for (int g = 0; g < 8 && page != PG_MST; g++) tap(BT_RIGHT);
    for (int g = 0; g < 10 && !(page == PG_MST && slot == MS_BPM); g++) tap(BT_DOWN);
    stickR(1.f, 300); rec("Tempo aendern", 3);
    UCHECK(eng.proj.bpm != 120, "Aufgabe Tempo (neu)");
    simReset(); simFrame(); goTrack(1); v2GoTo(V2_SOUND, PG_SEND, 0); stickR(1.f, 300); rec("Spur 2: Delay-Send", 3);
    simReset(); simFrame(); goTrack(4); v2GoTo(V2_SOUND, PG_LFO1, LF_DEST); tap(BT_X);
    for (int g = 0; g < 400 && fbOpen && !fbDir[0] && !(fbEnt[fbSel].dir && fbEnt[fbSel].idx == 2) && fbSel < fbN - 1; g++) tap(BT_DOWN);
    tap(BT_A); for (int g = 0; g < 400 && fbOpen && fbEnt[fbSel].idx != P_FREQ + 1 && fbSel < fbN - 1; g++) tap(BT_DOWN);
    tap(BT_A); tap(BT_DOWN); stickR(1.f, 300); rec("Spur 5: LFO1 -> Filter", 5);
    simReset(); simFrame(); goTrack(4); v2GoTo(V2_SOUND, PG_ENV1, 0); stickR(1.f, 200); v2GoTo(V2_SOUND, PG_ENV1, 3); stickR(1.f, 200); rec("Spur 5: Attack + Release", 5);
    simReset(); simFrame(); goTrack(4); v2GoTo(V2_SEQ, PG_ARP, 0); tap(BT_X); rec("Spur 5: Arpeggiator an", 3);
    simReset(); simFrame(); goTrack(4); v2GoTo(V2_SEQ, PG_SEQ, 1); stickR(1.f, 200); rec("Spur 5: Spurlänge", 3);
    simReset(); simFrame(); combo(BT_SELECT, BT_START); rec("Projekt speichern", 1);
    simReset(); simFrame(); tap(BT_SELECT); simWait(50); rec("MEHR-Menue oeffnen", 1);
    tap(BT_B);
    bout("\nBEDIENWEG - NEUE OBERFLAECHE (gleiche Aufgaben)\n");
    bout("  Aufgabe                          Tasten  ca. Zeit\n");
    int sum = 0;
    for (int i = 0; i < n; i++) {
        double sec = T[i].keys * 0.28 + T[i].steps * 1.2;
        bout("  %-32s %6d %7.1f s%s\n", T[i].name, T[i].keys, sec, T[i].keys >= 12 ? "  <- lang" : "");
        sum += T[i].keys;
    }
    bout("  Summe %d Tasten\n", sum);
}
static void uiMonkey(int frames) {
    uint32_t r = 12345;
    auto rnd = [&]() { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return r; };
    simReset();
    simDraw = true;
    int bad = 0;
    for (int i = 0; i < frames; i++) {
        uint32_t x = rnd();
        if (x % 7 == 0) { int b = (int)(rnd() % BT_COUNT); if (b != BT_START || rnd() % 4 == 0) simDown[b] = !simDown[b]; }
        if (x % 11 == 0) { simLX = (float)((int)(rnd() % 3) - 1); simLY = (float)((int)(rnd() % 3) - 1); }
        if (x % 13 == 0) { simRX = ((int)(rnd() % 200) - 100) / 100.f; simRY = ((int)(rnd() % 200) - 100) / 100.f; }
        if (x % 97 == 0) for (int b = 0; b < BT_COUNT; b++) simDown[b] = false;
        if (confirmOpen && x % 3 == 0) { confirmOpen = false; }   // keine Abfragen bestaetigen (Laden/Loeschen)
        quitReq = false;
        simFrame();
        if (page < 0 || page >= NUI_PAGES || track < 0 || track >= NTRACKS || slot < 0 || slot >= nSlots() || cursor < 0 || cursor >= NSTEPS) {
            if (bad++ < 5) printf("  FEHLER Monkey Bild %d: page %d track %d slot %d cursor %d\n", i, page, track, slot, cursor);
        }
    }
    simDraw = false;
    for (int b = 0; b < BT_COUNT; b++) simDown[b] = false;
    UCHECK(bad == 0, "Monkey: %d Zustandsfehler", bad);
    printf("  UI-Monkey: %d zufaellige Bilder ok\n", frames);
}

// Engine-Stress: zufaellige Parameter, Locks, Engines, Tempo, MIDI, Speichern/Laden waehrend der Wiedergabe
static void engineStress(double secs) {
    uint32_t r = 777;
    auto rnd = [&]() { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return r; };
    static float blk[512 * 2];
    eng.defaultProject(true);
    eng.start();
    int blocks = (int)(secs * eng.sr / 256);
    float pk = 0; int nan = 0; double tot = 0, worst = 0;
    char tmpPath[700]; snprintf(tmpPath, sizeof tmpPath, "%sstress.gbx", basePath);
    for (int i = 0; i < blocks; i++) {
        for (int k = 0; k < 4; k++) {
            uint32_t x = rnd();
            Pattern& P = eng.proj.pat[eng.curPat];
            int t = (int)(x % NTRACKS), pi = (int)((x >> 3) % NPARAMS), s = (int)((x >> 10) % NSTEPS);
            ParamDef d = paramDef(P.machine[t], pi);
            int v = d.max > 0 ? (int)(rnd() % (d.max + 1)) : 0;
            if (pi == P_QLEN) { P.len[t] = (uint8_t)(1 + v % NSTEPS); continue; }
            if (pi == P_QSPD) { P.speed[t] = (uint8_t)(v % 7); continue; }
            switch ((x >> 20) % 8) {
            case 0: case 1: P.p[t][pi] = (uint8_t)v; break;
            case 2: P.s[t][s].on = (uint8_t)(rnd() % 3); P.s[t][s].lock[pi] = (uint8_t)v; break;
            case 3: P.s[t][s].on = (uint8_t)(rnd() % 2); break;
            case 4: if (rnd() % 50 == 0) Engine::setMachine(P, t, (int)(rnd() % M_COUNT)); break;
            case 5: if (rnd() % 200 == 0) eng.proj.bpm = (uint16_t)(30 + rnd() % 271); break;
            case 6: eng.midiIn((uint8_t)(0x90 | (rnd() % 8)), (uint8_t)(rnd() % 128), (uint8_t)(rnd() % 128)); break;
            case 7: if (rnd() % 300 == 0) eng.nextPat = (int)(rnd() % NPATTERNS); break;
            }
        }
        if (i % 2000 == 1999) { eng.save(tmpPath); eng.load(tmpPath); }
        int n = 64 + (int)(rnd() % 449);   // wechselnde Blockgroessen
        double t0 = nowMs();
        eng.render(blk, n);
        double el = nowMs() - t0; tot += el; if (el > worst) worst = el;
        for (int k = 0; k < n * 2; k++) { float y = blk[k]; if (!(y == y) || y > 1e6f || y < -1e6f) nan++; else if (fabsf(y) > pk) pk = fabsf(y); }
    }
    eng.stop();
    remove(tmpPath);
    UCHECK(nan == 0, "Stress: %d NaN/Inf-Werte", nan);
    UCHECK(pk <= 1.01f, "Stress: Ausgang %.2f über Vollaussteuerung", pk);
    printf("  Engine-Stress: %.0f s Audio, Spitze %.2f, Last %.1f%%, laengster Block %.2f ms\n", secs, pk, tot / (secs * 1000) * 100, worst);
}

// Sprachtest: alles auf Englisch zeichnen und angezeigte Texte auf deutsche Reste pruefen
static char langSeen[1500][100]; static int langN = 0;
static void langHook(const char* t) {
    if (strlen(t) < 2 || langN >= 1500) return;
    for (int i = 0; i < langN; i++) if (!strcmp(langSeen[i], t)) return;
    snprintf(langSeen[langN++], 100, "%s", t);
}
static bool looksGerman(const char* t) {
    if (strstr(t, "Sprache / Language") || !strcmp(t, "Deutsch")) return false;
    for (const unsigned char* c = (const unsigned char*)t; *c; c++) if (*c == 0xC3) return true;   // Umlaut/ß
    static const char* const W[] = {"SPUR", "SPUREN", "WAEHLEN", "LADEN", "SPEICHERN", "STUMM", "LOESCHEN", "ZURUECK", "KLANG", "AUS", "AN", "LEER",
        "SEITE", "QUELLE", "WERT", "REGLER", "NEU", "UND", "DER", "DIE", "DAS", "MIT", "VON", "IST", "EIN", "EINE", "NICHT", "ALLE", "PROJEKT",
        "EINSTELLUNGEN", "TASTEN", "ABBRUCH", "AKTIV", "GELADEN", "GESPEICHERT", "SCHLAG", "STEUERKREUZ", "AUFNAHME", "SCHRITTE", "AUSSETZER",
        "EFFEKTE", "KOMPRESSOR", "MEHR", "INTERN", "AUSGANG", "PUFFER", "OHNE", "HALTEN", "ZIEL", "SENDEN", "SUMME", "BEREIT", "KANAL", "ZEILE", "EINTRAG", 0};
    char tok[64]; int k = 0;
    for (const char* c = t;; c++) {
        if (isalpha((unsigned char)*c) && k < 63) { tok[k++] = (char)toupper((unsigned char)*c); continue; }
        tok[k] = 0;
        if (k >= 2) for (int i = 0; W[i]; i++) if (!strcmp(tok, W[i])) return true;
        k = 0;
        if (!*c) break;
    }
    return false;
}
static void langCheck() {
    int saveLang = g_lang;
    g_lang = 1; langN = 0; textHook = langHook; msgUntil = 0; msg[0] = 0;
    simReset();
    static const int engs[] = {M_KICK, M_SYNTH, M_FM, M_WTBL, M_SAMPLE, M_CHOP, M_GRAN};
    for (int e : engs) {
        Engine::setMachine(curP(), 4, e); track = 4;
        for (int p = 0; p < NUI_PAGES; p++) {
            if (!pageAvail(p)) continue;
            page = p;
            for (int sl = 0; sl < 8; sl++) { slot = sl; drawAll(false); }
        }
    }
    page = PG_SYS; for (int i = 0; i < SI_COUNT; i++) { setSel = i; setScroll = i - 7.5f; drawAll(false); }
    page = PG_TRG; drawAll(true);                                     // Hilfe
    menuOpen = true; drawAll(false); menuOpen = false;
    for (int m = KB_SAVEAS; m <= KB_PSAVE; m++) { openKeyboard(m, "x"); drawAll(false); kbOpen = false; }
    fbParam = P_L1 + LF_DEST;
    for (int m = FB_SAMPLE; m <= FB_MACH; m++) { openBrowser(m); drawAll(false); if (m == FB_LDEST && fbN > 1) { fbSel = 1; fbPick(); drawAll(false); } fbClose(true); }
    { int ot = track; track = 6; lockA(); Engine::setMachine(curP(), 6, M_WTBL); unlockA(); openWtEditor(); drawAll(false); wteMenu = true; drawAll(false); wteMenu = false; wteClose(false); track = ot; }
    for (int b = 0; b < NBUS; b++) for (int ty = 0; ty < BF_COUNT; ty++) { lockA(); Engine::setBusType(eng.proj, b, ty); unlockA(); page = b == 0 ? PG_FX : (b == 1 ? PG_FXB : PG_FXC); for (int k = 0; k < 8; k++) { slot = k; drawAll(false); } }
    { int ot = track; track = 5; lockA(); Engine::setMachine(curP(), 5, M_FM); unlockA(); for (int pg = PG_SRC3; pg <= PG_SRC5; pg++) { page = pg; drawAll(false); } page = PG_SRC2; drawAll(false); track = ot; }
    eng.recording = true; drawAll(false); eng.recording = false;
    textHook = nullptr; g_lang = saveLang;
    int bad = 0;
    for (int i = 0; i < langN; i++) if (looksGerman(langSeen[i])) { if (bad++ < 40) printf("  DEUTSCH IN ENGLISCH: '%s'\n", langSeen[i]); }
    printf("  Sprachtest: %d verschiedene Texte, %d mit deutschen Resten\n", langN, bad);
    UCHECK(bad == 0, "Englische Oberflaeche enthaelt %d deutsche Texte", bad);
    Engine::setMachine(curP(), 4, M_SYNTH);
}

static int runSelftest(double stressSecs) {
    selfTesting = true;
    char tmp[600]; snprintf(tmp, sizeof tmp, "/tmp/gb-selftest-%d/", (int)getpid());
    mkdir(tmp, 0755);
    char sub[700]; snprintf(sub, sizeof sub, "%sprojects", tmp); mkdir(sub, 0755);
    char smp[700]; snprintf(smp, sizeof smp, "%ssamples", tmp);
    char src[700]; snprintf(src, sizeof src, "%ssamples", basePath);
    {   // eigene Kopie der Test-Samples: Editoren speichern dort (nie in die echten Samples schreiben)
        char cp[1500]; snprintf(cp, sizeof cp, "cp -rL '%s' '%s' 2>/dev/null", src, smp);
        if (system(cp) != 0 && symlink(src, smp) != 0) {}
    }
    snprintf(basePath, sizeof basePath, "%s", tmp);
    reloadSamples();   // Bibliothek aus der Kopie (dort speichern die Editoren)
    snprintf(cfgPath, sizeof cfgPath, "%sinput.cfg", tmp);
    snprintf(setPath, sizeof setPath, "%ssettings.cfg", tmp);   // nicht die echten Einstellungen ueberschreiben
    snprintf(cfg.project, sizeof cfg.project, "Selbsttest");
    projectPath(cfg.project, projPath, sizeof projPath);
    printf("UI-Szenarien\n");
    uiScenarios();
    uiScenariosV2();
    uiTasks();
    v2ListInfo();
    printf("Sprachtest\n");
    uiCase = "Sprache";
    langCheck();
    printf("UI-Monkey\n");
    uiMonkey(20000);
    printf("Engine-Stress\n");
    engineStress(stressSecs);
    printf("SZENARIEN (Bedienung):\n");
    for (int i = 0; i < nCases; i++) printf("  %s %s (%d Pruefungen%s)\n", uiCases[i].fail ? "FEHLER" : "OK    ", uiCases[i].name[0] ? uiCases[i].name : "Grundbedienung", uiCases[i].pass + uiCases[i].fail, uiCases[i].fail ? ", Fehler!" : "");
    printf("%s: %d UI-Pruefungen ok, %d Fehler\n", uiFail ? "FEHLGESCHLAGEN" : "OK", uiPass, uiFail);
    char cmd[700]; snprintf(cmd, sizeof cmd, "rm -rf '%s'", tmp);
    if (system(cmd) != 0) {}
    return uiFail ? 1 : 0;
}
