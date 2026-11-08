// Web-Build: Funktionen fuer die Seite (web/local.js), am Ende von main.cpp eingebunden (Zugriff auf den UI-Zustand)
#include <emscripten.h>

// Projekt + Einstellungen jetzt schreiben (vor dem Sync, beim Wechsel in den Hintergrund)
extern "C" EMSCRIPTEN_KEEPALIVE int gbSaveNow() {
    bool ok = saveProject();
    saveSettings();
    return ok ? 1 : 0;
}

// Name des offenen Projekts (fuer den Sync: wurde es von aussen ersetzt, laedt gbReloadProject es neu)
extern "C" EMSCRIPTEN_KEEPALIVE const char* gbProjectName() { return cfg.project; }

// Offenes Projekt von der Platte neu laden (nach "Geraet -> Handy"), ohne vorher zu speichern
extern "C" EMSCRIPTEN_KEEPALIVE int gbReloadProject() {
    reloadSamples();
    lockA(); eng.stop(); bool ok = eng.load(projPath); unlockA();
    if (ok) { poolLoadForProject(projPath); memcpy(&lastSaved, &eng.proj, sizeof(Project)); }
    return ok ? 1 : 0;
}

// Text in der Kopfzeile des Geraets anzeigen (Meldungen der Seite, z. B. nach dem Sync)
extern "C" EMSCRIPTEN_KEEPALIVE void gbToast(const char* msg) { toast("%s", msg); msgUntil = SDL_GetTicks() + 3000; }

// Zustand fuer Tests/Anzeige: Bit 0 spielt, Bit 1 Song-Modus
extern "C" EMSCRIPTEN_KEEPALIVE int gbState() { return (eng.playing ? 1 : 0) | (eng.songOn ? 2 : 0); }
