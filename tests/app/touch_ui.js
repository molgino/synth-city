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
  // Ein dispatchEvent trifft auch unsichtbare Knoepfe - genau so ist der MEHR-Knopf durchgerutscht,
  // der in einer ausgeblendeten Leiste sass. Jeder Tap prueft darum erst, ob die Flaeche wirklich frei liegt.
  const frei = sel => pg.evaluate(s => { const e = document.querySelector(s);
    if (!e) return 'fehlt';
    const b = e.getBoundingClientRect();
    if (b.width < 1 || b.height < 1) return 'unsichtbar';
    if (b.top < 0 || b.left < 0 || b.bottom > innerHeight || b.right > innerWidth) return 'ausserhalb des Schirms';
    const t = document.elementFromPoint(b.left + b.width / 2, b.top + b.height / 2);
    return (t === e || e.contains(t)) ? '' : 'verdeckt von ' + (t ? (t.id || t.className || t.tagName) : '?'); }, sel);
  const goView = async v => { await tapSel(`#tVws .vw[data-v="${v}"]`); await pg.waitForTimeout(700); };
  const tapSel = async sel => {
    const bad = await frei(sel);
    if (bad) check(false, `${sel} ist bedienbar`, bad);
    await pg.evaluate(s => { const e = document.querySelector(s); const r = e.getBoundingClientRect();
      const o = {bubbles: true, pointerId: 77, clientX: r.left + r.width / 2, clientY: r.top + r.height / 2};
      e.dispatchEvent(new PointerEvent('pointerdown', o));
      e.dispatchEvent(new PointerEvent('pointerup', o)); }, sel);   // Auswahlzeilen wirken erst beim Abheben
  };

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
    const _o = {bubbles: true, clientX: r.left + 20, clientY: r.top + r.height / 2}; e.dispatchEvent(new PointerEvent('pointerdown', _o)); e.dispatchEvent(new PointerEvent('pointerup', _o)); });
  await pg.waitForTimeout(500);
  m2 = await model();
  check(m2.track === 4, 'Spur antippen waehlt sie', 'track=' + m2.track);

  // Mute
  const muteBefore = m2.tracks[4].m;
  await pg.evaluate(() => { const e = [...document.querySelectorAll('.ttr')][4].querySelector('.tm'); const r = e.getBoundingClientRect();
    const _o = {bubbles: true, clientX: r.left + r.width / 2, clientY: r.top + r.height / 2}; e.dispatchEvent(new PointerEvent('pointerdown', _o)); e.dispatchEvent(new PointerEvent('pointerup', _o)); });
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
  await tapSel('#tM1');
  await pg.waitForTimeout(900);
  await goView(1);   // Ansicht KLANG (die Liste kann auch auf SEQUENZ/EFFEKTE/MASTER stehen)
  let snd = await pg.evaluate(() => ({snd: document.body.classList.contains('snd'),
    grps: document.querySelectorAll('#tGrps .tg').length, rows: document.querySelectorAll('#tPar .tr').length}));
  check(snd.snd && snd.grps > 3 && snd.rows > 0, 'SOUND zeigt Gruppen und Regler', `${snd.grps} Gruppen, ${snd.rows} Regler`);

  const picked = await pg.evaluate(() => {   // eine Gruppe mit mindestens einem Regler ueber 1 Schritt
    const g = [...document.querySelectorAll('#tGrps .tg')].find(x => x.textContent === 'FILTER') || document.querySelectorAll('#tGrps .tg')[1];
    const r = g.getBoundingClientRect();
    const _o = {bubbles: true, clientX: r.left + 10, clientY: r.top + r.height / 2}; g.dispatchEvent(new PointerEvent('pointerdown', _o)); g.dispatchEvent(new PointerEvent('pointerup', _o));
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
    d.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, pointerId: 11, clientX: r.left + r.width / 2, clientY: r.top + r.height / 2})); });   // bleibt gedrueckt: das Halten ist der Test
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
  // Gemeldet: "ich kann die Trig-Locks nur einzeln editieren und muss immer beenden, um zum naechsten
  // zu kommen" - TRIG ◀ / TRIG ▶ springen im Lock-Modus zum vorigen/naechsten Step MIT Trig.
  {
    const vorher = await pg.evaluate(() => ({hold: tm.hold, jmp: tm.jmp, steps: tm.steps.slice(0, 16)}));
    check(vorher.jmp === 1, 'das Modell meldet, dass es einen weiteren Trig gibt', JSON.stringify(vorher.steps));
    await tapSel('#tLockN'); await pg.waitForTimeout(900);
    const sprung = await pg.evaluate(() => ({hold: tm.hold, steps: tm.steps}));
    check(sprung.hold >= 0 && sprung.hold !== vorher.hold, 'TRIG ▶ springt zum naechsten Step, ohne den Lock-Modus zu beenden',
          `Step ${vorher.hold + 1} -> ${sprung.hold + 1}`);
    check(sprung.steps[sprung.hold] > 0, 'und der neue Step hat wirklich einen Trig', 'on=' + sprung.steps[sprung.hold]);
    await tapSel('#tLockP'); await pg.waitForTimeout(900);
    const zurueck = await pg.evaluate(() => tm.hold);
    check(zurueck === vorher.hold, 'TRIG ◀ kommt wieder zurueck', `${sprung.hold + 1} -> ${zurueck + 1}`);
  }

  // Der Step wird erst beim BEENDEN des Lock-Modus zum Traeger (leer + Wert = trigless). Vorher stand
  // hier die Pruefung mitten im Halten - sie ging nur durch, weil ein frueherer Lauf den Step hinterlassen hatte.
  // FERTIG gibt es nur WAEHREND des Haltens - ein zweiter Tap darauf traf vorher einen
  // unsichtbaren Knopf (der Test lief nur durch, weil dispatchEvent das nicht merkt).
  await tapSel('#tLockE'); await pg.waitForTimeout(900);
  const after = await pg.evaluate(() => ({hold: tm.hold, v: tm.params[0].v, lk: tm.params[0].lk,
    step: tm.steps[5], eW: Math.round(document.querySelector('#tLockE').getBoundingClientRect().width)}));
  check(after.step > 0, 'nach FERTIG traegt der Step den Lock (Trig oder trigless)', 'on=' + after.step);
  check(after.hold < 0 && after.v === patBase && !after.lk, 'FERTIG beendet den Lock-Modus, der Pattern-Wert bleibt', `hold=${after.hold}, Wert ${after.v}`);
  check(after.eW < 1, 'und FERTIG ist danach weg', 'Breite=' + after.eW);

  // Auswahl-Regler (ENGINE): antippen oeffnet eine Liste statt eines Schiebers
  await pg.evaluate(() => { const g = [...document.querySelectorAll('#tGrps .tg')].find(x => /MAKRO/.test(x.textContent));
    if (g) { const r = g.getBoundingClientRect(); g.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, clientX: r.left + 10, clientY: r.top + r.height / 2})); } });
  await pg.waitForTimeout(800);
  const ei = await pg.evaluate(() => tm.params.findIndex(p => p.n === 'ENGINE'));
  check(ei >= 0, 'ENGINE ist in den Makros');
  if (ei >= 0) {
    await pg.evaluate(i => { const d = document.querySelectorAll('#tPar .tr')[i], r = d.getBoundingClientRect();
      const _o = {bubbles: true, pointerId: 12, clientX: r.left + 40, clientY: r.top + r.height / 2}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); }, ei);
    await pg.waitForTimeout(600);
    const li = await pg.evaluate(() => ({open: document.body.classList.contains('pick'), n: document.querySelectorAll('#tPickL .pk').length}));
    check(li.open && li.n === 15, 'ENGINE antippen oeffnet die Auswahl mit allen Engines', li.n + ' Eintraege');
    const engBefore = await pg.evaluate(() => tm.tracks[tm.track].n);
    await pg.evaluate(() => { const b = [...document.querySelectorAll('#tPickL .pk')][4], r = b.getBoundingClientRect();
      const _o = {bubbles: true, clientX: r.left + 10, clientY: r.top + 10}; b.dispatchEvent(new PointerEvent('pointerdown', _o)); b.dispatchEvent(new PointerEvent('pointerup', _o)); });
    await pg.waitForTimeout(1000);
    const engAfter = await pg.evaluate(() => ({n: tm.tracks[tm.track].n, open: document.body.classList.contains('pick')}));
    check(engAfter.n === 'SYNTH' && !engAfter.open, 'die gewaehlte Engine landet auf der Spur', `${engBefore} -> ${engAfter.n}`);
  }

  // Klaviatur: Noten vorhoeren, und bei gehaltenem Step wird die Note dessen Lock.
  // Dafuer erst wieder einen Step halten - der vorige Lock-Modus wurde oben mit FERTIG beendet.
  await pg.evaluate(() => { const d = [...document.querySelectorAll('.tp')][5], r = d.getBoundingClientRect();
    d.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, pointerId: 40, clientX: r.left + r.width / 2, clientY: r.top + r.height / 2})); });   // bleibt gedrueckt: das Halten ist der Test
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
    const _o = {bubbles: true, pointerId: 41, clientX: r.left + r.width / 2, clientY: r.top + r.height - 8}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); });
  await pg.waitForTimeout(900);
  const noteAfter = await pg.evaluate(() => tm.note);
  check(noteAfter !== noteBefore, 'Taste im Lock-Modus setzt die Note des Steps', `${noteBefore} -> ${noteAfter}`);
  await tapSel('#tLockE'); await pg.waitForTimeout(800);
  const lockedStep = await pg.evaluate(() => ({step: tm.steps[5], hold: tm.hold}));
  check(lockedStep.hold < 0 && lockedStep.step > 0, 'nach FERTIG traegt der Step die Note', 'on=' + lockedStep.step);
  await pg.evaluate(() => tSetMode(1)); await pg.waitForTimeout(500);   // fuer den Scroll-Test zurueck zu den Reglern

  // Samples: die Plaetze, die das Projekt geladen hat, als Liste mit Namen
  await pg.evaluate(() => { const e = [...document.querySelectorAll('.ttr')][7], r = e.getBoundingClientRect();
    const _o = {bubbles: true, clientX: r.left + 20, clientY: r.top + r.height / 2}; e.dispatchEvent(new PointerEvent('pointerdown', _o)); e.dispatchEvent(new PointerEvent('pointerup', _o)); });
  await pg.waitForTimeout(700);
  await pg.evaluate(() => tSetMode(1)); await pg.waitForTimeout(1100);
  const smp = await pg.evaluate(() => { const p = tm.params.find(x => x.n === 'SMPL'); return p ? {slots: p.slots ? p.slots.length : 0, v: p.v, i: tm.params.indexOf(p)} : null; });
  check(smp && smp.slots >= 1, 'SMPL bietet die geladenen Sample-Plaetze an', smp ? smp.slots + ' Plaetze' : 'kein SMPL-Regler');
  if (smp && smp.slots >= 1) {
    await pg.evaluate(i => { const d = document.querySelectorAll('#tPar .tr')[i], r = d.getBoundingClientRect();
      const _o = {bubbles: true, pointerId: 50, clientX: r.left + 40, clientY: r.top + r.height / 2}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); }, smp.i);
    await pg.waitForTimeout(600);
    const wide = await pg.evaluate(() => ({open: document.body.classList.contains('pick'), wide: document.getElementById('tPickL').classList.contains('wide')}));
    check(wide.open && wide.wide, 'die Sample-Liste ist breit (Namen statt Zahlen)');
    // einen anderen Platz als den aktuellen nehmen - sonst passiert beim zweiten Lauf nichts mehr
    await pg.evaluate(v => { const l = [...document.querySelectorAll('#tPickL .pk')];
      const b = l.find(e => +e.dataset.v !== v) || l[l.length - 1], r = b.getBoundingClientRect();
      const _o = {bubbles: true, clientX: r.left + 10, clientY: r.top + 10}; b.dispatchEvent(new PointerEvent('pointerdown', _o)); b.dispatchEvent(new PointerEvent('pointerup', _o)); }, smp.v);
    await pg.waitForTimeout(900);
    const picked = await pg.evaluate(() => tm.params.find(x => x.n === 'SMPL').v);
    check(picked !== smp.v, 'der gewaehlte Platz landet auf der Spur', `${smp.v} -> ${picked}`);
  }

  await pg.evaluate(() => { if (document.body.classList.contains('pick')) tPickClose(); });
  await pg.waitForTimeout(300);
  // Gemeldet: "die Sequencer-Seite kann ich nicht zuverlaessig abrufen" - der Modus war EIN Knopf
  // im Kreis, der das Ziel statt des Zustands zeigte. Jetzt drei Knoepfe: jeder Modus mit einem Tipp.
  for (const [from, to, cls] of [['#tM1', '#tM0', 'tui'], ['#tM2', '#tM0', 'tui'], ['#tM3', '#tM0', 'tui'], ['#tM0', '#tM2', 'tui keys'], ['#tM1', '#tM2', 'tui keys'], ['#tM2', '#tM1', 'tui snd'], ['#tM0', '#tM3', 'tui mix'], ['#tM3', '#tM4', 'tui song']]) {
    await tapSel(from); await pg.waitForTimeout(450);
    await tapSel(to); await pg.waitForTimeout(450);
    const got = await pg.evaluate(() => document.body.className.trim());
    check(got === cls, `ein Tipp von ${from} auf ${to} reicht`, `body="${got}" statt "${cls}"`);
  }
  const marked = await pg.evaluate(() => [0, 1, 2, 3, 4].map(k => document.getElementById('tM' + k).classList.contains('on')));
  check(marked.filter(Boolean).length === 1, 'genau ein Modus ist markiert', JSON.stringify(marked));

  // Lange Listen (LFO-Ziel): stehen einmal je Modell unter lists, der Regler verweist darauf
  await tapSel('#tM1'); await pg.waitForTimeout(600); await goView(1);
  const gi = await pg.evaluate(() => tm.grps.findIndex(g => /LFO 1/.test(g)));
  check(gi >= 0, 'Gruppe LFO 1 ist da');
  await pg.evaluate(i => tAct(10, i, 0), gi); await pg.waitForTimeout(900);
  const dst = await pg.evaluate(() => { const i = tm.params.findIndex(p => p.list === 'dest');
    return {i, n: i >= 0 ? tm.params[i].n : '', v: i >= 0 ? tm.params[i].v : -1, len: tm.lists && tm.lists.dest ? tm.lists.dest.length : 0}; });
  check(dst.i >= 0 && dst.len > 50, 'LFO-Ziel bietet die lange Liste an', `${dst.n}, ${dst.len} Eintraege`);
  if (dst.i >= 0) {
    // ueber die Zeile, nicht ueber tPickOpen: genau dort fehlte der Fall "lange Liste" und am Handy
    // passierte beim Antippen nichts
    await pg.evaluate(i => { const d = [...document.querySelectorAll('#tPar .tr')][i], b = d.getBoundingClientRect();
      d.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, pointerId: 44, clientX: b.left + b.width / 2, clientY: b.top + b.height / 2}));
      d.dispatchEvent(new PointerEvent('pointerup', {bubbles: true, pointerId: 44})); }, dst.i);
    await pg.waitForTimeout(500);
    const wide = await pg.evaluate(() => ({open: document.body.classList.contains('pick'),
      wide: document.getElementById('tPickL').classList.contains('wide'),
      zeilen: document.querySelectorAll('#tPickL .pk').length}));
    check(wide.open && wide.wide && wide.zeilen === dst.len, 'die Zielliste steht als breite Auswahl da', JSON.stringify(wide));
    // Gemeldet: "die Ziele kann ich immer noch nicht waehlen, die Liste ist nicht scrollbar".
    // Ursache: die Zeile wirkte auf pointerdown MIT preventDefault - das beendet die Wischgeste des
    // Browsers, also war unter 114 Zielen alles ausserhalb des ersten Schirms unerreichbar.
    const sc = await pg.evaluate(() => { const l = document.getElementById('tPickL');
      return {sh: l.scrollHeight, ch: l.clientHeight, ta: getComputedStyle(l).touchAction}; });
    check(sc.sh > sc.ch + 10, 'die Zielliste ist laenger als der Schirm (es gibt also was zu scrollen)', `${sc.sh} in ${sc.ch}`);
    check(/pan-y|auto/.test(sc.ta), 'senkrecht wischen gehoert dem Browser', 'touch-action=' + sc.ta);
    // ein Wisch ueber eine Zeile darf nichts auswaehlen
    const vorWisch = await pg.evaluate(() => tm.params.find(p => p.list === 'dest').v);
    await pg.evaluate(() => { const b = document.querySelector('#tPickL .pk'), r = b.getBoundingClientRect();
      const o = y => ({bubbles: true, pointerId: 45, clientX: r.left + 20, clientY: r.top + 20 + y});
      b.dispatchEvent(new PointerEvent('pointerdown', o(0)));
      b.dispatchEvent(new PointerEvent('pointerup', o(-70))); });
    await pg.waitForTimeout(600);
    check((await pg.evaluate(() => tm.params.find(p => p.list === 'dest').v)) === vorWisch,
          'ein Wisch ueber die Liste waehlt nichts aus', 'Wert ' + vorWisch);
    // Die Liste ist ein Menue: eine Zeile je Bedienseite, antippen klappt ihre Ziele auf
    const men = await pg.evaluate(() => { const l = document.getElementById('tPickL');
      return {menu: l.classList.contains('menu'), grp: l.querySelectorAll('.pg').length,
              offen: l.querySelectorAll('.pgi.open').length,
              sichtbar: [...l.querySelectorAll('.pk')].filter(e => e.offsetParent).length}; });
    check(men.menu && men.grp > 5, 'die Ziele stehen als Menue nach Seiten da', `${men.grp} Gruppen`);
    check(men.offen === 1, 'genau die Gruppe des aktuellen Ziels ist offen', men.offen + ' offen');
    check(men.sichtbar < dst.len / 2, 'zugeklappt ist die Liste kurz', `${men.sichtbar} von ${dst.len} Zeilen`);
    // eine zugeklappte Gruppe antippen: ihre Ziele erscheinen
    const auf = await pg.evaluate(() => { const l = document.getElementById('tPickL');
      const h = [...l.querySelectorAll('.pg')].find(x => !x.nextElementSibling.classList.contains('open'));
      if (!h) return {ok: false};
      const r = h.getBoundingClientRect(), o = {bubbles: true, pointerId: 48, clientX: r.left + 20, clientY: r.top + 20};
      h.dispatchEvent(new PointerEvent('pointerdown', o)); h.dispatchEvent(new PointerEvent('pointerup', o));
      return {ok: true, name: h.textContent, offen: h.nextElementSibling.classList.contains('open'),
              zeilen: [...h.nextElementSibling.querySelectorAll('.pk')].map(e => +e.dataset.v)}; });
    check(auf.ok && auf.offen && auf.zeilen.length > 0, 'eine Gruppe antippen klappt sie auf', JSON.stringify(auf).slice(0, 90));
    await pg.waitForTimeout(200);
    // und ein Ziel daraus landet auf dem LFO
    const ziel = await pg.evaluate(v => { const l = document.getElementById('tPickL');
      const b = [...l.querySelectorAll('.pgi.open .pk')].find(e => +e.dataset.v !== v && +e.dataset.v > 0 && e.offsetParent);
      if (!b) return -1;
      b.scrollIntoView({block: 'nearest'});
      const r = b.getBoundingClientRect(), o = {bubbles: true, pointerId: 46, clientX: r.left + 10, clientY: r.top + 10};
      b.dispatchEvent(new PointerEvent('pointerdown', o)); b.dispatchEvent(new PointerEvent('pointerup', o));
      return +b.dataset.v; }, dst.v);
    check(ziel > 0, 'in der offenen Gruppe liegt ein Ziel zum Antippen', 'v=' + ziel);
    await pg.waitForTimeout(900);
    const nv = await pg.evaluate(() => tm.params.find(p => p.list === 'dest').v);
    check(nv === ziel, 'das gewaehlte Ziel landet auf dem LFO', `${dst.v} -> ${nv} (erwartet ${ziel})`);
  }

  // Gemeldet: "im horizontalen Modus kann ich die oberen Elemente nicht bedienen" - der Picker war das
  // einzige Element ohne Safe-Area und liess sich nur ueber ein 21 px hohes ABBRECHEN schliessen.
  {
    const pi = await pg.evaluate(() => tm.params.findIndex(p => p.opts || p.slots || p.list));
    await pg.evaluate(i => tPickOpen(i), pi); await pg.waitForTimeout(400);
    const geo = await pg.evaluate(() => { const x = document.getElementById('tPickX').getBoundingClientRect();
      const cs = getComputedStyle(document.getElementById('tPick'));
      return {h: Math.round(x.height), w: Math.round(x.width), padTop: parseInt(cs.paddingTop), padLeft: parseInt(cs.paddingLeft)}; });
    check(geo.h >= 44 && geo.w >= 44, 'ABBRECHEN ist eine echte Fingerflaeche', `${geo.w}x${geo.h}`);
    check(geo.padTop >= 10 && geo.padLeft >= 10, 'der Picker haelt Abstand zum Rand (Safe-Area)', JSON.stringify(geo));
    await pg.evaluate(() => { const e = document.getElementById('tPick');
      const o = {bubbles: true, pointerId: 47, clientX: innerWidth - 6, clientY: innerHeight - 6};
      e.dispatchEvent(new PointerEvent('pointerdown', o)); e.dispatchEvent(new PointerEvent('pointerup', o)); });
    await pg.waitForTimeout(400);
    check(!(await pg.evaluate(() => document.body.classList.contains('pick'))), 'daneben tippen schliesst die Auswahl');
    // und danach ist die Kopfzeile wieder bedienbar
    const frei = await pg.evaluate(() => ['tPlay', 'tM0', 'tView'].every(id => { const e = document.getElementById(id), b = e.getBoundingClientRect();
      const t = document.elementFromPoint(b.left + b.width / 2, b.top + b.height / 2); return t === e || e.contains(t); }));
    check(frei, 'danach ist die Kopfzeile wieder frei');
  }

  // Song: die Clip-Matrix. Der Test raeumt am Ende alle Zeilen wieder weg.
  await tapSel('#tM4'); await pg.waitForTimeout(800);
  {
    const vor = await pg.evaluate(() => tm.sng.len);
    check(await pg.evaluate(() => !!tm.sng && Array.isArray(tm.sng.pats)), 'das Modell liefert den Song');
    await tapSel('#tSgAdd'); await pg.waitForTimeout(700);
    await tapSel('#tSgAdd'); await pg.waitForTimeout(700);
    const nach = await pg.evaluate(() => ({len: tm.sng.len, zeilen: document.querySelectorAll('.sgr').length}));
    check(nach.len === vor + 2 && nach.zeilen === nach.len, '+ ZEILE legt Zeilen an', JSON.stringify(nach));
    // eine Zelle auf "schweigt" setzen
    await pg.evaluate(() => { const c = document.querySelectorAll('.sgr')[0].querySelectorAll('.cl')[2], r = c.getBoundingClientRect();
      const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; c.dispatchEvent(new PointerEvent('pointerdown', _o)); c.dispatchEvent(new PointerEvent('pointerup', _o)); });
    await pg.waitForTimeout(400);
    const pk = await pg.evaluate(() => ({offen: document.body.classList.contains('pick'), titel: document.getElementById('tPickT').textContent}));
    check(pk.offen && /ZEILE 1/.test(pk.titel) && /SPUR 3/.test(pk.titel), 'eine Zelle oeffnet die Pattern-Wahl', JSON.stringify(pk));
    await pg.evaluate(() => { const b = [...document.querySelectorAll('#tPickL .pk')].find(e => e.textContent === '---'), r = b.getBoundingClientRect();
      const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; b.dispatchEvent(new PointerEvent('pointerdown', _o)); b.dispatchEvent(new PointerEvent('pointerup', _o)); });
    await pg.waitForTimeout(800);
    check((await pg.evaluate(() => tm.sng.rows[0].p[2])) === 255, 'die Spur schweigt jetzt in dieser Zeile');
    // Wiederholungen
    const r0 = await pg.evaluate(() => tm.sng.rows[0].r);
    await pg.evaluate(() => { const d = document.querySelectorAll('.sgr')[0].querySelectorAll('.rp div')[2], r = d.getBoundingClientRect();
      const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); });
    await pg.waitForTimeout(700);
    check((await pg.evaluate(() => tm.sng.rows[0].r)) === r0 + 1, 'die Wiederholungen lassen sich erhoehen');
    // Song an und wieder aus
    await tapSel('#tSgOn'); await pg.waitForTimeout(800);
    check((await pg.evaluate(() => tm.sng.on)) === 1 && (await pg.evaluate(() => tm.song)) === 1, 'SONG AN schaltet den Song-Modus ein');
    await tapSel('#tSgOn'); await pg.waitForTimeout(700);
    check((await pg.evaluate(() => tm.sng.on)) === 0, 'und wieder aus');
    // aufraeumen, damit der naechste Lauf gleich startet
    for (let i = 0; i < 8; i++) {
      if (!(await pg.evaluate(() => tm.sng.len))) break;
      await pg.evaluate(() => { const d = document.querySelectorAll('.sgr')[0].querySelector('.del'), r = d.getBoundingClientRect();
        const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); });
      await pg.waitForTimeout(600);
    }
    check((await pg.evaluate(() => tm.sng.len)) === vor, 'Zeilen lassen sich wieder loeschen');
  }
  await tapSel('#tM0'); await pg.waitForTimeout(400);

  // Projekt: speichern, laden, neu - ohne das kam man am Handy nicht an seine Projekte
  {
    const p0 = await pg.evaluate(() => tm.prj);
    check(p0 && typeof p0.n === 'string' && Array.isArray(p0.l), 'das Modell nennt Projekt und Liste', JSON.stringify(p0));
    await tapSel('#tPrj'); await pg.waitForTimeout(500);
    const sh = await pg.evaluate(() => ({offen: document.body.classList.contains('sheet'),
      knoepfe: [...document.querySelectorAll('#tShB .big')].map(e => e.textContent).join(','),
      zeilen: document.querySelectorAll('#tShB .pl div').length}));
    check(sh.offen && /PROJEKT SPEICHERN/.test(sh.knoepfe) && sh.zeilen === p0.l.length,
          'das Projekt-Blatt zeigt Speichern und alle Projekte', JSON.stringify(sh));
    // speichern
    await pg.evaluate(() => { const b = [...document.querySelectorAll('#tShB .big')][0], r = b.getBoundingClientRect();
      const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; b.dispatchEvent(new PointerEvent('pointerdown', _o)); b.dispatchEvent(new PointerEvent('pointerup', _o)); });
    await pg.waitForTimeout(1200);
    check((await pg.evaluate(() => tm.prj.d)) === 0, 'nach SPEICHERN meldet das Modell keine Aenderungen mehr');
    // ein anderes Projekt laden und wieder zurueck
    if (p0.l.length > 1) {
      const andere = p0.l.findIndex(n => n !== p0.n);
      await pg.evaluate(() => { const b = document.getElementById('tPrj'), r = b.getBoundingClientRect();
        const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; b.dispatchEvent(new PointerEvent('pointerdown', _o)); b.dispatchEvent(new PointerEvent('pointerup', _o)); });
      await pg.waitForTimeout(400);
      await pg.evaluate(i => { const d = [...document.querySelectorAll('#tShB .pl div')][i], r = d.getBoundingClientRect();
        const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); }, andere);
      await pg.waitForTimeout(1600);
      const jetzt = await pg.evaluate(() => tm.prj.n);
      check(jetzt === p0.l[andere], 'ein Tipp auf die Zeile laedt genau dieses Projekt', `${p0.n} -> ${jetzt} (erwartet ${p0.l[andere]})`);
      // zurueck auf das Ausgangsprojekt, damit der naechste Lauf gleich startet
      await tapSel('#tPrj'); await pg.waitForTimeout(400);
      const zurueck = await pg.evaluate(n => [...document.querySelectorAll('#tShB .pl div')].findIndex(d => d.textContent === n), p0.n);
      await pg.evaluate(i => { const d = [...document.querySelectorAll('#tShB .pl div')][i], r = d.getBoundingClientRect();
        const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); }, zurueck);
      await pg.waitForTimeout(1600);
      check((await pg.evaluate(() => tm.prj.n)) === p0.n, 'und wieder zurueck');
    }
    // Sampler im selben Blatt: Quelle, Aufnahme, Hoeren, Sichern
    await tapSel('#tPrj'); await pg.waitForTimeout(500);
    const sb = await pg.evaluate(() => [...document.querySelectorAll('#tShB .big')].map(e => e.textContent));
    check(sb.includes('QUELLE') && sb.some(t => /AUFNAHME|STOPP/.test(t)) && sb.includes('HÖREN') && sb.includes('SAMPLE SICHERN'),
          'der Sampler steht im selben Blatt', JSON.stringify(sb));
    {   // Quelle umstellen - und wieder zurueck, damit der naechste Lauf gleich startet
      const q0 = await pg.evaluate(() => tm.smp.src);
      await pg.evaluate(() => { const d = [...document.querySelectorAll('#tShB .big')].find(e => e.textContent === 'QUELLE'), r = d.getBoundingClientRect();
        const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); });
      await pg.waitForTimeout(450);
      const quellen = await pg.evaluate(() => document.querySelectorAll('#tPickL .pk').length);
      check(quellen === 10, 'die Quelle bietet USB, Master und acht Spuren an', String(quellen));
      const ziel = q0 === 1 ? 0 : 1;
      await pg.evaluate(i => { const d = [...document.querySelectorAll('#tPickL .pk')][i], r = d.getBoundingClientRect();
        const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); }, ziel);
      await pg.waitForTimeout(800);
      check((await pg.evaluate(() => tm.smp.src)) === ziel, 'die gewaehlte Quelle kommt an', `${q0} -> ${ziel}`);
      // Aufnahme an, wieder aus
      const recBtn = () => pg.evaluate(() => { const d = [...document.querySelectorAll('#tShB .big')].find(e => /AUFNAHME|STOPP/.test(e.textContent)), r = d.getBoundingClientRect();
        const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); });
      await recBtn(); await pg.waitForTimeout(800);
      check((await pg.evaluate(() => tm.smp.rec)) === 1, 'AUFNAHME startet den Sampler');
      await recBtn(); await pg.waitForTimeout(800);
      check((await pg.evaluate(() => tm.smp.rec)) === 0, 'und STOPP beendet sie wieder');
      await pg.evaluate(() => { const d = [...document.querySelectorAll('#tShB .big')].find(e => e.textContent === 'QUELLE'), r = d.getBoundingClientRect();
        const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); });
      await pg.waitForTimeout(450);
      await pg.evaluate(i => { const d = [...document.querySelectorAll('#tPickL .pk')][i], r = d.getBoundingClientRect();
        const _o = {bubbles: true, clientX: r.left + 5, clientY: r.top + 5}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); }, q0);
      await pg.waitForTimeout(700);
    }
    await pg.evaluate(() => tSheetClose()); await pg.waitForTimeout(300);
    check(!(await pg.evaluate(() => document.body.classList.contains('sheet'))), 'das Blatt laesst sich schliessen');
  }

  // Gemeldet: "master kann ich nicht editieren, also das tempo" - am Handy gab es nur die Ansicht KLANG.
  // Damit fehlten Tempo/Skala/Kompressor, die drei Effekt-Busse UND die Sequenzer-Regler (NOTE, VEL, COND, ARP).
  {
    await tapSel('#tM1'); await pg.waitForTimeout(600);
    const erwartet = [[0, /TRIG/], [1, /MAKROS|KLANG/], [2, /EFFEKT-BUS A/], [3, /MASTER/]];
    for (const [v, re] of erwartet) {
      await goView(v);
      const g = await pg.evaluate(() => ({view: tm.view, grps: tm.grps.join(',')}));
      check(g.view === v && re.test(g.grps), `Ansicht ${v} ist erreichbar`, JSON.stringify(g));
    }
    const markiert = await pg.evaluate(() => [...document.querySelectorAll('#tVws .vw')].filter(d => d.classList.contains('on')).length);
    check(markiert === 1, 'genau eine Ansicht ist markiert', String(markiert));
    // Tempo in MASTER wirklich verstellen
    await goView(3);
    const bi = await pg.evaluate(() => tm.params.findIndex(p => p.n === 'BPM'));
    const bpmP = await pg.evaluate(i => tm.params[i], bi);
    check(bi >= 0 && bpmP.mn === 30 && bpmP.mx === 300, 'BPM hat einen echten Bereich (30-300)', JSON.stringify(bpmP));
    const b0 = await pg.evaluate(() => tm.bpm);
    await pg.evaluate(i => { const d = [...document.querySelectorAll('#tPar .tr')][i], b = d.getBoundingClientRect();
      d.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, pointerId: 55, clientX: b.left + b.width * 0.5, clientY: b.top + 10}));
      d.dispatchEvent(new PointerEvent('pointermove', {bubbles: true, pointerId: 55, clientX: b.left + b.width * 0.58, clientY: b.top + 10}));
      d.dispatchEvent(new PointerEvent('pointerup', {bubbles: true, pointerId: 55})); }, bi);
    await pg.waitForTimeout(900);
    const b1 = await pg.evaluate(() => tm.bpm);
    check(b1 > b0 && b1 <= 300, 'das Tempo laesst sich am Handy einstellen', `${b0} -> ${b1}`);
    // zurueck auf den Ausgangswert, damit der naechste Lauf gleich startet
    await pg.evaluate(a => tAct(11, a.i, a.v), {i: bi, v: b0}); await pg.waitForTimeout(600);
    check((await pg.evaluate(() => tm.bpm)) === b0, 'und wieder zurueckstellen');
    await goView(1);
  }

  // Mixer: acht Spuren nebeneinander, dieselben Zeilen wie die MIX-Seite am Geraet
  await tapSel('#tM3'); await pg.waitForTimeout(900);
  const mx = await pg.evaluate(() => ({cls: document.body.className.trim(),
    spalten: document.querySelectorAll('.mxc').length, fader: document.querySelectorAll('.mxc:first-child .mxf').length,
    werte: tm.tracks[0].mix ? tm.tracks[0].mix.length : 0}));
  check(mx.cls === 'tui mix' && mx.spalten === 8 && mx.fader === 6 && mx.werte === 6,
        'Mixer zeigt 8 Spuren mit 6 Zeilen', JSON.stringify(mx));
  {   // VOL einer anderen Spur ziehen - die Engine muss den Wert DIESER Spur aendern, nicht der gewaehlten.
    // Erst ganz runter, dann ganz hoch: so haengt der Test an keinem Startwert (der Lauf davor hat ihn veraendert).
    const zieh = ab => pg.evaluate(a => { const f = [...document.querySelectorAll('.mxc')][2].querySelectorAll('.mxf')[0], b = f.getBoundingClientRect();
      f.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, pointerId: 31, clientX: b.left + b.width / 2, clientY: b.top + b.height / 2}));
      f.dispatchEvent(new PointerEvent('pointermove', {bubbles: true, pointerId: 31, clientX: b.left + b.width / 2, clientY: b.top + b.height * (a ? 1.6 : -0.6)}));
      f.dispatchEvent(new PointerEvent('pointerup', {bubbles: true, pointerId: 31})); }, ab);
    const t0 = await pg.evaluate(() => tm.tracks[0].mix[0]);
    await zieh(true); await pg.waitForTimeout(800);
    const unten = await pg.evaluate(() => tm.tracks[2].mix[0]);
    check(unten < 20, 'VOL der dritten Spur laesst sich nach unten ziehen', `${unten}`);
    await zieh(false); await pg.waitForTimeout(800);
    const oben = await pg.evaluate(() => tm.tracks[2].mix[0]);
    check(oben > 100, 'und wieder nach oben', `${unten} -> ${oben}`);
    check((await pg.evaluate(() => tm.tracks[0].mix[0])) === t0, 'dabei bleibt die erste Spur unberuehrt');
  }
  {   // CMP ist ein Schalter, kein Schieber
    const vor = await pg.evaluate(() => tm.tracks[1].mix[5]);
    await pg.evaluate(() => { const f = [...document.querySelectorAll('.mxc')][1].querySelectorAll('.mxf')[5], b = f.getBoundingClientRect();
      const _o = {bubbles: true, clientX: b.left + 5, clientY: b.top + 5}; f.dispatchEvent(new PointerEvent('pointerdown', _o)); f.dispatchEvent(new PointerEvent('pointerup', _o)); });
    await pg.waitForTimeout(800);
    const nach = await pg.evaluate(() => tm.tracks[1].mix[5]);
    check(nach !== vor && (nach === 0 || nach === 1), 'CMP schaltet um', `${vor} -> ${nach}`);
  }
  {   // Mute aus dem Mixer
    const vor = await pg.evaluate(() => tm.tracks[4].m);
    await pg.evaluate(() => { const d = [...document.querySelectorAll('.mxc')][4].querySelector('.mxb').children[0], b = d.getBoundingClientRect();
      const _o = {bubbles: true, clientX: b.left + 5, clientY: b.top + 5}; d.dispatchEvent(new PointerEvent('pointerdown', _o)); d.dispatchEvent(new PointerEvent('pointerup', _o)); });
    await pg.waitForTimeout(800);
    check((await pg.evaluate(() => tm.tracks[4].m)) !== vor, 'Mute geht auch aus dem Mixer');
  }
  await tapSel('#tM0'); await pg.waitForTimeout(500);

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

  // Gemeldet: "die MEHR-Seite gibt es auch nicht" - sie sass in einer Leiste, die bei nur einer
  // Page ausgeblendet wurde. Darum: jeder Knopf der Leisten muss in jeder Groesse wirklich dasein.
  {
    const groessen = [[852, 393], [393, 852], [667, 375], [360, 640]];
    for (const [w, h] of groessen) {
      await pg.setViewportSize({width: w, height: h});
      await pg.waitForTimeout(700);
      const schlecht = [];
      for (const id of ['#tPlay', '#tPatD', '#tPatU', '#tM0', '#tM1', '#tM2', '#tM3', '#tM4', '#tView', '#tPrj']) {
        const bad = await frei(id);
        if (bad) schlecht.push(id + ': ' + bad);
      }
      check(schlecht.length === 0, `alle Knoepfe bedienbar bei ${w}x${h}`, schlecht.join(' | '));
    }
    await pg.setViewportSize({width: 852, height: 393});
    await pg.waitForTimeout(600);
  }

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
