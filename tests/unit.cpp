// Unit-Tests fuer Sequencer, Engines, Filter und Projektformat (ohne UI/SDL-Fenster).
//   make test            -> baut und startet tests/unit
//   make test SAN=1      -> mit AddressSanitizer/UBSan (Speicherfehler, undefiniertes Verhalten)
#include "../src/engine.h"
#include "../src/samples.h"
#include "../src/factory.h"
#include "../src/pcm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int nPass = 0, nFail = 0;
static const char* curTest = "";
#define CHECK(c, ...) do { if (c) nPass++; else { nFail++; printf("  FEHLER [%s] %s:%d: ", curTest, __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static Engine E;   // gross (Projekt + Puffer): statisch
static float buf[44100 * 2 * 4];

// leeres Projekt, alle Spuren still (KICK ohne Trigs), keine Sends
static void fresh(int bpm = 120) {
    E.stop();
    E.defaultProject(false);
    E.proj.bpm = (uint16_t)bpm;
    E.songOn = false;
    for (int t = 0; t < NTRACKS; t++) { E.proj.pat[0].p[t][P_DLY] = 0; E.proj.pat[0].p[t][P_REV] = 0; }
    E.curPat = 0; E.nextPat = -1;
}
static uint32_t trigs(int t) { return E.v[t].trigs; }
// rendert n Frames in Bloecken wie die Soundkarte (256), liefert Spitze, prueft auf NaN/Inf
static float run(int frames, bool* bad = nullptr) {
    float pk = 0;
    while (frames > 0) {
        int n = frames > 256 ? 256 : frames;
        E.render(buf, n);
        for (int i = 0; i < n * 2; i++) {
            float x = buf[i];
            if (!(x == x) || x > 1e6f || x < -1e6f) { if (bad) *bad = true; }
            else if (fabsf(x) > pk) pk = fabsf(x);
        }
        frames -= n;
    }
    return pk;
}
static int barFrames(int bpm) { return (int)(44100.0 * 60.0 / bpm * 4.0); }   // 16 Sechzehntel

static void testTiming() {
    curTest = "Timing";
    static const int BPMS[4] = {60, 120, 174, 300};
    for (int bpm : BPMS) {
        fresh(bpm);
        for (int s = 0; s < 16; s++) E.proj.pat[0].s[0][s].on = 1;
        uint32_t t0 = trigs(0);
        E.start();
        run(barFrames(bpm) * 2 - 200);   // knapp 2 Takte
        CHECK(trigs(0) - t0 == 32, "BPM %d: %u statt 32 Trigs in 2 Takten", bpm, trigs(0) - t0);
    }
    // Geschwindigkeit 2x und 1/2x
    fresh(120);
    for (int s = 0; s < 16; s++) { E.proj.pat[0].s[0][s].on = 1; E.proj.pat[0].s[1][s].on = 1; }
    E.proj.pat[0].speed[0] = 6;   // 2x
    E.proj.pat[0].speed[1] = 2;   // 1/2x
    uint32_t a = trigs(0), b = trigs(1);
    E.start();
    run(barFrames(120) - 200);
    CHECK(trigs(0) - a == 32, "2x: %u statt 32", trigs(0) - a);
    CHECK(trigs(1) - b == 8, "1/2x: %u statt 8", trigs(1) - b);
}

static void testPolymeter() {
    curTest = "Polymetrie";
    fresh(120);
    E.proj.pat[0].len[0] = 3;
    E.proj.pat[0].s[0][0].on = 1;   // jeder 3. Step
    uint32_t a = trigs(0);
    E.start();
    run(barFrames(120) * 3 - 200);   // 48 Steps -> 16 Trigs
    CHECK(trigs(0) - a == 16, "Laenge 3: %u statt 16", trigs(0) - a);
}

static void testProbCond() {
    curTest = "Wahrscheinlichkeit/Bedingung";
    fresh(120);
    for (int s = 0; s < 16; s++) {
        Step& st = E.proj.pat[0].s[0][s]; st.on = 1; st.lock[P_PROB] = 0;
        Step& s2 = E.proj.pat[0].s[1][s]; s2.on = 1; s2.lock[P_PROB] = 50;
    }
    uint32_t a = trigs(0), b = trigs(1);
    E.start();
    run(barFrames(120) * 8 - 200);
    CHECK(trigs(0) - a == 0, "PROB 0: %u Trigs", trigs(0) - a);
    unsigned n = trigs(1) - b;
    CHECK(n > 40 && n < 88, "PROB 50: %u von 128 (erwartet ~64)", n);
    // 1:2 = nur jeder zweite Durchlauf
    fresh(120);
    E.proj.pat[0].s[0][0].on = 1;
    int a2 = -1, b2 = 0; bool ng = false;
    for (int v = CND_AB; v < CND_AB_END; v++) { int x, y; bool neg; condDecode(v, x, y, neg); if (x == 1 && y == 2 && !neg) { a2 = v; b2 = y; ng = neg; break; } }
    (void)b2; (void)ng;
    CHECK(a2 >= 0, "Bedingung 1:2 nicht gefunden");
    E.proj.pat[0].s[0][0].lock[P_COND] = (uint8_t)a2;
    uint32_t c = trigs(0);
    E.start();
    run(barFrames(120) * 4 - 200);
    CHECK(trigs(0) - c == 2, "1:2 in 4 Takten: %u statt 2", trigs(0) - c);
}

// LAST: nur im letzten Durchlauf vor einem Pattern-Wechsel; CHG: Wechsel nach festen Steps
static void testLastChg() {
    curTest = "LAST/CHG";
    fresh(120);
    E.proj.pat[0].s[0][4].on = 1; E.proj.pat[0].s[0][4].lock[P_COND] = CND_LAST;
    E.proj.pat[0].s[1][4].on = 1; E.proj.pat[0].s[1][4].lock[P_COND] = CND_NLAST;
    uint32_t a = trigs(0), b = trigs(1);
    E.start();
    run(barFrames(120) * 2);   // kein Wechsel vorgemerkt: LAST nie, !LAST jeden Durchlauf
    CHECK(trigs(0) - a == 0 && trigs(1) - b == 2, "ohne Wechsel: LAST %u (0), !LAST %u (2)", trigs(0) - a, trigs(1) - b);
    run(barFrames(120) / 32);   // knapp nach Step 1 des 3. Durchlaufs: Wechsel vormerken -> dieser Durchlauf ist der letzte
    a = trigs(0); b = trigs(1);
    E.nextPat = 1;
    run(barFrames(120) / 2);
    CHECK(trigs(0) - a == 1 && trigs(1) - b == 0, "mit Wechsel: LAST %u (1), !LAST %u (0)", trigs(0) - a, trigs(1) - b);
    // CHG 4: der vorgemerkte Wechsel greift nach 4 Steps statt nach 16
    fresh(120);
    E.proj.pat[0].chg = 4;
    E.start();
    run(barFrames(120) / 64);           // erster Tick ist durch (sonst wuerde sofort gewechselt)
    E.nextPat = 2;
    run(barFrames(120) / 16 * 2);       // Mitte von Step 3: noch Pattern 1
    CHECK(E.curPat == 0, "CHG 4: zu frueh gewechselt (%d)", E.curPat);
    run(barFrames(120) / 16 * 3);       // nach Step 4 + Reserve
    CHECK(E.curPat == 2, "CHG 4: nicht nach 4 Steps gewechselt (%d)", E.curPat);
    E.proj.pat[0].chg = 0;
}

static void testEuclid() {
    curTest = "Euklid";
    fresh(120);
    E.proj.pat[0].p[0][P_PULS] = 5;
    uint32_t a = trigs(0);
    E.start();
    run(barFrames(120) * 2 - 200);
    CHECK(trigs(0) - a == 10, "5 aus 16: %u statt 10 in 2 Takten", trigs(0) - a);
}

static void testTrigless() {
    curTest = "Trigless-Locks";
    fresh(120);
    Pattern& P = E.proj.pat[0];
    Engine::initTrack(P, 4, M_SYNTH);
    P.s[4][0].on = 1;
    P.s[4][4].on = 2; P.s[4][4].lock[P_FREQ] = 11;
    uint32_t a = trigs(4);
    E.start();
    run(barFrames(120) / 16 * 5 + 100);   // bis kurz nach Step 5
    CHECK(trigs(4) - a == 1, "trigless darf nicht ausloesen (%u Trigs)", trigs(4) - a);
    CHECK(E.v[4].lk[P_FREQ] == 11, "Lock FREQ nicht uebernommen (%d)", E.v[4].lk[P_FREQ]);
}

static void testSong() {
    curTest = "Song-Modus";
    fresh(120);
    E.proj.songLen = 2;
    Engine::rowFromPattern(E.proj, E.proj.row[0], 0); E.proj.row[0].rep = 2;
    Engine::rowFromPattern(E.proj, E.proj.row[1], 3);
    E.songStart(0); E.songOn = true;
    E.start();
    run(barFrames(120) * 2 + 2000);
    CHECK(E.patOf(0) == 3 && E.patOf(7) == 3, "nach 2 Takten Pattern %d statt 4", E.patOf(0) + 1);
    run(barFrames(120));
    CHECK(E.patOf(0) == 0, "nach 3 Takten Pattern %d statt 1 (Schleife)", E.patOf(0) + 1);
}

// Clip-Matrix: jede Spur spielt ihr eigenes Pattern, leere Zelle und Mute schweigen
static void testClipMatrix() {
    curTest = "Clip-Matrix";
    fresh(120);
    for (int pi = 0; pi < 3; pi++) for (int t = 0; t < 3; t++) for (int st = 0; st < NSTEPS; st++) E.proj.pat[pi].s[t][st].on = 0;
    for (int st = 0; st < 16; st += 4) E.proj.pat[0].s[0][st].on = 1;    // Pattern 1, Spur 1: 4 Trigs
    for (int st = 0; st < 16; st += 2) E.proj.pat[5].s[1][st].on = 1;    // Pattern 6, Spur 2: 8 Trigs
    E.proj.pat[5].len[1] = 16;
    for (int st = 0; st < 16; st++) E.proj.pat[0].s[2][st].on = 1;       // Spur 3 in Pattern 1 voll, aber Zelle leer
    E.proj.songLen = 1;
    Project::SongRow& r = E.proj.row[0];
    Engine::rowFromPattern(E.proj, r, 0);
    r.pat[1] = 5; r.pat[2] = SONG_EMPTY;
    E.songStart(0);
    uint32_t a0 = trigs(0), a1 = trigs(1), a2 = trigs(2);
    E.start();
    run(barFrames(120) - 500);
    CHECK(trigs(0) - a0 == 4, "Spur 1: %u Trigs statt 4", trigs(0) - a0);
    CHECK(trigs(1) - a1 == 8, "Spur 2 (Pattern 6): %u Trigs statt 8", trigs(1) - a1);
    CHECK(trigs(2) - a2 == 0, "leere Zelle spielt (%u Trigs)", trigs(2) - a2);
    E.stop();
    // Zeilen-Mute
    r.mute = 1u << 1;
    E.songStart(0); a1 = trigs(1);
    E.start(); run(barFrames(120) - 500); E.stop();
    CHECK(trigs(1) - a1 == 0, "Zeilen-Mute wirkt nicht (%u Trigs)", trigs(1) - a1);
    // Pattern-Mute (ohne Song) und Uebernahme in neue Zeile
    E.songOn = false; E.curPat = 0;
    E.toggleMute(0);
    CHECK(E.proj.patMute[0] == 1 && E.muted(0), "Pattern-Mute nicht gesetzt");
    a0 = trigs(0);
    E.start(); run(barFrames(120) - 500); E.stop();
    CHECK(trigs(0) - a0 == 0, "Pattern-Mute wirkt nicht (%u Trigs)", trigs(0) - a0);
    Project::SongRow r2; Engine::rowFromPattern(E.proj, r2, 0);
    CHECK(r2.mute == 1 && r2.pat[3] == 0, "neue Zeile uebernimmt Pattern-Mutes nicht (%02x)", r2.mute);
    E.toggleMute(0);
}

static void testArp() {
    curTest = "Arpeggiator";
    fresh(120);
    Pattern& P = E.proj.pat[0];
    Engine::initTrack(P, 4, M_SYNTH);
    P.s[4][0].on = 1;
    P.p[4][P_LEN] = 127;          // lang halten
    P.p[4][P_AMOD] = 1; P.p[4][P_ACHD] = 2; P.p[4][P_ARAT] = 2;   // UP, MAJ, 1/16
    uint32_t a = trigs(4);
    E.start();
    run(barFrames(120) / 4);      // 4 Sechzehntel
    CHECK(trigs(4) - a >= 3, "Arp: nur %u Noten in 4 Sechzehnteln", trigs(4) - a);
}

static void testChord() {
    curTest = "Akkord";
    fresh(120);
    Pattern& P = E.proj.pat[0];
    Engine::initTrack(P, 4, M_SYNTH);
    P.s[4][0].on = 1; P.p[4][P_LEN] = 100;
    P.p[4][P_AMOD] = ARP_CHORD; P.p[4][P_ACHD] = 5;   // MAJ7: 4 Toene
    E.start();
    bool bad = false;
    run(4410, &bad);
    int act = E.v[4].active;
    for (int k = 0; k < NCHORDV; k++) act += E.cv[4][k].active;
    CHECK(act == 4, "MAJ7-Akkord: %d statt 4 klingende Stimmen", act);
    CHECK(!bad, "Akkord: NaN");
    E.stop(); run(44100 * 3);
    act = 0; for (int k = 0; k < NCHORDV; k++) act += E.cv[4][k].active;
    CHECK(act == 0, "Akkordstimmen klingen nach Stop weiter (%d)", act);
}

// Freie Zusatznoten je Step: NT2-NT4 statt Akkordtyp-Tabelle
static void testChordNotes() {
    curTest = "Freie Akkordnoten";
    fresh(120);
    Pattern& P = E.proj.pat[0];
    Engine::initTrack(P, 4, M_SYNTH);
    P.s[4][0].on = 1; P.p[4][P_LEN] = 100;
    P.p[4][P_NOTE] = 60;                                  // C3
    P.p[4][P_NT2] = NOTEX_ZERO + 4; P.p[4][P_NT3] = NOTEX_ZERO + 7;   // Intervalle: + gr. Terz + Quinte, NT4 bleibt aus
    E.start();
    bool bad = false;
    run(4410, &bad);
    int act = E.v[4].active ? 1 : 0;
    for (int k = 0; k < NCHORDV; k++) act += E.cv[4][k].active ? 1 : 0;
    CHECK(act == 3, "C-E-G aus freien Noten: %d statt 3 Stimmen", act);
    CHECK(!bad, "Freie Akkordnoten: NaN");
    // die Tonhoehen muessen stimmen, nicht nur die Anzahl
    bool has64 = false, has67 = false;
    for (int k = 0; k < NCHORDV; k++) if (E.cv[4][k].active) {
        int n = (int)(E.cv[4][k].note + 0.5f);
        if (n == 64) has64 = true; if (n == 67) has67 = true;
    }
    CHECK((int)(E.v[4].note + 0.5f) == 60, "Hauptnote ist %d statt 60", (int)(E.v[4].note + 0.5f));
    CHECK(has64 && has67, "Zusatznoten 64/67 fehlen (%d/%d)", has64, has67);
    // freie Noten gehen dem Akkordtyp vor
    P.p[4][P_AMOD] = ARP_CHORD; P.p[4][P_ACHD] = 5;       // MAJ7 waere 4 Toene
    E.stop(); run(44100 * 2); E.start(); run(4410, &bad);
    act = E.v[4].active ? 1 : 0;
    for (int k = 0; k < NCHORDV; k++) act += E.cv[4][k].active ? 1 : 0;
    CHECK(act == 3, "freie Noten muessen MAJ7 verdraengen: %d statt 3", act);
    E.stop(); run(44100 * 3);
    act = 0; for (int k = 0; k < NCHORDV; k++) act += E.cv[4][k].active ? 1 : 0;
    CHECK(act == 0, "Zusatznoten klingen nach Stop weiter (%d)", act);
    {   // Intervalle wandern mit der Hauptnote (NT2-NT4 sind relativ, nicht absolut)
        P.p[4][P_AMOD] = 0;
        P.p[4][P_NOTE] = 67;                              // G3 statt C3 -> B3 und D4
        E.stop(); run(44100 * 2); E.start(); run(4410, &bad);
        bool h71 = false, h74 = false;
        for (int k = 0; k < NCHORDV; k++) if (E.cv[4][k].active) {
            int n = (int)(E.cv[4][k].note + 0.5f);
            if (n == 71) h71 = true; if (n == 74) h74 = true;
        }
        CHECK((int)(E.v[4].note + 0.5f) == 67, "Hauptnote ist %d statt 67", (int)(E.v[4].note + 0.5f));
        CHECK(h71 && h74, "Intervalle wandern nicht mit der Hauptnote (71 %d, 74 %d)", h71, h74);
    }
    {   // negatives Intervall: eine Oktave tiefer
        P.p[4][P_NOTE] = 60; P.p[4][P_NT2] = NOTEX_ZERO - 12; P.p[4][P_NT3] = 0;
        E.stop(); run(44100 * 2); E.start(); run(4410, &bad);
        bool h48 = false;
        for (int k = 0; k < NCHORDV; k++) if (E.cv[4][k].active && (int)(E.cv[4][k].note + 0.5f) == 48) h48 = true;
        CHECK(h48, "negatives Intervall (-12) fehlt");
        E.stop(); run(44100 * 3);
    }
}

// Stimmen-Modus: MONO laesst nur eine Stimme, LEGATO bindet (Huellkurve laeuft weiter)
static void testVoiceMode() {
    curTest = "Stimmen-Modus";
    fresh(120);
    Pattern& P = E.proj.pat[0];
    Engine::initTrack(P, 4, M_SYNTH);
    P.s[4][0].on = 1; P.p[4][P_LEN] = 100;
    P.p[4][P_NOTE] = 60; P.p[4][P_NT2] = 64; P.p[4][P_NT3] = 67;
    P.p[4][P_VMOD] = VM_MONO;
    bool bad = false;
    E.start(); run(4410, &bad);
    int act = 0; for (int k = 0; k < NCHORDV; k++) act += E.cv[4][k].active ? 1 : 0;
    CHECK(act == 0, "MONO spielt %d Zusatzstimmen", act);
    CHECK(E.v[4].active, "MONO: Hauptstimme klingt nicht");
    E.stop(); run(44100 * 3);

    // LEGATO: ein Trig auf die klingende Stimme darf die Huellkurve nicht neu ausloesen.
    // Attack 0 + langes Decay + Sustain 0: die Kurve faellt. Ein neuer Anschlag wirft sie
    // zurueck in die Attack-Phase (st 1, Pegel hoch), Legato laesst sie weiter fallen.
    fresh(120);
    Pattern& Q = E.proj.pat[0];
    Engine::initTrack(Q, 4, M_SYNTH);
    Q.p[4][P_AATK] = 0; Q.p[4][P_ADEC] = 115; Q.p[4][P_ASUS] = 0; Q.p[4][P_AREL] = 100;
    Q.p[4][P_LEN] = 127;                     // bis zum naechsten Trig halten
    Q.s[4][0].on = 1; Q.s[4][4].on = 1;      // zwei Trigs im Abstand von 4 Steps
    Q.s[4][4].lock[P_NOTE] = 67;             // zweiter Trig: andere Tonhoehe
    Q.p[4][P_NOTE] = 60;
    const int upTo = barFrames(120) / 16 * 4 + 400;    // kurz nach dem zweiten Trig

    Q.p[4][P_VMOD] = VM_LEGATO;
    E.start(); run(upTo, &bad);
    float vLeg = E.v[4].aenv.v; int stLeg = E.v[4].aenv.st;
    int nLeg = (int)(E.v[4].note + 0.5f);
    E.stop(); run(44100 * 3);

    Q.p[4][P_VMOD] = VM_MONO;                          // gleicher Ablauf ohne Bindung
    E.start(); run(upTo, &bad);
    float vMono = E.v[4].aenv.v; int stMono = E.v[4].aenv.st;
    E.stop(); run(44100 * 3);

    printf("  Legato: Huellkurve %.3f (Phase %d) | Mono: %.3f (Phase %d)\n", vLeg, stLeg, vMono, stMono);
    CHECK(nLeg == 67, "LEGATO: Tonhoehe wechselt nicht (%d statt 67)", nLeg);
    CHECK(stLeg != 1, "LEGATO: Huellkurve in der Attack-Phase, also neu ausgeloest");
    CHECK(vMono > vLeg * 1.5f, "LEGATO bindet nicht: Huellkurve %.3f, MONO %.3f", vLeg, vMono);
    CHECK(!bad, "Stimmen-Modus: NaN");

    // Gefunden beim Bau der DRONE-Engine: steht die Huellkurve nach einem Note-Off im Ausklang,
    // darf ein Legato-Trig NICHT anbinden (er wuerde den Ton ersticken), sondern muss neu anschlagen.
    fresh(120);
    Pattern& G = E.proj.pat[0];
    Engine::initTrack(G, 2, M_SYNTH);
    G.p[2][P_VMOD] = VM_LEGATO;
    G.p[2][P_ASUS] = 127; G.p[2][P_AREL] = 70;
    G.s[2][0].on = 1; G.s[2][8].on = 1;      // zwei Trigs, dazwischen laeuft das Gate ab
    G.p[2][P_LEN] = 20;                      // kurz: die erste Note wird vor dem zweiten Trig losgelassen
    E.start();
    run(barFrames(120) / 16 * 8 + 600, &bad);
    CHECK(E.v[2].active, "Legato nach Note-Off: Stimme ist tot statt neu angeschlagen");
    CHECK(E.v[2].aenv.st != 0 && E.v[2].aenv.st != 4,
          "Legato nach Note-Off: Huellkurve haengt im Ausklang (Phase %d)", E.v[2].aenv.st);
    E.stop(); run(44100 * 3);
}

// Neue Engines: PERC (Koerper + Transient) und DRONE (stehende Flaeche)
static void testPercDrone() {
    curTest = "PERC und DRONE";
    // --- PERC: Einzelschlag, Pegel und Ausklingen; die Grundwerte sollen eine Bassdrum geben
    fresh(120);
    Pattern& P = E.proj.pat[0];
    Engine::initTrack(P, 0, M_PERC);
    P.s[0][0].on = 1;
    bool bad = false;
    E.start();
    float pk = run(22050, &bad);
    printf("  PERC Grundwerte: Spitze %.3f\n", pk);
    CHECK(pk > 0.05f && pk < 1.0f, "PERC Spitze %.3f (erwartet 0,05-1,0)", pk);
    CHECK(!bad, "PERC: NaN");
    E.stop(); run(44100 * 4);
    CHECK(!E.v[0].active, "PERC klingt nach 4 s noch");

    // RISE dreht den Sweep nach oben: der Klang muss sich messbar aendern
    float eDown = 0, eUp = 0;
    for (int pass = 0; pass < 2; pass++) {
        fresh(120);
        Pattern& Q = E.proj.pat[0];
        Engine::initTrack(Q, 0, M_PERC);
        Q.s[0][0].on = 1;
        Q.p[0][P_S4] = pass ? 100 : 0;        // RISE
        E.start();
        float e = 0; for (int i = 0; i < 20; i++) { float x = run(1024, &bad); e += x * (i + 1); }   // Schwerpunkt
        (pass ? eUp : eDown) = e;
        E.stop(); run(44100 * 2);
    }
    printf("  PERC RISE: Schwerpunkt ab %.2f, auf %.2f\n", eDown, eUp);
    CHECK(eDown != eUp, "RISE aendert nichts am Klang");

    // Transient allein (Koerper stumm): deckt Snare/Clap/Shaker ab
    fresh(120);
    Pattern& R = E.proj.pat[0];
    Engine::initTrack(R, 0, M_PERC);
    R.s[0][0].on = 1;
    R.p[0][P_S1] = 0;          // DCAY ganz kurz = kaum Koerper
    R.p[0][P_X0] = 110;        // NLVL hoch
    R.p[0][P_X5] = 3;          // RPTS: Clap-artige Wiederholungen
    E.start();
    pk = run(22050, &bad);
    printf("  PERC Transient+RPTS: Spitze %.3f\n", pk);
    CHECK(pk > 0.02f, "PERC Transient zu leise (%.3f)", pk);
    CHECK(!bad, "PERC Transient: NaN");
    E.stop(); run(44100 * 4);
    CHECK(!E.v[0].active, "PERC mit RPTS klingt nach 4 s noch");

    // --- DRONE: steht und bleibt stehen, solange der Trig haelt
    fresh(120);
    Pattern& D = E.proj.pat[0];
    Engine::initTrack(D, 1, M_DRONE);
    D.s[1][0].on = 1;
    D.p[1][P_LEN] = 127;       // halten
    D.p[1][P_ASUS] = 127;      // Haltepegel voll
    E.start();
    run(22050, &bad);
    float a = run(22050, &bad), b2 = run(22050, &bad);
    printf("  DRONE: Pegel nach 0,5 s %.3f, nach 1,0 s %.3f\n", a, b2);
    CHECK(a > 0.02f && b2 > 0.02f, "DRONE steht nicht (%.3f / %.3f)", a, b2);
    CHECK(a < 1.0f && b2 < 1.0f, "DRONE uebersteuert (%.3f / %.3f)", a, b2);
    CHECK(!bad, "DRONE: NaN");
    // alle Teiltonzahlen und Wellenformen durchprobieren: nichts darf NaN oder still werden
    for (int part = 0; part <= 7; part++) for (int wave = 0; wave < 4; wave++) {
        D.p[1][P_S0] = (uint8_t)part; D.p[1][P_S3] = (uint8_t)wave;
        E.stop(); run(22050); E.start();
        float x = run(8192, &bad);
        CHECK(x > 0.001f, "DRONE still bei PART %d WAVE %d", part + 1, wave);
        CHECK(!bad, "DRONE NaN bei PART %d WAVE %d", part + 1, wave);
    }
    E.stop(); run(44100 * 6);
    float tail = run(4410);
    CHECK(tail < 1e-3f, "DRONE klingt nach Stop weiter (%.5f)", tail);
}

static void testFactory() {
    curTest = "Werksklaenge";
    for (int i = 0; i < NFACTORY; i++) {
        fresh(120);
        Pattern& P = E.proj.pat[0];
        applyFactory(P, 0, i);
        for (int s = 0; s < 16; s += 4) P.s[0][s].on = 1;
        E.start();
        bool bad = false;
        float pk = run(44100, &bad);
        printf("  %-18s Spitze %.2f\n", FACTORY[i].name, pk);
        CHECK(!bad, "%s: NaN", FACTORY[i].name);
        CHECK(pk > 0.05f, "%s zu leise (%.3f)", FACTORY[i].name, pk);
        CHECK(pk < 0.999f, "%s uebersteuert (%.2f)", FACTORY[i].name, pk);
    }
    curTest = "Demo-Song";
    E.stop();
    buildDemo(E);
    E.songOn = true; E.songStart(0);
    E.start();
    bool bad = false; float pk = 0; int seen = 0;
    for (int bar = 0; bar < 12; bar++) { float p = run(barFrames(E.proj.bpm), &bad); if (p > pk) pk = p; seen |= 1 << E.curPat; }
    printf("  Demo: Spitze %.2f, Patterns gespielt %x\n", pk, seen);
    CHECK(!bad, "Demo: NaN");
    CHECK(pk > 0.2f && pk < 1.01f, "Demo-Pegel %.2f", pk);
    CHECK(seen == 0xF, "Demo spielt nicht alle 4 Patterns (%x)", seen);
    E.songOn = false;
}

// alle Engines x alle Filtertypen x Extremwerte: endlich, begrenzt
static void testStability() {
    curTest = "Stabilitaet";
    int worstM = -1, worstF = -1; float worst = 0;
    for (int m = 0; m < M_COUNT; m++)
        for (int ft = 0; ft < FT_COUNT; ft++)
            for (int ex = 0; ex < 3; ex++) {
                fresh(140);
                Pattern& P = E.proj.pat[0];
                Engine::initTrack(P, 0, m);
                for (int s = 0; s < 16; s++) P.s[0][s].on = 1;
                P.p[0][P_FTYP] = (uint8_t)ft;
                P.p[0][P_RESO] = 127;
                P.p[0][P_FREQ] = ex == 0 ? 0 : (ex == 1 ? 64 : 127);
                P.p[0][P_FENV] = ex == 2 ? 127 : 64;
                P.p[0][P_DRIV] = ex == 2 ? 127 : 0;
                P.p[0][P_VOL] = 127;
                if (ex == 2) for (int i = P_S0; i <= P_S7; i++) P.p[0][i] = (uint8_t)paramDef(m, i).max;
                bool bad = false;
                E.start();
                float pk = run(44100 / 2, &bad);
                CHECK(!bad, "Engine %s Filter %d Fall %d: NaN/Inf", MACHINE_NAMES[m], ft, ex);
                if (pk > worst) { worst = pk; worstM = m; worstF = ft; }
            }
    CHECK(worst < 4.f, "Spitze %.2f (Engine %s, Filter %d) zu hoch", worst, worstM >= 0 ? MACHINE_NAMES[worstM] : "-", worstF);
    printf("  Stabilitaet: groesste Spitze %.2f (%s, Filter %d)\n", worst, worstM >= 0 ? MACHINE_NAMES[worstM] : "-", worstF);
}

// nach dem Ausklingen muss Stille kommen (keine Denormal-/Rauschreste)
static void testDecay() {
    curTest = "Ausklingen";
    fresh(120);
    for (int t = 0; t < NTRACKS; t++) E.proj.pat[0].s[t][0].on = 1;
    E.start();
    run(barFrames(120) / 16);
    E.stop();
    run(44100 * 6);
    float pk = run(4410);
    CHECK(pk < 1e-3f, "nach Stop + 6 s noch Pegel %.5f", pk);
}

// altes Projekt (Format v9-v12, 96 Parameter je Spur) Byte fuer Byte erzeugen
struct OldProj {
    uint8_t* b; uint32_t ver; long size;
    explicit OldProj(uint32_t v) : ver(v) {
        size = v == 12 ? PROJECT_V12_SIZE : PROJECT_V11_SIZE;
        b = (uint8_t*)calloc(1, size);
        memcpy(b, "GBX1", 4); memcpy(b + 4, &ver, 4); uint16_t bpm = 120; memcpy(b + 8, &bpm, 2);
        for (int i = 0; i < 8; i++) b[10 + i] = MST_DEFS[i].def;
        for (int pi = 0; pi < OLD_NPAT; pi++) for (int t = 0; t < NTRACKS; t++) {
            machine(pi, t, M_KICK); pat(pi)[8 + t] = 16; pat(pi)[16 + t] = SPEED_1X;
            for (int st = 0; st < NSTEPS; st++) memset(lock(pi, t, st), NO_LOCK, OLD_NPARAMS);
        }
        for (int i = 0; i < 8; i++) { tail()[i] = CMP_DEFS[i].def; fx()[i] = FX_DEFS[i].def; }
    }
    ~OldProj() { free(b); }
    uint8_t* pat(int pi) { return b + 20 + (long)pi * OLD_PAT_SIZE; }
    void machine(int pi, int t, int m) {
        pat(pi)[t] = (uint8_t)m;
        for (int i = 0; i < OLD_NPARAMS; i++) { int j = Engine::mapOldParam(m, i); p(pi, t)[i] = 0; if (j >= 0 && m != M_FM) p(pi, t)[i] = paramDef(m, j).def; }
    }
    uint8_t* p(int pi, int t) { return pat(pi) + 24 + t * OLD_NPARAMS; }
    uint8_t* lock(int pi, int t, int st) { return pat(pi) + 24 + 8 * OLD_NPARAMS + (t * NSTEPS + st) * (1 + OLD_NPARAMS) + 1; }
    uint8_t* tail() { return b + 20 + (long)OLD_NPAT * OLD_PAT_SIZE; }
    uint8_t* fx() { return tail() + 8; }
    uint8_t* song(int i) { return tail() + 16 + i * 2; }
    uint8_t& songLen() { return tail()[144]; }
    uint8_t* patMute() { return tail() + 148; }
    uint8_t* row(int i) { return tail() + 148 + OLD_NPAT + i * 12; }
    bool write(const char* path, bool trailer = false) {
        if (ver == 12) for (int i = 0; i < 64; i++) { uint8_t* r = row(i); if (!r[NTRACKS]) r[NTRACKS] = 1; }
        FILE* g = fopen(path, "wb"); if (!g) return false;
        bool ok = fwrite(b, 1, size, g) == (size_t)size;
        if (trailer) ok = ok && fwrite("SPL1\0\0", 1, 6, g) == 6;
        fclose(g); return ok;
    }
};

// Effekt-Busse: jeder Typ antwortet, bleibt endlich und begrenzt; Delay bringt das Echo zur eingestellten Zeit
static void testFxBus() {
    curTest = "Effekt-Busse";
    static FxBus fb;
    fb.init(44100);
    static float in[44100 * 6], oL[44100 * 6], oR[44100 * 6];
    for (int ty = 1; ty < BF_COUNT; ty++) {
        uint8_t p[8];
        for (int k = 0; k < 8; k++) p[k] = busDef(0, ty, k).def;
        p[BX_TYPE] = (uint8_t)ty;
        int N = 44100 * 4;
        for (int i = 0; i < N; i++) in[i] = 0.f;
        for (int k = 0; k < 4; k++) for (int i = 0; i < 400; i++) in[k * 22050 + i] = (i & 1 ? 0.5f : -0.5f) * (1.f - i / 400.f);
        double stepS = 44100 * 60.0 / (120 * 4.0);
        for (int i = 0; i < N; i += 32) fb.process(in + i, oL + i, oR + i, N - i < 32 ? N - i : 32, p, stepS);
        float pk = 0, energy = 0; bool fin = true;
        for (int i = 0; i < N; i++) { if (!isfinite(oL[i]) || !isfinite(oR[i])) fin = false; pk = fmaxf(pk, fmaxf(fabsf(oL[i]), fabsf(oR[i]))); energy += oL[i] * oL[i]; }
        CHECK(fin, "%s: NaN/Inf im Ausgang", BUS_TYPE_NAMES[ty]);
        CHECK(pk > 0.01f && pk < 2.f, "%s: Spitze %.3f", BUS_TYPE_NAMES[ty], pk);
        CHECK(energy > 1e-3f, "%s: kein Ausgang", BUS_TYPE_NAMES[ty]);
        printf("  %-8s Spitze %.2f\n", BUS_TYPE_NAMES[ty], pk);
    }
    {   // Delay 1/4 (= 4 Steps): erstes Echo nach 0,5 s bei 120 BPM
        uint8_t p[8]; for (int k = 0; k < 8; k++) p[k] = busDef(0, BF_DELAY, k).def;
        p[BX_TYPE] = BF_DELAY; p[BX_P1] = 10; p[BX_P2] = 0; p[BX_P4] = 0;
        fb.reset();
        int N = 44100;
        for (int i = 0; i < N; i++) in[i] = i < 64 ? 0.8f : 0.f;
        for (int i = 0; i < N; i += 32) fb.process(in + i, oL + i, oR + i, N - i < 32 ? N - i : 32, p, 44100 * 60.0 / (120 * 4.0));
        int at = -1; for (int i = 0; i < N && at < 0; i++) if (fabsf(oL[i]) > 0.05f) at = i;
        CHECK(at > 22050 - 300 && at < 22050 + 300, "Delay-Echo bei %d statt 22050", at);
    }
    {   // Reverb: lange Hallfahne klingt aus, kein Aufschaukeln bei maximaler Groesse
        uint8_t p[8]; for (int k = 0; k < 8; k++) p[k] = busDef(0, BF_REVERB, k).def;
        p[BX_TYPE] = BF_REVERB; p[BX_P1] = 127;
        fb.reset();
        int N = 44100 * 6;
        for (int i = 0; i < N; i++) in[i] = i < 2000 ? (i & 1 ? 0.5f : -0.5f) : 0.f;
        for (int i = 0; i < N; i += 32) fb.process(in + i, oL + i, oR + i, N - i < 32 ? N - i : 32, p, 5512.5);
        float e1 = 0, e2 = 0; for (int i = 44100; i < 88200; i++) e1 += oL[i] * oL[i]; for (int i = N - 44100; i < N; i++) e2 += oL[i] * oL[i];
        CHECK(e2 < e1, "Reverb waechst statt abzuklingen (%.4f -> %.4f)", e1, e2);
    }
}

static void testSaveLoad() {
    curTest = "Speichern/Laden";
    fresh(133);
    Pattern& P = E.proj.pat[2];
    P.s[5][77].on = 2; P.s[5][77].lock[P_FREQ] = 42; P.len[5] = 99; P.speed[5] = 6;
    E.proj.songLen = 3; Engine::rowFromPattern(E.proj, E.proj.row[1], 7); E.proj.row[1].pat[2] = SONG_EMPTY; E.proj.row[1].mute = 0x30; E.proj.patMute[4] = 0x81;
    for (int pi = 0; pi < NPATTERNS; pi++) E.proj.pat[pi].used = 1;   // alle belegt: alles muss genau zurueckkommen
    static Project ref;
    memcpy(&ref, &E.proj, sizeof(Project));
    const char* path = "/tmp/gb-unit-test.gbx";
    CHECK(E.save(path), "Speichern fehlgeschlagen");
    E.defaultProject(false);
    CHECK(E.load(path), "Laden fehlgeschlagen");
    CHECK(!memcmp(&ref, &E.proj, sizeof(Project)), "Projekt nach Laden verschieden");
    {   // v16 sparsam: nur belegte Patterns in der Datei, unbelegte kommen frisch (Grundklaenge) zurueck
        E.defaultProject(false);
        E.proj.pat[100].s[3][5].on = 1; E.proj.pat[100].p[3][P_FREQ] = 11; E.proj.pat[7].p[2][P_FREQ] = 12;   // 7: unbelegt + ohne Steps
        CHECK(E.save(path), "Speichern v16");
        FILE* g = fopen(path, "rb"); fseek(g, 0, SEEK_END); long n = ftell(g); long de = projectDataEnd(g); fclose(g);
        CHECK(n < 40000 && de == n, "v16-Datei zu gross oder Ende falsch (%ld / %ld)", n, de);
        CHECK(E.load(path), "Laden v16");
        CHECK(E.proj.pat[100].used && E.proj.pat[100].s[3][5].on == 1 && E.proj.pat[100].p[3][P_FREQ] == 11 && E.proj.pat[0].used, "v16: belegtes Pattern verloren");
        static Pattern fr; Engine::initPattern(fr);
        CHECK(!E.proj.pat[7].used && !memcmp(&E.proj.pat[7], &fr, sizeof(Pattern)), "v16: unbelegtes Pattern nicht frisch");
        uint8_t garb[64]; memset(garb, 0xFF, sizeof garb);
        g = fopen(path, "r+b"); fseek(g, 20 + PROJECT_TAIL_SIZE + 2 + 5, SEEK_SET); fwrite(garb, 1, sizeof garb, g); fclose(g);
        E.load(path);   // kaputte Pattern-Daten: darf nicht abstuerzen
    }
    // v9-Datei: SYNTH mit LP12 -> LP24
    {
        OldProj o(9);
        o.machine(0, 4, M_SYNTH); o.p(0, 4)[P_FTYP] = FT_LP12;
        CHECK(o.write(path), "v9 schreiben");
        CHECK(E.load(path), "Laden v9");
        CHECK(E.proj.version == PROJECT_VERSION, "Version %u", E.proj.version);
        CHECK(E.proj.pat[0].p[4][P_FTYP] == FT_LP24, "Migration LP12->LP24 fehlt (%d)", E.proj.pat[0].p[4][P_FTYP]);
    }
    // v11-Datei (Song als Kette, ohne Clip-Matrix): alter Teil + Sample-Liste dahinter
    {
        OldProj o(11);
        o.songLen() = 2; o.song(0)[0] = 3; o.song(0)[1] = 4; o.song(1)[0] = 9; o.song(1)[1] = 1;
        CHECK(o.write(path, true), "v11 schreiben");
        memset(E.proj.row, 0x77, sizeof E.proj.row);
        CHECK(E.load(path), "Laden v11");
        CHECK(E.proj.songLen == 2 && E.proj.row[0].pat[0] == 3 && E.proj.row[0].pat[7] == 3 && E.proj.row[0].rep == 4 && E.proj.row[1].pat[5] == 9,
              "Song v11 nicht in Zeilen umgestellt (%d %d %d)", E.proj.row[0].pat[0], E.proj.row[0].rep, E.proj.row[1].pat[5]);
        CHECK(E.proj.row[0].mute == 0 && E.proj.patMute[0] == 0, "Mutes nach Umstellung nicht leer");
    }
    // v12-Datei: FM mit 4 Operatoren, Sends auf die SEND-Seite, SYNTH-2. Seite, LFO-Ziele, Locks, Effekte -> Busse
    {
        OldProj o(12);
        uint8_t* f = o.p(2, 0);
        o.machine(2, 0, M_FM);
        f[P_S0] = 2; f[P_S1] = 30; f[P_S2] = 3; f[P_S3] = 10; f[P_S4] = 3; f[P_S5] = 3; f[P_S6] = 96; f[P_S7] = 10;
        f[P_X0] = 70; f[P_X1] = 0; f[P_X2] = 0; f[P_X3] = 127; f[P_X4] = 40; f[P_X5] = 50; f[P_X6] = 50; f[P_X7] = 90;
        o.lock(2, 0, 5)[P_S3] = 7;                       // Lock: RAT2 = 3.0
        uint8_t* y = o.p(2, 1);
        o.machine(2, 1, M_SYNTH); y[OLD_P_DLY] = 50; y[OLD_P_REV] = 60; y[P_L1 + LF_DEST] = OLD_P_DLY + 1;
        o.lock(2, 1, 3)[OLD_P_REV] = 99;
        o.fx()[FX_DTIM] = 6; o.fx()[FX_DFBK] = 70; o.fx()[FX_RSIZ] = 100;
        o.patMute()[2] = 0x81; o.songLen() = 1; o.row(0)[0] = 2;
        CHECK(o.write(path, true), "v12 schreiben");
        CHECK(E.load(path), "Laden v12");
        const Pattern& P = E.proj.pat[2];
        const uint8_t* q = P.p[0];
        CHECK(P.machine[0] == M_FM && q[P_S0] == 2 && q[P_S1] == 30 && q[P_S2] == 90 && q[P_S5] == 10, "FM: ALGO/FBK/VELO/NOIS falsch (%d %d %d %d)", q[P_S0], q[P_S1], q[P_S2], q[P_S5]);
        CHECK(FM_RATIOS[q[FM_OPB[0] + OP_RAT]] == 1.f && FM_RATIOS[q[FM_OPB[1] + OP_RAT]] == 5.f, "FM: Ratios falsch (%g %g)", FM_RATIOS[q[FM_OPB[0] + OP_RAT]], FM_RATIOS[q[FM_OPB[1] + OP_RAT]]);
        CHECK(q[FM_OPB[0] + OP_LVL] == 127 && q[FM_OPB[1] + OP_LVL] == 70 && q[FM_OPB[2] + OP_LVL] == 0, "FM: Pegel falsch");
        CHECK(q[FM_OPB[0] + OP_SUS] == 127 && q[FM_OPB[1] + OP_SUS] == 0 && q[FM_OPB[1] + OP_DEC] > 0, "FM: Huellkurven falsch (SUS1 %d SUS2 %d DEC2 %d)", q[FM_OPB[0] + OP_SUS], q[FM_OPB[1] + OP_SUS], q[FM_OPB[1] + OP_DEC]);
        CHECK(q[FM_OPB[1] + OP_DETN] > 64 && q[FM_OPB[0] + OP_DETN] == 64, "FM: FINE nicht in DETN umgerechnet (%d)", q[FM_OPB[1] + OP_DETN]);
        CHECK(FM_RATIOS[P.s[0][5].lock[FM_OPB[1] + OP_RAT]] == 3.f, "FM: Ratio-Lock falsch (%d)", P.s[0][5].lock[FM_OPB[1] + OP_RAT]);
        const uint8_t* w = P.p[1];
        // Sends: Zahl wurde auf die dB-Kurve umgerechnet (v19), der Anteil muss derselbe geblieben sein
        CHECK(fabsf(levGain(w[P_DLY], 127) / (50 / 127.f) - 1.f) < 0.03f && fabsf(levGain(w[P_REV], 127) / (60 / 127.f) - 1.f) < 0.03f
              && w[P_CMPS] == 1 && w[P_A7] == 0, "Sends nicht verschoben (%d %d), COMP %d", w[P_DLY], w[P_REV], w[P_CMPS]);
        CHECK(w[P_L1 + LF_DEST] == P_DLY + 1, "LFO-Ziel nicht mitgezogen (%d)", w[P_L1 + LF_DEST]);
        CHECK(P.s[1][3].lock[P_REV] != NO_LOCK && fabsf(levGain(P.s[1][3].lock[P_REV], 127) / (99 / 127.f) - 1.f) < 0.03f
              && P.s[1][3].lock[OLD_P_REV] == NO_LOCK, "Send-Lock nicht verschoben (%d)", P.s[1][3].lock[P_REV]);
        CHECK(w[P_X0] == 3 && w[P_X2] == 127 && w[P_X3] == 127, "SYNTH 2. Seite nicht auf Grundwerte (%d %d)", w[P_X0], w[P_X2]);
        CHECK(E.proj.bus[0][BX_TYPE] == BF_DELAY && E.proj.bus[0][BX_P1] == 6 && E.proj.bus[0][BX_P2] == 70, "Delay nicht auf Bus A");
        CHECK(E.proj.bus[1][BX_TYPE] == BF_REVERB && E.proj.bus[1][BX_P1] == 100, "Reverb nicht auf Bus B");
        CHECK(E.proj.patMute[2] == 0x81 && E.proj.songLen == 1 && E.proj.row[0].pat[0] == 2, "v12: Mutes/Song verloren");
    }
    // v13-Datei (128 Parameter): Werte/Locks bleiben, ENV1 -> VCA, ENV2 -> FREQ mit alter Filter-ENV-Tiefe
    {
        static Project src; memcpy(&src, &ref, sizeof(Project));
        src.pat[2].p[5][P_FENV] = 100; src.pat[2].s[5][77].lock[P_FENV] = 20; src.pat[2].p[5][P_L1 + LF_DEST] = P_PAN + 1;
        src.bus[1][BX_P2] = 33; src.songLen = 5;
        static uint8_t old[PROJECT_V13_SIZE];
        memset(old, 0, sizeof old);
        memcpy(old, &src, 20); uint32_t v13 = 13; memcpy(old + 4, &v13, 4);
        for (int pi = 0; pi < OLD_NPAT; pi++) {
            uint8_t* q = old + 20 + (long)pi * V13_PAT_SIZE; const Pattern& S = src.pat[pi];
            memcpy(q, S.machine, 8); memcpy(q + 8, S.len, 8); memcpy(q + 16, S.speed, 8);
            for (int t = 0; t < NTRACKS; t++) {
                memcpy(q + 24 + t * V13_NPARAMS, S.p[t], V13_NPARAMS);
                for (int st = 0; st < NSTEPS; st++) { uint8_t* sp = q + 24 + 8 * V13_NPARAMS + (t * NSTEPS + st) * (1 + V13_NPARAMS); sp[0] = S.s[t][st].on; memcpy(sp + 1, S.s[t][st].lock, V13_NPARAMS); }
            }
        }
        uint8_t* r = old + 20 + (long)OLD_NPAT * V13_PAT_SIZE;
        memcpy(r, src.cmp, 8); memcpy(r + 8, src.bus, NBUS * 8); r[8 + NBUS * 8] = src.songLen;
        memcpy(r + 12 + NBUS * 8, src.patMute, OLD_NPAT); memcpy(r + 12 + NBUS * 8 + OLD_NPAT, src.row, sizeof src.row);
        FILE* f13 = fopen(path, "wb"); fwrite(old, 1, sizeof old, f13); fclose(f13);
        CHECK(E.load(path), "Laden v13");
        const Pattern& P = E.proj.pat[2];
        CHECK(E.proj.version == PROJECT_VERSION, "v13: Version %u", E.proj.version);
        CHECK(P.s[5][77].on == 2 && P.s[5][77].lock[P_FREQ] == 42 && P.s[5][77].lock[P_FENV] == 20 && P.len[5] == 99 && P.speed[5] == 6, "v13: Step/Locks verloren");
        CHECK(P.p[5][P_FENV] == 100 && P.p[5][P_E2D1] == P_FREQ + 1 && P.p[5][P_E1D1] == MOD_VCA && P.p[5][P_E1A1] == 127, "v13: Huellkurven-Ziele falsch (%d %d %d)", P.p[5][P_E2D1], P.p[5][P_E1D1], P.p[5][P_E1A1]);
        CHECK(P.p[5][P_L1 + LF_DEST] == P_PAN + 1 && P.p[5][P_L1D2] == 0 && P.s[5][77].lock[P_E3D1] == NO_LOCK, "v13: LFO-Ziel/neue Locks falsch");
        CHECK(E.proj.bus[1][BX_P2] == 33 && E.proj.songLen == 5 && E.proj.patMute[4] == 0x81 && E.proj.row[1].mute == 0x30, "v13: Busse/Song verloren");
    }
    // v14-Datei (ohne Wechsel-Laenge je Pattern): alles bleibt, chg = AUTO
    {
        static Project src; memcpy(&src, &ref, sizeof(Project));
        src.pat[3].len[2] = 77; src.pat[3].s[2][70].on = 1; src.pat[3].s[2][70].lock[P_E3D1] = 12; src.songLen = 7; src.row[6].rep = 3;
        static uint8_t old[PROJECT_V14_SIZE];
        memcpy(old, &src, 20); uint32_t v14 = 14; memcpy(old + 4, &v14, 4);
        for (int pi = 0; pi < OLD_NPAT; pi++) memcpy(old + 20 + (long)pi * V14_PAT_SIZE, &src.pat[pi], V14_PAT_SIZE);
        { uint8_t* r = old + 20 + (long)OLD_NPAT * V14_PAT_SIZE;
          memcpy(r, src.cmp, 8); memcpy(r + 8, src.bus, NBUS * 8); r[8 + NBUS * 8] = src.songLen;
          memcpy(r + 12 + NBUS * 8, src.patMute, OLD_NPAT); memcpy(r + 12 + NBUS * 8 + OLD_NPAT, src.row, sizeof src.row); }
        FILE* f14 = fopen(path, "wb"); fwrite(old, 1, sizeof old, f14); fwrite("SPL1\0\0", 1, 6, f14); fclose(f14);
        E.proj.pat[3].chg = 9;
        CHECK(E.load(path), "Laden v14");
        CHECK(E.proj.version == PROJECT_VERSION && E.proj.pat[3].chg == 0, "v14: Version %u / chg %d", E.proj.version, E.proj.pat[3].chg);
        CHECK(E.proj.pat[3].len[2] == 77 && E.proj.pat[3].s[2][70].lock[P_E3D1] == 12 && E.proj.songLen == 7 && E.proj.row[6].rep == 3, "v14: Daten verloren");
    }
    // v15-Datei (16 Patterns roh): Bank A belegt, B-H frisch
    {
        static Project src; memcpy(&src, &ref, sizeof(Project));
        src.pat[15].chg = 24; src.pat[15].s[1][9].on = 1; src.patMute[15] = 0x42; src.songLen = 4; src.row[3].pat[1] = 15;
        static uint8_t old[PROJECT_V15_SIZE];
        memcpy(old, &src, 20); uint32_t v15 = 15; memcpy(old + 4, &v15, 4);
        for (int pi = 0; pi < OLD_NPAT; pi++) memcpy(old + 20 + (long)pi * sizeof(Pattern), &src.pat[pi], sizeof(Pattern));
        uint8_t* r = old + 20 + (long)OLD_NPAT * sizeof(Pattern);
        memcpy(r, src.cmp, 8); memcpy(r + 8, src.bus, NBUS * 8); r[8 + NBUS * 8] = src.songLen;
        memcpy(r + 12 + NBUS * 8, src.patMute, OLD_NPAT); memcpy(r + 12 + NBUS * 8 + OLD_NPAT, src.row, sizeof src.row);
        FILE* f15 = fopen(path, "wb"); fwrite(old, 1, sizeof old, f15); fwrite("SPL1\0\0", 1, 6, f15); fclose(f15);
        CHECK(E.load(path), "Laden v15");
        CHECK(E.proj.version == PROJECT_VERSION && E.proj.pat[15].chg == 24 && E.proj.pat[15].s[1][9].on == 1 && E.proj.patMute[15] == 0x42 && E.proj.row[3].pat[1] == 15, "v15: Daten verloren");
        CHECK(E.proj.pat[3].used && E.proj.pat[15].used && !E.proj.pat[16].used && E.proj.patMute[16] == 0, "v15: Belegung falsch");
        FILE* g = fopen(path, "rb"); long de = projectDataEnd(g); fclose(g);
        CHECK(de == (long)PROJECT_V15_SIZE, "v15: Datenende %ld", de);
    }
    // kaputte Datei darf nicht geladen werden
    FILE* f = fopen(path, "wb"); fwrite("GBX1xxxx", 1, 8, f); fclose(f);
    CHECK(!E.load(path), "kaputte Datei wurde geladen");
    remove(path);
}

static Engine MC[2];   // zwei frische Engines: gleicher Startzustand, 1 bzw. 2 Kerne
static void testMultiCore() {
    curTest = "Mehrkern";
    static float out[2][44100 * 2];
    static const int ms[NTRACKS] = {M_SYNTH, M_WTBL, M_FM, M_GRAN, M_SYNTH, M_SAMPLE, M_FM, M_SNARE};
    for (int k = 0; k < 2; k++) {
        Engine& X = MC[k];
        X.init(44100);
        if (k && !X.helperReady()) { printf("  Mehrkern: nur ein Kern verfuegbar - uebersprungen\n"); return; }
        X.audioThreads = k ? 2 : 1;
        X.defaultProject(false);
        X.proj.bpm = 150;
        Pattern& P = X.proj.pat[0];
        for (int t = 0; t < NTRACKS; t++) {
            Engine::initTrack(P, t, ms[t]);
            for (int s = 0; s < 16; s++) P.s[t][s].on = 1;
            P.p[t][P_LEN] = 100; P.p[t][P_ITYP] = (uint8_t)(1 + t % 6); P.p[t][P_IMIX] = 100;
            if (t < 3) { P.p[t][P_AMOD] = ARP_CHORD; P.p[t][P_ACHD] = 5; }
        }
        X.proj.cmp[CM_SRC] = 3; X.proj.cmp[CM_THR] = 40;   // Sidechain-Spur kann auf beiden Kernen landen
        X.start();
        for (int i = 0; i < 44100; i += 256) X.render(out[k] + i * 2, 44100 - i < 256 ? 44100 - i : 256);
    }
    float md = 0; for (int i = 0; i < 44100 * 2; i++) { float d = fabsf(out[0][i] - out[1][i]); if (d > md) md = d; }
    printf("  Mehrkern: %ld Bloecke zweikernig, max. Abweichung %.2e\n", MC[1].wkBlocks, md);
    CHECK(MC[1].wkBlocks > 0, "zweiter Kern hat nie mitgerechnet");
    CHECK(MC[0].wkBlocks == 0, "1-Kern-Modus hat trotzdem aufgeteilt");
    CHECK(md < 1e-4f, "1 Kern und 2 Kerne klingen verschieden (%.2e)", md);
}

// Routing: Spur 1 + Spur 2 auf dasselbe Paar = Summe der einzelnen Ausgaenge
// Modulation v14: LFO mit 3 Zielen, Huellkurve 3 auf einen Parameter, VCA ohne Huellkurve (Gate)
static void testModTargets() {
    curTest = "Mod-Ziele";
    static Engine X; static float out[512];
    auto setup = [&]() {
        X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 120;
        Pattern& P = X.proj.pat[0];
        Engine::initTrack(P, 4, M_SYNTH);
        for (int t = 0; t < NTRACKS; t++) for (int s2 = 0; s2 < 64; s2++) P.s[t][s2].on = 0;
        P.s[4][0].on = 1; P.p[4][P_LEN] = 127;
        return &P;
    };
    // LFO1: Ziel 1 aus, Ziel 2 = PAN, Ziel 3 = RESO
    Pattern* P = setup();
    P->p[4][P_L1 + LF_DEST] = 0; P->p[4][P_L1D2] = P_PAN + 1; P->p[4][P_L1A2] = 127; P->p[4][P_L1D3] = P_RESO + 1; P->p[4][P_L1A3] = 100; P->p[4][P_L1 + LF_RATE] = 100;
    X.start();
    int panMin = 999, panMax = -1, resMax = 0;
    for (int i = 0; i < 44100; i += 256) { X.render(out, 256); if (X.v[4].active) { int pp = X.v[4].P(P_PAN); panMin = pp < panMin ? pp : panMin; panMax = pp > panMax ? pp : panMax; int r = X.v[4].P(P_RESO); resMax = r > resMax ? r : resMax; } }
    printf("  LFO Ziel 2 PAN %d..%d, Ziel 3 RESO bis %d\n", panMin, panMax, resMax);
    CHECK(panMax - panMin > 80 && resMax > 40, "LFO-Ziele 2/3 wirken nicht (PAN %d..%d RESO %d)", panMin, panMax, resMax);
    // Huellkurve 3 -> VOL: Wert faellt mit der Huellkurve
    P = setup();
    P->p[4][P_VOL] = 20; P->p[4][P_E3D1] = P_VOL + 1; P->p[4][P_E3A1] = 127; P->p[4][P_E3ATK] = 0; P->p[4][P_E3DEC] = 80; P->p[4][P_E3SUS] = 0;
    X.start();
    int v0 = -1, v1 = -1;
    for (int i = 0; i < 44100 / 2; i += 256) { X.render(out, 256); if (X.v[4].active) { int vv = X.v[4].P(P_VOL); if (vv > v0) v0 = vv; v1 = vv; } }
    printf("  ENV3 -> VOL (Grundwert 20, Tiefe +63): hoechstens %d, nach 0,5 s %d\n", v0, v1);
    CHECK(v0 > 100 && v1 < 30 && X.v[4].env3On, "ENV3 wirkt nicht auf VOL (%d -> %d)", v0, v1);
    // VCA ohne Huellkurve: ENV1 Ziel aus -> Gate; Stimme endet kurz nach dem Gate
    P = setup();
    P->p[4][P_E1D1] = 0; P->p[4][P_LEN] = 16; P->p[4][P_AREL] = 127;   // lange REL wuerde sonst klingen
    X.start();
    float pk = 0; int lastActive = -1;
    for (int i = 0; i < 44100; i += 256) { X.render(out, 256); for (int k = 0; k < 512; k++) pk = fmaxf(pk, fabsf(out[k])); if (X.v[4].active) lastActive = i; }
    printf("  VCA als Gate: Spitze %.2f, klingt bis %.2f s\n", pk, lastActive / 44100.0);
    CHECK(pk > 0.05f && lastActive >= 0 && lastActive < 44100 / 4, "Gate-VCA falsch (Spitze %.2f, Ende %d)", pk, lastActive);
}

// Stereo-Samples: links Ton, rechts Stille -> Ausgang bleibt getrennt (mit Filter, Insert, Warp und Granulator)
static void testStereoSample() {
    curTest = "Stereo-Sample";
    static Engine X; static float out[44100 * 2];
    Sample sm; memset(&sm, 0, sizeof sm);
    snprintf(sm.name, sizeof sm.name, "test-stereo");
    sm.rate = 44100; sm.len = 44100;
    sm.data = (float*)malloc(sizeof(float) * sm.len); sm.dataR = (float*)malloc(sizeof(float) * sm.len);
    for (int i = 0; i < sm.len; i++) { sm.data[i] = 0.5f * sinf(6.2831853f * 330.f * i / 44100.f); sm.dataR[i] = 0.f; }
    const int SL = 201;
    poolSwap(SL, sm); sampleFree(sm);
    struct Case { const char* name; int mach, ftyp, ins, warp; } C[5] = {
        {"SMPL", M_SAMPLE, FT_LP12, INS_OFF, 0}, {"SMPL+LP24", M_SAMPLE, FT_LP24, INS_OFF, 0}, {"SMPL+Drive", M_SAMPLE, FT_LP12, INS_DRIVE, 0},
        {"SMPL+Warp", M_SAMPLE, FT_LP12, INS_OFF, 1}, {"GRAN", M_GRAN, FT_LP12, INS_OFF, 0}};
    for (auto& c : C) {
        X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 120;
        Pattern& P = X.proj.pat[0];
        Engine::initTrack(P, 6, c.mach);
        for (int t = 0; t < NTRACKS; t++) for (int s2 = 0; s2 < 64; s2++) P.s[t][s2].on = 0;
        P.s[6][0].on = 1; P.p[6][P_LEN] = 127; P.p[6][P_S0] = SL; P.p[6][P_FTYP] = (uint8_t)c.ftyp; P.p[6][P_ITYP] = (uint8_t)c.ins;
        P.p[6][P_X0] = (uint8_t)c.warp; P.p[6][P_X1] = 8;
        for (int b = 0; b < NBUS; b++) Engine::setBusType(X.proj, b, BF_OFF);
        X.start();
        for (int i = 0; i < 22050; i += 256) X.render(out + i * 2, 256);
        float eL = 0, eR = 0; for (int i = 0; i < 22050; i++) { eL += fabsf(out[i * 2]); eR += fabsf(out[i * 2 + 1]); }
        printf("  Stereo %s: links %.0f, rechts %.1f\n", c.name, eL, eR);
        CHECK(eL > 50.f && eR < eL * 0.02f, "%s: Kanaele nicht getrennt (L %.0f, R %.1f)", c.name, eL, eR);
    }
    Sample e; memset(&e, 0, sizeof e); poolSwap(SL, e); sampleFree(e);
}

// Loop: am Loop-Punkt darf es nicht springen (Ueberblenden), LSTR + Feinwerte + ZERO setzen die Punkte
static void testLoop() {
    curTest = "Sample-Loop";
    static Engine X; static float out[44100 * 2 * 2];
    Sample sm; memset(&sm, 0, sizeof sm);
    snprintf(sm.name, sizeof sm.name, "test-loop");
    sm.rate = 44100; sm.len = 44100;
    sm.data = (float*)malloc(sizeof(float) * sm.len);
    for (int i = 0; i < sm.len; i++) sm.data[i] = 0.5f * sinf(6.2831853f * 220.7f * i / 44100.f) + 0.2f;   // Gleichanteil: harter Sprung waere deutlich
    const int SL = 202;
    poolSwap(SL, sm); sampleFree(sm);
    const Sample* S = sampleGet(SL);
    CHECK(smpPoint(S, 127, 0) == S->len - 1 && smpPoint(S, 0, 0) == 0 && smpPoint(S, 64, 0) == (int)(64 / 127.0 * (S->len - 1)), "smpPoint ohne Feinwert anders als frueher");
    CHECK(smpPoint(S, 10, 64) > smpPoint(S, 10, 0) && smpPoint(S, 10, 64) < smpPoint(S, 11, 0), "Feinwert liegt nicht zwischen zwei Grobwerten");
    int z = smpZero(S, 1000); CHECK(z > 0 && fabsf(S->data[z]) < 0.25f, "smpZero findet keinen Nulldurchgang (%.3f)", S->data[z]);
    float maxJump[2] = {0, 0};
    for (int xfOn = 0; xfOn < 2; xfOn++) {
        X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 120;
        Pattern& P = X.proj.pat[0];
        Engine::initTrack(P, 6, M_SAMPLE);
        for (int t = 0; t < NTRACKS; t++) for (int s2 = 0; s2 < 64; s2++) P.s[t][s2].on = 0;
        P.s[6][0].on = 1; P.p[6][P_LEN] = 127; P.p[6][P_S0] = SL; P.p[6][P_S5] = 1;   // LOOP
        P.p[6][P_S4] = 10;          // Ende bei ~3500 Samples: schnelle Durchlaeufe
        P.p[6][P_X3] = xfOn ? 4 : 0; // Loop-Anfang ~1400 (mit Material davor -> Ueberblenden), sonst ab STRT = 0 (kein Material davor)
        P.p[6][P_FTYP] = FT_LP12; P.p[6][P_FREQ] = 127;
        for (int b = 0; b < NBUS; b++) Engine::setBusType(X.proj, b, BF_OFF);
        X.start();
        for (int i = 0; i < 44100; i += 256) X.render(out + i * 2, 256);
        for (int i = 4410; i < 44100; i++) { float d = fabsf(out[i * 2] - out[i * 2 - 2]); if (d > maxJump[xfOn]) maxJump[xfOn] = d; }
        float e = 0; for (int i = 30000; i < 44100; i++) e += fabsf(out[i * 2]);
        CHECK(e > 100.f, "Loop %d: nach 0,7 s still (%.0f) - Loop laeuft nicht weiter", xfOn, e);
    }
    printf("  Loop: groesster Sprung ohne Ueberblenden %.3f, mit %.3f\n", maxJump[0], maxJump[1]);
    CHECK(maxJump[1] < maxJump[0] * 0.5f, "Ueberblenden glaettet den Loop-Punkt nicht (%.3f vs %.3f)", maxJump[1], maxJump[0]);
    Sample e; memset(&e, 0, sizeof e); poolSwap(SL, e); sampleFree(e);
}

// Sampler-Abgriff: eine Spur getrennt mitschreiben, Master bleibt gleich
static void testInsDelay() {   // Insert DELAY/ROOM: Wiederholung kommt nach der eingestellten Zeit, Fahne klingt aus
    curTest = "Insert Delay/Room";
    static float out[44100 * 2];
    for (int ty = 0; ty < 2; ty++) {
        static Engine RI[2];
        Engine& X = RI[ty];
        X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 120;
        Pattern& P = X.proj.pat[0];
        Engine::initTrack(P, 0, M_KICK);
        for (int t = 1; t < NTRACKS; t++) for (int s2 = 0; s2 < NSTEPS; s2++) P.s[t][s2].on = 0;
        for (int s2 = 0; s2 < NSTEPS; s2++) P.s[0][s2].on = 0;
        P.s[0][0].on = 1;
        P.p[0][P_ITYP] = (uint8_t)(ty ? INS_ROOM : INS_DELAY);
        P.p[0][P_IP1] = ty ? 90 : 60;     // TIME / SIZE
        P.p[0][P_IP2] = ty ? 40 : 100;    // FBK / DAMP
        P.p[0][P_IP3] = 64;
        P.p[0][P_IMIX] = 100;   // nicht ganz nass, damit der Anschlag selbst hoerbar bleibt
        P.p[0][P_AREL] = 0; P.p[0][P_ASUS] = 0;
        X.start();
        for (int i = 0; i < 44100; i += 256) { int n = 44100 - i < 256 ? 44100 - i : 256; X.render(out + i * 2, n); }
        float e1 = 0, e2 = 0, pk = 0; bool fin = true;
        for (int i = 0; i < 44100; i++) {
            float v = fabsf(out[i * 2]) + fabsf(out[i * 2 + 1]);
            if (!isfinite(out[i * 2]) || !isfinite(out[i * 2 + 1])) fin = false;
            if (v > pk) pk = v;
            if (i < 2000) e1 += v; else if (i < 20000) e2 += v;   // Anschlag / danach
        }
        const char* nm = ty ? "ROOM" : "DELAY";
        CHECK(fin, "%s: NaN/Inf im Ausgang", nm);
        CHECK(pk > 0.01f && pk < 4.f, "%s: Spitze %.3f", nm, pk);
        CHECK(e2 > e1 * 0.05f, "%s: keine Wiederholungen nach dem Anschlag (%.1f gegen %.1f)", nm, e2, e1);
        printf("  Insert %-5s Spitze %.2f, Fahne %.0f%% vom Anschlag\n", nm, pk, 100.0 * e2 / (e1 + 1e-9));
    }
    {   // Die Fahne ueberlebt das Ende der Huellkurve (Nachlauf), ohne Insert ist nach dem Anschlag Ruhe
        static Engine RJ[2]; float tail[2] = {0, 0};
        for (int run = 0; run < 2; run++) {
            Engine& X = RJ[run];
            X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 120;
            Pattern& P = X.proj.pat[0];
            Engine::initTrack(P, 0, M_KICK);
            for (int t = 0; t < NTRACKS; t++) for (int s2 = 0; s2 < NSTEPS; s2++) P.s[t][s2].on = 0;
            P.s[0][0].on = 1;
            if (run) { P.p[0][P_ITYP] = INS_DELAY; P.p[0][P_IP1] = 80; P.p[0][P_IP2] = 110; P.p[0][P_IP3] = 100; P.p[0][P_IMIX] = 127; }
            X.start();
            for (int i = 0; i < 44100; i += 256) { int n = 44100 - i < 256 ? 44100 - i : 256; X.render(out + i * 2, n); }
            for (int i = 22050; i < 44100; i++) tail[run] += fabsf(out[i * 2]) + fabsf(out[i * 2 + 1]);
        }
        CHECK(tail[1] > tail[0] + 0.5f, "Delay-Fahne bricht mit der Huellkurve ab (ohne %.3f, mit %.3f)", tail[0], tail[1]);
    }
}

static float goertzel(const float* x, int n, float f, float sr) {   // Energie bei einer Frequenz
    float w = 2.f * 3.14159265f * f / sr, c = 2.f * cosf(w);
    float s1 = 0, s2 = 0;
    for (int i = 0; i < n; i++) { float s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return sqrtf(s1 * s1 + s2 * s2 - c * s1 * s2) / n;
}

static void testShimmerCloud() {   // SHIMMER setzt die Rueckkopplung eine Oktave hoch, CLOUD friert ein
    curTest = "Bus SHIMMER/CLOUD";
    static FxBus fb;
    static float in[44100 * 4], oL[44100 * 4], oR[44100 * 4];
    const int N = 44100 * 3;
    const double stepS = 44100 * 60.0 / (120 * 4.0);
    {   // SHIMMER: 220 Hz hinein, im Schweif muss 440 Hz staerker sein als 220 Hz
        fb.init(44100);
        uint8_t p[8]; for (int k = 0; k < 8; k++) p[k] = busDef(0, BF_SHIMMER, k).def;
        p[BX_TYPE] = BF_SHIMMER; p[BX_P3] = 127;   // SHIM voll
        for (int i = 0; i < N; i++) in[i] = i < 22050 ? 0.4f * sinf(2.f * 3.14159265f * 220.f * i / 44100.f) : 0.f;
        for (int i = 0; i < N; i += 32) fb.process(in + i, oL + i, oR + i, N - i < 32 ? N - i : 32, p, stepS);
        bool fin = true; float pk = 0;
        for (int i = 0; i < N; i++) { if (!isfinite(oL[i]) || !isfinite(oR[i])) fin = false; float a = fabsf(oL[i]); if (a > pk) pk = a; }
        CHECK(fin, "SHIMMER: NaN/Inf im Ausgang");
        CHECK(pk > 0.005f && pk < 2.f, "SHIMMER: Spitze %.3f", pk);
        const float* tailp = oL + 26460;           // ab 0,6 s: der Eingang ist aus, der Schweif traegt
        int tn = 22050;
        float e440 = goertzel(tailp, tn, 440.f, 44100.f), e220 = goertzel(tailp, tn, 220.f, 44100.f);
        // Gegenprobe: derselbe Hall ohne Versatz hat im Schweif fast nur den Grundton (Verhaeltnis statt Pegel pruefen)
        fb.init(44100);
        uint8_t q[8]; for (int k = 0; k < 8; k++) q[k] = busDef(0, BF_REVERB, k).def;
        q[BX_TYPE] = BF_REVERB; q[BX_P1] = p[BX_P1]; q[BX_P2] = p[BX_P2];
        for (int i = 0; i < N; i += 32) fb.process(in + i, oL + i, oR + i, N - i < 32 ? N - i : 32, q, stepS);
        float r440 = goertzel(tailp, tn, 440.f, 44100.f), r220 = goertzel(tailp, tn, 220.f, 44100.f);
        float qs = e440 / (e220 + 1e-9f), qr = r440 / (r220 + 1e-9f);
        CHECK(qs > qr * 4.f, "SHIMMER: keine Oktave im Schweif (Anteil %.4f, Hall ohne Versatz %.4f)", qs, qr);
        printf("  SHIMMER Oktavanteil im Schweif: %.3f (Hall ohne Versatz %.3f), Spitze %.3f\n", qs, qr, pk);
    }
    {   // CLOUD: nach dem Eingang still - ohne FRZ verstummt die Wolke, mit FRZ spielt sie weiter
        float late[2] = {0, 0};
        for (int run = 0; run < 2; run++) {
            fb.init(44100);
            uint8_t p[8]; for (int k = 0; k < 8; k++) p[k] = busDef(0, BF_CLOUD, k).def;
            p[BX_TYPE] = BF_CLOUD; p[BX_P5] = 0;
            for (int i = 0; i < N; i++) in[i] = i < 22050 ? 0.4f * sinf(2.f * 3.14159265f * 330.f * i / 44100.f) : 0.f;
            for (int i = 0; i < N; i += 32) {
                if (run && i >= 22050) p[BX_P5] = 127;   // FRZ erst aufdrehen, wenn Material da ist
                fb.process(in + i, oL + i, oR + i, N - i < 32 ? N - i : 32, p, stepS);
            }
            bool fin = true;
            for (int i = 0; i < N; i++) if (!isfinite(oL[i]) || !isfinite(oR[i])) fin = false;
            CHECK(fin, "CLOUD%s: NaN/Inf im Ausgang", run ? " (FRZ)" : "");
            for (int i = 44100 * 2; i < N; i++) late[run] += fabsf(oL[i]) + fabsf(oR[i]);
        }
        CHECK(late[1] > late[0] * 4.f + 1.f, "CLOUD: FRZ haelt die Wolke nicht (ohne %.1f, mit %.1f)", late[0], late[1]);
        printf("  CLOUD nach 2 s: ohne FRZ %.1f, mit FRZ %.1f\n", late[0], late[1]);
    }
}

// v19: Pegel laufen in dB, Huellkurvenzeiten auf der gebogenen Kurve.
// Gemeldet: "es ist immer sehr laut und wird erst sehr spaet leise - nur die letzten 30 Werte
// bringen etwas" (Haltepegel lief linear) und "die FM-Envelopes sind zu kurz" (Reglermitte 94 ms).
static void testLevels19() {
    curTest = "Pegel und Huellkurven (v19)";
    // 1. gleichmaessig in dB: jeder Schritt bringt denselben Pegelsprung
    float d1 = 20.f * log10f(levGain(127, 127) / levGain(96, 127));
    float d2 = 20.f * log10f(levGain(96, 127) / levGain(64, 127));
    float d3 = 20.f * log10f(levGain(64, 127) / levGain(32, 127));
    CHECK(fabsf(d1 - 12.4f) < 0.3f && fabsf(d2 - d1) < 0.5f && fabsf(d3 - d1) < 0.5f,
          "Pegel nicht gleichmaessig in dB (%.1f %.1f %.1f)", d1, d2, d3);
    // 2. der gemeldete Fall: die obere Reglerhaelfte brachte linear nur 2,9 dB
    float alt = 20.f * log10f((127 / 127.f) / (64 / 127.f));
    float neu = 20.f * log10f(levGain(127, 127) / levGain(64, 127));
    CHECK(alt < 7.f && neu > 15.f, "obere Haelfte bringt zu wenig (alt %.1f dB, neu %.1f dB)", alt, neu);
    CHECK(levGain(0, 127) == 0.f && fabsf(levGain(127, 127) - 1.f) < 1e-6f && fabsf(levGain(100, 100) - 1.f) < 1e-6f,
          "0 ist nicht still oder der Einheitswert ist nicht 0 dB");
    for (int v = 1; v < 127; v++) CHECK(levGain(v, 127) < levGain(v + 1, 127), "Pegelkurve nicht monoton bei %d", v);
    // 3. Huellkurvenzeiten: Enden wie vorher, Mitte deutlich laenger
    CHECK(fabsf(Engine::envSeconds(0) - 0.0005f) < 1e-5f && Engine::envSeconds(127) > 15.f,
          "Zeitbereich verschoben (%.4f .. %.1f s)", Engine::envSeconds(0), Engine::envSeconds(127));
    CHECK(Engine::envSeconds(64) > 0.15f && Engine::envSeconds(64) < 0.30f,
          "Reglermitte liegt nicht bei ~0,2 s (%.3f s)", Engine::envSeconds(64));
    for (int v = 0; v < 127; v++) CHECK(Engine::envSeconds(v) < Engine::envSeconds(v + 1), "Zeitkurve nicht monoton bei %d", v);
    // 4. die Migration haelt Klang und Zeit: alte Zahl -> neue Zahl mit gleicher Wirkung
    for (int v = 4; v <= 127; v += 3) {
        float g = levGain(Engine::val19(M_SYNTH, P_ASUS, v), 127);
        CHECK(fabsf(g / (v / 127.f) - 1.f) < 0.04f, "Haltepegel %d nicht klangtreu umgerechnet (%.3f statt %.3f)", v, g, v / 127.f);
        float gv = levGain(Engine::val19(M_SYNTH, P_VOL, v), 100), ov = (v / 100.f) * (v / 100.f);
        if (ov > 0.011f) CHECK(fabsf(gv / ov - 1.f) < 0.05f, "VOL %d nicht klangtreu umgerechnet (%.3f statt %.3f)", v, gv, ov);
        else CHECK(gv < 0.012f, "sehr leises VOL %d muesste am unteren Ende landen (%.4f)", v, gv);   // unter -39 dB endet die Kurve
        float t = Engine::envSeconds(Engine::val19(M_SYNTH, P_ADEC, v)), ot = 0.0005f * powf(2.f, v * (15.f / 127.f));
        CHECK(fabsf(t / ot - 1.f) < 0.08f, "Zeit %d nicht klangtreu umgerechnet (%.4f statt %.4f s)", v, t, ot);
    }
    // 5. FM-Operatoren werden mitgezogen, O.SUS bleibt linear (Index, nicht Pegel)
    CHECK(Engine::val19(M_FM, FM_OPB[1] + OP_DEC, 60) != 60 && Engine::val19(M_FM, FM_OPB[1] + OP_SUS, 60) == 60,
          "FM-Operator falsch umgerechnet");
    CHECK(Engine::val19(M_KICK, P_S4, 25) == 25 && Engine::val19(M_FM, P_S4, 25) != 25,
          "P_S4 ist nur bei FM eine Huellkurvenzeit");
    // 6. hoerbar: die Spur klingt bei DEC 64 deutlich laenger als die alten 35 ms
    fresh(); E.setMachine(E.proj.pat[0], 0, M_SYNTH);
    E.proj.pat[0].p[0][P_ADEC] = 64; E.proj.pat[0].p[0][P_ASUS] = 0; E.proj.pat[0].p[0][P_AREL] = 0;
    uint8_t L[NPARAMS]; memset(L, NO_LOCK, NPARAMS);
    E.v[0].noteOn(M_SYNTH, E.proj.pat[0].p[0], L, 1.f, 1 << 20, 44100.f, false);
    float pk = 0; int ms20 = -1;
    for (int blk = 0; blk < 44100 * 2 / 256; blk++) {
        E.render(buf, 256);
        float m = 0; for (int i = 0; i < 512; i++) if (fabsf(buf[i]) > m) m = fabsf(buf[i]);
        if (m > pk) pk = m;
        if (ms20 < 0 && pk > 0 && m < pk * 0.1f) ms20 = (int)(blk * 256 / 44.1f);
    }
    // DEC 64 = 0,2 s bis -60 dB, also ~60 ms bis -20 dB; bis v18 waren es an derselben Stelle 35 ms
    CHECK(ms20 > 50 && ms20 < 110, "DEC 64 klingt nicht ~0,2 s (bis -20 dB: %d ms, bis v18: 35 ms)", ms20);
}

static void testCmpRoute() {   // Kompressor-Routing: Spuren mit COMP=AUS gehen am Kompressor vorbei
    curTest = "Kompressor-Routing";
    static float out[3][22050 * 2];
    float pk[3] = {0, 0, 0};
    for (int run = 0; run < 3; run++) {   // 0: ohne Kompressor, 1: beide Spuren hinein, 2: Spur 2 vorbei
        static Engine RC[3];
        Engine& X = RC[run];
        X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 120;
        Pattern& P = X.proj.pat[0];
        Engine::initTrack(P, 0, M_KICK); Engine::initTrack(P, 1, M_SNARE);
        for (int t = 2; t < NTRACKS; t++) for (int s2 = 0; s2 < NSTEPS; s2++) P.s[t][s2].on = 0;
        for (int s2 = 0; s2 < 16; s2 += 4) { P.s[0][s2].on = 1; P.s[1][s2 + 2].on = 1; }
        for (int i = 0; i < 8; i++) X.proj.cmp[i] = CMP_DEFS[i].def;
        if (run) { X.proj.cmp[CM_THR] = 10; X.proj.cmp[CM_RAT] = 8; X.proj.cmp[CM_MIX] = 127; X.proj.cmp[CM_ATK] = 0; }
        if (run == 2) P.p[1][P_CMPS] = 0;   // Snare am Kompressor vorbei
        X.start();
        for (int i = 0; i < 22050; i += 256) { int n = 22050 - i < 256 ? 22050 - i : 256; X.render(out[run] + i * 2, n); }
        for (int i = 0; i < 22050 * 2; i++) { float v = fabsf(out[run][i]); if (v > pk[run]) pk[run] = v; }
        CHECK(isfinite(pk[run]), "Lauf %d: NaN im Ausgang", run);
    }
    CHECK(pk[1] < pk[0] * 0.9f, "Kompressor greift nicht (ohne %.3f, mit %.3f)", pk[0], pk[1]);
    CHECK(pk[2] > pk[1] * 1.05f, "COMP=AUS aendert nichts (alle %.3f, Spur 2 vorbei %.3f)", pk[1], pk[2]);
    {   // Alle Spuren vorbei = wie ohne Kompressor (bis auf das Makeup)
        static Engine RB[2]; float p2[2] = {0, 0};
        for (int run = 0; run < 2; run++) {
            Engine& X = RB[run];
            X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 120;
            Pattern& P = X.proj.pat[0];
            Engine::initTrack(P, 0, M_KICK);
            for (int t = 1; t < NTRACKS; t++) for (int s2 = 0; s2 < NSTEPS; s2++) P.s[t][s2].on = 0;
            for (int s2 = 0; s2 < 16; s2 += 4) P.s[0][s2].on = 1;
            for (int i = 0; i < 8; i++) X.proj.cmp[i] = CMP_DEFS[i].def;
            if (run) { X.proj.cmp[CM_THR] = 10; X.proj.cmp[CM_RAT] = 8; X.proj.cmp[CM_MIX] = 127; P.p[0][P_CMPS] = 0; }
            X.start();
            for (int i = 0; i < 22050; i += 256) { int n = 22050 - i < 256 ? 22050 - i : 256; X.render(out[0] + i * 2, n); }
            for (int i = 0; i < 22050 * 2; i++) { float v = fabsf(out[0][i]); if (v > p2[run]) p2[run] = v; }
        }
        CHECK(fabsf(p2[0] - p2[1]) < 0.01f, "einzige Spur am Kompressor vorbei: %.3f statt %.3f", p2[1], p2[0]);
    }
    printf("  Kompressor: ohne %.3f, alle %.3f, Spur 2 vorbei %.3f\n", pk[0], pk[1], pk[2]);
}

static void testTrackTap() {
    curTest = "Spur-Abgriff";
    static float a[22050 * 2], b[22050 * 2], tap[22050 * 2];
    float eT[2] = {0, 0}, md = 0;
    for (int run = 0; run < 3; run++) {   // 0: ohne, 1: Spur 1 (Kick), 2: Spur 4 (leer)
        static Engine RT[3];
        Engine& X = RT[run];
        X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 120;
        Pattern& P = X.proj.pat[0];
        Engine::initTrack(P, 0, M_KICK); Engine::initTrack(P, 1, M_SNARE);
        for (int t = 2; t < NTRACKS; t++) for (int s2 = 0; s2 < 64; s2++) P.s[t][s2].on = 0;
        for (int s2 = 0; s2 < 16; s2 += 2) { P.s[0][s2].on = 1; P.s[1][s2 + 1].on = 1; }
        X.start();
        float* o = run ? b : a;
        for (int i = 0; i < 22050; i += 256) {
            int n = 22050 - i < 256 ? 22050 - i : 256;
            if (run) { X.tapTrack = run == 1 ? 0 : 3; X.tapOut = tap + i * 2; }
            X.render(o + i * 2, n);
            X.tapOut = nullptr; X.tapTrack = -1;
        }
        if (run) { float e = 0; for (int i = 0; i < 22050 * 2; i++) e += fabsf(tap[i]); eT[run - 1] = e; }
        if (run) for (int i = 0; i < 22050 * 2; i++) md = fmaxf(md, fabsf(a[i] - b[i]));
    }
    printf("  Spur-Abgriff: Kick %.0f, leere Spur %.0f, Master-Abweichung %.1e\n", eT[0], eT[1], md);
    CHECK(eT[0] > 10.f && eT[1] == 0.f, "Abgriff falsch (Kick %.0f, leer %.0f)", eT[0], eT[1]);
    CHECK(md < 1e-4f, "Master aendert sich durch den Abgriff (%.1e)", md);
}

// Time-Warp: Sample von 2 s (= 16 Steps bei 120 BPM) bei 240 BPM -> ~1 s lang, Tonhoehe bleibt (Nulldurchgaenge)
static void testWarp() {
    curTest = "Time-Warp";
    static Engine X; static float out[512];
    Sample sm; memset(&sm, 0, sizeof sm);
    snprintf(sm.name, sizeof sm.name, "test-warp");
    sm.rate = 44100; sm.len = 88200; sm.data = (float*)malloc(sizeof(float) * sm.len);
    for (int i = 0; i < sm.len; i++) sm.data[i] = 0.5f * sinf(6.2831853f * 220.f * i / 44100.f);
    const int SL = 200;
    poolSwap(SL, sm); sampleFree(sm);
    for (int run = 0; run < 2; run++) {
        X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 240;
        Pattern& P = X.proj.pat[0];
        Engine::initTrack(P, 6, M_SAMPLE);
        for (int t = 0; t < NTRACKS; t++) for (int s2 = 0; s2 < 64; s2++) P.s[t][s2].on = 0;
        P.s[6][0].on = 1; P.len[6] = 64; P.p[6][P_LEN] = 127;
        P.p[6][P_S0] = SL; P.p[6][P_X0] = run; P.p[6][P_X1] = 16; P.p[6][P_ASUS] = 127;
        X.start();
        int lastActive = 0, zc = 0, frames = 0; float prev = 0, pk = 0;
        for (int i = 0; i < 44100 * 3; i += 256) {
            X.render(out, 256);
            if (X.v[6].active) lastActive = i + 256;
            if (i > 4410 && i < 30870) for (int k = 0; k < 256; k++) { float y = out[k * 2]; if ((y > 0) != (prev > 0)) zc++; prev = y; frames++; pk = fmaxf(pk, fabsf(y)); }
        }
        float hz = zc / 2.f / (frames / 44100.f);
        printf("  Warp %s: klingt %.2f s, Tonhoehe %.0f Hz, Spitze %.2f\n", run ? "AN " : "AUS", lastActive / 44100.f, hz, pk);
        if (run) {
            CHECK(lastActive > 44100 * 0.85f && lastActive < 44100 * 1.15f, "Warp: Dauer %.2f s statt ~1 s", lastActive / 44100.f);
            CHECK(hz > 200.f && hz < 240.f, "Warp: Tonhoehe %.0f Hz statt ~220", hz);
            CHECK(pk > 0.1f, "Warp: zu leise (%.2f)", pk);
        } else CHECK(lastActive > 44100 * 1.8f, "ohne Warp: Dauer %.2f s statt 2 s", lastActive / 44100.f);
    }
    Sample e; memset(&e, 0, sizeof e); poolSwap(SL, e); sampleFree(e);
}

// LFO laeuft frei weiter, auch wenn die Stimme zwischen kurzen Noten stumm ist
static void testLfoFreeRun() {
    curTest = "LFO frei";
    static Engine X; static float out[512];
    X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 120;
    Pattern& P = X.proj.pat[0];
    Engine::initTrack(P, 4, M_SYNTH);
    for (int t = 0; t < NTRACKS; t++) for (int s2 = 0; s2 < 64; s2++) P.s[t][s2].on = 0;
    for (int s2 = 0; s2 < 16; s2 += 4) P.s[4][s2].on = 1;
    P.p[4][P_LEN] = 32;
    P.p[4][P_L1 + LF_DEST] = P_FREQ + 1; P.p[4][P_L1 + LF_DEPTH] = 120; P.p[4][P_L1 + LF_RATE] = 70;
    P.p[4][P_L1 + LF_MODE] = LM_FREE; P.p[4][P_L1 + LF_SYNC] = 0;
    X.start();
    int idle = 0; float moved = 0, prev = -1;
    for (int i = 0; i < 44100 * 3; i += 256) {
        X.render(out, 256);
        float ph = X.v[4].lfPh[0];
        if (!X.v[4].active && prev >= 0) { idle++; float d = ph - prev; if (d < 0) d += 1.f; moved += d; }
        prev = ph;
    }
    printf("  LFO frei: %d stumme Bloecke, Phase dabei um %.2f Zyklen weiter\n", idle, moved);
    CHECK(idle > 50 && moved > 1.f, "LFO steht zwischen den Noten still (%.2f Zyklen in %d Bloecken)", moved, idle);
}

static void testMultiRoute() {
    curTest = "USB-Routing";
    static float out[44100 * 2], a[44100 * 8], b[44100 * 8];
    for (int run = 0; run < 2; run++) {
        static Engine RT[2];   // zwei frische Engines mit gleicher Vorgeschichte -> gleicher Klang
        Engine& X = RT[run];
        X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 120;
        Pattern& P = X.proj.pat[0];
        Engine::initTrack(P, 0, M_KICK); Engine::initTrack(P, 1, M_SNARE);
        for (int s2 = 0; s2 < 16; s2 += 2) { P.s[0][s2].on = 1; P.s[1][s2 + 1].on = 1; }
        for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = -1;
        if (run == 0) { X.multiRoute[0] = 0; X.multiRoute[1] = 1; } else { X.multiRoute[0] = 2; X.multiRoute[1] = 2; }
        X.multiCh = 8; X.start();
        float* m = run ? b : a;
        for (int i = 0; i < 44100; i += 256) { X.multiBuf = m + (size_t)i * 8; X.render(out + i * 2, 44100 - i < 256 ? 44100 - i : 256); }
        X.multiBuf = nullptr; X.multiCh = 2;
        for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = -1;
    }
    float md = 0, e = 0;
    for (int i = 0; i < 44100; i++) for (int c = 0; c < 2; c++) {
        float sum = a[i * 8 + 2 + c] + a[i * 8 + 4 + c];
        float d = fabsf(b[i * 8 + 6 + c] - sum); if (d > md) md = d; e += fabsf(sum);
    }
    float rest = 0; for (int i = 0; i < 44100; i++) for (int c = 2; c < 6; c++) rest += fabsf(b[i * 8 + c]);
    printf("  USB-Routing: Summe auf einem Paar Abweichung %.2e (Energie %.0f), andere Paare %.2e\n", md, e, rest);
    CHECK(e > 1.f && md < 1e-4f, "zwei Spuren auf einem Paar ergeben nicht die Summe (%.2e)", md);
    CHECK(rest == 0.f, "nicht belegte Paare sind nicht still");
}

// 8 Kanaele: Master + FX A/B/C - die Bus-Kanaele muessen den Effektanteil tragen
static void testMultiOut8() {
    curTest = "Mehrkanal 8";
    static float out[44100 * 2], mb[44100 * 8];
    Engine& X = MC[0];
    X.init(44100); X.stop(); X.audioThreads = 1;
    X.defaultProject(false); X.proj.bpm = 140;
    Pattern& P = X.proj.pat[0];
    for (int t = 0; t < 4; t++) { Engine::initTrack(P, t, t ? M_SNARE : M_KICK); for (int s2 = 0; s2 < 16; s2 += 4) P.s[t][s2].on = 1; P.p[t][P_DLY] = 60; P.p[t][P_REV] = 60; P.p[t][P_SNDC] = 60; }
    X.multiCh = 8; X.start();
    for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = (signed char)(q >= NTRACKS ? q - NTRACKS : -1);
    for (int i = 0; i < 44100; i += 256) { X.multiBuf = mb + (size_t)i * 8; X.render(out + i * 2, 44100 - i < 256 ? 44100 - i : 256); }
    X.multiBuf = nullptr; X.multiCh = 2;
    for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = -1;
    float e[8] = {0};
    for (int i = 0; i < 44100; i++) for (int c = 0; c < 8; c++) e[c] += fabsf(mb[i * 8 + c]);
    printf("  Mehrkanal 8: Energie je Kanal %.0f %.0f | A %.0f %.0f | B %.0f %.0f | C %.0f %.0f\n", e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]);
    for (int c = 2; c < 8; c++) CHECK(e[c] > 1.f, "8 Kanaele: Kanal %d still", c + 1);
}

// 24 Kanaele (Host-Modus, z. B. MPC als Interface): jede Spur und jeder FX-Bus auf einem eigenen Paar
static void testMultiOut24() {
    curTest = "Mehrkanal 24";
    static float out[22050 * 2], mb[22050 * 24];
    Engine& X = MC[0];
    X.init(44100); X.stop(); X.audioThreads = 1;
    X.defaultProject(false); X.proj.bpm = 140;
    Pattern& P = X.proj.pat[0];
    static const int ms[NTRACKS] = {M_KICK, M_SNARE, M_HAT, M_CLAP, M_SYNTH, M_FM, M_WTBL, M_TOM};
    for (int t = 0; t < NTRACKS; t++) { Engine::initTrack(P, t, ms[t]); for (int s2 = 0; s2 < 16; s2 += 2) P.s[t][s2].on = 1; P.p[t][P_DLY] = 60; P.p[t][P_REV] = 60; P.p[t][P_SNDC] = 60; }
    X.multiCh = 24; X.start();
    for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = (signed char)q;
    for (int i = 0; i < 22050; i += 256) { X.multiBuf = mb + (size_t)i * 24; X.render(out + i * 2, 22050 - i < 256 ? 22050 - i : 256); }
    X.multiBuf = nullptr; X.multiCh = 2;
    for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = -1;
    float e[24] = {0};
    for (int i = 0; i < 22050; i++) for (int c = 0; c < 24; c++) e[c] += fabsf(mb[i * 24 + c]);
    int quiet = 0; for (int c = 0; c < 24; c += 2) if (e[c] + e[c + 1] <= 1.f) { quiet++; printf("  Kanal %d-%d still\n", c + 1, c + 2); }   // Paare: Ping-Pong-Delay ist rechts erst spaeter
    printf("  Mehrkanal 24: Master %.0f, Spur 1 %.0f, Spur 8 %.0f, FX C %.0f\n", e[0], e[2], e[16], e[22]);
    CHECK(quiet == 0, "24 Kanaele: %d Paare still", quiet);
}

// Mehrkanal-Ausgang (USB): Master gleich wie ohne Mehrkanal, jede Spur einzeln hoerbar - auf 1 und 2 Kernen
static void testMultiOut() {
    curTest = "Mehrkanal";
    static float ref[44100 * 2], out[44100 * 2], mb[44100 * 24];
    static const int ms[NTRACKS] = {M_KICK, M_SNARE, M_HAT, M_CLAP, M_SYNTH, M_FM, M_WTBL, M_SYNTH};
    // 0: Referenz ohne Mehrkanal (MC[0]), 1: Mehrkanal 1 Kern (MC[1], gleiche Vorgeschichte -> gleicher Klang), 2: Mehrkanal 2 Kerne (MC[0], nur Kanaele)
    for (int run = 0; run < 3; run++) {
        Engine& X = MC[run == 1 ? 1 : 0];
        if (run == 2 && !X.helperReady()) { printf("  Mehrkanal: nur ein Kern - 2-Kern-Lauf uebersprungen\n"); break; }
        X.init(44100); X.stop(); X.audioThreads = run == 2 ? 2 : 1;
        X.defaultProject(false); X.proj.bpm = 140;
        Pattern& P = X.proj.pat[0];
        for (int t = 0; t < NTRACKS; t++) { Engine::initTrack(P, t, ms[t]); for (int s2 = 0; s2 < 16; s2 += 2) P.s[t][s2].on = 1; P.p[t][P_PAN] = (uint8_t)(t * 16); P.p[t][P_REV] = 40; }
        X.multiBuf = run ? mb : nullptr; X.multiCh = run ? 24 : 2;
        for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = (signed char)q;   // jede Quelle ein eigenes Paar
        X.start();
        float* o = run ? out : ref;
        for (int i = 0; i < 44100; i += 256) { if (run) X.multiBuf = mb + (size_t)i * 24; X.render(o + i * 2, 44100 - i < 256 ? 44100 - i : 256); }
        X.multiBuf = nullptr; X.multiCh = 2;
        for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = -1;
        if (!run) continue;
        float md = 0, mm = 0;
        for (int i = 0; i < 44100 * 2; i++) { float d = fabsf(out[i] - ref[i]); if (d > md) md = d; }
        for (int i = 0; i < 44100; i++) for (int c = 0; c < 2; c++) { float d = fabsf(mb[i * 24 + c] - out[i * 2 + c]); if (d > mm) mm = d; }
        int silent = 0; float busE = 0;
        for (int t = 0; t < NTRACKS; t++) { float e = 0; for (int i = 0; i < 44100; i++) e += fabsf(mb[i * 24 + 2 + t * 2]) + fabsf(mb[i * 24 + 3 + t * 2]); if (e < 1.f) silent++; }
        for (int i = 0; i < 44100; i++) for (int c = 18; c < 24; c++) busE += fabsf(mb[i * 24 + c]);
        printf("  Mehrkanal (%d Kern%s): Abweichung zur Referenz %.2e, Kanal 1-2 = Ausgang %.2e, stumme Spuren %d, Bus-Energie %.0f\n", run, run > 1 ? "e" : "", md, mm, silent, busE);
        if (run == 1) CHECK(md < 1e-4f, "Mehrkanal veraendert den Master (%.2e)", md);
        CHECK(mm == 0.f, "Kanal 1-2 ist nicht der Master");
        CHECK(silent == 0, "%d Spuren fehlen im Mehrkanal-Ausgang", silent);
        CHECK(busE > 1.f, "Effekt-Bus-Kanaele sind still");
    }
}

static void testDeterminism() {
    curTest = "Reproduzierbarkeit";
    static float a[44100 * 2], b[44100 * 2];
    for (int k = 0; k < 2; k++) {
        E.init(44100);
        fresh(120);
        for (int s = 0; s < 16; s += 2) E.proj.pat[0].s[0][s].on = 1;
        E.start();
        E.render(k ? b : a, 22050);
    }
    CHECK(!memcmp(a, b, sizeof(float) * 44100), "zwei gleiche Durchlaeufe klingen verschieden (Zufall ohne festen Startwert)");
}

// 24-Bit-Ausgabe (WAV/USB): Rundung, Begrenzung, Dither, leise Signale bleiben erhalten, WAV liest sich wieder ein
static void testPcm24(const char* sdir) {
    curTest = "PCM 24 Bit";
    Dither d;
    float maxErr = 0;
    for (int i = 0; i < 20000; i++) {
        float x = sinf(i * 0.0137f) * 0.97f;
        float y = pcmQuant(x, 24, d) / 8388607.f;
        maxErr = fmaxf(maxErr, fabsf(x - y));
    }
    CHECK(maxErr < 3.f / 8388607.f, "24-Bit-Rundung zu ungenau (%.3g)", maxErr);
    CHECK(pcmQuant(1.5f, 24, d) == 8388607 && pcmQuant(-1.5f, 24, d) == -8388608, "24 Bit nicht begrenzt");
    CHECK(pcmQuant(1.5f, 16, d) == 32767 && pcmQuant(-1.5f, 16, d) == -32768, "16 Bit nicht begrenzt");
    uint8_t b3[3];
    put24(b3, -8388608); CHECK(get24(b3) == -8388608, "put24/get24 negativ");
    put24(b3, 1234567); CHECK(get24(b3) == 1234567, "put24/get24 positiv");
    // Stille -> nur Dither (+-1 LSB), mittelwertfrei
    long sum = 0; int amax = 0;
    for (int i = 0; i < 100000; i++) { int v = pcmQuant(0.f, 24, d); sum += v; if (abs(v) > amax) amax = abs(v); }
    CHECK(amax <= 1 && labs(sum) < 2000, "Dither zu stark/unsymmetrisch (max %d, Summe %ld)", amax, sum);
    // Sinus bei -100 dBFS: in 16 Bit fast weg (0,3 LSB), in 24 Bit sauber (84 LSB)
    double sig = 0, err = 0;
    for (int i = 0; i < 44100; i++) {
        float x = 1e-5f * sinf(i * 2.f * 3.14159265f * 997.f / 44100.f);
        float y = pcmQuant(x, 24, d) / 8388607.f;
        sig += (double)x * x; err += (double)(x - y) * (x - y);
    }
    double snr = 10 * log10(sig / err);
    CHECK(snr > 30, "leises Signal in 24 Bit verrauscht (SNR %.1f dB)", snr);
    // WAV schreiben (Stereo, 24 Bit, 48 kHz) und mit dem Sample-Leser wieder einlesen
    char path[600]; snprintf(path, sizeof path, "%s/zz-unit-24bit.wav", sdir);
    FILE* f = fopen(path, "wb");
    if (!f) { printf("  (WAV-Test uebersprungen: %s nicht beschreibbar)\n", sdir); return; }
    enum { N = 4800 };
    static float st[N * 2]; static uint8_t raw[N * 6];
    for (int i = 0; i < N; i++) { st[i * 2] = 0.5f * sinf(i * 0.05f); st[i * 2 + 1] = 0.25f; }
    uint8_t h[44]; wavHeader(h, 48000, 2, 24, N);
    pcmTo24(raw, st, N * 2, d);
    fwrite(h, 1, 44, f); fwrite(raw, 6, N, f); fclose(f);
    Sample s;
    bool ok = sampleRead("zz-unit-24bit", s);
    remove(path);
    CHECK(ok, "24-Bit-WAV nicht lesbar");
    if (!ok) return;
    CHECK(s.rate == 48000 && s.len == N, "WAV-Kopf falsch (Rate %d, Laenge %d)", s.rate, s.len);
    float me = 0;
    CHECK(s.dataR != nullptr, "Stereo-WAV wurde mono geladen");
    for (int i = 0; i < s.len && i < N; i++) me = fmaxf(me, fmaxf(fabsf(s.data[i] - st[i * 2]), s.dataR ? fabsf(s.dataR[i] - st[i * 2 + 1]) : 1.f));
    CHECK(me < 1e-5f, "24-Bit-WAV liest sich anders zurueck (%.3g)", me);
    sampleFree(s);
}

int main(int argc, char** argv) {
    const char* sdir = argc > 1 ? argv[1] : "samples";
    printf("Samples: %d aus %s\n", samplesLoad(sdir), sdir);
    E.init(44100);
    testTiming();
    testPolymeter();
    testProbCond();
    testLastChg();
    testEuclid();
    testTrigless();
    testSong();
    testClipMatrix();
    testArp();
    testChord();
    testChordNotes();
    testVoiceMode();
    testPercDrone();
    testFactory();
    testStability();
    testDecay();
    testFxBus();
    testSaveLoad();
    testDeterminism();
    testMultiCore();
    testMultiOut();
    testMultiOut8();
    testMultiOut24();
    testMultiRoute();
    testLfoFreeRun();
    testModTargets();
    testWarp();
    testTrackTap();
    testShimmerCloud();
    testLevels19();
    testCmpRoute();
    testInsDelay();
    testStereoSample();
    testLoop();
    testPcm24(sdir);
    printf("%s: %d Pruefungen ok, %d Fehler\n", nFail ? "FEHLGESCHLAGEN" : "OK", nPass, nFail);
    return nFail ? 1 : 0;
}
