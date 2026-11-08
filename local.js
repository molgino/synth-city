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
      const c = audioCtx(); if (c) { M.SDL2 = M.SDL2 || {}; M.SDL2.audioContext = c; }   // SDL nimmt diesen Kontext
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

  // Ton: eigener AudioContext, den SDL uebernimmt (Module.SDL2.audioContext). iOS laesst Ton nur zu, wenn er
  // innerhalb einer Beruehrung startet - SDL versucht es bei Safari aber spaeter per Timer (navigator.userActivation),
  // das blockiert iOS. Deshalb hier: bei jeder Beruehrung resume() + ein stiller Puffer, bis der Kontext laeuft.
  // Stummschalter: audioSession 'playback' (iOS 17+), sonst ein stilles <audio> in Schleife (schaltet iOS auf Wiedergabe).
  let actx = null, silentEl = null;
  function audioCtx() {
    if (!actx) {
      const AC = window.AudioContext || window.webkitAudioContext;
      if (AC) { try { actx = new AC({ latencyHint: 'playback' }); } catch (e) { actx = new AC(); } }
      if (actx) actx.onstatechange = () => { if (actx.state !== 'running') armUnlock(); };
    }
    return actx;
  }
  function silentWav() {   // 0,5 s Stille, 8 kHz Mono 8 Bit
    const n = 4000, b = new Uint8Array(44 + n), v = new DataView(b.buffer), w = (o, t) => { for (let i = 0; i < 4; i++) b[o + i] = t.charCodeAt(i); };
    w(0, 'RIFF'); v.setUint32(4, 36 + n, true); w(8, 'WAVE'); w(12, 'fmt '); v.setUint32(16, 16, true); v.setUint16(20, 1, true); v.setUint16(22, 1, true);
    v.setUint32(24, 8000, true); v.setUint32(28, 8000, true); v.setUint16(32, 1, true); v.setUint16(34, 8, true); w(36, 'data'); v.setUint32(40, n, true); b.fill(128, 44);
    let s = ''; for (let i = 0; i < b.length; i++) s += String.fromCharCode(b[i]);
    return 'data:audio/wav;base64,' + btoa(s);
  }
  function unlockAudio() {
    const c = audioCtx(); if (!c) return;
    try { if (navigator.audioSession) navigator.audioSession.type = 'playback'; } catch (e) {}
    if (!navigator.audioSession && !silentEl) {
      silentEl = document.createElement('audio'); silentEl.src = silentWav(); silentEl.loop = true; silentEl.setAttribute('playsinline', '');
      silentEl.play().catch(() => { silentEl = null; });
    }
    if (c.state !== 'running') { c.resume().catch(() => {}); }
    try { const b = c.createBuffer(1, 1, c.sampleRate), src = c.createBufferSource(); src.buffer = b; src.connect(c.destination); src.start(0); } catch (e) {}
    if (c.state === 'running') disarmUnlock();
  }
  const UNLOCK = ['touchend', 'pointerup', 'click', 'keydown', 'pointerdown'];
  function armUnlock() { UNLOCK.forEach(t => document.addEventListener(t, unlockAudio, true)); }
  function disarmUnlock() { UNLOCK.forEach(t => document.removeEventListener(t, unlockAudio, true)); }
  armUnlock();
  document.addEventListener('visibilitychange', () => { if (!document.hidden && actx && actx.state !== 'running') { actx.resume().catch(() => {}); armUnlock(); } });

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
      let reg = null;
      navigator.serviceWorker.register('sw.js', { updateViaCache: 'none' }).then(r => { reg = r; r.update(); }).catch(() => {});
      // iOS startet Home-Bildschirm-Apps selten neu: auch beim Zurueckkehren nach einer neuen Version schauen
      document.addEventListener('visibilitychange', () => { if (!document.hidden && reg) reg.update().catch(() => {}); });
      // neue Version ist da: gestoppt sofort neu laden (Projekt vorher speichern), beim Spielen nur Hinweis
      navigator.serviceWorker.addEventListener('controllerchange', () => {
        if (!hadSw) return;
        if (!ready || !(M._gbState() & 1)) { saveNow(() => location.reload()); return; }
        M.ccall('gbToast', null, ['string'], ['UPDATE GELADEN - APP NEU STARTEN']);
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
