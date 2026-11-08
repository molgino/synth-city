// ---------------------------------------------------------------- SAMPLER-Seite (PG_SMPR)
// Aufnahme vom USB-Eingang (PC/Handy -> R36S, ueber pcRec in usbaudio.cpp) oder vom Master (smpPush im Audio-Callback),
// bis 60 s im Speicher. Bearbeiten: START/ENDE, Chops (gleich oder an Transienten), Normalisieren, Fades.
// Vorhoeren der Auswahl bzw. eines Chops (smpPreviewMix im Audio-Callback). Speichern nach samples/sampler/smp-NN.wav,
// Chops zusaetzlich einzeln (smp-NN/01.wav ...), auf Wunsch direkt in die aktuelle Spur (SMPL oder CHOP).
static bool writeWav(const char* path, const float* data, int frames, int sr);
enum { SMP_SEC = 60, SMP_MAXCHOP = 64, SMP_COLS = 600 };
enum { SK_SRC, SK_START, SK_END, SK_CHOPS, SK_MODE, SK_NORM, SK_FADE, SK_DEST, SK_LEN, SK_THR, SK_COUNT };
static float* smpBuf = nullptr;                  // Stereo verschraenkt
static int smpCap = 0, smpRate = 44100;
static volatile int smpLen = 0;                  // Frames der Aufnahme
static volatile int smpState = 0;                // 0 bereit, 1 wartet auf Signal bzw. Page-Anfang (Master), 2 nimmt auf (Master), 3 fertig (Master)
static volatile float smpThr = 0.01f;           // Start-Schwelle (linear, < 0 = sofort)
static volatile int smpTarget = 0;               // feste Laenge in Frames (LAENGE = N Pages), 0 = FREI
static bool smpFromPc = false;                   // USB-Aufnahme laeuft ueber pcRec
static int smpSrc = 0, smpStart = 0, smpEnd = 0, smpChops = 0, smpMode = 0, smpNorm = 1, smpFade = 5, smpDest = 0;
static int smpCut[SMP_MAXCHOP + 1], smpNCut = 0;  // Grenzen der Chops (Frames), smpNCut Chops
static volatile int pvPos = 0, pvEnd = 0; static volatile bool pvOn = false;
static int smpChopPv = -1;                       // zuletzt vorgehoerter Chop
static float smpWMin[SMP_COLS], smpWMax[SMP_COLS]; static int smpWLen = -1;   // Wellenform-Cache
static float smpPeak = 0.f;                      // fuer Normalisieren / Anzeige
static int smpSavedNo = 0;

static bool smpAlloc() {
    int need = eng.sr * SMP_SEC;
    if (smpBuf && smpCap >= need) return true;
    pvOn = false;
    free(smpBuf); smpBuf = (float*)malloc(sizeof(float) * 2 * (size_t)need);
    smpCap = smpBuf ? need : 0;
    return smpBuf != nullptr;
}
// Audio-Thread: Quelle Spur n (smpSrc 2..9) -> Engine schreibt die Spur zusaetzlich nach smpTap
static float smpTap[4096 * 2];
static const float* smpAudioPre(int frames) {
    bool on = (smpState == 1 || smpState == 2) && smpSrc >= 2 && frames <= 4096;
    eng.tapTrack = on ? smpSrc - 2 : -1;
    eng.tapOut = on ? smpTap : nullptr;
    if (on) memset(smpTap, 0, sizeof(float) * 2 * (size_t)frames);
    return on ? smpTap : nullptr;
}
// Audio-Thread: Master bzw. Spur mitschneiden (startet mit dem ersten Signal)
static void smpPush(const float* st, int frames) {
    int s = smpState;
    int n = smpLen, tg = smpTarget, bar = eng.barOff;
    eng.barOff = -1;
    if (s != 1 && s != 2) return;
    int cap = tg > 0 && tg < smpCap ? tg : smpCap;
    for (int i = 0; i < frames; i++) {
        float L = st[i * 2], R = st[i * 2 + 1];
        if (s == 1) {
            if (tg > 0) { if (bar >= 0 && i >= bar) s = 2; else continue; }   // feste Laenge: ab Page-Anfang
            else if (fabsf(L) > smpThr || fabsf(R) > smpThr) s = 2; else continue;
        }
        if (n >= cap) { s = 3; break; }
        smpBuf[n * 2] = L; smpBuf[n * 2 + 1] = R; n++;
    }
    if (s == 2 && n >= cap) s = 3;
    smpLen = n; smpState = s;
}
// Laenge LAENGE in Frames bei Rate sr (Pages zu 16 Steps im aktuellen Tempo), 0 = FREI
static int smpPagesFrames(int sr) { return cfg.recPages ? (int)(cfg.recPages * 16.0 * 60.0 * sr / (eng.proj.bpm * 4.0) + 0.5) : 0; }
static float recThrLin() { return cfg.recThr <= -61 ? -1.f : powf(10.f, cfg.recThr / 20.f); }   // Schwelle dB -> linear
static float smpGain() { return smpNorm && smpPeak > 0.0001f ? 0.966f / smpPeak : 1.f; }   // -0,3 dB
// Audio-Thread: Vorhoeren
static void smpPreviewMix(float* st, int frames) {
    if (!pvOn || !smpBuf) return;
    int p = pvPos, e = pvEnd;
    float g = smpGain();
    for (int i = 0; i < frames && p < e; i++, p++) { st[i * 2] += smpBuf[p * 2] * g; st[i * 2 + 1] += smpBuf[p * 2 + 1] * g; }
    pvPos = p;
    if (p >= e) pvOn = false;
}
static void smpPeakCalc() {
    float pk = 0.f;
    for (int i = smpStart; i < smpEnd; i++) pk = fmaxf(pk, fmaxf(fabsf(smpBuf[i * 2]), fabsf(smpBuf[i * 2 + 1])));
    smpPeak = pk;
}
// Chops: gleich lang oder an den staerksten Einsaetzen (Energie in 10-ms-Fenstern, Anstieg, mind. 60 ms Abstand)
static void smpChopCalc() {
    smpNCut = 0;
    int a = smpStart, b = smpEnd, len = b - a;
    if (smpChops < 2 || len < 2000) return;
    int n = smpChops;
    if (smpMode == 0) {
        for (int i = 0; i <= n; i++) smpCut[i] = a + (int)((long long)len * i / n);
        smpNCut = n;
        return;
    }
    int hop = smpRate / 100, nh = len / hop;
    if (nh < 4) return;
    static float en[SMP_SEC * 100 + 8], on[SMP_SEC * 100 + 8];
    if (nh > SMP_SEC * 100) nh = SMP_SEC * 100;
    for (int h = 0; h < nh; h++) {
        float e = 0.f;
        for (int i = 0; i < hop; i++) { int k = a + h * hop + i; float m = smpBuf[k * 2] + smpBuf[k * 2 + 1]; e += m * m; }
        en[h] = log10f(e / hop + 1e-9f);
    }
    for (int h = 0; h < nh; h++) { float d = h ? en[h] - en[h - 1] : 0.f; on[h] = d > 0.f && en[h] > -5.f ? d : 0.f; }
    int cut[SMP_MAXCHOP]; int nc = 0;
    const int minGap = 6;   // 60 ms
    for (int k = 0; k < n - 1; k++) {   // wiederholt den staerksten Einsatz nehmen, Nachbarn sperren
        int best = -1; float bv = 0.3f;
        for (int h = 2; h < nh - 2; h++) if (on[h] > bv) { bv = on[h]; best = h; }
        if (best < 0) break;
        cut[nc++] = a + best * hop - hop / 2;
        for (int h = best - minGap; h <= best + minGap; h++) if (h >= 0 && h < nh) on[h] = 0.f;
    }
    for (int i = 1; i < nc; i++) for (int j = i; j > 0 && cut[j] < cut[j - 1]; j--) { int t = cut[j]; cut[j] = cut[j - 1]; cut[j - 1] = t; }
    smpCut[0] = a;
    for (int i = 0; i < nc; i++) smpCut[i + 1] = cut[i];
    smpCut[nc + 1] = b;
    smpNCut = nc + 1;
}
static void smpTakeReady() {   // neue Aufnahme liegt in smpBuf: Bereich, Chops, Wellenform
    smpStart = 0; smpEnd = smpLen;
    if (!cfg.recPages) while (smpEnd > smpRate / 10 && fabsf(smpBuf[(smpEnd - 1) * 2]) < 0.003f && fabsf(smpBuf[(smpEnd - 1) * 2 + 1]) < 0.003f) smpEnd--;   // Stille am Ende
    smpPeakCalc(); smpChopCalc(); smpWLen = -1; smpChopPv = -1;
    toast(TR("AUFNAHME: %.1f s", "RECORDED: %.1f s"), smpLen / (float)smpRate);
}
// Hauptschleife: USB-Aufnahme abholen bzw. Master-Aufnahme abschliessen
static bool smpTakePc(const float* d, int n) {
    if (!smpFromPc) return false;
    smpFromPc = false;
    if (!smpAlloc()) return true;
    if (n > smpCap) n = smpCap;
    int tg = smpPagesFrames(pcRecRate());
    if (tg > 0 && n > tg) n = tg;
    memcpy(smpBuf, d, sizeof(float) * 2 * (size_t)n);
    smpRate = pcRecRate(); smpLen = n;
    smpTakeReady();
    return true;
}
static void smpUpdate() {
    if (smpFromPc && cfg.recPages && pcRecState() == 2 && pcRecSeconds() * pcRecRate() >= smpPagesFrames(pcRecRate())) pcRecStop();   // USB: feste Laenge
    if (smpState == 3) { smpState = 0; if (smpLen > 0) smpTakeReady(); }
}
static float smpSeconds() { return smpLen > 0 ? smpLen / (float)smpRate : 0.f; }
static bool smpRecording() { return smpState == 1 || smpState == 2 || (smpFromPc && (pcRecState() == 1 || pcRecState() == 2)); }
static void smpRecToggle() {
    if (smpRecording()) {   // stoppen
        if (smpFromPc) pcRecStop();
        else smpState = smpState == 1 ? 0 : 3;
        return;
    }
    pvOn = false;
    if (smpSrc == 0) {   // USB vom PC/Handy
        if (usbMode != 2) { toast(TR("ERST USB-MODUS PC + AUDIO-INTERFACE", "SET USB MODE PC + AUDIO INTERFACE FIRST")); return; }
        if (pcRecState() == 3) pcRecDone();
        if (!pcRecStart(recThrLin())) { toast(TR("KEIN SPEICHER", "OUT OF MEMORY")); return; }
        smpFromPc = true;
        toast(TR("AM PC/HANDY ABSPIELEN - AUFNAHME STARTET MIT DEM TON", "PLAY ON PC/PHONE - RECORDING STARTS WITH THE SOUND"));
    } else {
        if (!smpAlloc()) { toast(TR("KEIN SPEICHER", "OUT OF MEMORY")); return; }
        int tg = smpPagesFrames(eng.sr);
        if (tg > smpCap) tg = smpCap;
        lockA(); smpRate = eng.sr; smpLen = 0; smpTarget = tg; smpThr = recThrLin(); eng.barOff = -1; smpState = 1; unlockA();
        if (tg) toast(eng.playing ? TR("AUFNAHME STARTET AM NÄCHSTEN PAGE-ANFANG", "RECORDING STARTS AT THE NEXT PAGE")
                                  : TR("START DRÜCKEN - AUFNAHME STARTET MIT DEM PATTERN", "PRESS START - RECORDING STARTS WITH THE PATTERN"));
        else toast(eng.playing ? TR("AUFNAHME STARTET MIT DEM TON", "RECORDING STARTS WITH THE SOUND") : TR("START DRÜCKEN - AUFNAHME STARTET MIT DEM TON", "PRESS START - RECORDING STARTS WITH THE SOUND"));
    }
}
static void smpPreview(int a, int b) {
    if (!smpBuf || smpLen <= 0 || b <= a) return;
    pvOn = false; pvPos = a; pvEnd = b; pvOn = true;
}
static void smpPreviewChop(int dir) {
    if (smpNCut < 1) { smpPreview(smpStart, smpEnd); return; }
    smpChopPv = smpChopPv < 0 ? (dir > 0 ? 0 : smpNCut - 1) : (smpChopPv + dir + smpNCut) % smpNCut;
    smpPreview(smpCut[smpChopPv], smpCut[smpChopPv + 1]);
}
static void smpEdit(int k, int d) {
    if (smpRecording()) return;
    int step = smpLen > 0 ? (smpLen / 500 > 1 ? smpLen / 500 : 1) : 1;
    switch (k) {
    case SK_SRC: smpSrc = (smpSrc + (d > 0 ? 1 : 9)) % 10; break;   // USB, MASTER, SPUR 1-8
    case SK_START: if (smpLen > 0) { smpStart = clampi(smpStart + d * step, 0, smpEnd - 64); smpPeakCalc(); smpChopCalc(); } break;
    case SK_END: if (smpLen > 0) { smpEnd = clampi(smpEnd + d * step, smpStart + 64, smpLen); smpPeakCalc(); smpChopCalc(); } break;
    case SK_CHOPS: { int c = smpChops + (d > 0 ? 1 : -1); if (c == 1) c = d > 0 ? 2 : 0; smpChops = clampi(c, 0, SMP_MAXCHOP); smpChopCalc(); smpChopPv = -1; } break;
    case SK_MODE: smpMode = !smpMode; smpChopCalc(); smpChopPv = -1; break;
    case SK_NORM: smpNorm = !smpNorm; break;
    case SK_FADE: smpFade = clampi(smpFade + d, 0, 100); break;
    case SK_DEST: smpDest = (smpDest + (d > 0 ? 1 : 2)) % 3; break;
    case SK_THR: {   // SOFORT, -60 ... -6 dB in 3-dB-Schritten
        int t = cfg.recThr <= -61 ? -63 : cfg.recThr;
        t = clampi(t + (d > 0 ? 3 : -3), -63, -6);
        cfg.recThr = t < -60 ? -99 : t; saveSettings();
    } break;
    case SK_LEN: cfg.recPages = clampi(cfg.recPages + (d > 0 ? 1 : -1), 0, 16); saveSettings(); break;
    }
}
// Bereich a..b mit Verstaerkung und Fades in out (Stereo) schreiben, liefert Frames
static int smpRender(int a, int b, float* out) {
    int n = b - a, fd = smpFade * smpRate / 1000;
    if (fd * 2 > n) fd = n / 2;
    float g = smpGain();
    for (int i = 0; i < n; i++) {
        float w = 1.f;
        if (fd > 0 && i < fd) w = i / (float)fd;
        if (fd > 0 && i >= n - fd) w = fminf(w, (n - 1 - i) / (float)fd);
        out[i * 2] = smpBuf[(a + i) * 2] * g * w; out[i * 2 + 1] = smpBuf[(a + i) * 2 + 1] * g * w;
    }
    return n;
}
static void smpSave() {
    if (smpRecording() || smpLen <= 0 || smpEnd - smpStart < 64) { toast(TR("NICHTS ZUM SPEICHERN", "NOTHING TO SAVE")); return; }
    char dir[700], path[800], name[64];
    snprintf(dir, sizeof dir, "%ssamples/sampler", basePath);
    mkdir(dir, 0755);
    struct stat st;
    int no = 1;
    for (; no < 1000; no++) { snprintf(path, sizeof path, "%s/smp-%02d.wav", dir, no); if (stat(path, &st) != 0) break; }
    snprintf(name, sizeof name, "smp-%02d", no);
    // Ganze Datei; Ziel CHOP mit Transienten-Chops: jede Scheibe auf gleiche Laenge auffuellen, damit CHOP sie trifft
    bool equalize = smpDest == 2 && smpNCut >= 2 && smpMode == 1;
    int maxC = 0; for (int i = 0; i < smpNCut; i++) maxC = smpCut[i + 1] - smpCut[i] > maxC ? smpCut[i + 1] - smpCut[i] : maxC;
    int total = equalize ? maxC * smpNCut : smpEnd - smpStart;
    float* out = (float*)calloc((size_t)total * 2, sizeof(float));
    if (!out) { toast(TR("KEIN SPEICHER", "OUT OF MEMORY")); return; }
    if (equalize) for (int i = 0; i < smpNCut; i++) smpRender(smpCut[i], smpCut[i + 1], out + (size_t)i * maxC * 2);
    else smpRender(smpStart, smpEnd, out);
    bool ok = writeWav(path, out, total, smpRate);
    // Chops einzeln
    if (ok && smpNCut >= 2) {
        char cd[760]; snprintf(cd, sizeof cd, "%s/%s", dir, name); mkdir(cd, 0755);
        float* c = (float*)malloc(sizeof(float) * 2 * (size_t)(maxC > 0 ? maxC : 1));
        for (int i = 0; c && i < smpNCut; i++) {
            int n = smpRender(smpCut[i], smpCut[i + 1], c);
            char cp[800]; snprintf(cp, sizeof cp, "%s/%02d.wav", cd, i + 1);
            writeWav(cp, c, n, smpRate);
        }
        free(c);
    }
    free(out);
    if (!ok) { toast(TR("SPEICHERN FEHLGESCHLAGEN", "SAVING FAILED")); return; }
    smpSavedNo = no;
    reloadSamples();
    char lib[96]; snprintf(lib, sizeof lib, "sampler/%s", name);
    if (smpDest > 0) {   // in die aktuelle Spur laden
        int sl = poolAssign(lib);
        if (sl >= 0) {
            lockA();
            Pattern& P = curP();
            int m = smpDest == 2 ? M_CHOP : M_SAMPLE;
            if (P.machine[track] != m) Engine::setMachine(P, track, m);
            P.p[track][P_S0] = (uint8_t)sl;
            if (m == M_CHOP && smpNCut >= 2) P.p[track][P_S1] = (uint8_t)(smpNCut - 1);   // SLCS = Scheiben - 1
            if (smpSrc >= 1) {   // vom Master/einer Spur: Laenge in Steps fuer den Time-Warp
                double steps = total / (eng.stepSamples() * smpRate / eng.sr);
                int stp = (int)(steps + 0.5);
                if (stp >= 1 && stp <= 128) P.p[track][P_X1] = (uint8_t)stp;
            }
            unlockA();
            toast(TR("GESPEICHERT: %s - SPUR %d", "SAVED: %s - TRACK %d"), name, track + 1);
            return;
        }
    }
    toast(TR("GESPEICHERT: SAMPLES/SAMPLER/%s", "SAVED: SAMPLES/SAMPLER/%s"), name);
}

// Tests/Screenshots: kuenstliche Aufnahme (Schlaege alle 0,25 s, dazwischen ein Ton)
static void smpDemo() {
    if (!smpAlloc()) return;
    smpRate = eng.sr; smpLen = smpRate * 2;
    unsigned r = 1;
    for (int i = 0; i < smpLen; i++) {
        float t = i / (float)smpRate, ph = fmodf(t, 0.25f);
        r = r * 1664525u + 1013904223u;
        float hit = expf(-ph * 30.f) * (((r >> 9) & 0xFFFF) / 32768.f - 1.f) * 0.6f;
        float tone = 0.15f * sinf(6.2831853f * 110.f * t) * expf(-ph * 6.f);
        smpBuf[i * 2] = smpBuf[i * 2 + 1] = hit + tone;
    }
    smpChops = 8; smpMode = 1; smpSrc = 1;
    smpTakeReady();
}

static void drawSampler() {
    char b[96];
    text(10, 46, "SAMPLER", C_ACC, 2);
    bool rec = smpRecording();
    if (rec) {
        float sec = smpFromPc ? pcRecSeconds() : smpLen / (float)(smpRate > 0 ? smpRate : 44100);
        bool wait = smpFromPc ? pcRecState() == 1 : smpState == 1;
        float tot = smpFromPc ? (cfg.recPages ? smpPagesFrames(44100) / 44100.f : (float)SMP_SEC) : (smpTarget ? smpTarget / (float)smpRate : (float)SMP_SEC);
        if (wait && !smpFromPc && smpTarget) snprintf(b, sizeof b, TR("WARTET AUF PAGE-ANFANG ...", "WAITING FOR PAGE START ..."));
        else if (wait) snprintf(b, sizeof b, TR("WARTET AUF SIGNAL ...", "WAITING FOR SIGNAL ..."));
        else snprintf(b, sizeof b, TR("AUFNAHME %.1f s / %.1f s", "RECORDING %.1f s / %.1f s"), sec, tot);
        text(140, 50, b, (SDL_GetTicks() / 300) % 2 ? C_TRIG : C_TEXT);
    } else if (smpLen > 0) {
        snprintf(b, sizeof b, TR("%.2f s von %.1f s, %d Hz", "%.2f s of %.1f s, %d Hz"), (smpEnd - smpStart) / (float)smpRate, smpLen / (float)smpRate, smpRate);
        text(140, 50, b, C_TEXT);
    } else text(140, 50, TR("LEER - A = AUFNAHME", "EMPTY - A = RECORD"), C_DIM);
    if (smpSavedNo) { snprintf(b, sizeof b, "smp-%02d", smpSavedNo); text(630 - textW(b), 50, b, C_GREEN); }
    // Wellenform
    int wx = 10, wy = 72, ww = 620, wh = 168;
    rrect(wx, wy, ww, wh, 8, C_PANEL);
    int cy = wy + wh / 2;
    fill(wx + 6, cy, ww - 12, 1, C_OFF2);
    if (rec && !smpFromPc && smpBuf) {   // Master: wachsende Wellenform auf fester Skala (60 s)
        int n = smpLen, cols = ww - 12, sc = smpTarget > 0 ? smpTarget : smpCap;   // Skala: feste Laenge bzw. 60 s
        for (int c = 0; c < cols; c++) {
            int a = (int)((long long)sc * c / cols), e = (int)((long long)sc * (c + 1) / cols);
            if (a >= n) break;
            if (e > n) e = n;
            float mx = 0.f; for (int i = a; i < e; i += 8) mx = fmaxf(mx, fabsf(smpBuf[i * 2]));
            int h = (int)(mx * (wh / 2 - 6)); fill(wx + 6 + c, cy - h, 1, h * 2 + 1, C_TRIG);
        }
    } else if (rec) {   // USB: Pegel des Eingangs (Takt-Ringe) statt Wellenform
        drawBeatRings(wx + 6, wy + 6, ww - 12, wh - 12, C_TRIG);
    } else if (smpLen > 0 && smpBuf) {
        int cols = ww - 12 > SMP_COLS ? SMP_COLS : ww - 12;
        if (smpWLen != smpLen) {
            for (int c = 0; c < cols; c++) {
                int a = (int)((long long)smpLen * c / cols), e = (int)((long long)smpLen * (c + 1) / cols);
                float mn = 0.f, mx = 0.f;
                for (int i = a; i < e; i++) { float m = (smpBuf[i * 2] + smpBuf[i * 2 + 1]) * 0.5f; mn = fminf(mn, m); mx = fmaxf(mx, m); }
                smpWMin[c] = mn; smpWMax[c] = mx;
            }
            smpWLen = smpLen;
        }
        float g = smpGain();
        auto colOf = [&](int f) { return wx + 6 + (int)((long long)f * cols / smpLen); };
        int xs = colOf(smpStart), xe = colOf(smpEnd);
        for (int c = 0; c < cols; c++) {
            int x = wx + 6 + c;
            bool in = x >= xs && x < xe;
            int y0 = (int)(smpWMax[c] * g * (wh / 2 - 6)), y1 = (int)(-smpWMin[c] * g * (wh / 2 - 6));
            if (y0 > wh / 2 - 6) y0 = wh / 2 - 6;
            if (y1 > wh / 2 - 6) y1 = wh / 2 - 6;
            fill(x, cy - y0, 1, y0 + y1 + 1, in ? C_CYAN : C_OFF2);
        }
        fill(xs, wy + 4, 2, wh - 8, C_GREEN); fill(xe - 1, wy + 4, 2, wh - 8, C_TRIG);
        for (int i = 1; i < smpNCut; i++) {   // Chop-Grenzen
            int x = colOf(smpCut[i]);
            for (int y = wy + 6; y < wy + wh - 6; y += 4) fill(x, y, 1, 2, C_LOCK);
        }
        if (smpChopPv >= 0 && smpChopPv < smpNCut) {   // gewaehlter Chop
            int a = colOf(smpCut[smpChopPv]), e = colOf(smpCut[smpChopPv + 1]);
            fill(a, wy + wh - 6, e - a, 3, C_LOCK);
            snprintf(b, sizeof b, "%d", smpChopPv + 1); text(a + 2, wy + 4, b, C_LOCK);
        }
        if (pvOn) { int x = colOf(pvPos); fill(x, wy + 2, 1, wh - 4, C_PLAY); }
    } else {
        filler(wx + 2, wy + 2, ww - 4, wh - 4, dimC(C_ACC, 0.8f), 77);
    }
    // Regler
    static const char* const NM_DE[SK_COUNT] = {"QUELLE", "START", "ENDE", "CHOPS", "CHOP-ART", "NORMAL.", "FADE", "ZIEL", "LÄNGE", "SCHWELLE"};
    static const char* const NM_EN[SK_COUNT] = {"SOURCE", "START", "END", "CHOPS", "CHOP MODE", "NORMAL.", "FADE", "TARGET", "LENGTH", "THRESHOLD"};
    int tw = 119, th = 40, ty = 248;
    for (int k = 0; k < SK_COUNT; k++) {
        int x = 10 + (k % 5) * (tw + 6), y = ty + (k / 5) * (th + 6);
        bool sel = k == slot;
        rrect(x, y, tw, th, 6, sel ? C_SEL : C_PANEL);
        if (sel) { rrect(x, y, tw, 2, 1, C_ACC); rrect(x, y + th - 2, tw, 2, 1, C_ACC); }
        hitAdd(x, y, tw, th, HK_SLOT, k);
        text(x + 7, y + 3, g_lang ? NM_EN[k] : NM_DE[k], sel ? C_ACC : C_DIM);
        switch (k) {
        case SK_SRC:
            if (smpSrc >= 2) snprintf(b, sizeof b, TR("SPUR %d %s", "TRACK %d %s"), smpSrc - 1, MACHINE_NAMES[curP().machine[smpSrc - 2] % M_COUNT]);
            else snprintf(b, sizeof b, "%s", smpSrc ? "MASTER" : TR("USB PC/HANDY", "USB PC/PHONE"));
            break;
        case SK_START: snprintf(b, sizeof b, "%.2f s", smpStart / (float)smpRate); break;
        case SK_END: snprintf(b, sizeof b, "%.2f s", smpEnd / (float)smpRate); break;
        case SK_CHOPS: if (smpChops < 2) snprintf(b, sizeof b, TR("AUS", "OFF")); else snprintf(b, sizeof b, smpNCut != smpChops ? "%d (%d)" : "%d", smpChops, smpNCut); break;
        case SK_MODE: snprintf(b, sizeof b, "%s", smpMode ? "TRANSIENT" : TR("GLEICH", "EQUAL")); break;
        case SK_NORM: snprintf(b, sizeof b, "%s", smpNorm ? TR("AN", "ON") : TR("AUS", "OFF")); break;
        case SK_FADE: snprintf(b, sizeof b, "%d ms", smpFade); break;
        case SK_THR: if (cfg.recThr <= -61) snprintf(b, sizeof b, TR("SOFORT", "INSTANT")); else snprintf(b, sizeof b, "%d dB", cfg.recThr); break;
        case SK_LEN:
            if (!cfg.recPages) snprintf(b, sizeof b, TR("FREI", "FREE"));
            else snprintf(b, sizeof b, "%d P  %.1fs", cfg.recPages, smpPagesFrames(44100) / 44100.f);
            break;
        default: snprintf(b, sizeof b, "%s", smpDest == 0 ? TR("NUR DATEI", "FILE ONLY") : (smpDest == 1 ? TR("SPUR %d SMPL", "TRACK %d SMPL") : TR("SPUR %d CHOP", "TRACK %d CHOP")), track + 1); break;
        }
        text(x + 7, y + 20, b, sel ? C_PLAY : C_TEXT);
    }
    // Knoepfe (auch fuer Touch)
    struct { const char* t; int bt; Col c; } BTN[4] = {
        {rec ? TR("A  STOP", "A  STOP") : TR("A  AUFNAHME", "A  RECORD"), BT_A, C_TRIG},
        {TR("B  HÖREN", "B  LISTEN"), BT_B, C_GREEN},
        {TR("< >  CHOP HÖREN", "< >  CHOP LISTEN"), BT_RIGHT, C_LOCK},
        {TR("^  SPEICHERN", "^  SAVE"), BT_UP, C_CYAN}};
    for (int i = 0; i < 4; i++) {
        int x = 10 + i * 157, y = 344;
        rrect(x, y, 151, 30, 8, C_OFF2);
        text(x + 10, y + 7, BTN[i].t, BTN[i].c);
        hitAdd(x, y, 151, 30, HK_BTN, BTN[i].bt);
    }
    // Erklaerung
    static const char* const HL_DE[SK_COUNT] = {"USB: PC/Handy. MASTER: Summe. SPUR 1-8: eine Spur (nach Insert/VOL/PAN, ohne FX-Busse)",
        "Anfang der Auswahl (grüne Linie)", "Ende der Auswahl (rote Linie)", "Anzahl Chops (Scheiben), AUS = ganzes Sample",
        "GLEICH: gleich lange Chops. TRANSIENT: an den Einsätzen (Drums)", "Auf volle Lautstärke bringen (-0,3 dB)",
        "Ein-/Ausblenden an den Schnitten gegen Knacken", "Nach dem Speichern in die aktuelle Spur laden (SMPL bzw. CHOP mit Chops)",
        "FREI: bis Stopp. 1-16 PAGES: genau N x 16 Steps, ab Page-Anfang",
        "Startpegel: Aufnahme beginnt erst über diesem Pegel. SOFORT = ohne Warten"};
    static const char* const HL_EN[SK_COUNT] = {"USB: PC/phone. MASTER: the mix. TRACK 1-8: one track (after insert/VOL/PAN, no FX buses)",
        "Start of the selection (green line)", "End of the selection (red line)", "Number of chops (slices), OFF = whole sample",
        "EQUAL: equal-length chops. TRANSIENT: at the hits (drums)", "Bring to full volume (-0.3 dB)",
        "Fade in/out at the cuts against clicks", "After saving, load into the current track (SMPL or CHOP with chops)",
        "FREE: until stop. 1-16 PAGES: exactly N x 16 steps, from a page start",
        "Start level: recording begins above this level. INSTANT = no waiting"};
    text(10, 384, g_lang ? HL_EN[slot % SK_COUNT] : HL_DE[slot % SK_COUNT], C_TEXT);
    text(10, 404, TR("Speichern: samples/sampler/smp-NN.wav (+ Chops im Ordner smp-NN/)",
                     "Save: samples/sampler/smp-NN.wav (+ chops in folder smp-NN/)"), C_DIM);
    hint(6, 452, TR("[L1/R1]REGLER [X/Y]WERT [A]AUFNAHME [B]HÖREN [<>]CHOP [^]SPEICHERN [L2/R2]SEITE",
                    "[L1/R1]CONTROL [X/Y]VALUE [A]RECORD [B]LISTEN [<>]CHOP [^]SAVE [L2/R2]PAGE"), C_ACC, C_DIM);
}

static void samplerInput(Input& I, Uint32 now) {
    if (slot >= SK_COUNT) slot = 0;
    int dl = (I.repeat(BT_R1, now, 300, 110) ? 1 : 0) - (I.repeat(BT_L1, now, 300, 110) ? 1 : 0);
    if (dl) slot = (slot + SK_COUNT + dl) % SK_COUNT;
    int mul = (I.heldMs(BT_X, now) > 1200 || I.heldMs(BT_Y, now) > 1200) ? 8 : 1;
    if (I.repeat(BT_X, now, 300, 45)) smpEdit(slot, mul);
    if (I.repeat(BT_Y, now, 300, 45)) smpEdit(slot, -mul);
    if (I.pressed(BT_A)) smpRecToggle();
    if (I.pressed(BT_B)) { if (pvOn) pvOn = false; else { smpChopPv = -1; smpPreview(smpStart, smpEnd); } }
    if (I.pressed(BT_RIGHT)) smpPreviewChop(1);
    if (I.pressed(BT_LEFT)) smpPreviewChop(-1);
    if (I.pressed(BT_UP)) smpSave();
    if (I.pressed(BT_DOWN)) pvOn = false;
    if (I.pressed(BT_L2)) gotoPage(-1);
    if (I.pressed(BT_R2)) gotoPage(1);
    if (startTap(I, now)) startStop();
}
