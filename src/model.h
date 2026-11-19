// Datenmodell: Projekt -> Pattern -> Track -> Step. Der wirksame Wert ist immer dieselbe Kette
// Lock -> (spaeter Scene) -> Track-Grundwert. Locks liegen duenn besetzt als Liste, nicht als
// Feld ueber alle Parameter - ein Step traegt selten mehr als eine Handvoll.
#pragma once
#include "param.h"
#include "doc.h"

enum { NTRACKS = 8, NSTEPS = 128, NPATTERNS = 16, NLOCKS = 12 };

struct Lock {
    uint16_t slot;      // Slot des Spurparameters (Reihenfolge der Tabelle)
    int16_t  val;
    uint8_t  slide;     // 1 = bis zum naechsten Trig auf diesen Wert gleiten
};

struct Step {
    uint8_t on = 0;     // 0 leer, 1 Trig, 2 trigless (nur Werte, kein Anschlag)
    uint8_t nLock = 0;
    Lock lock[NLOCKS];

    const Lock* findLock(int slot) const {
        for (int i = 0; i < nLock; i++) if (lock[i].slot == slot) return &lock[i];
        return nullptr;
    }
    bool setLock(int slot, int16_t v, uint8_t slide = 0) {
        for (int i = 0; i < nLock; i++)
            if (lock[i].slot == slot) { lock[i].val = v; lock[i].slide = slide; return true; }
        if (nLock >= NLOCKS) return false;
        lock[nLock++] = {(uint16_t)slot, v, slide};
        return true;
    }
};

struct Track {
    int16_t p[64];              // Spurparameter nach Slot
    uint8_t len = 16;           // Steps je Durchlauf (Polymetrik)
    uint8_t speed = 4;          // Taktteiler: 4 = 16tel, 2 = 8tel, 8 = 32stel
    Step s[NSTEPS];
};

struct Pattern {
    Track t[NTRACKS];
    uint8_t used = 0;
};

struct Project {
    int16_t g[32];              // globale Parameter nach Slot
    Pattern pat[NPATTERNS];
};

// --- Grundwerte aus der Parametertabelle --------------------------------------------------
inline void projectInit(Project& P) {
    for (int i = 0; i < NPARAMS; i++) {
        const ParamDef& d = PARAMS[i];
        int slot = paramSlot(i);
        if (d.scope == PS_GLOBAL) P.g[slot] = d.def;
    }
    for (int pi = 0; pi < NPATTERNS; pi++) {
        P.pat[pi].used = 0;
        for (int t = 0; t < NTRACKS; t++) {
            Track& tr = P.pat[pi].t[t];
            tr.len = 16; tr.speed = 4;
            for (int i = 0; i < NPARAMS; i++)
                if (PARAMS[i].scope == PS_TRACK) tr.p[paramSlot(i)] = PARAMS[i].def;
            for (int s = 0; s < NSTEPS; s++) { tr.s[s].on = 0; tr.s[s].nLock = 0; }
        }
    }
}

// Wirksamer Wert eines Spurparameters auf einem Step (Lock schlaegt Grundwert).
inline int16_t effective(const Track& tr, int step, int slot) {
    const Lock* l = tr.s[step].findLock(slot);
    return l ? l->val : tr.p[slot];
}

// --- Dokument schreiben ---------------------------------------------------------------------
// Geschrieben wird nur, was vom Standard abweicht: die Datei bleibt klein und lesbar, und ein
// Parameter, den es spaeter nicht mehr gibt, stoert niemanden.
inline void projectToDoc(const Project& P, Doc& d) {
    char k[DOC_KEY];
    for (int i = 0; i < NPARAMS; i++) {
        if (PARAMS[i].scope != PS_GLOBAL) continue;
        int slot = paramSlot(i);
        if (P.g[slot] != PARAMS[i].def) d.set(PARAMS[i].id, P.g[slot]);
    }
    for (int pi = 0; pi < NPATTERNS; pi++) {
        const Pattern& pt = P.pat[pi];
        if (!pt.used) continue;
        snprintf(k, sizeof k, "pat.%d.used", pi); d.set(k, 1);
        for (int t = 0; t < NTRACKS; t++) {
            const Track& tr = pt.t[t];
            if (tr.len != 16)  { snprintf(k, sizeof k, "pat.%d.trk.%d.len", pi, t);   d.set(k, tr.len); }
            if (tr.speed != 4) { snprintf(k, sizeof k, "pat.%d.trk.%d.speed", pi, t); d.set(k, tr.speed); }
            for (int i = 0; i < NPARAMS; i++) {
                if (PARAMS[i].scope != PS_TRACK) continue;
                int slot = paramSlot(i);
                if (tr.p[slot] == PARAMS[i].def) continue;
                docKeyTrack(k, sizeof k, pi, t, PARAMS[i].id);
                d.set(k, tr.p[slot]);
            }
            for (int s = 0; s < NSTEPS; s++) {
                const Step& st = tr.s[s];
                if (!st.on && !st.nLock) continue;
                docKeyStep(k, sizeof k, pi, t, s, "");
                d.set(k, st.on);
                for (int li = 0; li < st.nLock; li++) {
                    int pIdx = paramOfSlot(PS_TRACK, st.lock[li].slot);
                    if (pIdx < 0) continue;
                    char sub[48];
                    snprintf(sub, sizeof sub, "%s", PARAMS[pIdx].id + 4);
                    docKeyStep(k, sizeof k, pi, t, s, sub);
                    d.set(k, st.lock[li].val);
                    if (st.lock[li].slide) {
                        char sub2[56]; snprintf(sub2, sizeof sub2, "%s.slide", sub);
                        docKeyStep(k, sizeof k, pi, t, s, sub2);
                        d.set(k, 1);
                    }
                }
            }
        }
    }
}

// --- Dokument lesen --------------------------------------------------------------------------
inline void projectFromDoc(const Doc& d, Project& P) {
    projectInit(P);
    char k[DOC_KEY];
    for (int i = 0; i < NPARAMS; i++) {
        if (PARAMS[i].scope != PS_GLOBAL) continue;
        P.g[paramSlot(i)] = (int16_t)d.get(PARAMS[i].id, PARAMS[i].def);
    }
    for (int pi = 0; pi < NPATTERNS; pi++) {
        snprintf(k, sizeof k, "pat.%d.used", pi);
        if (!d.get(k, 0)) continue;
        Pattern& pt = P.pat[pi];
        pt.used = 1;
        for (int t = 0; t < NTRACKS; t++) {
            Track& tr = pt.t[t];
            snprintf(k, sizeof k, "pat.%d.trk.%d.len", pi, t);   tr.len = (uint8_t)d.get(k, 16);
            snprintf(k, sizeof k, "pat.%d.trk.%d.speed", pi, t); tr.speed = (uint8_t)d.get(k, 4);
            for (int i = 0; i < NPARAMS; i++) {
                if (PARAMS[i].scope != PS_TRACK) continue;
                docKeyTrack(k, sizeof k, pi, t, PARAMS[i].id);
                tr.p[paramSlot(i)] = (int16_t)d.get(k, PARAMS[i].def);
            }
            for (int s = 0; s < NSTEPS; s++) {
                docKeyStep(k, sizeof k, pi, t, s, "");
                int on = d.get(k, 0);
                Step& st = tr.s[s];
                st.on = (uint8_t)on; st.nLock = 0;
                for (int i = 0; i < NPARAMS; i++) {
                    if (PARAMS[i].scope != PS_TRACK || !(PARAMS[i].flags & PF_LOCK)) continue;
                    char sub[48]; snprintf(sub, sizeof sub, "%s", PARAMS[i].id + 4);
                    docKeyStep(k, sizeof k, pi, t, s, sub);
                    if (!d.has(k)) continue;
                    int v = d.get(k, 0);
                    char sub2[56]; snprintf(sub2, sizeof sub2, "%s.slide", sub);
                    char k2[DOC_KEY]; docKeyStep(k2, sizeof k2, pi, t, s, sub2);
                    st.setLock(paramSlot(i), (int16_t)v, (uint8_t)d.get(k2, 0));
                }
            }
        }
    }
}
