// Sync der Handy-App mit dem Geraet. Die App (https) darf nicht direkt mit http://<geraet> sprechen; sie oeffnet
// deshalb http://<geraet>/sync in einem eigenen Fenster und tauscht Dateien per postMessage aus (src/websync.cpp).
// Richtung: Handy -> Geraet, Geraet -> Handy oder beide (geaenderte Seite gewinnt, beide geaendert = beide behalten).
// Ohne Verbindung: alles als ZIP sichern / ein ZIP einspielen (auch am Geraet unter /sync).
'use strict';
window.GB_SYNC = (function () {
  const ROOTS = { projects: ['.gbx', 0], presets: ['.gbp', 1], samples: ['.wav', 8] };
  const css = `
#gbs{position:fixed;inset:0;z-index:20;background:#080612f2;display:none;overflow:auto;padding:max(14px,env(safe-area-inset-top)) 14px 20px;font:14px/1.5 ui-monospace,Menlo,Consolas,monospace;color:#d6eeff}
#gbs.on{display:block}#gbs .card{background:#141026;border:1px solid #3a2f60;border-radius:14px;padding:14px;margin:0 auto 12px;max-width:620px}
#gbs h2{margin:0 0 8px;font-size:16px;letter-spacing:.15em;color:#fff}#gbs p{margin:6px 0;color:#8a80b0}
#gbs button,#gbs .fl{display:inline-block;background:#20183a;color:#d6eeff;border:1px solid #3a2f60;border-radius:10px;font:inherit;padding:9px 14px;margin:6px 6px 0 0;cursor:pointer}
#gbs button.go{background:#ff2e88;border-color:#ff2e88;color:#fff;font-weight:700}#gbs button:disabled{opacity:.45}
#gbs input[type=text]{background:#080612;color:#d6eeff;border:1px solid #3a2f60;border-radius:8px;font:inherit;padding:7px 9px;width:12em}
#gbs label{color:#8a80b0;margin-right:12px}#gbs progress{width:100%;height:10px;accent-color:#ff2e88}
#gbsLog{white-space:pre-wrap;font-size:13px;max-height:34vh;overflow:auto}#gbsLog .ok{color:#2bff9a}#gbsLog .bad{color:#ff4f6a}#gbsLog .hi{color:#00e0ff}
#gbsX{float:right}`;
  let el = null, busy = false;

  function ui() {
    if (el) return el;
    const st = document.createElement('style'); st.textContent = css; document.head.appendChild(st);
    el = document.createElement('div'); el.id = 'gbs';
    el.innerHTML = `
<div class="card"><button id="gbsX">✕</button><h2>SYNC MIT DEM GERÄT</h2>
<p>R36S per USB anschließen (SET → USB-Modus = PC) oder ins selbe WLAN. Es öffnet sich ein zweites Fenster vom Gerät – dort „Erlauben“ tippen.
Hält das Handy ein Fenster an, einfach zwischen beiden hin- und herwechseln.</p>
<div>Gerät <input type="text" id="gbsIp" autocomplete="off" autocapitalize="off" spellcheck="false"></div>
<div style="margin-top:6px"><label><input type="checkbox" id="gbsSmp" checked> Samples</label><label><input type="checkbox" id="gbsRec"> Aufnahmen (rec)</label></div>
<button class="go" data-dir="up">Handy → Gerät</button><button class="go" data-dir="down">Gerät → Handy</button><button class="go" data-dir="both">Beides abgleichen</button>
<p style="font-size:12px">Handy → Gerät: das Gerät bekommt alle Stände vom Handy. Gerät → Handy: umgekehrt. Beides: Neues wird in beide Richtungen kopiert,
wurde eine Datei auf beiden Seiten geändert, bleiben beide erhalten („Name (Handy)“). Gelöscht wird nichts.</p></div>
<div class="card"><h2>OHNE VERBINDUNG</h2><p>Alles als ZIP sichern (Dateien-App, AirDrop …) oder ein ZIP einspielen – z. B. eins, das am Gerät unter /sync heruntergeladen wurde.</p>
<button id="gbsZip">Als ZIP sichern</button><label class="fl">ZIP einspielen<input type="file" id="gbsZipIn" accept=".zip,application/zip" hidden></label></div>
<div class="card"><progress id="gbsPr" max="1" value="0" hidden></progress><div id="gbsLog"></div></div>`;
    document.body.appendChild(el);
    const ip = el.querySelector('#gbsIp');
    try { ip.value = localStorage.gbSyncIp || '192.168.77.1'; } catch (e) { ip.value = '192.168.77.1'; }
    el.querySelector('#gbsX').onclick = () => { if (!busy) el.classList.remove('on'); };
    el.querySelectorAll('[data-dir]').forEach(b => b.onclick = () => run(b.dataset.dir));
    el.querySelector('#gbsZip').onclick = zipOut;
    el.querySelector('#gbsZipIn').onchange = zipIn;
    return el;
  }
  function log(t, cls) { const d = document.createElement('div'); if (cls) d.className = cls; d.textContent = t; const L = el.querySelector('#gbsLog'); L.appendChild(d); L.scrollTop = 1e9; }
  function prog(v) { const p = el.querySelector('#gbsPr'); p.hidden = v < 0; if (v >= 0) p.value = v; }
  function lock(on) { busy = on; el.querySelectorAll('button,input').forEach(b => { if (b.id !== 'gbsX') b.disabled = on; }); }

  // ---------------------------------------------------------------- Dateien im Handy (/data)
  const FSm = () => Module.FS;
  function localList(withSmp, withRec) {
    const out = [];
    for (const r in ROOTS) {
      if (r === 'samples' && !withSmp) continue;
      const [ext, maxD] = ROOTS[r];
      const walk = (sub, d) => {
        let names; try { names = FSm().readdir('/data/' + r + (sub ? '/' + sub : '')); } catch (e) { return; }
        for (const n of names) {
          if (n[0] === '.') continue;
          const rel = sub ? sub + '/' + n : n, st = FSm().stat('/data/' + r + '/' + rel);
          if (FSm().isDir(st.mode)) { if (d < maxD && (withRec || d > 0 || r !== 'samples' || n !== 'rec')) walk(rel, d + 1); }
          else if (n.toLowerCase().endsWith(ext)) out.push({ r, p: rel, s: st.size, t: st.mtime.getTime() });
        }
      };
      walk('', 0);
    }
    return out;
  }
  function mkdirs(path) { const parts = path.split('/'); let cur = ''; for (let i = 1; i < parts.length - 1; i++) { cur += '/' + parts[i]; try { FSm().mkdir(cur); } catch (e) {} } }
  function writeLocal(r, p, data) { const f = '/data/' + r + '/' + p; mkdirs(f); FSm().writeFile(f, new Uint8Array(data)); }
  function readLocal(r, p) { return FSm().readFile('/data/' + r + '/' + p).buffer; }
  function sameBytes(a, b) { if (a.byteLength !== b.byteLength) return false; const x = new Uint8Array(a), y = new Uint8Array(b); for (let i = 0; i < x.length; i++) if (x[i] !== y[i]) return false; return true; }
  function altName(p) { const i = p.lastIndexOf('.'); return p.slice(0, i) + ' (Handy)' + p.slice(i); }
  const key = f => f.r + '/' + f.p;

  // ---------------------------------------------------------------- Abgleich
  function stateLoad(ip) { try { return JSON.parse(localStorage['gbSync:' + ip] || '{}'); } catch (e) { return {}; } }
  function stateSave(ip, s) { try { localStorage['gbSync:' + ip] = JSON.stringify(s); } catch (e) {} }

  // Plan: welche Dateien hoch (put), welche runter (get), welche lokal umbenennen (Konflikt bei "beide")
  function makePlan(dir, local, remote, state) {
    const L = new Map(local.map(f => [key(f), f])), R = new Map(remote.map(f => [key(f), f]));
    const put = [], get = [], check = [], rename = [];
    const all = new Set([...L.keys(), ...R.keys()]);
    for (const k of all) {
      const l = L.get(k), r = R.get(k), s = state[k];
      if (l && !r) { if (dir !== 'down') put.push(l); continue; }
      if (r && !l) { if (dir !== 'up') get.push(r); continue; }
      const lCh = !s || s.ls !== l.s || s.lt !== l.t, rCh = !s || s.ds !== r.s || s.dt !== r.t;
      if (!lCh && !rCh) continue;
      if (dir === 'up') { if (lCh || l.s !== r.s) put.push(l); continue; }
      if (dir === 'down') { if (rCh || l.s !== r.s) get.push(r); continue; }
      if (s && lCh && !rCh) { put.push(l); continue; }
      if (s && rCh && !lCh) { get.push(r); continue; }
      // beide geaendert oder noch nie abgeglichen: gleich gross -> Inhalt vergleichen (holen), sonst beide behalten
      if (l.s === r.s) check.push(r); else rename.push(l);
    }
    return { put, get, check, rename };
  }

  // Fenster zum Geraet: Nachrichten mit Antwort (wartet auch, wenn das Fenster gerade angehalten ist)
  function bridge(ip) {
    let w = null, origin = 'http://' + ip, waiters = [];
    const onMsg = e => { if (e.origin !== origin || e.source !== w) return; const m = e.data; if (!m || !m.t) return; const ws = waiters; waiters = []; ws.forEach(f => f(m)); };
    addEventListener('message', onMsg);
    const next = (pred, ms) => new Promise((res, rej) => {
      const tm = setTimeout(() => rej(new Error('Keine Antwort vom Gerät')), ms);
      const f = m => { if (m.t === 'error') { clearTimeout(tm); rej(new Error(m.text)); } else if (m.t === 'denied') { clearTimeout(tm); rej(new Error('Am Gerät abgelehnt')); } else if (pred(m)) { clearTimeout(tm); res(m); } else waiters.push(f); };
      waiters.push(f);
    });
    return {
      async open() {
        w = window.open(origin + '/sync', 'gbsync');
        if (!w) throw new Error('Fenster wurde blockiert – Popups für diese App erlauben');
        let n = 0; const hello = setInterval(() => { try { w.postMessage({ t: 'hello', again: n++ > 0 }, origin); } catch (e) {} }, 500);
        try { await next(m => m.t === 'ready', 120000); } finally { clearInterval(hello); }
      },
      async ask(msg, reply, tr, ms) { w.postMessage(msg, origin, tr || []); return next(m => m.t === reply, ms || 600000); },
      done(text) { try { w.postMessage({ t: 'done', text }, origin); } catch (e) {} removeEventListener('message', onMsg); },
      close() { removeEventListener('message', onMsg); },
    };
  }

  async function run(dir) {
    const ip = el.querySelector('#gbsIp').value.trim().replace(/^https?:\/\//, '').replace(/\/.*$/, '');
    if (!ip) return;
    try { localStorage.gbSyncIp = ip; } catch (e) {}
    const withSmp = el.querySelector('#gbsSmp').checked, withRec = el.querySelector('#gbsRec').checked;
    el.querySelector('#gbsLog').textContent = ''; lock(true); prog(0);
    const b = bridge(ip);
    try {
      GB_LOCAL.saveNow();
      log('Verbinde mit ' + ip + ' … (im Gerät-Fenster „Erlauben“)', 'hi');
      await b.open();
      log('Verbunden', 'ok');
      let remote = (await b.ask({ t: 'manifest', rec: withRec }, 'manifest')).files;
      if (!withSmp) remote = remote.filter(f => f.r !== 'samples');
      const state = stateLoad(ip), local = localList(withSmp, withRec);
      const plan = makePlan(dir, local, remote, state);
      // Konflikte (beide Seiten geaendert): Handy-Version als "Name (Handy)" behalten und mit hochladen
      const doRename = l => { const np = altName(l.p); FSm().rename('/data/' + l.r + '/' + l.p, '/data/' + l.r + '/' + np); const st = FSm().stat('/data/' + l.r + '/' + np); const nf = { r: l.r, p: np, s: st.size, t: st.mtime.getTime() }; plan.put.push(nf); plan.get.push(remote.find(f => key(f) === key(l))); log('Beide geändert: ' + l.p + ' → Handy-Stand heißt jetzt „' + np + '“', 'hi'); };
      plan.rename.forEach(doRename);
      log(`Plan: ${plan.put.length} hoch, ${plan.get.length + plan.check.length} runter`);
      let res = await b.ask({ t: 'plan', rec: withRec, put: plan.put.map(f => ({ r: f.r, p: f.p, data: readLocal(f.r, f.p) })), get: [...plan.get, ...plan.check].map(f => ({ r: f.r, p: f.p })) }, 'result');
      // Ergebnis schreiben; "check": Inhalt gleich -> nichts, verschieden -> Konflikt (Handy-Stand umbenennen, Geraete-Stand uebernehmen)
      const checkKeys = new Set(plan.check.map(key)), extraPut = [];
      let wrote = 0;
      const cur = Module.UTF8ToString(Module._gbProjectName());
      let curChanged = false;
      for (const g of res.got) {
        if (checkKeys.has(key(g))) {
          let mine; try { mine = readLocal(g.r, g.p); } catch (e) { mine = null; }
          if (mine && sameBytes(mine, g.data)) continue;
          if (mine) { const np = altName(g.p); FSm().rename('/data/' + g.r + '/' + g.p, '/data/' + g.r + '/' + np); extraPut.push({ r: g.r, p: np, data: readLocal(g.r, np) }); log('Beide geändert: ' + g.p + ' → Handy-Stand heißt jetzt „' + np + '“', 'hi'); }
        }
        writeLocal(g.r, g.p, g.data); wrote++;
        if (g.r === 'projects' && g.p === cur + '.gbx') curChanged = true;
      }
      if (extraPut.length) res = await b.ask({ t: 'plan', rec: withRec, put: extraPut, get: [] }, 'result');
      // Zustand merken (fuer "geaendert seit dem letzten Sync")
      let remoteNow = res.files; if (!withSmp) remoteNow = remoteNow.filter(f => f.r !== 'samples');
      const localNow = localList(withSmp, withRec), Rn = new Map(remoteNow.map(f => [key(f), f]));
      for (const l of localNow) { const r = Rn.get(key(l)); if (r) state[key(l)] = { ls: l.s, lt: l.t, ds: r.s, dt: r.t }; }
      stateSave(ip, state);
      const up = plan.put.length + extraPut.length;
      const text = `Fertig: ${up} aufs Gerät, ${wrote} aufs Handy`;
      b.done(text);
      GB_LOCAL.persist();
      if (wrote) { Module.ccall('gbFilesChanged', null, ['string'], ['SYNC: ' + wrote + ' DATEIEN VOM GERÄT']); if (curChanged) Module._gbReloadProject(); }
      log(text, 'ok'); prog(1);
    } catch (e) {
      b.close();
      log('Fehler: ' + e.message, 'bad');
      if (/blockiert|Antwort/.test(e.message)) log('Tipp: Gerät erreichbar? Im Browser http://' + ip + '/sync öffnen – dort geht auch der ZIP-Weg.');
      prog(-1);
    }
    lock(false);
  }

  // ---------------------------------------------------------------- ZIP (ohne Kompression schreiben, lesen auch mit Deflate)
  const CRC = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
  function crc32(u) { let c = 0xffffffff; for (let i = 0; i < u.length; i++) c = CRC[(c ^ u[i]) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; }
  function zipBuild(list) {
    const te = new TextEncoder(), parts = [], cen = []; let off = 0;
    for (const f of list) {
      const n = te.encode(f.name), d = new Uint8Array(f.data), c = crc32(d), h = new DataView(new ArrayBuffer(30));
      h.setUint32(0, 0x04034b50, true); h.setUint16(4, 20, true); h.setUint16(6, 0x0800, true); h.setUint16(12, 0x21, true); h.setUint32(14, c, true); h.setUint32(18, d.length, true); h.setUint32(22, d.length, true); h.setUint16(26, n.length, true);
      parts.push(h.buffer, n, d);
      const ce = new DataView(new ArrayBuffer(46)); ce.setUint32(0, 0x02014b50, true); ce.setUint16(4, 20, true); ce.setUint16(6, 20, true); ce.setUint16(8, 0x0800, true); ce.setUint16(14, 0x21, true);
      ce.setUint32(16, c, true); ce.setUint32(20, d.length, true); ce.setUint32(24, d.length, true); ce.setUint16(28, n.length, true); ce.setUint32(42, off, true);
      cen.push(ce.buffer, n); off += 30 + n.length + d.length;
    }
    let cl = 0; cen.forEach(x => cl += x.byteLength);
    const e = new DataView(new ArrayBuffer(22)); e.setUint32(0, 0x06054b50, true); e.setUint16(8, list.length, true); e.setUint16(10, list.length, true); e.setUint32(12, cl, true); e.setUint32(16, off, true);
    return new Blob([...parts, ...cen, e.buffer], { type: 'application/zip' });
  }
  async function zipRead(buf) {
    const v = new DataView(buf), td = new TextDecoder(); let e = -1;
    for (let i = buf.byteLength - 22; i >= Math.max(0, buf.byteLength - 65557); i--) if (v.getUint32(i, true) === 0x06054b50) { e = i; break; }
    if (e < 0) throw new Error('kein ZIP');
    const n = v.getUint16(e + 10, true); let p = v.getUint32(e + 16, true); const out = [];
    for (let k = 0; k < n; k++) {
      if (v.getUint32(p, true) !== 0x02014b50) throw new Error('ZIP beschädigt');
      const m = v.getUint16(p + 10, true), cs = v.getUint32(p + 20, true), nl = v.getUint16(p + 28, true), xl = v.getUint16(p + 30, true), cl = v.getUint16(p + 32, true), lo = v.getUint32(p + 42, true);
      const name = td.decode(new Uint8Array(buf, p + 46, nl)); p += 46 + nl + xl + cl;
      if (name.endsWith('/')) continue;
      const ds = lo + 30 + v.getUint16(lo + 26, true) + v.getUint16(lo + 28, true); let data = buf.slice(ds, ds + cs);
      if (m === 8) { if (!window.DecompressionStream) throw new Error('Gepacktes ZIP: Browser zu alt'); data = await new Response(new Blob([data]).stream().pipeThrough(new DecompressionStream('deflate-raw'))).arrayBuffer(); }
      else if (m !== 0) throw new Error(name + ': Packverfahren ' + m + ' nicht unterstützt');
      out.push({ name, data });
    }
    return out;
  }
  function safePath(r, p) {
    if (!ROOTS[r] || !p.toLowerCase().endsWith(ROOTS[r][0])) return false;
    const segs = p.split('/');
    return segs.length - 1 <= ROOTS[r][1] && segs.every(s => s && s[0] !== '.' && !/[\\:*?"<>|\x00-\x1f]/.test(s));
  }

  async function zipOut() {
    el.querySelector('#gbsLog').textContent = ''; lock(true);
    try {
      GB_LOCAL.saveNow();
      const withRec = el.querySelector('#gbsRec').checked, withSmp = el.querySelector('#gbsSmp').checked;
      const list = localList(withSmp, withRec).map(f => ({ name: f.r + '/' + f.p, data: readLocal(f.r, f.p) }));
      const name = 'synth-city-handy-' + new Date().toISOString().slice(0, 10) + '.zip', blob = zipBuild(list);
      const file = new File([blob], name, { type: 'application/zip' });
      if (navigator.canShare && navigator.canShare({ files: [file] })) {
        try { await navigator.share({ files: [file], title: 'SYNTH CITY' }); log(list.length + ' Dateien geteilt', 'ok'); lock(false); return; }
        catch (e) { if (e.name === 'AbortError') { lock(false); return; } }
      }
      const a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = name; document.body.appendChild(a); a.click();
      setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 60000);
      log(list.length + ' Dateien (' + Math.round(blob.size / 1048576) + ' MB) gesichert', 'ok');
    } catch (e) { log('Fehler: ' + e.message, 'bad'); }
    lock(false);
  }
  async function zipIn(ev) {
    const f = ev.target.files[0]; ev.target.value = ''; if (!f) return;
    el.querySelector('#gbsLog').textContent = ''; lock(true);
    try {
      GB_LOCAL.saveNow();
      const es = await zipRead(await f.arrayBuffer()), cur = Module.UTF8ToString(Module._gbProjectName());
      let ok = 0, skip = 0, curChanged = false;
      for (const x of es) {
        const i = x.name.indexOf('/'), r = x.name.slice(0, i), p = x.name.slice(i + 1);
        if (i < 0 || !safePath(r, p)) { skip++; continue; }
        writeLocal(r, p, x.data); ok++;
        if (r === 'projects' && p === cur + '.gbx') curChanged = true;
      }
      GB_LOCAL.persist();
      if (ok) { Module.ccall('gbFilesChanged', null, ['string'], ['ZIP: ' + ok + ' DATEIEN']); if (curChanged) Module._gbReloadProject(); }
      log(ok + ' Dateien eingespielt' + (skip ? ', ' + skip + ' übersprungen' : ''), 'ok');
    } catch (e) { log('Fehler: ' + e.message, 'bad'); }
    lock(false);
  }

  return { open() { ui().classList.add('on'); }, _plan: makePlan, _zip: { zipBuild, zipRead } };
})();
