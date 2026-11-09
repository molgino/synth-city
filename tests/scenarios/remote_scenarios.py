#!/usr/bin/env python3
"""Szenarien Fernbedienung (Browser /screen): Verbindungen, Abbrueche, Ton.

Startet ein eigenes groovebox (ohne Anzeige, ohne Ton-Geraet) in einem Temp-Ordner und spricht
WebSocket direkt ueber Sockets (keine Zusatzpakete). Aufruf: tests/scenarios/remote_scenarios.py [pfad/zu/groovebox]
Ergebnis: Zeilen "OK|FEHLER Szenario: ..." und Exit-Code 0/1.
"""
import base64, os, re, shutil, socket, struct, subprocess, sys, tempfile, time

IDLE_MS = 2500
results = []


def scenario(name):
    def deco(fn):
        def run(ctx):
            t0 = time.time()
            try:
                fn(ctx)
                results.append((True, name, '', time.time() - t0))
            except Exception as e:  # noqa: BLE001 - jeder Fehler ist ein fehlgeschlagenes Szenario
                results.append((False, name, str(e), time.time() - t0))
        run.__name__ = fn.__name__
        return run
    return deco


class WS:
    def __init__(self, port, path):
        self.s = socket.create_connection(('127.0.0.1', port), timeout=5)
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall(f'GET {path} HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                       f'Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n'.encode())
        head = b''
        while b'\r\n\r\n' not in head:
            c = self.s.recv(1)
            if not c:
                raise AssertionError('Verbindung beim Aufbau geschlossen')
            head += c
        if b' 101 ' not in head.split(b'\r\n')[0]:
            raise AssertionError('kein WebSocket: ' + head.split(b'\r\n')[0].decode(errors='replace'))
        self.buf = b''

    def send(self, text):
        d = text.encode(); m = os.urandom(4)
        self.s.sendall(bytes([0x81, 0x80 | len(d)]) + m + bytes(b ^ m[i % 4] for i, b in enumerate(d)))

    def recv(self, timeout):
        """naechste Nachricht (op, daten) oder None bei Zeitablauf; ('closed', b'') wenn das Geraet schliesst"""
        end = time.time() + timeout
        while True:
            if len(self.buf) >= 2:
                op = self.buf[0] & 15; n = self.buf[1] & 127; h = 2
                if n == 126: n = struct.unpack('>H', self.buf[2:4])[0] if len(self.buf) >= 4 else -1; h = 4
                elif n == 127: n = struct.unpack('>Q', self.buf[2:10])[0] if len(self.buf) >= 10 else -1; h = 10
                if n >= 0 and len(self.buf) >= h + n:
                    d = self.buf[h:h + n]; self.buf = self.buf[h + n:]
                    return op, d
            left = end - time.time()
            if left <= 0:
                return None
            self.s.settimeout(left)
            try:
                c = self.s.recv(65536)
            except socket.timeout:
                return None
            except OSError:
                return ('closed', b'')
            if not c:
                return ('closed', b'')
            self.buf += c

    def wait(self, pred, timeout):
        end = time.time() + timeout
        while time.time() < end:
            m = self.recv(end - time.time())
            if m is None:
                return None
            if m[0] == 'closed' or pred(m):
                return m
        return None

    def close(self):
        try: self.s.close()
        except OSError: pass


def is_frame(m): return m[0] == 2
def is_pong(m): return m[0] == 1 and m[1][:1] == b'P'


@scenario('Gegeben ein Browser am Gerät, wenn er verbindet, dann kommt ein Bild und Pings werden beantwortet')
def s_basic(ctx):
    w = WS(ctx['port'], '/ws'); w.send('f')
    assert w.wait(is_frame, 5), 'kein Bild innerhalb 5 s'
    w.send('p 123')
    assert w.wait(is_pong, 3), 'kein Pong'
    w.close()


@scenario('Gegeben 8 abgerissene Verbindungen (WLAN weg, nie geschlossen), wenn neu verbunden wird, dann geht Bild und Bedienung sofort wieder')
def s_dead_flood(ctx):
    dead = [WS(ctx['port'], '/ws' if i % 2 == 0 else '/wsa') for i in range(8)]
    w = WS(ctx['port'], '/ws'); w.send('f')
    m = w.wait(is_frame, 5)
    assert m and m[0] != 'closed', 'neue Verbindung bekommt kein Bild (Plaetze von toten Verbindungen belegt)'
    w.send('p 1'); assert w.wait(is_pong, 3), 'Bedienung antwortet nicht'
    w.close()
    for d in dead: d.close()


@scenario('Gegeben eine Verbindung ohne Lebenszeichen, wenn die Ruhezeit um ist, dann schliesst das Gerät sie selbst')
def s_idle_close(ctx):
    w = WS(ctx['port'], '/ws')
    m = w.wait(lambda m: False, IDLE_MS / 1000 + 3)
    assert m and m[0] == 'closed', 'stille Verbindung bleibt offen (Thread haengt)'
    w.close()


@scenario('Gegeben Ton im Browser, wenn die Seite jede Sekunde pingt, dann bleibt der Ton länger als die Ruhezeit an')
def s_audio_alive(ctx):
    a = WS(ctx['port'], '/wsa')
    end = time.time() + IDLE_MS / 1000 + 2; pk = 0
    while time.time() < end:
        a.send('p')
        m = a.recv(1.0)
        if m and m[0] == 'closed':
            raise AssertionError('Ton-Verbindung trotz Ping geschlossen')
        if m and m[0] == 2: pk += 1
    assert pk > 0, 'keine Ton-Pakete'
    a.close()


@scenario('Gegeben 20 schnelle Neuverbindungen (Watchdog der Seite), dann läuft der Webserver weiter')
def s_reconnect_storm(ctx):
    for _ in range(20):
        w = WS(ctx['port'], '/ws'); w.send('f'); w.recv(0.05); w.close()
    w = WS(ctx['port'], '/ws'); w.send('f')
    assert w.wait(is_frame, 5), 'nach vielen Neuverbindungen kein Bild'
    w.close()


@scenario('Gegeben PC-Modus wird umgebaut (pc-modus.sh braucht 4 s), wenn man weiter bedient, dann friert das Bild nicht ein')
def s_usb_no_freeze(ctx):
    box = start_box(ctx['exe'], {'settings.cfg': 'usb_mode 1\nusb_audio 0\n',
                                 'pc-modus.sh': 'echo "OK: test" > "$(dirname "$0")/pc-modus.log"; sleep 4\n'})
    try:
        w = WS(box['port'], '/ws'); w.send('f')
        assert w.wait(is_frame, 5), 'kein erstes Bild'
        gap, last, t_end = 0.0, time.time(), time.time() + 7   # Umbau startet 1,5 s nach dem Start
        k = 0
        while time.time() < t_end:
            w.send(('d ' if k % 2 == 0 else 'u ') + 'RIGHT'); k += 1   # Cursor bewegen: jedes Bild ist anders
            m = w.recv(0.2)
            if m and m[0] == 'closed': raise AssertionError('Verbindung abgebrochen')
            if m and m[0] == 2:
                gap = max(gap, time.time() - last); last = time.time()
        gap = max(gap, time.time() - last)
        w.close()
    finally:
        stop_box(box)
    time.sleep(0.3)
    assert any('USB-Modus 1' in l for l in box['log']), 'USB-Umbau lief gar nicht (Test unwirksam)'
    assert gap < 1.5, f'Bild stand {gap:.1f} s still (Hauptschleife blockiert)'


def start_box(exe, files=None):
    tmp = tempfile.mkdtemp(prefix='gb-remote-')
    shutil.copy(exe, os.path.join(tmp, 'groovebox'))
    for n, c in (files or {}).items():
        with open(os.path.join(tmp, n), 'w') as f: f.write(c)
    env = dict(os.environ, GB_HEADLESS='1', GB_NOSPLASH='1', GB_WS_IDLE_MS=str(IDLE_MS), SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='dummy')
    p = subprocess.Popen(['stdbuf', '-oL', os.path.join(tmp, 'groovebox')], cwd=tmp, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    port = None; t0 = time.time(); log = []
    while time.time() - t0 < 20 and port is None:
        line = p.stdout.readline()
        if not line: break
        log.append(line)
        m = re.search(r'Web-Upload auf Port (\d+)', line)
        if m: port = int(m.group(1))
    import threading
    threading.Thread(target=lambda: [log.append(l) for l in p.stdout], daemon=True).start()
    if port is None:
        p.kill(); raise RuntimeError('Webserver nicht gestartet\n' + ''.join(log[-20:]))
    return {'p': p, 'port': port, 'tmp': tmp, 'log': log}


def stop_box(box):
    alive = box['p'].poll() is None
    box['p'].terminate()
    try: box['p'].wait(5)
    except subprocess.TimeoutExpired: box['p'].kill()
    shutil.rmtree(box['tmp'], ignore_errors=True)
    return alive


def main():
    exe = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '../../groovebox'))
    try:
        box = start_box(exe)
    except RuntimeError as e:
        print('FEHLER Start: ' + str(e)); return 1
    ctx = {'port': box['port'], 'exe': exe}
    for sc in (s_basic, s_dead_flood, s_idle_close, s_audio_alive, s_reconnect_storm):
        sc(ctx)
    alive = stop_box(box)
    s_usb_no_freeze(ctx)
    if not alive: results.append((False, 'Gerät läuft nach allen Szenarien noch', 'Programm beendet/abgestuerzt', 0))
    bad = 0
    for ok, name, err, dt in results:
        print(f"{'OK    ' if ok else 'FEHLER'} {name} ({dt:.1f} s){'' if ok else ' -> ' + err}")
        bad += not ok
    print(f"Fernbedienung: {len(results) - bad}/{len(results)} Szenarien ok")
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
