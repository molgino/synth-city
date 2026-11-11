// Szenarien Handy-App (Offline-App im Browser): echte Taps auf die Knoepfe/das Bild wie am Handy, Wirkung ueber gbInfo().
// Aufruf: node tests/app/app_scenarios.js [dist/webapp]   (Playwright + Chromium, dazu ein kleiner Webserver)
'use strict';
const { chromium } = require('playwright');
const http = require('http'), fs = require('fs'), path = require('path');

const dir = path.resolve(process.argv[2] || 'dist/webapp');
const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.json': 'application/json', '.png': 'image/png' };
const results = [];
async function scenario(name, fn) {
  const t0 = Date.now();
  try { await fn(); results.push([true, name, '', Date.now() - t0]); }
  catch (e) { results.push([false, name, String(e && e.message || e).split('\n')[0], Date.now() - t0]); }
}
function assert(c, m) { if (!c) throw new Error(m); }

(async () => {
  if (!fs.existsSync(path.join(dir, 'index.html'))) { console.log('FEHLER: ' + dir + ' fehlt (erst tools/webapp/build.py)'); process.exit(1); }
  const srv = http.createServer((q, r) => {
    let p = decodeURIComponent(q.url.split('?')[0]); if (p === '/') p = '/index.html';
    const f = path.join(dir, p);
    if (!f.startsWith(dir) || !fs.existsSync(f)) { r.writeHead(404); r.end(); return; }
    r.writeHead(200, { 'Content-Type': TYPES[path.extname(f)] || 'application/octet-stream' }); r.end(fs.readFileSync(f));
  }).listen(0);
  const port = srv.address().port, url = `http://127.0.0.1:${port}/`;
  const browser = await chromium.launch();
  const ctx = await browser.newContext({ viewport: { width: 900, height: 500 }, hasTouch: true });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(String(e)));
  const info = () => page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._gbInfo())));
  const ready = async () => {
    await page.waitForFunction(() => window.GB_LOCAL && GB_LOCAL.isReady() && window.Module && Module._gbInfo && JSON.parse(Module.UTF8ToString(Module._gbInfo())).frames > 5, null, { timeout: 30000 });
    await page.waitForTimeout(500);
  };
  // echter Tap auf einen Knopf der Seite (Pointer wie ein Finger)
  async function tapBtn(b, holdMs = 90) {
    const dsel = { UP: '.u', DOWN: '.d', LEFT: '.l', RIGHT: '.r' }[b];
    let el = null;
    for (const e of await page.$$(dsel ? '#dpad ' + dsel : '[data-b="' + b + '"]')) if (await e.isVisible()) { el = e; break; }
    assert(el, `Knopf ${b} fehlt`);
    const bb = await el.boundingBox(); const x = bb.x + bb.width / 2, y = bb.y + bb.height / 2;
    await page.mouse.move(x, y); await page.mouse.down(); await page.waitForTimeout(holdMs); await page.mouse.up(); await page.waitForTimeout(160);
  }
  // Tap aufs Bild an Groovebox-Koordinaten (640x480)
  async function tapScreen(gx, gy) {
    const bb = await (await page.$('#cv')).boundingBox();
    const x = bb.x + gx * bb.width / 640, y = bb.y + gy * bb.height / 480;
    await page.mouse.move(x, y); await page.mouse.down(); await page.waitForTimeout(80); await page.mouse.up(); await page.waitForTimeout(200);
  }
  async function dpad(b) { await tapBtn(b); }

  await page.goto(url);
  await scenario('Gegeben die App, wenn sie startet, dann laeuft die Groovebox im Browser und zeichnet Bilder', async () => {
    await ready();
    const a = await info(); await page.waitForTimeout(600); const b = await info();
    assert(b.frames > a.frames, `keine neuen Bilder (${a.frames} -> ${b.frames})`);
    const lit = await page.evaluate(() => { const c = document.getElementById('cv'); const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data; let n = 0; for (let i = 0; i < d.length; i += 400) n += d[i] + d[i + 1] + d[i + 2] > 60; return n; });
    assert(lit > 50, 'Bild ist leer');
  });
  await scenario('Gegeben die App, wenn START angetippt wird, dann spielt sie; nochmal = Stopp', async () => {
    await tapBtn('START'); let i = await info(); assert(i.playing === 1, 'spielt nicht');
    await tapBtn('START'); i = await info(); assert(i.playing === 0, 'stoppt nicht');
  });
  await scenario('Gegeben TRIG, wenn RECHTS und A angetippt werden, dann wird auf Step 2 ein Trig gesetzt', async () => {
    const a = await info(); await dpad('RIGHT'); const b = await info();
    assert(b.cursor === (a.cursor + 1) % 16, `Cursor ${a.cursor} -> ${b.cursor}`);
    await tapBtn('A'); const c = await info();
    assert(c.steps !== b.steps, `Trig nicht gesetzt (${b.steps} -> ${c.steps})`);
  });
  const ui2 = (await info()).ui2 === 1;   // neue Oberflaeche: SELECT = MEHR, L2/R2 = Ansichten-Kreis
  if (ui2) {
    await scenario('NEU: Gegeben SEQ, wenn RUNTER und L1/R1 angetippt werden, dann waehlt RUNTER einen Wert und R1 die Spur', async () => {
      const a = await info(); await dpad('DOWN'); const b = await info();
      assert(b.slot !== a.slot || b.page !== a.page, 'RUNTER waehlt keinen anderen Wert');
      assert(b.track === a.track, 'RUNTER hat die Spur gewechselt');
      await tapBtn('R1'); const c = await info(); assert(c.track === (a.track + 1) % 8, 'R1 wechselt die Spur nicht');
      await tapBtn('L1');
    });
    await scenario('NEU: Gegeben eine Ansicht, wenn SELECT angetippt wird, dann oeffnet MEHR; B schliesst es', async () => {
      await tapBtn('SELECT'); let i = await info(); assert(i.mr === 1 && i.menu === 0, 'MEHR nicht offen');
      await tapBtn('B'); i = await info(); assert(i.mr === 0, 'MEHR nicht zu');
    });
    await scenario('NEU: Gegeben MEHR, wenn R1 (Abschnitt STEPS), 5x RUNTER und A, dann schaltet NEUE PAGE um', async () => {
      const np = (await info()).newPage;
      await tapBtn('SELECT'); await tapBtn('R1'); for (let k = 0; k < 5; k++) await dpad('DOWN');
      await tapBtn('A'); let i = await info(); assert(i.newPage === 1 - np, 'NEUE PAGE nicht umgeschaltet');
      await tapBtn('B'); i = await info(); assert(i.mr === 0, 'MEHR nicht zu');
    });
    await scenario('NEU: Gegeben SEQ, wenn R2 und L2 angetippt werden, dann SOUND und zurueck', async () => {
      const a = await info(); await tapBtn('R2'); const b = await info(); assert(b.view === 1, 'R2: nicht SOUND (Ansicht ' + b.view + ')');
      await tapBtn('L2'); const c = await info(); assert(c.view === 0 && c.page === a.page, 'L2 kommt nicht zu SEQ zurueck');
    });
    await scenario('NEU: Gegeben SET (L2 von SEQ), wenn Eintraege geaendert und die Ansicht gewechselt wird, dann laeuft das Bild weiter', async () => {
      await tapBtn('L2'); assert((await info()).pageName === 'SET', 'SET nicht erreichbar (' + (await info()).pageName + ')');
      await tapBtn('A'); for (let k = 0; k < 4; k++) { await dpad('DOWN'); await dpad('RIGHT'); await dpad('LEFT'); }
      await tapBtn('R2'); await tapBtn('L2');
      const a = await info(); await page.waitForTimeout(700); const b = await info();
      assert(b.frames > a.frames + 3, `Bild steht (${a.frames} -> ${b.frames})`);
      await tapBtn('B'); await tapBtn('R2');
    });
  } else {
  await scenario('Gegeben eine Seite, wenn SELECT angetippt wird, dann oeffnet die Seitenkarte; B schliesst sie', async () => {
    await tapBtn('SELECT'); let i = await info(); assert(i.menu === 1, 'Karte nicht offen');
    await tapBtn('B'); i = await info(); assert(i.menu === 0, 'Karte nicht zu');
  });
  await scenario('Gegeben eine Spurseite, wenn PAGE unten links angetippt wird, dann oeffnet das PAGE-Menue; Zeile NEUE PAGE schaltet um', async () => {
    await tapScreen(58, 427); let i = await info(); assert(i.pgm === 1, 'PAGE-Menue nicht offen');
    const np = i.newPage;
    const h = 64 + 10 * 26 + 30, y0 = (480 - h) / 2 - 10;   // wie drawPageMenu (10 Eintraege, NEUE PAGE = 9)
    await tapScreen(320, y0 + 50 + 9 * 26 + 8); i = await info(); assert(i.newPage === 1 - np, 'NEUE PAGE nicht umgeschaltet');
    await tapScreen(10, 10); i = await info(); assert(i.pgm === 0, 'daneben tippen schliesst nicht');
  });
  await scenario('Gegeben L2/R2, wenn angetippt, dann wechselt die Seite und kommt zurueck', async () => {
    const a = await info(); await tapBtn('R2'); const b = await info(); assert(b.page !== a.page, 'R2 wechselt nicht');
    await tapBtn('L2'); const c = await info(); assert(c.page === a.page, 'L2 kommt nicht zurueck');
  });
  await scenario('Gegeben SET, wenn Eintraege geaendert und die Seite gewechselt wird, dann laeuft das Bild weiter (kein Einfrieren)', async () => {
    await tapBtn('SELECT'); await dpad('UP'); await dpad('UP');   // Seitenkarte: oberste Zeile PROJEKT
    for (let k = 0; k < 6 && (await info()).menuSel !== 'SET'; k++) await dpad('RIGHT');
    await tapBtn('A');
    assert((await info()).pageName === 'SET', 'SET nicht erreichbar (Karte: ' + (await info()).menuSel + ')');
    await tapBtn('A'); for (let k = 0; k < 4; k++) { await dpad('DOWN'); await dpad('RIGHT'); await dpad('LEFT'); }
    await tapBtn('R2'); await tapBtn('L2');
    const a = await info(); await page.waitForTimeout(700); const b = await info();
    assert(b.frames > a.frames + 3, `Bild steht (${a.frames} -> ${b.frames})`);
    await tapBtn('B'); await tapBtn('B');
  });
  }
  await scenario('Gegeben ein gesetzter Trig, wenn die App neu geladen wird, dann ist er noch da (Speichern in IndexedDB)', async () => {
    for (let k = 0; k < 14 && (await info()).pageName !== 'TRG'; k++) await tapBtn(ui2 ? 'R2' : 'B');
    const before = await info();
    await page.evaluate(() => new Promise(r => GB_LOCAL.saveNow(r)));
    await page.reload(); await ready();
    const after = await info();
    assert(after.project === before.project, `anderes Projekt nach Neuladen (${before.project} -> ${after.project})`);
    assert(after.steps === before.steps, `Steps nach Neuladen ${before.steps} -> ${after.steps}`);
  });
  await scenario('Gegeben die App, dann gibt es Manifest und Service-Worker (Offline, Homescreen)', async () => {
    const m = await page.evaluate(() => fetch('manifest.json').then(r => r.ok ? r.json() : null));
    assert(m && m.name, 'manifest.json fehlt');
    const sw = await page.evaluate(() => fetch('sw.js').then(r => r.ok));
    assert(sw, 'sw.js fehlt');
  });
  await scenario('Waehrend aller Szenarien: keine JavaScript-Fehler in der Seite', async () => { assert(!errors.length, errors.slice(0, 3).join(' | ')); });

  await browser.close(); srv.close();
  let bad = 0;
  for (const [ok, n, e, ms] of results) { console.log(`${ok ? 'OK    ' : 'FEHLER'} ${n} (${(ms / 1000).toFixed(1)} s)${ok ? '' : ' -> ' + e}`); bad += !ok; }
  console.log(`Handy-App: ${results.length - bad}/${results.length} Szenarien ok`);
  process.exit(bad ? 1 : 0);
})().catch(e => { console.log('FEHLER: ' + e); process.exit(1); });
