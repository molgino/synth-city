#!/usr/bin/env python3
"""SYNTH CITY als Handy-App (PWA, offline): WebAssembly-Build + Bedienseite -> dist/webapp/

Aufruf (im Repo, emcc im PATH, z. B. nach `source emsdk_env.sh`):
    python3 tools/webapp/build.py [--out dist/webapp] [--debug]

Die Bedienseite ist dieselbe wie /screen am Geraet (WEB_SCREEN in src/webpage.cpp); web/local.js schaltet sie
auf die eingebaute Groovebox um. Ergebnis: index.html, synthcity.js/.wasm, local.js, sync.js, manifest.json, sw.js, Icons.
Zum Testen: python3 -m http.server -d dist/webapp 8000  ->  http://localhost:8000
"""
import argparse, hashlib, json, os, re, shutil, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = ["src/main.cpp", "src/engine.cpp", "src/input.cpp", "src/samples.cpp", "src/fxbus.cpp", "web/webapp.cpp"]


def cstring(path, name):
    s = open(os.path.join(ROOT, path), encoding="utf-8").read()
    m = re.search(name + r'\[\] = R"HTML\((.*?)\)HTML";', s, re.S)
    if not m:
        sys.exit(f"{name} nicht gefunden in {path}")
    return m.group(1)


def icons():
    s = open(os.path.join(ROOT, "src/webicons.cpp"), encoding="utf-8").read()
    out = {}
    for n, body in re.findall(r"WEB_ICON(\d+)\[\] = \{(.*?)\};", s, re.S):
        out[n] = bytes(int(x) for x in re.findall(r"\d+", body))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "dist", "webapp"))
    ap.add_argument("--debug", action="store_true", help="ohne Optimierung, mit Pruefungen")
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)

    # 1) WebAssembly
    rev = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip() or "dev"
    opt = ["-O0", "-g", "-sASSERTIONS=2"] if a.debug else ["-O3"]
    cmd = ["emcc", *SRC, "-Isrc", "-Iweb/compat", "-sUSE_SDL=2", *opt,
           "-fno-exceptions", "-fno-rtti", "-fno-math-errno", "-DWITH_MIDI", "-Wno-unused-command-line-argument",
           "-sASYNCIFY", "-sASYNCIFY_IGNORE_INDIRECT", "-sASYNCIFY_STACK_SIZE=65536",
           "-sALLOW_MEMORY_GROWTH", "-sINITIAL_MEMORY=96MB", "-sSTACK_SIZE=1MB", "-sENVIRONMENT=web",
           "-lidbfs.js", "-sFORCE_FILESYSTEM",
           "-sEXPORTED_FUNCTIONS=_main,_malloc,_free",
           "-sEXPORTED_RUNTIME_METHODS=FS,IDBFS,stringToUTF8,UTF8ToString,HEAPU8,HEAPU32,addRunDependency,removeRunDependency",
           "-o", os.path.join(out, "synthcity.js")]
    print("emcc ...")
    r = subprocess.run(cmd, cwd=ROOT)
    if r.returncode:
        sys.exit("emcc fehlgeschlagen")
    with open(os.path.join(out, "VERSION"), "w") as f:
        f.write(rev + "\n")

    # 2) Bedienseite: WEB_SCREEN + Umschaltung auf die eingebaute Groovebox
    page = cstring("src/webpage.cpp", "WEB_SCREEN")
    page = page.replace('href="/manifest.json"', 'href="manifest.json"').replace('href="/apple-touch-icon.png"', 'href="icon-180.png"')
    page = page.replace('href="/icon-192.png"', 'href="icon-192.png"').replace("<title>SYNTH CITY – Fernbedienung</title>", "<title>SYNTH CITY</title>")
    marker = "<script>\n'use strict';"
    if page.count(marker) != 1:
        sys.exit("Skript-Anfang der Bedienseite nicht gefunden")
    page = page.replace(marker, '<canvas id="gbsdl" width="1" height="1" style="display:none"></canvas>\n'
                        '<script src="local.js"></script>\n<script src="sync.js"></script>\n' + marker)
    open(os.path.join(out, "index.html"), "w", encoding="utf-8").write(page)
    for f in ("local.js", "sync.js"):
        shutil.copy(os.path.join(ROOT, "web", f), os.path.join(out, f))
    for n, data in icons().items():
        open(os.path.join(out, f"icon-{n}.png"), "wb").write(data)

    manifest = {"name": "SYNTH CITY", "short_name": "SYNTH CITY", "start_url": "./", "scope": "./", "display": "fullscreen",
                "orientation": "landscape", "background_color": "#080612", "theme_color": "#080612",
                "icons": [{"src": "icon-192.png", "sizes": "192x192", "type": "image/png"},
                          {"src": "icon-512.png", "sizes": "512x512", "type": "image/png"}]}
    json.dump(manifest, open(os.path.join(out, "manifest.json"), "w"), indent=1)

    # 3) Service Worker: alle Dateien offline, Version = Inhalt (neue Version -> neuer Cache)
    files = ["./", "index.html", "local.js", "sync.js", "synthcity.js", "synthcity.wasm", "manifest.json", "VERSION",
             "icon-180.png", "icon-192.png", "icon-512.png"]
    h = hashlib.sha256()
    for f in files[1:]:
        h.update(open(os.path.join(out, f), "rb").read())
    ver = h.hexdigest()[:12]
    sw = open(os.path.join(ROOT, "web", "sw.js"), encoding="utf-8").read()
    sw = sw.replace("__VERSION__", ver).replace("__FILES__", json.dumps(files))
    open(os.path.join(out, "sw.js"), "w", encoding="utf-8").write(sw)
    size = sum(os.path.getsize(os.path.join(out, f)) for f in files[1:])
    print(f"Web-App: {out} ({size // 1024} KB, Version {rev}/{ver})")


if __name__ == "__main__":
    main()
