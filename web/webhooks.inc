// Web-Build: Funktionen fuer die Seite (web/local.js), am Ende von main.cpp eingebunden (Zugriff auf den UI-Zustand)
#include <emscripten.h>

// Projekt + Einstellungen jetzt schreiben (vor dem Sync, beim Wechsel in den Hintergrund)
extern "C" EMSCRIPTEN_KEEPALIVE int gbSaveNow() {   // nur bei Aenderungen (sonst wuerde jede Speicherung als "geaendert" gelten)
    bool ok = true;
    if (projDirty() || !fileExists(projPath)) ok = saveProject();
    saveSettings();
    return ok ? 1 : 0;
}

// Name des offenen Projekts (fuer den Sync: wurde es von aussen ersetzt, laedt gbReloadProject es neu)
extern "C" EMSCRIPTEN_KEEPALIVE const char* gbProjectName() { return cfg.project; }

// Offenes Projekt von der Platte neu laden (nach "Geraet -> Handy"), ohne vorher zu speichern
extern "C" EMSCRIPTEN_KEEPALIVE int gbReloadProject() {
    reloadSamples();
    lockA(); eng.stop(); bool ok = eng.load(projPath); unlockA();
    if (ok) { poolLoadForProject(projPath); markSaved(); }
    return ok ? 1 : 0;
}

// Text in der Kopfzeile des Geraets anzeigen (Meldungen der Seite, z. B. nach dem Sync)
extern "C" EMSCRIPTEN_KEEPALIVE void gbToast(const char* msg) { toast("%s", msg); msgUntil = SDL_GetTicks() + 3000; }

// Zustand fuer Tests/Anzeige: Bit 0 spielt, Bit 1 Song-Modus
extern "C" EMSCRIPTEN_KEEPALIVE int gbState() { return (eng.playing ? 1 : 0) | (eng.songOn ? 2 : 0); }

// Test-Schnittstelle (tests/app/app_scenarios.js): innerer Zustand als JSON
extern "C" EMSCRIPTEN_KEEPALIVE const char* gbInfo() {
    static char b[600];
    const Pattern& P = curP(); int on = 0; for (int s = 0; s < NSTEPS; s++) on += P.s[track][s].on ? 1 : 0;
    char m[80]; snprintf(m, sizeof m, "%s", msg); for (char* c = m; *c; c++) if (*c == '"' || *c == '\\') *c = '\'';
    snprintf(b, sizeof b, "{\"page\":%d,\"pageName\":\"%s\",\"track\":%d,\"slot\":%d,\"cursor\":%d,\"pat\":%d,\"playing\":%d,\"menu\":%d,\"pgm\":%d,\"fb\":%d,\"setIn\":%d,\"steps\":%d,\"newPage\":%d,\"frames\":%lu,\"project\":\"%s\",\"msg\":\"%s\",\"machine\":%d,\"menuSel\":\"%s\",\"ui2\":%d,\"mr\":%d,\"view\":%d}",
             page, PAGE_NAMES[page], track, slot, cursor, curPi(), eng.playing ? 1 : 0, menuOpen ? 1 : 0, pgmOpen ? 1 : 0, fbOpen ? 1 : 0, setIn ? 1 : 0, on, cfg.newPage, drawCount, cfg.project, m, P.machine[track], PAGE_NAMES[menuSel % NUI_PAGES], cfg.ui2, mrOpen ? 1 : 0, v2View);
    return b;
}
