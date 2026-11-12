// Touch-Oberflaeche (eigenes UI statt des gespiegelten Bildes): echte Taps gegen ein laufendes groovebox
// auf Port 80. Geprueft wird nicht, wie es aussieht, sondern dass die Engine danach den neuen Zustand meldet.
const {chromium} = require('playwright');
const PORT = process.env.GB_PORT || '80';
let pass = 0, fail = 0;
function check(ok, what, extra) { if (ok) { pass++; console.log('  OK     ' + what); } else { fail++; console.log('  FEHLER ' + what + (extra ? ' – ' + extra : '')); } }

(async () => {
  const br = await chromium.launch({args: ['--no-sandbox']});
  const ctx = await br.newContext({viewport: {width: 852, height: 393}, deviceScaleFactor: 2, isMobile: true, hasTouch: true});
  const pg = await ctx.newPage();
  await pg.addInitScript(() => { try { localStorage.clear(); } catch (e) {} });   // jeder Lauf startet gleich
  const errs = [];
  pg.on('console', m => { if (m.type() === 'error') errs.push(m.text()); });
  await pg.goto(`http://127.0.0.1:${PORT}/screen`, {waitUntil: 'domcontentloaded'});
  await pg.waitForTimeout(1500);
  await pg.evaluate(() => document.getElementById('bTui').click());
  await pg.waitForTimeout(2000);

  const model = () => pg.evaluate(() => window.tm || null);
  // kurz tippen heisst: aufsetzen UND abheben - ohne das laeuft der Halte-Timer weiter und es wird ein P-Lock
  const tapPad = i => pg.evaluate(n => { const p = [...document.querySelectorAll('.tp')][n]; const r = p.getBoundingClientRect();
    p.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, pointerId: 21, clientX: r.left + r.width / 2, clientY: r.top + r.height / 2}));
    p.dispatchEvent(new PointerEvent('pointerup', {bubbles: true, pointerId: 21})); }, i);
  const tapSel = sel => pg.evaluate(s => { const e = document.querySelector(s); const r = e.getBoundingClientRect();
    e.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, clientX: r.left + r.width / 2, clientY: r.top + r.height / 2})); }, sel);

  let m = await model();
  check(!!m && m.tracks && m.tracks.length === 8, 'Modell kommt an (8 Spuren)', m ? JSON.stringify(m).slice(0, 80) : 'keins');
  if (!m) { await br.close(); process.exit(1); }

  // Step setzen und wieder loeschen
  const before = m.steps[2];
  await tapPad(2); await pg.waitForTimeout(500);
  let m2 = await model();
  check(m2.steps[2] !== before, 'Pad antippen setzt den Step in der Engine', `vorher ${before}, nachher ${m2.steps[2]}`);
  await tapPad(2); await pg.waitForTimeout(500);
  m2 = await model();
  check(m2.steps[2] === before, 'nochmal antippen loescht ihn wieder');

  // Spur waehlen
  await pg.evaluate(() => { const e = [...document.querySelectorAll('.ttr')][4]; const r = e.getBoundingClientRect();
    e.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, clientX: r.left + 20, clientY: r.top + r.height / 2})); });
  await pg.waitForTimeout(500);
  m2 = await model();
  check(m2.track === 4, 'Spur antippen waehlt sie', 'track=' + m2.track);

  // Mute
  const muteBefore = m2.tracks[4].m;
  await pg.evaluate(() => { const e = [...document.querySelectorAll('.ttr')][4].querySelector('.tm'); const r = e.getBoundingClientRect();
    e.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, clientX: r.left + r.width / 2, clientY: r.top + r.height / 2})); });
  await pg.waitForTimeout(500);
  m2 = await model();
  check(m2.tracks[4].m !== muteBefore, 'M in der Spurzeile schaltet stumm', `${muteBefore} -> ${m2.tracks[4].m}`);

  // Transport
  const playBefore = m2.play;
  await tapSel('#tPlay'); await pg.waitForTimeout(600);
  m2 = await model();
  check(m2.play !== playBefore, 'Play/Stop schaltet den Sequencer', `${playBefore} -> ${m2.play}`);
  if (m2.play) { await pg.waitForTimeout(900); const m3 = await model(); check(m3.now >= 0, 'der laufende Step wird gemeldet', 'now=' + m3.now); await tapSel('#tPlay'); }

  // Pattern weiter
  const patBefore = (await model()).patIdx;
  await tapSel('#tPatU'); await pg.waitForTimeout(600);
  m2 = await model();
  check(m2.patIdx !== patBefore, 'Pattern weiterschalten', `${patBefore} -> ${m2.patIdx}`);
  await tapSel('#tPatD');

  // Klang schrauben: Umschalten, Gruppe waehlen, Regler ziehen
  await pg.evaluate(() => { if (!document.body.classList.contains('snd')) { const e = document.getElementById('tMode'), r = e.getBoundingClientRect();
    e.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, clientX: r.left + 5, clientY: r.top + 5})); } });
  await pg.waitForTimeout(900);
  let snd = await pg.evaluate(() => ({snd: document.body.classList.contains('snd'),
    grps: document.querySelectorAll('#tGrps .tg').length, rows: document.querySelectorAll('#tPar .tr').length}));
  check(snd.snd && snd.grps > 3 && snd.rows > 0, 'SOUND zeigt Gruppen und Regler', `${snd.grps} Gruppen, ${snd.rows} Regler`);

  const picked = await pg.evaluate(() => {   // eine Gruppe mit mindestens einem Regler ueber 1 Schritt
    const g = [...document.querySelectorAll('#tGrps .tg')].find(x => x.textContent === 'FILTER') || document.querySelectorAll('#tGrps .tg')[1];
    const r = g.getBoundingClientRect();
    g.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, clientX: r.left + 10, clientY: r.top + r.height / 2}));
    return g.textContent;
  });
  await pg.waitForTimeout(900);
  const drag = (from, to) => pg.evaluate(([a, b]) => {
    const d = document.querySelector('#tPar .tr'), r = d.getBoundingClientRect();
    d.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, pointerId: 7, clientX: r.left + r.width * a, clientY: r.top + r.height / 2}));
    d.dispatchEvent(new PointerEvent('pointermove', {bubbles: true, pointerId: 7, clientX: r.left + r.width * b, clientY: r.top + r.height / 2}));
    d.dispatchEvent(new PointerEvent('pointerup', {bubbles: true, pointerId: 7}));
  }, [from, to]);
  const val = () => pg.evaluate(() => (window.tm && tm.params && tm.params[0]) ? tm.params[0].v : -1);
  // erst hoch, dann runter - so ist der Test unabhaengig davon, wo der Regler gerade steht
  await drag(0.2, 0.8); await pg.waitForTimeout(900);
  const vUp = await val();
  await drag(0.8, 0.2); await pg.waitForTimeout(900);
  const vDown = await val();
  check(vUp > 0, `Regler nach rechts ziehen erhoeht den Wert (Gruppe ${picked})`, 'Wert ' + vUp);
  check(vDown < vUp, 'Regler nach links ziehen senkt ihn wieder', `${vUp} -> ${vDown}`);

  // P-Lock: Pad lange druecken haelt den Step, Reglerwerte werden dann Locks
  await pg.evaluate(() => { const d = [...document.querySelectorAll('.tp')][5], r = d.getBoundingClientRect();
    d.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, pointerId: 11, clientX: r.left + r.width / 2, clientY: r.top + r.height / 2})); });
  await pg.waitForTimeout(650);
  await pg.evaluate(() => [...document.querySelectorAll('.tp')][5].dispatchEvent(new PointerEvent('pointerup', {bubbles: true, pointerId: 11})));
  await pg.waitForTimeout(900);
  const held = await pg.evaluate(() => ({hold: tm.hold, cls: document.body.classList.contains('hold')}));
  check(held.hold === 5 && held.cls, 'Pad lange druecken haelt den Step fest (P-Lock)', 'hold=' + held.hold);
  await pg.evaluate(() => { const g = [...document.querySelectorAll('#tGrps .tg')].find(x => x.textContent === 'FILTER');
    if (g) { const r = g.getBoundingClientRect(); g.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, clientX: r.left + 10, clientY: r.top + r.height / 2})); } });
  await pg.waitForTimeout(800);
  const patBase = await pg.evaluate(() => tm.params[0].v);
  await drag(0.1, 0.7); await pg.waitForTimeout(900);
  const locked = await pg.evaluate(() => ({v: tm.params[0].v, lk: tm.params[0].lk, step: tm.steps[5]}));
  check(locked.lk === 1 && locked.v !== patBase, 'im Lock-Modus wird der Wert als Lock geschrieben', `${patBase} -> ${locked.v}, lk=${locked.lk}`);
  check(locked.step > 0, 'der Step traegt den Lock (Trig oder trigless)', 'on=' + locked.step);
  await tapSel('#tLockE'); await pg.waitForTimeout(900);
  const after = await pg.evaluate(() => ({hold: tm.hold, v: tm.params[0].v, lk: tm.params[0].lk}));
  check(after.hold < 0 && after.v === patBase && !after.lk, 'FERTIG beendet den Lock-Modus, der Pattern-Wert bleibt', `hold=${after.hold}, Wert ${after.v}`);

  // Auswahl-Regler (ENGINE): antippen oeffnet eine Liste statt eines Schiebers
  await pg.evaluate(() => { const g = [...document.querySelectorAll('#tGrps .tg')].find(x => /MAKRO/.test(x.textContent));
    if (g) { const r = g.getBoundingClientRect(); g.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, clientX: r.left + 10, clientY: r.top + r.height / 2})); } });
  await pg.waitForTimeout(800);
  const ei = await pg.evaluate(() => tm.params.findIndex(p => p.n === 'ENGINE'));
  check(ei >= 0, 'ENGINE ist in den Makros');
  if (ei >= 0) {
    await pg.evaluate(i => { const d = document.querySelectorAll('#tPar .tr')[i], r = d.getBoundingClientRect();
      d.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, pointerId: 12, clientX: r.left + 40, clientY: r.top + r.height / 2})); }, ei);
    await pg.waitForTimeout(600);
    const li = await pg.evaluate(() => ({open: document.body.classList.contains('pick'), n: document.querySelectorAll('#tPickL .pk').length}));
    check(li.open && li.n === 15, 'ENGINE antippen oeffnet die Auswahl mit allen Engines', li.n + ' Eintraege');
    const engBefore = await pg.evaluate(() => tm.tracks[tm.track].n);
    await pg.evaluate(() => { const b = [...document.querySelectorAll('#tPickL .pk')][4], r = b.getBoundingClientRect();
      b.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, clientX: r.left + 10, clientY: r.top + 10})); });
    await pg.waitForTimeout(1000);
    const engAfter = await pg.evaluate(() => ({n: tm.tracks[tm.track].n, open: document.body.classList.contains('pick')}));
    check(engAfter.n === 'SYNTH' && !engAfter.open, 'die gewaehlte Engine landet auf der Spur', `${engBefore} -> ${engAfter.n}`);
  }

  // Klaviatur: Noten vorhoeren, und bei gehaltenem Step wird die Note dessen Lock.
  // Dafuer erst wieder einen Step halten - der vorige Lock-Modus wurde oben mit FERTIG beendet.
  await pg.evaluate(() => { const d = [...document.querySelectorAll('.tp')][5], r = d.getBoundingClientRect();
    d.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, pointerId: 40, clientX: r.left + r.width / 2, clientY: r.top + r.height / 2})); });
  await pg.waitForTimeout(650);
  await pg.evaluate(() => [...document.querySelectorAll('.tp')][5].dispatchEvent(new PointerEvent('pointerup', {bubbles: true, pointerId: 40})));
  await pg.waitForTimeout(700);
  await pg.evaluate(() => tSetMode(2));
  await pg.waitForTimeout(800);
  const kb = await pg.evaluate(() => ({keys: document.body.classList.contains('keys'),
    w: document.querySelectorAll('#tKb .wk').length, b: document.querySelectorAll('#tKb .bk').length,
    h: Math.round((document.querySelector('#tKb .wk') || {getBoundingClientRect: () => ({height: 0})}).getBoundingClientRect().height)}));
  check(kb.keys && kb.w === 14 && kb.b === 10, 'Klaviatur zeigt zwei Oktaven', `${kb.w} weisse, ${kb.b} schwarze`);
  check(kb.h >= 44, 'die Tasten sind fingergross', kb.h + ' px hoch');
  const noteBefore = await pg.evaluate(() => tm.note);
  await pg.evaluate(() => { const d = [...document.querySelectorAll('#tKb .wk')][3], r = d.getBoundingClientRect();
    d.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, pointerId: 41, clientX: r.left + r.width / 2, clientY: r.top + r.height - 8})); });
  await pg.waitForTimeout(900);
  const noteAfter = await pg.evaluate(() => tm.note);
  check(noteAfter !== noteBefore, 'Taste im Lock-Modus setzt die Note des Steps', `${noteBefore} -> ${noteAfter}`);
  await tapSel('#tLockE'); await pg.waitForTimeout(800);
  const lockedStep = await pg.evaluate(() => ({step: tm.steps[5], hold: tm.hold}));
  check(lockedStep.hold < 0 && lockedStep.step > 0, 'nach FERTIG traegt der Step die Note', 'on=' + lockedStep.step);
  await pg.evaluate(() => tSetMode(1)); await pg.waitForTimeout(500);   // fuer den Scroll-Test zurueck zu den Reglern

  // Gemeldet: scrollen ging nicht - #tui hatte touch-action:none (blockiert jede Geste im Overlay)
  // und die Reglerliste war gar kein Scrollcontainer.
  // fuer den Scroll-Fall eine moeglichst volle Gruppe nehmen (MAKROS hat 9 Regler, FILTER nur 5)
  await pg.evaluate(() => {
    const g = [...document.querySelectorAll('#tGrps .tg')].find(x => /MAKRO/.test(x.textContent));
    if (g) { const r = g.getBoundingClientRect(); g.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, clientX: r.left + 10, clientY: r.top + r.height / 2})); }
  });
  await pg.waitForTimeout(900);
  const sc = await pg.evaluate(() => {
    const pr = document.getElementById('tPar');
    const ta = n => getComputedStyle(n).touchAction;
    pr.scrollTop = 500;
    return {ui: ta(document.getElementById('tui')), row: pr.children[0] ? ta(pr.children[0]) : '-',
            over: getComputedStyle(pr).overflowY, scrollH: pr.scrollHeight, clientH: pr.clientHeight, top: pr.scrollTop};
  });
  check(sc.ui !== 'none', 'das Overlay blockiert keine Gesten mehr', 'touch-action ' + sc.ui);
  check(sc.row === 'pan-y', 'Reglerzeile: senkrecht scrollen erlaubt, waagerecht gehoert dem Wert', sc.row);
  check(sc.over === 'auto' || sc.over === 'scroll', 'Reglerliste ist ein Scrollcontainer', sc.over);
  if (sc.scrollH > sc.clientH + 2) check(sc.top > 0, 'passt die Liste nicht auf den Schirm, laesst sie sich scrollen', `${sc.scrollH} in ${sc.clientH}, scrollTop ${sc.top}`);
  else check(true, `Liste passt ganz auf den Schirm (${sc.scrollH} in ${sc.clientH})`);

  await pg.evaluate(() => tSetMode(0)); await pg.waitForTimeout(600);
  const back = await pg.evaluate(() => !document.body.classList.contains('snd') && !document.body.classList.contains('keys')
    && [...document.querySelectorAll('.tp')].some(p => p.style.display !== 'none'));
  check(back, 'zurueck zu STEPS');

  // Flaeche + keine JS-Fehler
  const area = await pg.evaluate(() => { let u = 0; document.querySelectorAll('#tui .tp,#tui .ttr,#tui .tb,#tui #tPlay').forEach(e => { const b = e.getBoundingClientRect(); if (b.width > 0) u += b.width * b.height; });
    return Math.round(100 * u / (innerWidth * innerHeight)); });
  check(area >= 60, `Bedienflaeche nutzt den Schirm (${area} %)`);
  check(errs.length === 0, 'keine JS-Fehler', errs.join(' | '));

  await ctx.close();

  // Gemeldet: in der App blieb alles unter der Kopfzeile leer - die Seite holt das Modell dort ueber
  // GB_LOCAL.model(), und genau diese Funktion fehlte. Beide Faelle werden hier festgehalten.
  for (const withModel of [false, true]) {
    const c2 = await br.newContext({viewport: {width: 852, height: 393}, deviceScaleFactor: 2, isMobile: true, hasTouch: true});
    const p2 = await c2.newPage();
    await p2.addInitScript(({wm}) => {
      const steps = Array.from({length: 16}, (_, i) => i % 4 === 0 ? 1 : 0);
      const tracks = ['KICK', 'SNARE', 'HAT', 'CLAP', 'SYNTH', 'SYNTH', 'FM', 'SMPL'].map(n => ({n, m: 0, s: 0, c: '#888888', len: 16, on: 0}));
      const mdl = JSON.stringify({play: 0, song: 0, bpm: 120, pat: 'A01', patIdx: 0, track: 0, len: 16, page: 0, pages: 1, rec: 0, msg: '', tracks, steps, now: -1});
      window.GB_LOCAL = Object.assign({start() {}, send() {}, saveNow() {}, persist() {}, isReady: () => true}, wm ? {model: () => mdl} : {});
    }, {wm: withModel});
    await p2.goto(`http://127.0.0.1:${PORT}/screen`, {waitUntil: 'domcontentloaded'});
    await p2.waitForTimeout(700);
    await p2.evaluate(() => document.getElementById('bTui').click());
    await p2.waitForTimeout(3600);
    const r = await p2.evaluate(() => ({
      pads: [...document.querySelectorAll('.tp')].filter(p => p.style.display !== 'none').length,
      trks: document.querySelectorAll('.ttr').length,
      wait: getComputedStyle(document.getElementById('tWait')).display !== 'none'}));
    if (withModel) {
      check(r.pads === 16 && r.trks === 8, 'App (GB_LOCAL.model): Pads und Spuren erscheinen', `${r.pads} Pads, ${r.trks} Spuren`);
      check(!r.wait, 'App: kein Hinweis, wenn das Modell da ist');
    } else {
      check(r.wait, 'App ohne Modell: Hinweis statt leerer Flaeche');
    }
    await c2.close();
  }

  await br.close();
  console.log(`Touch-Oberflaeche: ${pass} ok, ${fail} Fehler`);
  process.exit(fail ? 1 : 0);
})();
