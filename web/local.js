// SYNTH CITY als Handy-App: die Bedienseite (WEB_SCREEN aus src/webpage.cpp) spricht statt mit dem Geraet
// mit der Groovebox in dieser Seite (WebAssembly, synthcity.js). Daten liegen in IndexedDB (Ordner /data).
'use strict';
window.GB_LOCAL = (function () {
  let ready = false, queue = [], msgPtr = 0, syncing = false, syncAgain = false;
  const M = window.Module = {
    canvas: null,   // SDL bekommt ein eigenes, verstecktes Canvas (Bild kommt ueber gbFrame)
    noExitRuntime: true,
    print: t => console.log(t),
    printErr: t => console.warn(t),
    preRun: [() => {   // M ist das Modul selbst (MODULARIZE: createSynthCity(M) fuellt dieses Objekt)
      M.canvas = document.getElementById('gbsdl');
      M.FS.mkdir('/data');
      M.FS.mount(M.FS.filesystems.IDBFS, {}, '/data');
      M.addRunDependency('gb-idb');
      M.FS.syncfs(true, err => { if (err) console.warn('IndexedDB laden:', err); M.removeRunDependency('gb-idb'); });
    }],
    onRuntimeInitialized: () => {
      msgPtr = M._malloc(256);
      ready = true;
      queue.forEach(send); queue = [];
      setStatus('');
    },
  };

  // Speichern: IndexedDB nach jeder Aenderung (IDBFS schreibt nur geaenderte Dateien), Projekt beim Verlassen
  function persist(cb) {
    if (!ready) { if (cb) cb(); return; }
    if (syncing) { syncAgain = true; if (cb) setTimeout(() => persist(cb), 200); return; }
    syncing = true;
    M.FS.syncfs(false, err => {
      syncing = false;
      if (err) console.warn('IndexedDB speichern:', err);
      if (syncAgain) { syncAgain = false; persist(); }
      if (cb) cb(err);
    });
  }
  function saveNow(cb) { if (ready) M._gbSaveNow(); persist(cb); }
  setInterval(persist, 4000);
  document.addEventListener('visibilitychange', () => { if (document.hidden) { if (ready) M._gbMsg && send('c'); saveNow(); } });
  addEventListener('pagehide', () => saveNow());
  if (navigator.storage && navigator.storage.persist) navigator.storage.persist().catch(() => {});

  function send(msg) {
    if (!ready) { if (queue.length < 200) queue.push(msg); return; }
    M.stringToUTF8(msg, msgPtr, 256);
    M._gbMsg(msgPtr);
  }

  // Ton: Browser starten Audio erst nach einer Beruehrung; iPhone: auch bei Stummschalter abspielen
  function unlockAudio() {
    try { if (navigator.audioSession) navigator.audioSession.type = 'playback'; } catch (e) {}
    const sdl = M.SDL2 || window.SDL2;
    if (sdl && sdl.audioContext && sdl.audioContext.state !== 'running') sdl.audioContext.resume();
  }
  ['pointerdown', 'keydown', 'touchend'].forEach(t => document.addEventListener(t, unlockAudio, true));

  function setStatus(t) { const el = document.getElementById('gbload'); if (el) { el.textContent = t; el.style.display = t ? 'flex' : 'none'; } }

  function start() {
    // Kopfzeile: kein Browser-Ton/Dateien-Link (die gibt es nur am Geraet), dafuer Sync
    ['bAud', 'vol'].forEach(id => { const e = document.getElementById(id); if (e) e.style.display = 'none'; });
    const files = document.querySelector('header a[href="/"]');
    if (files) {
      const b = document.createElement('button'); b.className = 'hb'; b.id = 'bSync'; b.textContent = '⇅ Sync';
      b.onclick = () => window.GB_SYNC ? GB_SYNC.open() : alert('Sync kommt gleich');
      files.replaceWith(b);
    }
    const ld = document.createElement('div'); ld.id = 'gbload';
    ld.style.cssText = 'position:absolute;inset:0;display:flex;align-items:center;justify-content:center;color:#d6eeff;background:#080612;z-index:4;font-size:15px';
    ld.textContent = 'SYNTH CITY startet …';
    document.getElementById('cvw').appendChild(ld);
    const s = document.createElement('script'); s.src = 'synthcity.js'; s.async = true;
    s.onload = () => createSynthCity(M).catch(e => setStatus('Fehler beim Start: ' + e));
    s.onerror = () => setStatus('Laden fehlgeschlagen – Seite neu laden');
    document.body.appendChild(s);
    if ('serviceWorker' in navigator && window.isSecureContext) {
      const hadSw = !!navigator.serviceWorker.controller;
      navigator.serviceWorker.register('sw.js').then(r => r.update()).catch(() => {});
      // neue Version im Hintergrund geladen: Bescheid geben (laeuft erst nach dem Neustart der App)
      navigator.serviceWorker.addEventListener('controllerchange', () => {
        if (hadSw && ready) M.ccall('gbToast', null, ['string'], ['UPDATE GELADEN - APP NEU STARTEN']);
      });
    }
  }

  return { start, send, saveNow, persist, isReady: () => ready };
})();

// Bild aus dem WebAssembly-Speicher (ARGB8888) ins Canvas der Bedienseite (px/dirty/frames gehoeren zur Seite)
function gbFrame(p32, pitch, w, h) {
  if (w !== W || h !== H) { W = w; H = h; cv.width = w; cv.height = h; img = cx.createImageData(w, h); px = new Uint32Array(img.data.buffer); }
  const src = Module.HEAPU32;
  for (let y = 0, i = 0; y < h; y++) {
    let s = p32 + y * pitch;
    for (let x = 0; x < w; x++, i++) { const v = src[s++]; px[i] = (v & 0xff00ff00) | ((v >>> 16) & 0xff) | ((v & 0xff) << 16); }
  }
  dirty = true; frames++;
}
