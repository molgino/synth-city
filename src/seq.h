// Sequencer und Scheduler. Zeit rechnet in SAMPLES: jeder Step hat eine absolute Sample-Position,
// Micro-Timing verschiebt sie um Bruchteile eines Steps - auch nach VORN, ganz ohne Sonderfall.
// Entschieden wird einen Step im Voraus (Look-ahead), gespielt wird auf die Sample genau.
#pragma once
#include "model.h"
#include <math.h>

enum EvKind : uint8_t { EV_TRIG, EV_LOCKS };   // Anschlag bzw. nur Werte (trigless)

struct Ev {
    int64_t at;         // absolute Sample-Position
    uint8_t track;
    EvKind  kind;
    uint8_t step;
    int16_t note, vel;
    int32_t len;        // Tonlaenge in Samples
};

struct Sink { virtual void ev(const Ev& e, int offset) = 0; virtual ~Sink() {} };

// --- Trig-Bedingungen ------------------------------------------------------------------------
// 0 = keine. 1-8 die festen. 9-108 = Wahrscheinlichkeit 1-100 % - sie ist eine CONDITION, kein
// Parameter daneben: nur so kann PRE auf einen Zufall reagieren.
enum {
    CND_NONE = 0, CND_FILL, CND_NFILL, CND_PRE, CND_NPRE, CND_NEI, CND_NNEI, CND_1ST, CND_N1ST,
    CND_PROB1 = 9, CND_PROB100 = 108, CND_AB0 = 110, CND_NAB0 = 166, CND_MAX = 221
};
inline int condProb(int percent) { return CND_PROB1 + (percent < 1 ? 0 : (percent > 100 ? 99 : percent - 1)); }
inline int condAB(int a, int b, bool neg) { return (neg ? CND_NAB0 : CND_AB0) + (b - 2) * 8 + (a - 1); }

struct Seq {
    double sr = 48000.0;
    double bpm = 120.0;
    int64_t pos = 0;            // Samples seit Start
    bool running = false;
    bool fill = false;          // Performance-Ebene: aendert nie die Daten
    uint32_t seed = 0x5eed1234;

    int64_t nextK[NTRACKS];     // naechster noch nicht bewerteter Schritt je Spur
    bool lastCond[NTRACKS];     // Ergebnis der zuletzt AUSGEWERTETEN Condition dieser Spur
    uint32_t rng[NTRACKS];
    Ev pend[256];
    int nPend = 0;

    Seq() { reset(); }
    void reset() {
        pos = 0; nPend = 0;
        for (int t = 0; t < NTRACKS; t++) { nextK[t] = 0; lastCond[t] = false; rng[t] = seed + 0x9e37u * (t + 1); }
    }
    void start() { reset(); running = true; }
    void stop()  { running = false; }

    static uint32_t xorshift(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni(int t) { return (xorshift(rng[t]) >> 8) * (1.0f / 16777216.0f); }

    double stepLen(const Track& tr) const { return sr * 60.0 / (bpm * (tr.speed ? tr.speed : 4)); }

    // PRE/NEI sehen durch Trigs OHNE Condition hindurch, und PRE selbst zaehlt nicht als
    // ausgewertet - nur so steuert ein einziger Zufalls-Trig eine ganze Gruppe.
    bool condOk(int t, int cond, int64_t pass) {
        if (cond <= CND_NONE || cond > CND_MAX) return true;
        bool r;
        bool feeds = true;
        switch (cond) {
        case CND_FILL:  r = fill; break;
        case CND_NFILL: r = !fill; break;
        case CND_PRE:   r = lastCond[t];                         feeds = false; break;
        case CND_NPRE:  r = !lastCond[t];                        feeds = false; break;
        case CND_NEI:   r = t > 0 ? lastCond[t - 1] : false;     feeds = false; break;
        case CND_NNEI:  r = t > 0 ? !lastCond[t - 1] : true;     feeds = false; break;
        case CND_1ST:   r = pass == 0; break;
        case CND_N1ST:  r = pass != 0; break;
        default:
            if (cond <= CND_PROB100) { int p = cond - CND_PROB1 + 1; r = uni(t) * 100.0f < (float)p; }
            else {
                int base = cond >= CND_NAB0 ? CND_NAB0 : CND_AB0;
                int i = cond - base, b = i / 8 + 2, a = i % 8 + 1;
                r = (int)(pass % b) == a - 1;
                if (cond >= CND_NAB0) r = !r;
            }
        }
        if (feeds) lastCond[t] = r;
        return r;
    }

    void push(const Ev& e) { if (nPend < (int)(sizeof pend / sizeof *pend)) pend[nPend++] = e; }

    // Einen Schritt bewerten und - wenn er klingt - seine Events mit absoluter Zeit ablegen.
    void evalStep(const Track& tr, int t, int64_t k, double sp) {
        int len = tr.len < 1 ? 1 : tr.len;
        int s = (int)(((k % len) + len) % len);
        int64_t pass = k / len;
        const Step& st = tr.s[s];
        if (!st.on) return;

        static const int slotCond  = paramSlot(paramFind("trk.cond"));
        static const int slotProb  = paramSlot(paramFind("trk.prob"));
        static const int slotMicro = paramSlot(paramFind("trk.micro"));
        static const int slotNote  = paramSlot(paramFind("trk.note"));
        static const int slotVel   = paramSlot(paramFind("trk.vel"));
        static const int slotLen   = paramSlot(paramFind("trk.len"));
        static const int slotRtrg  = paramSlot(paramFind("trk.ratchet"));

        double nominal = (double)k * sp;
        double micro = (effective(tr, s, slotMicro) - 24) / 24.0;   // -1 .. +1 Step
        int64_t at = (int64_t)llround(nominal + micro * sp);

        if (st.on == 2) {   // trigless: Werte gelten ab hier, ohne Anschlag
            push({at, (uint8_t)t, EV_LOCKS, (uint8_t)s, 0, 0, 0});
            return;
        }
        if (!condOk(t, effective(tr, s, slotCond), pass)) return;
        int prob = effective(tr, s, slotProb);
        if (prob < 100 && uni(t) * 100.0f >= (float)prob) return;

        int rt = effective(tr, s, slotRtrg);
        if (rt < 1) rt = 1;
        int16_t note = effective(tr, s, slotNote), vel = effective(tr, s, slotVel);
        int32_t dur = (int32_t)(sp * effective(tr, s, slotLen) / 12.0);
        for (int r = 0; r < rt; r++)
            push({at + (int64_t)llround(sp * r / rt), (uint8_t)t, EV_TRIG, (uint8_t)s, note, vel,
                  rt > 1 ? (int32_t)(sp / rt) : dur});
    }

    // Ein Block: erst bewerten (einen Step im Voraus), dann alles ausgeben, was in diesen Block faellt.
    void process(const Project& P, int patIdx, int frames, Sink& sink) {
        if (!running) { pos += frames; return; }
        const Pattern& pt = P.pat[patIdx];
        for (int t = 0; t < NTRACKS; t++) {
            const Track& tr = pt.t[t];
            double sp = stepLen(tr);
            double horizon = (double)(pos + frames) + sp;    // Look-ahead: ein Step dieser Spur
            for (int64_t k = nextK[t]; (double)k * sp < horizon; k++) { evalStep(tr, t, k, sp); nextK[t] = k + 1; }
        }
        int64_t end = pos + frames;
        for (int i = 0; i < nPend - 1; i++)                  // nach Zeit sortieren (wenige Eintraege)
            for (int j = i + 1; j < nPend; j++)
                if (pend[j].at < pend[i].at) { Ev tmp = pend[i]; pend[i] = pend[j]; pend[j] = tmp; }
        int keep = 0;
        for (int i = 0; i < nPend; i++) {
            if (pend[i].at < end) {
                int off = (int)(pend[i].at - pos);
                sink.ev(pend[i], off < 0 ? 0 : off);         // nie vor den Blockanfang
            } else pend[keep++] = pend[i];
        }
        nPend = keep;
        pos = end;
    }
};
