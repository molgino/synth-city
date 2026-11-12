// ---------------------------------------------------------------- Modell fuer die Touch-Oberflaeche
// Die App im Browser zeichnet ihre eigene, fuer Finger ausgelegte Oberflaeche. Damit sie und die
// Geraete-Oberflaeche nicht auseinanderlaufen, liest das Modell aus denselben Quellen (Pattern,
// MACHINE_NAMES, TCOL, eng.*) - ein neuer Parameter oder eine neue Engine taucht automatisch auf.
// Gelesen wird im Hauptthread (uiModelPush, ~10/s), geschickt vom Web-Thread; Aktionen laufen
// ueber remoteActPush zurueck in den Hauptthread (uiActionsRun in handleInput).

enum { UA_STEP = 1, UA_TRACK, UA_MUTE, UA_PLAY, UA_PATTERN, UA_PAGE, UA_SOLO, UA_TRIGLESS, UA_VIEW, UA_GROUP, UA_PARAM, UA_HOLD, UA_LOCKCLR, UA_NOTE };

// Regler der Gruppe, in der der Cursor steht - aus derselben Liste (v2Build/v2It), die auch die
// Geraete-Oberflaeche zeichnet. uiGrpRange liefert den Bereich [a,b) der Eintraege dieser Gruppe.
// Regler, die am Geraet einen Namen statt einer Zahl zeigen und wenige Werte haben: die bekommen am
// Handy eine Auswahlliste statt eines Schiebers (zielen auf 15 Engines mit dem Finger geht sonst nicht).
// Lange Listen (Sample, LFO-Ziel, Trig-Bedingung) laufen weiter ueber das Geraetebild.
static bool uiHasOpts(int kind, int mx) {
    if (mx < 1 || mx > 31) return false;
    switch (kind) {
    case K_MACH: case K_INS: case K_FTYPE: case K_FXT: case K_VMOD: case K_OFFON: case K_LWAV:
    case K_LMODE: case K_LSYNC: case K_ARPM: case K_ACHD: case K_ARAT: case K_DIR: case K_TSPD:
    case K_OCT: case K_SUBO: case K_WTAB: case K_RTRG: case K_CNT: case K_CRAT: case K_SCSRC:
        return true;
    default: return false;
    }
}

static void uiGrpRange(int& from, int& to, int& grpIdx, int& grpCount) {
    v2Build(v2View);
    int f = v2Foc[v2View]; if (f < 0 || f >= v2N) f = 0;
    from = f; while (from > 0 && !v2It[from].hdr) from--;
    grpIdx = 0; for (int i = 0; i < from; i++) if (v2It[i].hdr) grpIdx++;
    from++;                                        // der Kopf selbst ist kein Regler
    to = from; while (to < v2N && !v2It[to].hdr) to++;
    grpCount = v2Groups;
}

static int uiModelJson(char* b, int n) {
    const Pattern& P = curP();
    int len = P.len[track] < 1 ? 1 : P.len[track];
    int w = snprintf(b, n,
        "{\"play\":%d,\"song\":%d,\"bpm\":%d,\"pat\":\"%s\",\"patIdx\":%d,\"track\":%d,\"len\":%d,\"page\":%d,\"pages\":%d,\"rec\":%d,\"hold\":%d,\"msg\":\"",
        eng.playing ? 1 : 0, eng.songOn ? 1 : 0, eng.proj.bpm, PN(curPi()), curPi(), track, len,
        cursor / 16, (len + 15) / 16, eng.recording ? 1 : 0,
        aHeld && holdTrack == track && holdPat == curPi() ? holdStep : -1);   // P-Lock: welcher Step haengt gerade
    for (const char* c = msg; *c && w < n - 8; c++) {   // Meldung (Anfuehrungszeichen entschaerfen)
        if (*c == '"' || *c == '\\') continue;
        b[w++] = *c;
    }
    w += snprintf(b + w, n - w, "\",\"tracks\":[");
    for (int t = 0; t < NTRACKS && w < n - 200; t++) {
        const Col& c = TCOL[t];
        w += snprintf(b + w, n - w, "%s{\"n\":\"%s\",\"m\":%d,\"s\":%d,\"c\":\"#%02x%02x%02x\",\"len\":%d,\"on\":%d}",
                      t ? "," : "", MACHINE_NAMES[P.machine[t] % M_COUNT], eng.muted(t) ? 1 : 0, eng.solo[t] ? 1 : 0,
                      c.r, c.g, c.b, P.len[t] < 1 ? 1 : P.len[t], eng.playing && eng.playedStep[t] >= 0 ? 1 : 0);
    }
    w += snprintf(b + w, n - w, "],\"steps\":[");
    for (int s = 0; s < len && w < n - 40; s++)   // Zustand je Step: 0 leer, 1 Trig, 2 trigless
        w += snprintf(b + w, n - w, "%s%d", s ? "," : "", P.s[track][s].on);
    // Noten: ist die Spur melodisch, und welche Note zeigt der gehaltene Step (sonst die der Spur)?
    {
        bool hold = aHeld && holdTrack == track && holdPat == curPi();
        int note = hold && holdS().lock[P_NOTE] != NO_LOCK ? holdS().lock[P_NOTE] : P.p[track][P_NOTE];
        w += snprintf(b + w, n - w, "],\"drum\":%d,\"note\":%d", machineIsDrum(P.machine[track] % M_COUNT) ? 1 : 0, note);
    }
    w += snprintf(b + w, n - w, ",\"now\":%d", eng.playing ? eng.playedStep[track] : -1);
    // Klang schrauben: Gruppen der Ansicht und die Regler der aktuellen Gruppe
    if (v2Active()) {
        int from, to, gi, gc;
        uiGrpRange(from, to, gi, gc);
        w += snprintf(b + w, n - w, ",\"view\":%d,\"grp\":%d,\"grps\":[", v2View, gi);
        for (int i = 0, g = 0; i < v2N && w < n - 120; i++)
            if (v2It[i].hdr) { w += snprintf(b + w, n - w, "%s\"%s\"", g ? "," : "", v2It[i].title ? v2It[i].title : pageLong(v2It[i].pg)); g++; }
        w += snprintf(b + w, n - w, "],\"params\":[");
        int savePage = page, saveSlot = slot;
        for (int i = from; i < to && w < n - 160; i++) {
            page = v2It[i].pg; slot = v2It[i].k;
            ParamDef d; int val, bv; bool lk; char lab[24], vb[40];
            if (!knobInfo(v2It[i].k, d, val, lk, bv, lab, sizeof lab)) continue;
            knobText(d, val, vb, sizeof vb, v2It[i].k);
            if (lab[0]) snprintf(vb, sizeof vb, "%.36s", lab);   // Sample- bzw. Zielname statt der Nummer (wie in der Liste am Geraet)
            for (char* c = vb; *c; c++) if (*c == '"' || *c == '\\') *c = '\'';
            w += snprintf(b + w, n - w, "%s{\"n\":\"%s\",\"v\":%d,\"t\":\"%s\",\"mx\":%d,\"bip\":%d,\"lk\":%d",
                          i > from ? "," : "", d.name, val, vb, d.max > 0 ? d.max : 1, d.kind == K_BIP ? 1 : 0, lk ? 1 : 0);
            if (d.kind == K_SMPL) {   // Samples: die Plaetze, die das Projekt geladen hat (die Bibliothek
                w += snprintf(b + w, n - w, ",\"slots\":[{\"v\":%d,\"n\":\"---\"}", SMP_NONE);   // waere zu lang fuers Modell)
                for (int k = 0; k < PREVIEW_SLOT && w < n - 140; k++) {
                    if (!sampleGet(k)) continue;
                    char sn[40]; snprintf(sn, sizeof sn, "%.36s", sampleName(k));
                    for (char* c = sn; *c; c++) if (*c == '"' || *c == '\\') *c = '\'';
                    w += snprintf(b + w, n - w, ",{\"v\":%d,\"n\":\"%s\"}", k, sn);
                }
                w += snprintf(b + w, n - w, "]");
            } else if (uiHasOpts(d.kind, d.max)) {   // Auswahlliste statt Schieber
                w += snprintf(b + w, n - w, ",\"opts\":[");
                for (int v = 0; v <= d.max && w < n - 80; v++) {
                    char ob[40];
                    knobText(d, v, ob, sizeof ob, v2It[i].k);
                    for (char* c = ob; *c; c++) if (*c == '"' || *c == '\\') *c = '\'';
                    w += snprintf(b + w, n - w, "%s\"%s\"", v ? "," : "", ob);
                }
                w += snprintf(b + w, n - w, "]");
            }
            w += snprintf(b + w, n - w, "}");
        }
        page = savePage; slot = saveSlot;
        w += snprintf(b + w, n - w, "]");
    }
    w += snprintf(b + w, n - w, "}");
    return w < n ? w : n - 1;
}

static void uiActionsRun() {   // Hauptthread: Aktionen aus der Touch-Oberflaeche ausfuehren
    RemoteAct e;
    while (remoteActPop(e)) {
        switch (e.kind) {
        case UA_STEP: {
            Pattern& P = curP();
            int len = P.len[track] < 1 ? 1 : P.len[track];
            if (e.a < 0 || e.a >= len) break;
            undoTrack(SDL_GetTicks());
            Step& st = P.s[track][e.a];
            st.on = st.on ? 0 : 1;
            if (!st.on) memset(st.lock, NO_LOCK, NPARAMS);
            cursor = e.a;
        } break;
        case UA_TRIGLESS: {   // Trig mit Locks auf trigless schalten und zurueck
            Pattern& P = curP();
            int len = P.len[track] < 1 ? 1 : P.len[track];
            if (e.a < 0 || e.a >= len) break;
            undoTrack(SDL_GetTicks());
            Step& st = P.s[track][e.a];
            st.on = st.on == 1 ? 2 : (st.on == 2 ? 1 : 1);
            cursor = e.a;
        } break;
        case UA_TRACK: if (e.a >= 0 && e.a < NTRACKS) { track = e.a; v2Dirty(); } break;
        case UA_MUTE:  if (e.a >= 0 && e.a < NTRACKS) eng.toggleMute(e.a); break;
        case UA_SOLO:  if (e.a >= 0 && e.a < NTRACKS) eng.solo[e.a] = !eng.solo[e.a]; break;
        case UA_PLAY:  startStop(); break;
        case UA_PATTERN: {
            int pi = e.a;
            if (e.b) pi = (curPi() + (e.a > 0 ? 1 : NPATTERNS - 1)) % NPATTERNS;   // b = relativ
            if (pi >= 0 && pi < NPATTERNS) switchPattern(pi);
        } break;
        case UA_VIEW: if (e.a >= 0 && e.a < V2_VIEWS) { v2GoView(e.a); v2Dirty(); } break;
        case UA_GROUP: {   // Gruppe direkt anspringen (am Geraet waere das mehrmals <>)
            v2Build(v2View);
            int seen = 0;
            for (int i = 0; i < v2N; i++) if (v2It[i].hdr) {
                if (seen == e.a) { v2Foc[v2View] = v2Clamp(i, 1); v2Apply(); break; }
                seen++;
            }
        } break;
        case UA_PARAM: {   // Regler e.a der aktuellen Gruppe auf den Wert e.b: ueber editValue, damit
            int from, to, gi, gc;              // Locks, globale Seiten und Grenzen genau wie am Geraet laufen
            uiGrpRange(from, to, gi, gc);
            int i = from + e.a;
            if (i < from || i >= to) break;
            int savePage = page, saveSlot = slot;
            page = v2It[i].pg; slot = v2It[i].k;
            ParamDef d; int val, bv; bool lk; char lab[24];
            if (knobInfo(slot, d, val, lk, bv, lab, sizeof lab) && e.b != val) {
                if (d.kind == K_MACH) {   // ENGINE zaehlt in editValue ueber MACHINE_ORDER und nur schrittweise;
                    Pattern& P = curP();  // die Auswahl schickt den Engine-Wert, also derselbe Weg wie die Liste am Geraet
                    int m = e.b % M_COUNT;
                    if (!aHeld && m != P.machine[track]) {
                        lockA(); Engine::setMachine(P, track, m); unlockA();
                        lockA(); eng.preview(track, nullptr); unlockA();
                    }
                } else if (d.kind == K_SMPL) {   // Sample-Platz direkt setzen (editValue zaehlt nur schrittweise)
                    int idx = slotParam(slot);
                    if (idx >= 0) {
                        undoTrack(SDL_GetTicks());
                        if (aHeld) { holdS().lock[idx] = (uint8_t)e.b; aEdited = true; }
                        else curP().p[track][idx] = (uint8_t)e.b;
                    }
                } else editValue(e.b - val);
            }
            page = savePage; slot = saveSlot;
            v2Sync();
        } break;
        case UA_HOLD: {   // P-Lock: Step festhalten; danach schreiben alle Reglerwerte Locks statt Pattern-Werte
            if (e.b) {
                const Pattern& P = curP();
                int len = P.len[track] < 1 ? 1 : P.len[track];
                if (e.a < 0 || e.a >= len) break;
                holdPat = curPi(); holdTrack = track; holdStep = e.a;
                aHeld = true; aEdited = false; lockLatch = true; aPrevOn = P.s[track][e.a].on;
                cursor = e.a;
            } else {
                // Locks auf einem leeren Step wirken nur, wenn er mindestens trigless ist
                if (aHeld && aEdited) { Step& st = holdS(); if (!st.on) st.on = 2; }
                aHeld = false; lockLatch = false; aEdited = false;
            }
        } break;
        case UA_LOCKCLR: {   // alle Locks dieses Steps weg (am Geraet: B im Lock-Modus)
            if (!aHeld) break;
            undoTrack(SDL_GetTicks());
            Step& st = holdS();
            memset(st.lock, NO_LOCK, NPARAMS);
            if (st.on == 2) st.on = 0;
            aEdited = false;
        } break;
        case UA_NOTE: {   // Klaviatur: haengt ein Step im P-Lock, bekommt er die Note; sonst nur vorhoeren
            int note = e.a < 0 ? 0 : (e.a > 127 ? 127 : e.a);
            if (aHeld) {
                undoTrack(SDL_GetTicks());
                holdS().lock[P_NOTE] = (uint8_t)note;
                aEdited = true;
            }
            static Step tmp;
            memset(tmp.lock, NO_LOCK, NPARAMS);
            tmp.on = 1;
            tmp.lock[P_NOTE] = (uint8_t)note;
            lockA(); eng.preview(track, &tmp); unlockA();
        } break;
        case UA_PAGE: {
            const Pattern& P = curP();
            int len = P.len[track] < 1 ? 1 : P.len[track], pg = (len + 15) / 16;
            if (e.a >= 0 && e.a < pg) cursor = e.a * 16;
        } break;
        }
    }
}

static void uiModelPush() {   // ~10/s aus der Hauptschleife, nur wenn jemand die Oberflaeche nutzt
    static uint32_t last = 0;
    uint32_t now = SDL_GetTicks();
    if (!remoteModelWanted() || now - last < 70) return;
    last = now;
    static char b[16384];
    uiModelJson(b, sizeof b);
    remoteModelSet(b);
}
