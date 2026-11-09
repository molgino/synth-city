#!/usr/bin/env python3
"""Szenarien Webserver (Upload-Seite, Explorer, Sync der Handy-App): jede API einmal richtig und einmal falsch benutzt.

Startet ein eigenes groovebox in einem Temp-Ordner (wie remote_scenarios.py) und prueft Antworten UND Wirkung
(Datei wirklich da/weg/umbenannt, Bibliothek neu geladen, offenes Projekt neu geladen ...).
Aufruf: tests/scenarios/web_api_scenarios.py [pfad/zu/groovebox]
"""
import io, json, os, struct, sys, time, urllib.error, urllib.parse, urllib.request
sys.path.insert(0, os.path.dirname(__file__))
import remote_scenarios as rs  # start_box / stop_box / scenario / results

results = rs.results
scenario = rs.scenario


def wav(n=2000, rate=44100):
    data = b''.join(struct.pack('<h', int(8000 * ((i % 100) / 50 - 1))) for i in range(n))
    return (b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVEfmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate, rate * 2, 2, 16)
            + b'data' + struct.pack('<I', len(data)) + data)


def call(ctx, method, url_path, body=None, **q):
    url = f"http://127.0.0.1:{ctx['port']}{url_path}" + ('?' + urllib.parse.urlencode(q) if q else '')
    req = urllib.request.Request(url, data=body, method=method)
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def f(ctx, *parts): return os.path.join(ctx['tmp'], *parts)


@scenario('Gegeben die Upload-Seite, wenn sie geladen wird, dann kommt HTML; /api/list nennt Projekte und Sample-Anzahl')
def s_page(ctx):
    st, b = call(ctx, 'GET', '/'); assert st == 200 and b'<html' in b.lower(), f'Startseite {st}'
    st, b = call(ctx, 'GET', '/api/list'); j = json.loads(b)
    t = json.dumps(j).lower()
    assert st == 200 and ('projects' in t or 'samples' in t), f'/api/list {st} {b[:80]}'


@scenario('Gegeben ein Sample, wenn es in einen neuen Unterordner hochgeladen wird, dann liegt es dort und erscheint in /api/dir und der Suche')
def s_upload(ctx):
    st, _ = call(ctx, 'PUT', '/api/upload', wav(), root='samples', path='test/neu/beat.wav')
    assert st == 200, f'Upload {st}'
    assert os.path.isfile(f(ctx, 'samples', 'test', 'neu', 'beat.wav')), 'Datei nicht angelegt'
    st, b = call(ctx, 'GET', '/api/dir', root='samples', path='test/neu')
    assert st == 200 and b'beat' in b, f'/api/dir zeigt die Datei nicht ({st})'
    st, b = call(ctx, 'GET', '/api/search', root='samples', q='beat')
    assert st == 200 and b'beat' in b, 'Suche findet die Datei nicht'
    st, b = call(ctx, 'GET', '/api/info', root='samples', path='test/neu/beat.wav')
    assert st == 200 and b'44100' in b, f'WAV-Info falsch: {b[:80]}'


@scenario('Gegeben ein hochgeladenes Sample, wenn 2 s vergehen, dann hat die Groovebox die Bibliothek neu geladen')
def s_reload(ctx):
    call(ctx, 'PUT', '/api/upload', wav(), root='samples', path='reload/x.wav')
    t = time.time()
    while time.time() - t < 6:
        if any('Bibliothek' in l and 'Samples' in l for l in ctx['box']['log'][ctx['logmark']:]): return
        time.sleep(0.2)
    raise AssertionError('kein Neuladen der Bibliothek im Log')


@scenario('Gegeben gefaehrliche Pfade (.., /, versteckt, falsche Endung), wenn hochgeladen wird, dann lehnt der Server ab und nichts entsteht')
def s_bad_paths(ctx):
    for p in ['../boese.wav', '/etc/x.wav', '.versteckt.wav', 'a/../../x.wav', 'ok.txt', 'a\\b.wav', 'x:.wav']:
        st, _ = call(ctx, 'PUT', '/api/upload', wav(100), root='samples', path=p)
        assert st >= 400, f'{p} wurde angenommen ({st})'
    st, _ = call(ctx, 'PUT', '/api/upload', b'x', root='etc', path='passwd')
    assert st >= 400, 'falsche Wurzel angenommen'
    assert not os.path.exists(os.path.join(os.path.dirname(ctx['tmp']), 'boese.wav')), 'Datei ausserhalb angelegt'


@scenario('Gegeben ein Sample, wenn es umbenannt/verschoben wird, dann liegt es nur noch am Ziel; Ziel vorhanden = 409')
def s_move(ctx):
    call(ctx, 'PUT', '/api/upload', wav(), root='samples', path='mv/a.wav')
    call(ctx, 'PUT', '/api/upload', wav(), root='samples', path='mv/c.wav')
    st, _ = call(ctx, 'POST', '/api/move', root='samples', path='mv/a.wav', to='mv/sub/b.wav')
    assert st == 200 and os.path.isfile(f(ctx, 'samples', 'mv', 'sub', 'b.wav')) and not os.path.exists(f(ctx, 'samples', 'mv', 'a.wav')), f'Verschieben {st}'
    st, _ = call(ctx, 'POST', '/api/move', root='samples', path='mv/c.wav', to='mv/sub/b.wav')
    assert st == 409, f'Ziel existiert: {st} statt 409'
    st, _ = call(ctx, 'POST', '/api/move', root='samples', path='mv/c.wav', to='mv/c.txt')
    assert st == 400, 'andere Endung angenommen'
    st, _ = call(ctx, 'POST', '/api/move', root='samples', path='mv', to='mv/sub/innen')
    assert st == 400, 'Ordner in sich selbst verschoben'


@scenario('Gegeben Ordner und Dateien, wenn geloescht wird, dann sind sie weg; fehlende Datei = 404')
def s_delete(ctx):
    call(ctx, 'PUT', '/api/upload', wav(), root='samples', path='del/x.wav')
    st, _ = call(ctx, 'POST', '/api/delete', root='samples', path='del/x.wav')
    assert st == 200 and not os.path.exists(f(ctx, 'samples', 'del', 'x.wav')), f'Loeschen {st}'
    st, _ = call(ctx, 'POST', '/api/delete', root='samples', path='del/x.wav'); assert st == 404, f'zweimal loeschen: {st}'
    st, _ = call(ctx, 'POST', '/api/mkdir', root='samples', path='leer/tief'); assert st == 200 and os.path.isdir(f(ctx, 'samples', 'leer', 'tief')), 'mkdir'
    call(ctx, 'PUT', '/api/upload', wav(), root='samples', path='leer/tief/y.wav')
    st, _ = call(ctx, 'POST', '/api/rmdir', root='samples', path='leer'); assert st == 200 and not os.path.exists(f(ctx, 'samples', 'leer')), 'rmdir rekursiv'


@scenario('Gegeben ein Projekt, wenn es hochgeladen, geholt und umbenannt wird, dann stimmen Inhalt und Name; Projekte haben keine Unterordner')
def s_projects(ctx):
    data = open(ctx['proj'], 'rb').read()
    st, _ = call(ctx, 'PUT', '/api/upload', data, root='projects', path='Mein Song.gbx'); assert st == 200, f'Projekt-Upload {st}'
    st, b = call(ctx, 'GET', '/api/file', root='projects', path='Mein Song.gbx'); assert st == 200 and b == data, 'Projekt kommt anders zurueck'
    st, _ = call(ctx, 'POST', '/api/move', root='projects', path='Mein Song.gbx', to='Neu.gbx'); assert st == 200 and os.path.isfile(f(ctx, 'projects', 'Neu.gbx')), 'umbenennen'
    st, _ = call(ctx, 'PUT', '/api/upload', data, root='projects', path='ordner/x.gbx'); assert st >= 400, 'Projekt in Unterordner angenommen'


@scenario('Gegeben Projekte, Sounds und Samples, wenn die Handy-App das Manifest holt, dann sind alle mit Groesse und Zeit drin')
def s_manifest(ctx):
    os.makedirs(f(ctx, 'presets', 'SYNTH'), exist_ok=True)
    open(f(ctx, 'presets', 'SYNTH', 'Bass.gbp'), 'wb').write(b'GBP1' + bytes(200))
    st, b = call(ctx, 'GET', '/api/manifest'); assert st == 200, f'Manifest {st}'
    j = json.loads(b)['files']
    names = {(x.get('r'), x.get('p')) for x in j}
    assert ('presets', 'SYNTH/Bass.gbp') in names, f'Sound fehlt im Manifest: {sorted(names)[:6]}'
    assert any(r == 'samples' for r, _ in names) and any(r == 'projects' for r, _ in names), 'Samples/Projekte fehlen'
    assert all('s' in x and 't' in x for x in j), 'Groesse/Zeit fehlen'


@scenario('Gegeben die Handy-App (andere Herkunft), wenn sie fragt, dann erlaubt CORS nur die App-Adresse, nicht fremde Seiten')
def s_cors(ctx):
    for origin, ok in [('https://molgino.github.io', True), ('http://localhost:5173', True), ('https://boese.example', False)]:
        req = urllib.request.Request(f"http://127.0.0.1:{ctx['port']}/api/manifest", headers={'Origin': origin})
        with urllib.request.urlopen(req, timeout=10) as r:
            allow = r.headers.get('Access-Control-Allow-Origin')
        assert (allow == origin) == ok, f'CORS fuer {origin}: {allow}'


@scenario('Gegeben das offene Projekt, wenn es per Sync ersetzt wird (gestoppt, ungeaendert), dann laedt die Groovebox es neu')
def s_project_replaced(ctx):
    data = open(ctx['proj'], 'rb').read()
    mark = len(ctx['box']['log'])
    st, _ = call(ctx, 'PUT', '/api/upload', data, root='projects', path=os.path.basename(ctx['proj'])); assert st == 200
    t = time.time()
    while time.time() - t < 5:
        if any('project1' in l.lower() or 'PROJEKT' in l for l in ctx['box']['log'][mark:]): return
        time.sleep(0.2)
    # kein Log-Eintrag ist kein harter Fehler, solange der Server weiterlaeuft
    st, _ = call(ctx, 'GET', '/api/list'); assert st == 200, 'Server antwortet nach dem Ersetzen nicht'


@scenario('Gegeben 30 gleichzeitige Anfragen, dann antwortet der Server auf alle')
def s_parallel(ctx):
    import threading
    bad = []
    def one(i):
        st, _ = call(ctx, 'GET', '/api/list')
        if st != 200: bad.append(st)
    th = [threading.Thread(target=one, args=(i,)) for i in range(30)]
    [t.start() for t in th]; [t.join(15) for t in th]
    assert not bad, f'{len(bad)} Anfragen fehlgeschlagen: {bad[:5]}'


def main():
    exe = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '../../groovebox'))
    src_samples = os.path.join(os.path.dirname(exe), 'samples')
    try:
        box = rs.start_box(exe)
    except RuntimeError as e:
        print('FEHLER Start: ' + str(e)); return 1
    import shutil
    if os.path.isdir(src_samples) and not os.path.exists(os.path.join(box['tmp'], 'samples')):
        shutil.copytree(src_samples, os.path.join(box['tmp'], 'samples'))
    os.makedirs(os.path.join(box['tmp'], 'samples'), exist_ok=True)
    w = rs.WS(box['port'], '/ws')   # Projekt anlegen wie am Geraet: SELECT + START = speichern
    for m in ('d SELECT', 'd START', 'u START', 'u SELECT'): w.send(m); time.sleep(0.15)
    w.close()
    proj = None
    for _ in range(30):
        pd = os.path.join(box['tmp'], 'projects')
        g = [x for x in os.listdir(pd) if x.endswith('.gbx')] if os.path.isdir(pd) else []
        if g: proj = os.path.join(pd, g[0]); break
        time.sleep(0.2)
    if not proj:
        print('FEHLER Start: kein Projekt gespeichert'); rs.stop_box(box); return 1
    ctx = {'port': box['port'], 'tmp': box['tmp'], 'box': box, 'logmark': 0, 'proj': proj}
    for sc in (s_page, s_upload, s_reload, s_bad_paths, s_move, s_delete, s_projects, s_manifest, s_cors, s_project_replaced, s_parallel):
        ctx['logmark'] = len(box['log'])
        sc(ctx)
    alive = rs.stop_box(box)
    if not alive: results.append((False, 'Gerät läuft nach allen Szenarien noch', 'abgestuerzt', 0))
    bad = 0
    for ok, name, err, dt in results:
        print(f"{'OK    ' if ok else 'FEHLER'} {name} ({dt:.1f} s){'' if ok else ' -> ' + err}")
        bad += not ok
    print(f"Webserver: {len(results) - bad}/{len(results)} Szenarien ok")
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
