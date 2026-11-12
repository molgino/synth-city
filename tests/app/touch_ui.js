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
  const errs = [];
  pg.on('console', m => { if (m.type() === 'error') errs.push(m.text()); });
  await pg.goto(`http://127.0.0.1:${PORT}/screen`, {waitUntil: 'domcontentloaded'});
  await pg.waitForTimeout(1500);
  await pg.evaluate(() => document.getElementById('bTui').click());
  await pg.waitForTimeout(2000);

  const model = () => pg.evaluate(() => window.tm || null);
  const tapPad = i => pg.evaluate(n => { const p = [...document.querySelectorAll('.tp')][n]; const r = p.getBoundingClientRect();
    p.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, clientX: r.left + r.width / 2, clientY: r.top + r.height / 2})); }, i);
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

  // Flaeche + keine JS-Fehler
  const area = await pg.evaluate(() => { let u = 0; document.querySelectorAll('#tui .tp,#tui .ttr,#tui .tb,#tui #tPlay').forEach(e => { const b = e.getBoundingClientRect(); if (b.width > 0) u += b.width * b.height; });
    return Math.round(100 * u / (innerWidth * innerHeight)); });
  check(area >= 60, `Bedienflaeche nutzt den Schirm (${area} %)`);
  check(errs.length === 0, 'keine JS-Fehler', errs.join(' | '));

  await br.close();
  console.log(`Touch-Oberflaeche: ${pass} ok, ${fail} Fehler`);
  process.exit(fail ? 1 : 0);
})();
