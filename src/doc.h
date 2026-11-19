// Das Projekt ist ein DOKUMENT: eine Liste aus (stabile ID, Wert), nicht das Abbild einer struct.
// Darum braucht eine neue Version keine Migration: was sie nicht kennt, schreibt sie unveraendert
// zurueck, und was sie vermisst, bekommt seinen Standardwert.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum { DOC_MAX = 24000, DOC_KEY = 96, DOC_SLOTS = 65536 };   // Schluessel wie "pat.0.trk.3.st.12.flt.freq.slide"   // Slots: Zweierpotenz > 2 * DOC_MAX

struct Doc {
    struct E { char k[DOC_KEY]; int32_t v; };
    E e[DOC_MAX];
    int n = 0;
    int32_t idx[DOC_SLOTS];      // offene Adressierung, -1 = frei
    bool overflow = false;       // Datei groesser als der Puffer: nichts still verlieren

    Doc() { clear(); }
    void clear() { n = 0; overflow = false; for (int i = 0; i < DOC_SLOTS; i++) idx[i] = -1; }

    static uint32_t hash(const char* s) {
        uint32_t h = 2166136261u;
        for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
        return h;
    }
    int find(const char* k) const {
        uint32_t i = hash(k) & (DOC_SLOTS - 1);
        for (int step = 0; step < DOC_SLOTS; step++, i = (i + 1) & (DOC_SLOTS - 1)) {
            if (idx[i] < 0) return -1;
            if (!strcmp(e[idx[i]].k, k)) return idx[i];
        }
        return -1;
    }
    bool set(const char* k, int32_t v) {
        int at = find(k);
        if (at >= 0) { e[at].v = v; return true; }
        if (n >= DOC_MAX || strlen(k) >= DOC_KEY) { overflow = true; return false; }
        snprintf(e[n].k, DOC_KEY, "%s", k);
        e[n].v = v;
        uint32_t i = hash(k) & (DOC_SLOTS - 1);
        while (idx[i] >= 0) i = (i + 1) & (DOC_SLOTS - 1);
        idx[i] = n++;
        return true;
    }
    int32_t get(const char* k, int32_t def) const { int at = find(k); return at < 0 ? def : e[at].v; }
    bool has(const char* k) const { return find(k) >= 0; }

    bool save(const char* path) const {
        FILE* f = fopen(path, "w");
        if (!f) return false;
        fprintf(f, "# synthcity 1\n");
        for (int i = 0; i < n; i++) fprintf(f, "%s = %d\n", e[i].k, (int)e[i].v);
        fclose(f);
        return true;
    }
    bool load(const char* path) {
        FILE* f = fopen(path, "r");
        if (!f) return false;
        clear();
        char line[DOC_KEY + 32];
        while (fgets(line, sizeof line, f)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            char* eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            char* k = line;
            while (*k == ' ') k++;
            char* end = eq - 1;
            while (end > k && (*end == ' ' || *end == '\t')) *end-- = 0;
            set(k, (int32_t)strtol(eq + 1, nullptr, 10));
        }
        fclose(f);
        return true;
    }
};

// Schluessel bauen - eine Stelle, damit Schreiben und Lesen nicht auseinanderlaufen.
inline void docKeyTrack(char* out, int nOut, int pat, int trk, const char* id) {
    snprintf(out, nOut, "pat.%d.trk.%d.%s", pat, trk, id + (strncmp(id, "trk.", 4) ? 0 : 4));
}
inline void docKeyStep(char* out, int nOut, int pat, int trk, int step, const char* suffix) {
    snprintf(out, nOut, "pat.%d.trk.%d.st.%d%s%s", pat, trk, step, suffix[0] ? "." : "", suffix);
}
