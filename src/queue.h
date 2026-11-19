// Lock-freie Einweg-Warteschlange (ein Schreiber, ein Leser). Die Oberflaeche schiebt Befehle
// hinein, der Audio-Thread holt sie ab - kein Mutex, kein Anhalten des Callbacks.
#pragma once
#include <stdint.h>
#include <atomic>

struct Cmd {
    enum Kind : uint8_t { NONE, PARAM, STEP, LOCK, TRANSPORT } kind = NONE;
    int16_t a = 0, b = 0, c = 0;
    int32_t v = 0;
};

template <int N> struct Queue {     // N muss eine Zweierpotenz sein
    Cmd q[N];
    std::atomic<uint32_t> w{0}, r{0};
    bool push(const Cmd& c) {
        uint32_t wi = w.load(std::memory_order_relaxed);
        if (wi - r.load(std::memory_order_acquire) >= (uint32_t)N) return false;   // voll
        q[wi & (N - 1)] = c;
        w.store(wi + 1, std::memory_order_release);
        return true;
    }
    bool pop(Cmd& out) {
        uint32_t ri = r.load(std::memory_order_relaxed);
        if (ri == w.load(std::memory_order_acquire)) return false;
        out = q[ri & (N - 1)];
        r.store(ri + 1, std::memory_order_release);
        return true;
    }
};
