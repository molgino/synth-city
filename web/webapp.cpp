// SYNTH CITY im Browser (WebAssembly, Emscripten): Ersatz fuer alles, was es nur auf dem Geraet gibt
// (USB-Gadget, MIDI, Webserver, DRM-Anzeige, Fernbedienung). Eingaben und Bild laufen ueber die Seite
// (web/local.js): Tasten/Touch kommen als Textnachrichten wie beim WebSocket der Fernbedienung ("d A", "t 0 x y"),
// das Bild geht per gbPresent direkt ins Canvas.
#include <SDL.h>
#include <emscripten.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "remote.h"
#include "web.h"
#include "midi.h"
#include "usbaudio.h"
#include "drmout.h"

// ---------------------------------------------------------------- Eingaben (gleiches Format wie remote.cpp)
volatile bool remoteDown[BT_COUNT];
volatile bool remoteHit[BT_COUNT];
volatile float remoteAxis[4];

void remoteInput(const char* name, int value) {
    static const char* const names[BT_COUNT] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y",
                                                "L1", "R1", "L2", "R2", "SELECT", "START"};
    for (int b = 0; b < BT_COUNT; b++)
        if (!strcmp(name, names[b])) { remoteDown[b] = value != 0; if (value) remoteHit[b] = true; return; }
    static const char* const ax[4] = {"LX", "LY", "RX", "RY"};
    for (int a = 0; a < 4; a++)
        if (!strcmp(name, ax[a])) { remoteAxis[a] = value > 0 ? 1.f : (value < 0 ? -1.f : 0.f); return; }
}

static RemoteTouch g_tq[256];
static unsigned g_tqHead = 0, g_tqTail = 0;
static void touchPush(char type, int id, int x, int y) {
    if (g_tqHead - g_tqTail >= 256) return;
    if (type == 'm' && g_tqHead != g_tqTail) { RemoteTouch& l = g_tq[(g_tqHead - 1) & 255]; if (l.type == 'm' && l.id == id) { l.x = x; l.y = y; return; } }
    g_tq[g_tqHead++ & 255] = RemoteTouch{type, id, x, y};
}
bool remoteTouchPop(RemoteTouch& e) {
    if (g_tqHead == g_tqTail) return false;
    e = g_tq[g_tqTail++ & 255];
    return true;
}

extern "C" EMSCRIPTEN_KEEPALIVE void gbMsg(const char* msg) {
    char b[16] = ""; int v = 0, id, x, y;
    if (msg[0] == 'd' && sscanf(msg + 2, "%15s", b) == 1) remoteInput(b, 1);
    else if (msg[0] == 'u' && sscanf(msg + 2, "%15s", b) == 1) remoteInput(b, 0);
    else if (msg[0] == 'a' && sscanf(msg + 2, "%15s %d", b, &v) == 2) remoteInput(b, v);
    else if ((msg[0] == 't' || msg[0] == 'm' || msg[0] == 'e') && msg[1] == ' ' && sscanf(msg + 2, "%d %d %d", &id, &x, &y) == 3) touchPush(msg[0], id, x, y);
    else if (msg[0] == 'c') {   // Seite im Hintergrund: nichts bleibt gedrueckt
        for (int k = 0; k < BT_COUNT; k++) remoteDown[k] = false;
        for (int a = 0; a < 4; a++) remoteAxis[a] = 0.f;
        touchPush('c', 0, 0, 0);
    }
}

// Touch-Knoepfe (Explorer zurueck/abbrechen ...) zeigt die Oberflaeche nur, wenn jemand per Touch bedient: hier immer
bool remoteWanted() { return true; }
void remoteSnapshot(const uint32_t*, int, int, int) {}
size_t remoteEncode(unsigned, uint8_t**, size_t*) { return 0; }
bool remoteWsAccept(int, const char*, bool) { return false; }
void remoteAudioPush(const float*, int, int) {}
bool remoteAudioWanted() { return false; }

// ---------------------------------------------------------------- Bild: Surface (ARGB8888) ins Canvas der Seite
EM_JS(void, gbPresentJs, (const uint32_t* px, int pitch, int w, int h), {
    if (typeof gbFrame === 'function') gbFrame(px >> 2, pitch, w, h);
});
static bool g_drmOpen = false;
bool drmOutOpen() { g_drmOpen = true; return true; }
bool drmOutActive() { return g_drmOpen; }
void drmOutPresent(SDL_Surface* s) { if (s) gbPresentJs((const uint32_t*)s->pixels, s->pitch / 4, s->w, s->h); }
void drmOutClose() {}
void drmOutSetByteOffset(int) {}
void drmOutStats(double* c, double* f) { if (c) *c = 0; if (f) *f = 0; }
bool drmOutSingleBuffer() { return true; }

// ---------------------------------------------------------------- Dateien von aussen (Sync/Import): Samples neu einlesen
static volatile int g_filesChanged = 0;
static char g_evMsg[96] = "";
static int g_evSeq = 0;
extern "C" EMSCRIPTEN_KEEPALIVE void gbFilesChanged(const char* msg) {
    g_filesChanged = 1;
    if (msg && msg[0]) { snprintf(g_evMsg, sizeof g_evMsg, "%s", msg); g_evSeq++; }
}
bool webStart(const char*) { return false; }
int webPort() { return 0; }
bool webTakeChange(Uint32, Uint32) { if (!g_filesChanged) return false; g_filesChanged = 0; return true; }
int webLastEvent(char* out, int n) { snprintf(out, n, "%s", g_evMsg); return g_evSeq; }
bool webUpdateReady() { return false; }

// ---------------------------------------------------------------- MIDI: im Browser (noch) nicht
volatile uint32_t midiActivity = 0;
bool midiStart() { return false; }
void midiStop() {}
bool midiInPop(MidiMsg&) { return false; }
bool midiOutPush(const MidiMsg&) { return false; }
double midiNow() { return emscripten_get_now() / 1000.0; }
int midiPortInfo(char* buf, int n) { if (n > 0) buf[0] = 0; return 0; }
const char* midiBackend() { return "AUS"; }
int hwVolumeSet(int) { return -1; }
int audioJackPoll() { return -1; }
void midiCounts(unsigned* i, unsigned* o) { if (i) *i = 0; if (o) *o = 0; }
uint32_t midiNoteEvents() { return 0; }
uint32_t midiClockEvents() { return 0; }

// ---------------------------------------------------------------- USB-Audio / PC-Eingang: gibt es nur am Geraet
bool usbAudioStart() { return false; }
void usbAudioStop() {}
bool usbAudioStartHost(int) { return false; }
bool usbAudioHost() { return false; }
const char* usbAudioHostName() { return ""; }
void usbAudioLogDevices() {}
bool usbAudioActive() { return false; }
int usbAudioStatus() { return 0; }
int usbAudioRate() { return 0; }
void usbAudioSetGain(float) {}
void usbAudioTestTone(int) {}
void usbAudioPush(const float*, int) {}
void usbAudioSetChannels(int) {}
int usbAudioChannels() { return 2; }
void usbAudioPushMulti(const float*, int) {}
void usbAudioSetEngineRate(int) {}
bool pcInStart() { return false; }
void pcInStop() {}
bool pcInActive() { return false; }
int pcInStatus() { return 0; }
void pcInMix(float*, int, float) {}
bool pcRecStart() { return false; }
void pcRecStop() {}
int pcRecState() { return 0; }
float pcRecSeconds() { return 0.f; }
int pcRecTake(float** d) { if (d) *d = nullptr; return 0; }
int pcRecRate() { return 44100; }
void pcRecDone() {}

int runDhcpd(const char*) { return 1; }
extern "C" int pthread_setschedparam(pthread_t, int, const struct sched_param*) { return -1; }

// Speichern: Dateien liegen im Speicher (MEMFS unter IDBFS), local.js schreibt sie gesammelt nach IndexedDB.
// fsync/sync wuerden mit ASYNCIFY selbst syncfs starten und den Aufrufer schlafen legen (bricht, wenn die Seite
// direkt speichert) - deshalb hier ohne Wirkung.
extern "C" int fsync(int) { return 0; }
extern "C" void sync(void) {}
bool webTakeProjectUpload(char*, int) { return false; }
