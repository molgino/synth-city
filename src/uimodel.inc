// ---------------------------------------------------------------- Modell fuer die Touch-Oberflaeche
// Die App im Browser zeichnet ihre eigene, fuer Finger ausgelegte Oberflaeche. Damit sie und die
// Geraete-Oberflaeche nicht auseinanderlaufen, liest das Modell aus denselben Quellen (Pattern,
// MACHINE_NAMES, TCOL, eng.*) - ein neuer Parameter oder eine neue Engine taucht automatisch auf.
// Gelesen wird im Hauptthread (uiModelPush, ~10/s), geschickt vom Web-Thread; Aktionen laufen
// ueber remoteActPush zurueck in den Hauptthread (uiActionsRun in handleInput).

enum { UA_STEP = 1, UA_TRACK, UA_MUTE, UA_PLAY, UA_PATTERN, UA_PAGE, UA_SOLO, UA_TRIGLESS, UA_VIEW, UA_GROUP, UA_PARAM, UA_HOLD, UA_LOCKCLR, UA_NOTE, UA_TRKP, UA_PRJ, UA_SONG, UA_SNGCELL, UA_SMP };
// Mixer: dieselben Zeilen wie die MIX-Seite am Geraet (MIX_P), damit beide dasselbe zeigen
enum { UMIX_N = 6 };

// Regler der Gruppe, in der der Cursor steht - aus derselben Liste (v2Build/v2It), die auch die
// Geraete-Oberflaeche zeichnet. uiGrpRange liefert den Bereich [a,b) der Eintraege dieser Gruppe.
// Regler, die am Geraet einen Namen statt einer Zahl zeigen und wenige Werte haben: die bekommen am
// Handy eine Auswahlliste statt eines Schiebers (zielen auf 15 Engines mit dem Finger geht sonst nicht).
// Lange Listen stehen EINMAL je Modell unter "lists" (ein LFO hat drei DEST-Regler - je Regler
// waere dieselbe 150-Eintraege-Liste dreimal drin); der Regler verweist mit "list" darauf.
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

// Projekte des Geraets, alphabetisch. Modell UND Aktion lesen dieselbe Liste, sonst laedt ein Tipp
// auf "Zeile 3" je nach Reihenfolge ein anderes Projekt.
enum { UPRJ_MAX = 60 };
static int uiProjects(char out[UPRJ_MAX][64]) {
    char d[700]; snprintf(d, sizeof d, "%sprojects", basePath);
    DIR* dd = opendir(d);
    if (!dd) return 0;
    int n = 0;
    struct dirent* e;
    while ((e = readdir(dd)) && n < UPRJ_MAX) {
        size_t l = strlen(e->d_name);
        if (l <= 4 || strcasecmp(e->d_name + l - 4, ".gbx")) continue;
        snprintf(out[n], 64, "%.*s", (int)(l - 4), e->d_name);
        n++;
    }
    closedir(dd);
    for (int i = 1; i < n; i++) { char t[64]; snprintf(t, sizeof t, "%s", out[i]);
        int j = i - 1; while (j >= 0 && strcasecmp(out[j], t) > 0) { snprintf(out[j + 1], 64, "%s", out[j]); j--; }
        snprintf(out[j + 1], 64, "%s", t); }
    return n;
}

// Regler mit langer Auswahl: Name der gemeinsamen Liste (nullptr = keine)
static const char* uiListOf(int kind) {
    if (kind == K_LDEST) return "dest";
    if (kind == K_EDEST) return "edest";
    if (kind == K_COND) return "cond";
    return nullptr;
}
// Eine der gemeinsamen Listen als JSON-Paare {v,n} schreiben
static int uiListJson(char* b, int n, int w, const char* name, int kind, int mach) {
    w += snprintf(b + w, n - w, ",\"%s\":[", name);
    bool first = true;
    if (kind == K_COND) {
        ParamDef d = paramDef(mach, P_COND);
        for (int v = 0; v <= d.max && w < n - 60; v++) {
            char ob[40]; knobText(d, v, ob, sizeof ob, 0);
            for (char* c = ob; *c; c++) if (*c == '"' || *c == '\\') *c = '\'';
            w += snprintf(b + w, n - w, "%s{\"v\":%d,\"n\":\"%s\"}", first ? "" : ",", v, ob); first = false;
        }
    } else {   // Modulationsziel: "---", jeder echte Klangparameter mit seiner Seite, bei ENV zusaetzlich VCA
        w += snprintf(b + w, n - w, "{\"v\":0,\"n\":\"---\"}"); first = false;
        if (kind == K_EDEST) w += snprintf(b + w, n - w, ",{\"v\":%d,\"n\":\"VCA\"}", MOD_VCA);
        for (int u = 0; u < NUI_PAGES && w < n - 80; u++) {
            if (!pageAvail(u)) continue;
            for (int k = 0; k < uiSlots(u) && w < n - 80; k++) {
                int pi = uiParam(u, k);
                if (pi < 0 || !ldestValid(mach, pi) || ldestUiPage(pi) != u) continue;
                w += snprintf(b + w, n - w, ",{\"v\":%d,\"n\":\"%s %s\"}", pi + 1, PAGE_NAMES[u], paramDef(mach, pi).name);
            }
        }
    }
    return w + snprintf(b + w, n - w, "]");
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
        w += snprintf(b + w, n - w, "%s{\"n\":\"%s\",\"m\":%d,\"s\":%d,\"c\":\"#%02x%02x%02x\",\"len\":%d,\"on\":%d,\"mix\":[",
                      t ? "," : "", MACHINE_NAMES[P.machine[t] % M_COUNT], eng.muted(t) ? 1 : 0, eng.solo[t] ? 1 : 0,
                      c.r, c.g, c.b, P.len[t] < 1 ? 1 : P.len[t], eng.playing && eng.playedStep[t] >= 0 ? 1 : 0);
        for (int r = 0; r < UMIX_N; r++) w += snprintf(b + w, n - w, "%s%d", r ? "," : "", P.p[t][MIX_P[r]]);
        w += snprintf(b + w, n - w, "]}");
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
    {   // Sampler: Zustand, Laenge, Quelle - Aufnehmen, Hoeren und Speichern gehen damit vom Handy
        const char* sn;
        char sb[40];
        if (smpSrc >= 2) { snprintf(sb, sizeof sb, TR("SPUR %d", "TRACK %d"), smpSrc - 1); sn = sb; }
        else sn = smpSrc ? "MASTER" : TR("USB PC/HANDY", "USB PC/PHONE");
        w += snprintf(b + w, n - w, ",\"smp\":{\"st\":%d,\"rec\":%d,\"sec\":%d,\"src\":%d,\"srcn\":\"%s\",\"peak\":%d}",
                      smpState, smpRecording() ? 1 : 0, smpRate > 0 ? smpLen * 10 / smpRate : 0, smpSrc, sn,
                      (int)(smpPeak * 100.f));
    }
    {   // Song: die Clip-Matrix als Zeilen, dazu die belegten Patterns zur Auswahl
        const Project& J = eng.proj;
        w += snprintf(b + w, n - w, ",\"sng\":{\"on\":%d,\"len\":%d,\"pos\":%d,\"rows\":[",
                      eng.songOn ? 1 : 0, J.songLen, eng.songActive() ? eng.songPos : -1);
        for (int i = 0; i < J.songLen && w < n - 200; i++) {
            w += snprintf(b + w, n - w, "%s{\"r\":%d,\"p\":[", i ? "," : "", J.row[i].rep);
            for (int t = 0; t < NTRACKS; t++) w += snprintf(b + w, n - w, "%s%d", t ? "," : "", J.row[i].pat[t]);
            w += snprintf(b + w, n - w, "]}");
        }
        w += snprintf(b + w, n - w, "],\"pats\":[{\"v\":%d,\"n\":\"---\"}", SONG_EMPTY);
        for (int i = 0; i < NPATTERNS && w < n - 120; i++)
            if (J.pat[i].used) w += snprintf(b + w, n - w, ",{\"v\":%d,\"n\":\"%s\"}", i, PN(i));
        w += snprintf(b + w, n - w, "]}");
    }
    {   // Projekt: Name, ungespeicherte Aenderungen, Liste zum Laden
        static char pl[UPRJ_MAX][64];
        int pn = uiProjects(pl);
        w += snprintf(b + w, n - w, ",\"prj\":{\"n\":\"%.40s\",\"d\":%d,\"l\":[", cfg.project, projDirtyCached() ? 1 : 0);
        for (int i = 0; i < pn && w < n - 90; i++) {
            char nm[64]; snprintf(nm, sizeof nm, "%.56s", pl[i]);
            for (char* c = nm; *c; c++) if (*c == '"' || *c == '\\') *c = '\'';
            w += snprintf(b + w, n - w, "%s\"%s\"", i ? "," : "", nm);
        }
        w += snprintf(b + w, n - w, "]}");
    }
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
            } else if (uiListOf(d.kind)) {   // lange Liste: nur der Verweis, die Liste steht unter "lists"
                w += snprintf(b + w, n - w, ",\"list\":\"%s\"", uiListOf(d.kind));
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
        bool need[3] = {false, false, false};   // dest / edest / cond - nur was die Gruppe wirklich braucht
        for (int i = from; i < to; i++) {
            page = v2It[i].pg; slot = v2It[i].k;
            ParamDef d; int val, bv; bool lk; char lab[24];
            if (!knobInfo(v2It[i].k, d, val, lk, bv, lab, sizeof lab)) continue;
            if (d.kind == K_LDEST) need[0] = true;
            else if (d.kind == K_EDEST) need[1] = true;
            else if (d.kind == K_COND) need[2] = true;
        }
        page = savePage; slot = saveSlot;
        w += snprintf(b + w, n - w, "]");
        if (need[0] || need[1] || need[2]) {
            int mach = P.machine[track] % M_COUNT;
            w += snprintf(b + w, n - w, ",\"lists\":{");
            int first = w;
            if (need[0]) w = uiListJson(b, n, w, "dest", K_LDEST, mach);
            if (need[1]) w = uiListJson(b, n, w, "edest", K_EDEST, mach);
            if (need[2]) w = uiListJson(b, n, w, "cond", K_COND, mach);
            if (b[first] == ',') memmove(b + first, b + first + 1, strlen(b + first + 1) + 1), w--;   // erstes Komma weg
            w += snprintf(b + w, n - w, "}");
        }
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
                        if (cfg.preview) { lockA(); eng.preview(track, nullptr); unlockA(); }
                    }
                } else if (d.kind == K_NOTEX || d.kind == K_SMPL || uiListOf(d.kind)) {   // direkt setzen: editValue zaehlt bei SMPL nur ueber
                                                      // belegte Plaetze und springt bei NT2-NT4 von AUS auf Unison
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
        case UA_SMP: {   // Sampler: 0 Aufnahme an/aus, 1 hoeren, 2 speichern, 3 Quelle (b = 0 USB, 1 Master, 2.. Spur)
            if (e.a == 0) smpRecToggle();
            else if (e.a == 1) { smpChopPv = -1; smpPreview(smpStart, smpEnd); }
            else if (e.a == 2) smpSave();
            else if (e.a == 3 && e.b >= 0 && e.b < 10) smpSrc = e.b;
        } break;
        case UA_SONG: {   // 0 Song an/aus, 1 ab Zeile b spielen, 2 Zeile hinter b einfuegen, 3 Zeile b weg, 4/5 rep +/-
            Project& J = eng.proj;
            int r = e.b;
            if (e.a == 0) {
                if (!J.songLen) { toast(TR("SONG IST LEER", "SONG IS EMPTY")); break; }
                lockA();
                if (eng.songOn) { eng.songOn = false; if (eng.playing) eng.nextPat = eng.curPat; }
                else eng.songStart(r >= 0 && r < J.songLen ? r : 0);
                unlockA();
                toast(eng.songOn ? TR("SONG-MODUS AN", "SONG MODE ON") : TR("SONG-MODUS AUS", "SONG MODE OFF"));
            } else if (e.a == 1) {
                if (r < 0 || r >= J.songLen) break;
                lockA(); eng.songStart(r); if (!eng.playing) eng.start(); unlockA();
            } else if (e.a == 2) {   // wie L1 am Geraet: Kopie der Zeile dahinter
                if (J.songLen >= SONG_MAX) { toast(TR("SONG VOLL (%d)", "SONG FULL (%d)"), SONG_MAX); break; }
                lockA();
                int at = J.songLen ? clampi(r, 0, J.songLen - 1) + 1 : 0;
                for (int i = J.songLen; i > at; i--) J.row[i] = J.row[i - 1];
                if (J.songLen) J.row[at] = J.row[at - 1]; else Engine::rowFromPattern(J, J.row[at], curPi());
                J.row[at].rep = 1;
                J.songLen++;
                if (eng.songActive() && eng.songPos >= at && J.songLen > 1) eng.songPos++;
                unlockA();
            } else if (e.a == 3) {
                if (r < 0 || r >= J.songLen) break;
                lockA();
                for (int i = r; i < J.songLen - 1; i++) J.row[i] = J.row[i + 1];
                J.songLen--;
                if (eng.songPos >= J.songLen) eng.songPos = 0;
                if (!J.songLen) eng.songOn = false; else if (eng.songOn) eng.rowPending = true;
                unlockA();
            } else if (e.a == 4 || e.a == 5) {
                if (r < 0 || r >= J.songLen) break;
                lockA(); J.row[r].rep = (uint8_t)clampi(J.row[r].rep + (e.a == 4 ? 1 : -1), 1, 64); unlockA();
            }
        } break;
        case UA_SNGCELL: {   // eine Zelle der Matrix: a = Zeile*16 + Spur, b = Pattern (SONG_EMPTY = schweigt)
            int r = (e.a >> 4) & 63, t = e.a & 15;
            Project& J = eng.proj;
            if (r >= J.songLen || t >= NTRACKS) break;
            if (e.b != SONG_EMPTY && (e.b < 0 || e.b >= NPATTERNS)) break;
            lockA(); J.row[r].pat[t] = (uint8_t)e.b; unlockA();
        } break;
        case UA_PRJ: {   // 0 speichern, 1 laden (b = Zeile der Liste), 2 neues Projekt mit freiem Namen
            if (e.a == 0) { toast(saveProject() ? TR("PROJEKT GESPEICHERT", "PROJECT SAVED") : TR("SPEICHERN FEHLGESCHLAGEN", "SAVING FAILED")); break; }
            if (e.a == 1) {
                static char pl[UPRJ_MAX][64];
                int pn = uiProjects(pl);
                if (e.b >= 0 && e.b < pn) doLoadProject(pl[e.b]);
            } else if (e.a == 2) {
                char nm[64]; freeProjectName(TR("Projekt", "Project"), nm, sizeof nm);
                doNewProject(nm);
            }
        } break;
        case UA_TRKP: {   // Mixer: Wert einer beliebigen Spur setzen (a = Spur*16 + Zeile)
            int t = (e.a >> 4) & 15, r = e.a & 15;
            if (t >= NTRACKS || r >= UMIX_N) break;
            int idx = MIX_P[r], mx = idx == P_CMPS ? 1 : 127;
            undoTrack(SDL_GetTicks());
            lockA(); curP().p[t][idx] = (uint8_t)clampi(e.b, 0, mx); unlockA();
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
    static char b[24576];
    uiModelJson(b, sizeof b);
    remoteModelSet(b);
}
