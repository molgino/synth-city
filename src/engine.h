// Audio-Engine: Sequencer, 8 monophone Spuren, Delay + Reverb, Master
#pragma once
#include "params.h"
#include "samples.h"
#include "fxbus.h"
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

struct Step { uint8_t on; uint8_t lock[NPARAMS]; };

// Sample-Punkte (Start/Ende/Loop) aus Grob+Fein, Nulldurchgang suchen (auch fuer den Sample-Editor)
struct Sample;
int smpPoint(const Sample* sm, int coarse, int fine);
int smpZero(const Sample* sm, int p);

struct Pattern {
    uint8_t machine[NTRACKS];
    uint8_t len[NTRACKS];
    uint8_t speed[NTRACKS];      // Index in SPEED_TPS (ab v5)
    uint8_t p[NTRACKS][NPARAMS];
    Step s[NTRACKS][NSTEPS];
    uint8_t chg;                 // ab v15: Pattern-Wechsel nach chg Steps (16tel), 0 = AUTO (laengste Spur)
    uint8_t used;                // ab v16: 1 = belegt (wird gespeichert); unbelegt + leer = bekommt beim Wechsel die Sounds des aktuellen Patterns
    uint8_t chgPad[6];
};

struct Project {
    char magic[4];
    uint32_t version;
    uint16_t bpm;
    uint8_t mst[8];
    uint8_t pad[2];
    Pattern pat[NPATTERNS];
    uint8_t cmp[8];          // Kompressor
    uint8_t bus[NBUS][8];    // ab v13: Effekt-Busse A/B/C (Typ + Regler, siehe BUS_DEFS); vorher fx[8] = Delay/Reverb
    uint8_t songLen;         // Anzahl Song-Zeilen
    uint8_t pad2[3];
    uint8_t patMute[NPATTERNS];   // Mutes je Pattern (Bit je Spur)
    struct SongRow { uint8_t pat[NTRACKS]; uint8_t rep, mute, pad[2]; } row[64];   // Song als Clip-Matrix; pat = SONG_EMPTY: Spur schweigt
};
enum { PROJECT_VERSION = 18, SONG_MAX = 64, SONG_EMPTY = 255 };
// Groessen der Formate v9-v12 (96 Parameter je Spur): danach steht ggf. die Sample-Namensliste
enum { OLD_PAT_SIZE = 24 + 8 * OLD_NPARAMS + NTRACKS * NSTEPS * (1 + OLD_NPARAMS),
       PROJECT_V11_SIZE = 20 + OLD_NPAT * OLD_PAT_SIZE + 8 + 8 + 128 + 4,
       PROJECT_V12_SIZE = PROJECT_V11_SIZE + OLD_NPAT + 64 * 12 };
// v13: 128 Parameter je Spur (danach ab v14 168)
enum { V13_PAT_SIZE = 24 + 8 * V13_NPARAMS + NTRACKS * NSTEPS * (1 + V13_NPARAMS),
       PROJECT_V13_SIZE = 20 + OLD_NPAT * V13_PAT_SIZE + 8 + NBUS * 8 + 4 + OLD_NPAT + 64 * 12 };
static_assert(PROJECT_V11_SIZE == 1602088, "Layout v11 veraendert");
static_assert(PROJECT_V13_SIZE == 2131144, "Layout v13 veraendert");
// v14: wie v15, aber ohne chg je Pattern
enum { V14_PAT_SIZE = 24 + 8 * NPARAMS + NTRACKS * NSTEPS * (1 + NPARAMS),
       PROJECT_V14_SIZE = 20 + OLD_NPAT * V14_PAT_SIZE + 8 + NBUS * 8 + 4 + OLD_NPAT + 64 * 12 };
static_assert(sizeof(Pattern) == V14_PAT_SIZE + 8, "Pattern-Layout veraendert");
// v15: 16 Patterns roh (wie v14 + chg je Pattern); v16: Kopf + Rest roh, dann nur belegte Patterns komprimiert (projectWrite)
enum { PROJECT_V15_SIZE = PROJECT_V14_SIZE + OLD_NPAT * 8,
       PROJECT_TAIL_SIZE = 8 + NBUS * 8 + 4 + NPATTERNS + 64 * 12 };   // cmp .. row (v16)
static_assert(sizeof(Project) == 20 + NPATTERNS * sizeof(Pattern) + PROJECT_TAIL_SIZE, "Projekt-Layout veraendert");
inline long projectDataSize(uint32_t ver) { return ver >= 15 ? (long)PROJECT_V15_SIZE : (ver == 14 ? (long)PROJECT_V14_SIZE : (ver == 13 ? (long)PROJECT_V13_SIZE : (ver == 12 ? (long)PROJECT_V12_SIZE : (long)PROJECT_V11_SIZE))); }

// Projektdatei v16 schreiben (ohne Sample-Trailer). lockFn(true/false) schuetzt das Kopieren je Pattern (nullptr = ohne)
bool projectWrite(FILE* f, const Project& P, void (*lockFn)(bool));
// Ende der Projektdaten in der Datei (dort beginnt ggf. der Sample-Trailer), -1 = unbekannt
long projectDataEnd(FILE* f);
// Pattern belegt? (Flag oder irgendein Step mit Trig/Lock)
bool patternUsed(const Pattern& P);
uint64_t projectHash(const Project& P);

struct Rng {
    uint32_t s = 0x9E3779B9u;
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (int32_t)s * (1.0f / 2147483648.0f); }
    inline float uni() { return (next() + 1.f) * 0.5f; }
};

struct SVF {
    float ic1 = 0, ic2 = 0, a1 = 1, a2 = 0, a3 = 0, k = 2;
    void set(float fc, float res, float sr);
    inline void proc(float v0, float& lo, float& bp, float& hp) {
        float v3 = v0 - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        lo = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0; }
    void coefFrom(const SVF& o) { a1 = o.a1; a2 = o.a2; a3 = o.a3; k = o.k; }   // Stereo: rechter Kanal, gleiche Einstellung
};

struct ADSR {
    int st = 0; float v = 0, ainc = 1, dco = 0, s = 1, rco = 0;
    void set(float a, float d, float sus, float r, float sr);
    void on() { st = 1; }
    void off() { if (st) st = 4; }
    inline float tick() {
        switch (st) {
        case 1: v += ainc; if (v >= 1.f) { v = 1.f; st = 2; } break;
        case 2: v = s + (v - s) * dco; if (fabsf(v - s) < 1e-4f) { v = s; st = 3; } break;
        case 3: v = s; break;
        case 4: v *= rco; if (v < 1e-5f) { v = 0; st = 0; } break;
        }
        return v;
    }
};

enum { DLY_N = 16384 };   // Insert-Delay/Room: Puffer je Stimme (2^n, Maske DLY_N-1)

struct Voice {
    int machine = 0;
    bool active = false;
    uint8_t lk[NPARAMS];
    const uint8_t* base = nullptr;
    float vel = 1, note = 60, curNote = 60;
    int gate = -1;
    // Oszillatoren
    float ph1 = 0, ph2 = 0, phs = 0, phM = 0, fbPrev = 0;
    float hph[6] = {0};
    // PERC: Koerper + Transient (teilt sich penv/denv/nenv/aux mit den klassischen Drums)
    float pcFold = 0, pcHarm = 0, pcRise = 0, pcSweep = 0, pcNLvl = 0, pcGrit = 0, pcNAtk = 0;
    int pcRpts = 0, pcLeft = 0, pcTimer = 0, pcIval = 1;
    bool pcAtkDone = false;   // Rausch-Attack fertig -> ab jetzt faellt die Huellkurve
    // DRONE: stehende Flaeche aus bis zu 8 verstimmten Partials
    float drPh[8] = {0}, drRat[8] = {0}, drLvl[8] = {0}, drMov = 0, drSm = 0, drSmT = 0;
    float drSubPh = 0, drShmPh = 0;   // eigene Phasen (phs/phM gehoeren SYNTH: Sync und Kreuzmodulation)
    int drN = 0;
    float drNois = 0, drMovInc = 0, drMovDep = 0, drShmr = 0, drSub = 0, drRand = 0, drWave = 0;
    // Drum-Huellkurven
    float denv = 0, ddco = 0, penv = 0, pco = 0, nenv = 0, nco = 0, cenv = 0, cco = 0;
    int clapLeft = 0, clapTimer = 0, clapInterval = 1; float burst = 0, bco = 0; bool clapTail = false;
    float menv = 0, mco = 0;
    // FM mit 4 Operatoren
    float fmPh[4] = {0}, fmEnv[4] = {0}, fmDco[4] = {1, 1, 1, 1}, fmInc[4] = {0}, fmMod[4] = {0}, fmCar[4] = {0};
    float fmAtk[4] = {1, 1, 1, 1}, fmSus[4] = {1, 1, 1, 1}, fmRco[4] = {1, 1, 1, 1}, fmPenv = 0;
    int fmSt[4] = {0};   // Operator-Huellkurve: 0 aus, 1 Attack, 2 Decay/Sustain, 3 Release
    float fmFbAmt = 0, fmFbPrev = 0;
    int fmAlgo = 0;
    int scaleVal = 0;   // globale Tonskala (vom Engine gesetzt)
    // neue Engines
    float wtPos = 0, wtWarp = 0, wtDet = 1, wtPenv = 0, sAtkInc = 0, shkLvl = 0, shkGrit = 0, shkGain = 1;
    int shkStage = 0, wtTab = 0;
    const Sample* wtUser = nullptr;   // WTBL: eigene Wavetable (TABL = USER)
    int chopStep = 0, chopFade = 64;
    struct Grain { bool on; double pos; float inc, age, len; } gr[12];
    float grTimer = 0, grInterval = 1000, grLen = 2000, grPosF = 0, grSpray = 0, grPitch = 1, grPSpr = 0;
    ADSR aenv, fenv, env3;   // Huellkurven 1-3 (ENV1 = frueher AMP, ENV2 = frueher Filter)
    ADSR gateEnv;            // VCA ohne volle Huellkurve: Gate mit kurzem Ausklang
    SVF filt, aux;
    SVF filt2, filt3;          // weitere Stufen (24 dB, Formant)
    // Filter 1: Typ (FT_*), Ladder-Zustaende, Kammfilter-Puffer, Formant-Pegel
    float lad[4] = {0}, ladG = 0.5f, ladK = 0;
    float cmb[2048]; int cmbW = 0; float cmbD = 100, cmbFb = 0, cmbLp = 0;
    float fmtG[3] = {1, 0.6f, 0.4f};
    float drift1 = 0, drift2 = 0;   // analoge Tonhoehen-Drift (Cent)
    float nzHp = 0, hatHp = 0;      // Hochpass-Zustaende (Snare-Rauschen, Hi-Hat)
    // 3 LFOs pro Stimme (Ziel = beliebiger Spurparameter)
    float lfPh[NLFO] = {0}, lfVal[NLFO] = {0}, lfSH[NLFO] = {0}, lfSHp[NLFO] = {0}, lfRun[NLFO] = {0}, lfFade[NLFO] = {1, 1, 1};
    float lfHeld[NLFO] = {0};
    // Modulation (LFO 1-3 und Huellkurven 1-3 mit je 3 Zielen): je Block neu berechnet
    enum { NMOD = (NLFO + NENV) * NMODT };
    int nMod = 0, mDst[NMOD], mMax[NMOD]; float mOff[NMOD];
    float noteMod = 0;
    float envCrv[NENV] = {1, 1, 1};                 // Huellkurvenform: Wert^k
    int fEnvN = 0, fEnvE[NENV * NMODT]; float fEnvAmt[NENV * NMODT];   // Huellkurve -> Filterfrequenz (je Block im Filter)
    int vcaN = 0, vcaE[NENV * NMODT]; float vcaDep[NENV * NMODT];       // Huellkurve -> Lautstaerke (je Sample)
    bool vcaFull = true, vcaSimple = true, env3On = false;             // vcaSimple: nur ENV1 voll -> schneller Weg wie frueher
    float vcaVel = 1;                                                  // Anschlagdynamik der Lautstaerke
    inline ADSR& envOf(int e) { return e == 0 ? aenv : (e == 1 ? fenv : env3); }
    float envVal(int e);   // aktueller Wert mit Kurvenform
    bool vcaDone();
    // Die Stimme wird noch gehalten (kein Note-Off, nicht im Ausklang) - nur dann darf Legato anbinden
    bool held() const { return vcaFull ? (aenv.st != 0 && aenv.st != 4) : (gateEnv.st != 0 && gateEnv.st != 4); }
    float stepSec = 0.125f;   // Laenge eines 16tels (fuer synchrone LFOs)
    float hold = 0; int holdC = 0;
    float glideCo = 0;
    float lastS = 0, dcOff = 0; bool dcArm = false;   // Knackschutz beim Neuanschlag einer klingenden Stimme
    // Sample-Wiedergabe
    const Sample* smp = nullptr;
    double spos = 0; float sinc = 0, sGain = 1; int sStart = 0, sEnd = 0; bool sRev = false, sLoop = false, sDone = false;
    int sLoopS = 0, sXf = 0;       // SMPL: Loop-Anfang (Sample) und Ueberblend-Laenge am Loop-Ende (0 = harter Sprung)
    // Time-Warp (SMPL/CHOP): spos laeuft im Projekttempo, zwei Lese-Koepfe mit Dreiecksfenster spielen die Tonhoehe
    bool warp = false; float wPitch = 1, wGr = 1800, wPh[2] = {0, 0.5f}; double wSt[2] = {0, 0};
    float warpRead(float& r);
    void warpInit();
    void warpSetup(int regionLen, float sr);
    Rng rng;
    // pro Block berechnet
    float pitchHz = 100, gainL = 0, gainR = 0, snd[NBUS] = {0}, drive = 0, crushQ = 0;
    int srrN = 1, ftype = 0;
    float fBase = 127, reso = 0;
    SVF bwHp, bwLp;              // Base-Width-Filter
    // Stereo-Samples (SMPL/CHOP/GRAN mit dataR): rechter Kanal mit eigenem Filter-Zustand, gleiche Einstellungen
    bool st = false;
    SVF filtR, filt2R, filt3R, bwHpR, bwLpR;
    float ladR[4] = {0}, cmbR[2048]; int cmbWR = 0; float cmbLpR = 0, holdR = 0, lastSR = 0, dcOffR = 0;
    float bwLo = 20, bwHi = 20000; bool bwOn = false;
    // Insert-Effekt
    float ib[2048];
    int iw = 0;
    // Delay/Room-Insert: eigener Puffer (371 ms bei 44,1 kHz), nur diese beiden Typen nutzen ihn
    float db[DLY_N] = {0};
    int dw = 0;
    float dLp = 0, dLpR = 0, insDly = 0, insDly2 = 0, insFb2 = 0, insDamp = 1;
    int insTail = 0;   // Nachlauf, solange die Delay-/Raumfahne noch klingt
    float insPh = 0, insRate = 0, insMix = 0, insP2 = 0, insP3 = 0, insFb = 0;
    float apZL[6] = {0}, apZR[6] = {0}, apA[2] = {0}, phFbL = 0, phFbR = 0, odLp = 0, odLpR = 0, odG = 1, odLvl = 1, odCo = 1;
    int insType = 0;
    // fuer die Anzeige
    volatile uint32_t trigs = 0;
    volatile float peakHold = 0;

    inline int PR(int i) const { return lk[i] != NO_LOCK ? lk[i] : base[i]; }   // ohne LFO
    bool lfAny = false;   // mindestens ein Ziel moduliert einen Parameter (sonst schneller Weg in P)
    uint64_t lfMask[(NPARAMS + 63) / 64] = {0};   // Bit je modulierten Parameter
    inline int P(int i) const {                                                   // mit LFO-/Huellkurven-Modulation
        if (!lfAny || !((lfMask[i >> 6] >> (i & 63)) & 1)) return PR(i);
        int r = PR(i); float off = 0; int mx = 127;
        for (int k = 0; k < nMod; k++) if (i == mDst[k]) { off += mOff[k]; mx = mMax[k]; }
        r += (int)off; if (r < 0) r = 0; if (r > mx) r = mx;
        return r;
    }
    int seqMod(int i, int r) const {   // Sequencer-Parameter (PROB, LEN ...) mit Modulation
        if (!lfAny || !((lfMask[i >> 6] >> (i & 63)) & 1)) return r;
        float off = 0; int mx = 127;
        for (int k = 0; k < nMod; k++) if (i == mDst[k]) { off += mOff[k]; mx = mMax[k]; }
        r += (int)off; return r < 0 ? 0 : (r > mx ? mx : r);
    }
    void addMod(int dest, float out, float noteSemi);
    void lfoUpdate(int n, float sr);
    void lfoTrigger();
    void lfoStep(int k, int n, float sr);
    void lfoAdvance(int n, float sr);
    // legato: ein Trig auf eine noch klingende Stimme wechselt nur die Tonhoehe (Huellkurven/LFOs laufen weiter)
    void noteOn(int machine, const uint8_t* base, const uint8_t* locks, float velScale, int gateSamples, float sr, bool legato = false);
    void noteOff() { aenv.off(); fenv.off(); env3.off(); gateEnv.off(); for (int k = 0; k < 4; k++) if (fmSt[k]) fmSt[k] = 3; }
    // Engines, die ohne Note-Off endlos weiterklingen wuerden (Stop/neuer Trig muss sie loslassen)
    bool gated() const { return machine == M_SYNTH || machine == M_FM || machine == M_WTBL || machine == M_GRAN
                             || machine == M_DRONE || (machine == M_SAMPLE && P(P_S5)); }
    void update(int n, float sr, float bpm);
    void render(float* L, float* R, float (*S)[32], int n, float sr);   // S = Sends an Bus A/B/C
    template <bool RIGHT> inline float filter1t(float x);
    inline float filter1(float x) { return filter1t<false>(x); }
};

struct Comb { float* b = nullptr; int n = 0, i = 0; float store = 0; };
struct Allpass { float* b = nullptr; int n = 0, i = 0; };

struct Reverb {
    Comb cl[8], cr[8]; Allpass al[4], ar[4];
    void init(int sr);
    void process(float in, float size, float damp, float& oL, float& oR);
};

class Engine {
public:
    Project proj;
    int sr = 44100;
    volatile bool playing = false;
    volatile int curPat = 0, nextPat = -1;
    volatile int trackPos[NTRACKS] = {0};
    volatile int playedStep[NTRACKS] = {0};
    double clock = 0;
    volatile bool fill = false;        // FILL-Modus fuer Trig-Bedingungen
    volatile int loopCount[NTRACKS] = {0};
    uint32_t tmReg[NTRACKS] = {0};                    // Turing-Register (generativ)
    volatile float dj = 0;     // -1..1 DJ-Filter (rechter Stick)
    bool solo[NTRACKS] = {false};
    // Song-Zeile, die gerade gilt (Song-Modus an und Zeilen vorhanden), sonst nullptr
    const Project::SongRow* songRow() const { return songOn && proj.songLen ? &proj.row[songPos % proj.songLen] : nullptr; }
    bool songActive() const { return songOn && proj.songLen > 0; }
    // Pattern, das Spur t gerade spielt: im Song je Spur aus der Zeile, sonst das aktuelle Pattern
    int patOf(int t) const { return songActive() ? trkPat[t] : curPat; }
    Pattern& TP(int t) { return proj.pat[patOf(t)]; }
    const Pattern& TP(int t) const { return proj.pat[patOf(t)]; }
    // Stumm: im Song die Mutes der Zeile (leere Zelle = stumm), sonst die Mutes des Patterns
    bool muted(int t) const {
        if (const Project::SongRow* r = songRow()) return r->pat[t] == SONG_EMPTY || ((r->mute >> t) & 1);
        return (proj.patMute[curPat % NPATTERNS] >> t) & 1;
    }
    void toggleMute(int t) {
        if (songActive()) { Project::SongRow& r = proj.row[songPos % proj.songLen]; if (r.pat[t] != SONG_EMPTY) r.mute ^= (uint8_t)(1u << t); }
        else proj.patMute[curPat % NPATTERNS] ^= (uint8_t)(1u << t);
    }
    void setMute(int t, bool m) { if (muted(t) != m) toggleMute(t); }
    bool audible(int t) const {   // Mute/Solo
        if (muted(t)) return false;
        for (int i = 0; i < NTRACKS; i++) if (solo[i]) return solo[t];
        return true;
    }
    float djSm = 0;
    volatile float peakL = 0, peakR = 0;
    volatile float grPeak = 0;   // Gain-Reduktion in dB (fuer die Anzeige)
    // Summe: Brickwall-Limiter mit Vorausschau (statt die Summe in die Saettigung zu fahren)
    enum { LIM_LA = 64 };        // Vorausschau in Samples (1,5 ms), Zweierpotenz
    float limXL[LIM_LA] = {0}, limXR[LIM_LA] = {0}, limReq[LIM_LA], limR[LIM_LA];
    float dcXL = 0, dcXR = 0, dcYL = 0, dcYR = 0;   // DC-Sperre der Summe (Hochpass 3 Hz)
    float limRel = 1.f; int limPos = 0; bool limReady = false;
    volatile float limGr = 0;    // groesste Absenkung des Limiters in dB (Anzeige/Diagnose)
    FxBus bus[NBUS];   // Effekt-Busse A/B/C
    Voice v[NTRACKS];
    Voice cv[NTRACKS][NCHORDV];   // Akkord-Zusatzstimmen (ARP-Modus AKKD)

    // MIDI (Ausgabe ueber Callback aus dem Audio-Thread; frameOff = Position im aktuellen Puffer)
    typedef void (*MidiOutFn)(uint8_t st, uint8_t d1, uint8_t d2, int frameOff);
    MidiOutFn midiOut = nullptr;
    volatile bool notesOut = false, recording = false, midiInOn = true;
    volatile int clockMode = 0;   // 0 aus, 1 senden, 2 empfangen
    volatile int chMode = 0;      // 0 = AUTO (aktive Spur), 1 = Kanal 1-8 -> Spur 1-8
    volatile int uiTrack = 0, uiPage = 0, uiCursor = 0;
    volatile int stepRecCount = 0;
    // Song-Modus: Pattern-Kette proj.song abspielen
    volatile bool songOn = false;
    volatile int songPos = 0, songRep = 0;
    volatile uint8_t trkPat[NTRACKS] = {0};   // Song: Pattern je Spur (aus der laufenden Zeile)
    volatile bool rowPending = false;          // Song: neue Zeile beim naechsten Pattern-Anfang uebernehmen
    void applyRow(int pos);                    // Pattern je Spur aus Zeile pos setzen
    static void rowFromPattern(Project& J, Project::SongRow& r, int pat);   // Zeile: alle Spuren pat, Mutes vom Pattern
    static void setBusType(Project& J, int b, int type);   // Effekt-Bus: Typ setzen, Regler auf dessen Grundwerte
    static int mapOldParam(int m, int i);                  // Parameterindex bis v12 -> v13 (-1 = entfaellt)
    static void migrateTrack13(int m, const uint8_t* o, uint8_t* out, bool lock);   // 96 alte Werte/Locks -> 128
    void songStart(int pos);   // ab Eintrag pos (nur Engine-Zustand; Wiedergabe startet mit start())

    void init(int sampleRate);
    void setRate(int sampleRate);
    void midiIn(uint8_t st, uint8_t d1, uint8_t d2);
    void defaultProject(bool demo = true);
    static void initPattern(Pattern& p);
    static void setMachine(Pattern& p, int t, int m);
    static void initTrack(Pattern& p, int t, int m);   // Klang einer Spur auf Grundwerte der Engine
    void start();
    void stop();
    void preview(int t, const Step* s);
    void dropSamples() { for (int t = 0; t < NTRACKS; t++) if (v[t].smp) { v[t].smp = nullptr; v[t].active = false; } }   // vor dem Austausch der Sample-Bank
    void render(float* out, int frames);
    bool save(const char* path);
    bool load(const char* path);
    double stepSamples() const { return sr * 60.0 / (proj.bpm * 4.0); }

private:
    struct Retrig { int left = 0; int interval = 0; int timer = 0; float vel = 1, fade = 0; uint8_t lk[NPARAMS]; } rt[NTRACKS];
    // Arpeggiator je Spur: spielt waehrend der Trig-Laenge die Akkordtoene
    struct Arp { int left = 0; int interval = 0; int timer = 0; int idx = 0, n = 0, mode = 0, gate = 0; float vel = 1, acc = 0;
                 bool legato = false;   // Spur steht auf LEGATO: der Arp bindet die Toene
                 uint8_t notes[48]; uint8_t lk[NPARAMS]; Rng rng; } ar[NTRACKS];
    void arpFire(int t);
    void playNote(int t, const uint8_t* lk, float velScale, int gs, bool toMidi, bool legato = false);
    SVF djL, djR;
    float cEnv = 0;
    Rng rng;
    int curOff = 0;
    int midiNote[NTRACKS], midiGate[NTRACKS], liveNote[NTRACKS], skipStep[NTRACKS];
    uint8_t liveLk[NTRACKS][NPARAMS];
    // Sequencer-Raster: 12 Ticks pro 16tel, jede Spur mit eigener Geschwindigkeit
    double nextTickAt = 0;
    long globalTick = 0, patTick = 0;
    int trackTick[NTRACKS] = {0};
    bool lastCond[NTRACKS] = {false};
    int dirSt[NTRACKS] = {1, 1, 1, 1, 1, 1, 1, 1};   // Ping-Pong-Richtung
    long stepCnt[NTRACKS] = {0};                      // gespielte Steps seit Start (fuer Durchlaeufe)
    uint8_t genLk[NTRACKS][NPARAMS];
    int seqP(int t, int i);                           // SEQ-Parameter inkl. LFO-Modulation
    void resetSeq(int t);
    // Mikrotiming: verzoegerte bzw. vorgezogene Trigs
    struct Pend { bool on; double at; int step; uint8_t lk[NPARAMS]; } pend[NTRACKS][2];
    int preDone[NTRACKS];                             // Step, der schon vorgezogen ausgeloest wurde
    bool trigFor(int t, int s, bool on, uint32_t reg, uint8_t* outLk);
    void schedule(int t, double at, int s, const uint8_t* lk);
    double stepStartT[NTRACKS] = {0}, stepLenT[NTRACKS] = {1};
    int tickCount = 0; double tickHist[25]; int tickHistN = 0;
    void doTick();
    void stepTrack(int t);
    bool condOk(int t, int cond);
    long masterTicks() const;      // Laenge eines Pattern-Durchlaufs in Ticks (Song-Zeile bzw. CHG oder laengste Spur)
    bool lastPass(int t) const;    // Bedingung LAST: dieser Durchlauf der Spur ist der letzte vor einem Pattern-Wechsel
    double trackStepSamples(int t) const { return stepSamples() * SPEED_TPS[TP(t).speed[t] % 7] / 12.0; }
    bool sendStart = false, sendStop = false;
    void initFx(int sampleRate);
    void emit(uint8_t st, uint8_t d1, uint8_t d2, int off) { if (midiOut) midiOut(st, d1, d2, off); }
    void allNotesOff();
    void liveNoteOn(int t, int note, int vel);
    void clockTick();
    void trigger(int t, const uint8_t* lk, float velScale, bool fromSeq, bool toMidi = true);
    void processBlock(float* out, int n);
    // Mehrkern: ein Hilfs-Thread rechnet pro Block etwa die Haelfte der klingenden Stimmen
    struct WkItem { Voice* v; signed char dst; int t; };   // dst: 0 = in den Kompressor, 1 = Sidechain-Spur, 2 = am Kompressor vorbei
    pthread_t wkThread;
    sem_t wkGo;
    volatile int wkGoSeq = 0, wkSleeping = 0, wkDone = 0;   // __atomic: Blocknummer, Schlafzustand, vom Helfer fertige Stimmen
    volatile uint32_t wkClaim = 0;                          // (Block & 0xFFFF) << 16 | naechste Stimme
    bool wkOn = false, wkPrioSet = false;
    WkItem wkList[NTRACKS * (1 + NCHORDV)];
    int wkN = 0, wkNs = 0;
    volatile bool wkMo = false;
    bool wkClaimOne(uint32_t tag, int& idx);
    float wL[32], wR[32], wS[NBUS][32], wKL[32], wKR[32], wUL[32], wUR[32];
    float TL[NTRACKS][32], TR[NTRACKS][32], wTL[NTRACKS][32], wTR[NTRACKS][32];   // Mehrkanal: je Spur (nur wenn multiCh > 2)
    float moDly[22][LIM_LA]; int moDlyPos = 0;   // Spuren/Busse um die Limiter-Vorausschau verzoegern (gleiche Lage wie der Master)
    int moPos = 0;
    static void* wkMain(void* arg);
public:
    // Mehrkanal-Ausgang (USB): render() schreibt zusaetzlich frames x multiCh Werte nach multiBuf.
    // Kanal 0-1 = Master, danach Stereo-Paare; multiRoute[q] = Paar (0 = Kanal 3-4 ...) fuer Quelle q (0-7 Spuren, 8-10 FX A/B/C), -1 = nur Master.
    // Mehrere Quellen auf demselben Paar werden addiert.
    float* multiBuf = nullptr;
    // Sampler: Spur tapTrack (nach Insert/VOL/PAN, ohne Busse) zusaetzlich als Stereo nach tapOut (frames x 2), -1 = aus
    float* tapOut = nullptr; int tapTrack = -1;
    // Sampler: Frame im zuletzt gerenderten Block, an dem eine Page (16 Steps ab Pattern-Anfang) beginnt, sonst -1
    // (der Sampler setzt es nach jedem Block im Audio-Callback zurueck)
    int barOff = -1;
    int multiCh = 2;
    signed char multiRoute[NTRACKS + NBUS] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    bool helperReady() const { return wkOn; }
    int audioThreads = 1;   // 1 oder 2 (gesetzt in init: 2, wenn mehrere Kerne und GB_AUDIO_THREADS != 1)
    long wkBlocks = 0;      // Bloecke, die zweikernig gerechnet wurden (Anzeige/Test)
};
