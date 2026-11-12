// ---------------------------------------------------------------- Modell fuer die Touch-Oberflaeche
// Die App im Browser zeichnet ihre eigene, fuer Finger ausgelegte Oberflaeche. Damit sie und die
// Geraete-Oberflaeche nicht auseinanderlaufen, liest das Modell aus denselben Quellen (Pattern,
// MACHINE_NAMES, TCOL, eng.*) - ein neuer Parameter oder eine neue Engine taucht automatisch auf.
// Gelesen wird im Hauptthread (uiModelPush, ~10/s), geschickt vom Web-Thread; Aktionen laufen
// ueber remoteActPush zurueck in den Hauptthread (uiActionsRun in handleInput).

enum { UA_STEP = 1, UA_TRACK, UA_MUTE, UA_PLAY, UA_PATTERN, UA_PAGE, UA_SOLO, UA_TRIGLESS, UA_VIEW, UA_GROUP, UA_PARAM };

// Regler der Gruppe, in der der Cursor steht - aus derselben Liste (v2Build/v2It), die auch die
// Geraete-Oberflaeche zeichnet. uiGrpRange liefert den Bereich [a,b) der Eintraege dieser Gruppe.
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
        "{\"play\":%d,\"song\":%d,\"bpm\":%d,\"pat\":\"%s\",\"patIdx\":%d,\"track\":%d,\"len\":%d,\"page\":%d,\"pages\":%d,\"rec\":%d,\"msg\":\"",
        eng.playing ? 1 : 0, eng.songOn ? 1 : 0, eng.proj.bpm, PN(curPi()), curPi(), track, len,
        cursor / 16, (len + 15) / 16, eng.recording ? 1 : 0);
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
    w += snprintf(b + w, n - w, "],\"now\":%d", eng.playing ? eng.playedStep[track] : -1);
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
            for (char* c = vb; *c; c++) if (*c == '"' || *c == '\\') *c = '\'';
            w += snprintf(b + w, n - w, "%s{\"n\":\"%s\",\"v\":%d,\"t\":\"%s\",\"mx\":%d,\"bip\":%d,\"lk\":%d}",
                          i > from ? "," : "", d.name, val, vb, d.max > 0 ? d.max : 1, d.kind == K_BIP ? 1 : 0, lk ? 1 : 0);
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
            if (knobInfo(slot, d, val, lk, bv, lab, sizeof lab) && e.b != val) editValue(e.b - val);
            page = savePage; slot = saveSlot;
            v2Sync();
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
