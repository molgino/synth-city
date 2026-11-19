// Parameter werden GENAU EINMAL deklariert. Aus dieser Tabelle entstehen spaeter Oberflaeche,
// Plugin-Parameterliste, MIDI-Learn, Hilfe und die Datei - nie dieselbe Angabe an zwei Stellen.
// Ein neuer Parameter ist eine Zeile hier; er wird dann automatisch gespeichert, geladen und
// mit seinem Standardwert vorbelegt (siehe tests/unit.cpp "Projekt ueberlebt neue Parameter").
#pragma once
#include <stdint.h>
#include <string.h>

// FNV-1a: der Name steht als Text in der Datei, die Zahl ist nur die Abkuerzung zur Laufzeit.
constexpr uint32_t pidHash(const char* s, uint32_t h = 2166136261u) {
    return *s ? pidHash(s + 1, (uint32_t)((h ^ (uint8_t)*s) * 16777619u)) : h;
}

enum PScope : uint8_t { PS_GLOBAL, PS_TRACK };
enum PKind  : uint8_t { PK_NUM, PK_BIP, PK_ONOFF, PK_NOTE, PK_LEVEL, PK_TIME, PK_ENUM, PK_COND };
enum : uint8_t { PF_LOCK = 1, PF_MOD = 2 };   // je Step lockbar / als Modulationsziel erlaubt

struct ParamDef {
    const char* id;      // stabile ID - steht so in der Datei, wird nie umbenannt
    const char* name;    // Anzeige
    PScope scope;
    PKind kind;
    int16_t lo, hi, def;
    const char* unit;
    uint8_t flags;
};

// Reihenfolge ist Anzeigereihenfolge; sie darf sich aendern, ohne Dateien zu brechen.
static const ParamDef PARAMS[] = {
    // --- global ---
    {"bpm",          "TEMPO",  PS_GLOBAL, PK_NUM,    30, 300, 120, "BPM", 0},
    {"swing",        "SWING",  PS_GLOBAL, PK_NUM,    50,  80,  50, "%",   0},
    {"master.vol",   "SUMME",  PS_GLOBAL, PK_LEVEL,   0, 127, 100, "dB",  0},
    // --- Spur: Anschlag ---
    {"trk.note",     "NOTE",   PS_TRACK,  PK_NOTE,    0, 127,  60, "",    PF_LOCK},
    {"trk.vel",      "VEL",    PS_TRACK,  PK_NUM,     0, 127, 100, "",    PF_LOCK},
    {"trk.len",      "LEN",    PS_TRACK,  PK_TIME,    1, 127,  12, "",    PF_LOCK},
    {"trk.prob",     "PROB",   PS_TRACK,  PK_NUM,     0, 100, 100, "%",   PF_LOCK},
    {"trk.cond",     "COND",   PS_TRACK,  PK_COND,    0, 255,   0, "",    PF_LOCK},
    {"trk.micro",    "MTIM",   PS_TRACK,  PK_BIP,     0,  48,  24, "",    PF_LOCK},
    {"trk.ratchet",  "RTRG",   PS_TRACK,  PK_NUM,     1,   8,   1, "",    PF_LOCK},
    // --- Spur: Quelle ---
    {"trk.machine",  "ENGINE", PS_TRACK,  PK_ENUM,    0,  15,   0, "",    0},
    {"trk.src.a",    "TUNE",   PS_TRACK,  PK_BIP,     0, 127,  64, "",    PF_LOCK | PF_MOD},
    {"trk.src.b",    "DECAY",  PS_TRACK,  PK_NUM,     0, 127,  64, "",    PF_LOCK | PF_MOD},
    // --- Spur: Filter ---
    {"trk.flt.freq", "FREQ",   PS_TRACK,  PK_NUM,     0, 127, 127, "Hz",  PF_LOCK | PF_MOD},
    {"trk.flt.reso", "RESO",   PS_TRACK,  PK_NUM,     0, 127,   0, "",    PF_LOCK | PF_MOD},
    {"trk.flt.type", "TYPE",   PS_TRACK,  PK_ENUM,    0,  10,   0, "",    PF_LOCK},
    {"trk.flt.env",  "ENV",    PS_TRACK,  PK_BIP,     0, 127,  64, "",    PF_LOCK | PF_MOD},
    // --- Spur: Huellkurve und Ausgang ---
    {"trk.amp.atk",  "ATK",    PS_TRACK,  PK_TIME,    0, 127,   0, "s",   PF_LOCK | PF_MOD},
    {"trk.amp.dec",  "DEC",    PS_TRACK,  PK_TIME,    0, 127,  71, "s",   PF_LOCK | PF_MOD},
    {"trk.amp.sus",  "SUS",    PS_TRACK,  PK_LEVEL,   0, 127,   0, "dB",  PF_LOCK | PF_MOD},
    {"trk.amp.rel",  "REL",    PS_TRACK,  PK_TIME,    0, 127,  40, "s",   PF_LOCK | PF_MOD},
    {"trk.vol",      "VOL",    PS_TRACK,  PK_LEVEL,   0, 127, 100, "dB",  PF_LOCK | PF_MOD},
    {"trk.pan",      "PAN",    PS_TRACK,  PK_BIP,     0, 127,  64, "",    PF_LOCK | PF_MOD},
    {"trk.send.a",   "S.A",    PS_TRACK,  PK_LEVEL,   0, 127,   0, "dB",  PF_LOCK | PF_MOD},
    {"trk.send.b",   "S.B",    PS_TRACK,  PK_LEVEL,   0, 127,   0, "dB",  PF_LOCK | PF_MOD},
};
enum { NPARAMS = (int)(sizeof PARAMS / sizeof *PARAMS) };

// Index innerhalb des Gueltigkeitsbereichs (global bzw. Spur) - so liegen die Werte im Modell.
inline int paramSlot(int i) {
    int k = 0;
    for (int j = 0; j < i; j++) if (PARAMS[j].scope == PARAMS[i].scope) k++;
    return k;
}
inline int paramCount(PScope s) {
    int k = 0;
    for (int i = 0; i < NPARAMS; i++) if (PARAMS[i].scope == s) k++;
    return k;
}
inline int paramFind(const char* id) {
    for (int i = 0; i < NPARAMS; i++) if (!strcmp(PARAMS[i].id, id)) return i;
    return -1;
}
// Spurparameter nach Slot (die Reihenfolge der Tabelle)
inline int paramOfSlot(PScope s, int slot) {
    int k = 0;
    for (int i = 0; i < NPARAMS; i++) if (PARAMS[i].scope == s) { if (k == slot) return i; k++; }
    return -1;
}
