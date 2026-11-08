#!/usr/bin/env python3
"""SYNTH CITY Handbuch: docs/handbuch.md -> PDF mit Screenshots aus der App.

Aufruf (im Repo, nach `make`):
    python3 tools/handbuch/build.py [--groovebox ./groovebox] [--out dist/SYNTH-CITY-Handbuch.pdf] [--html]

Bilder: im Markdown `![Bildunterschrift](shot:N)` -> `./groovebox --shot bild.bmp N` (N wie bei --shot).
Braucht: python3-markdown, Pillow, Chromium/Chrome (headless, Druck nach PDF).
"""
import argparse, datetime, html, os, re, shutil, subprocess, sys, tempfile

import markdown
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
HERE = os.path.dirname(os.path.abspath(__file__))


def find_chrome():
    for c in (os.environ.get("CHROME"), "google-chrome", "google-chrome-stable", "chromium", "chromium-browser",
              "/opt/pw-browsers/chromium-1243/chrome-linux64/chrome"):
        if c and (shutil.which(c) or os.path.isfile(c)):
            return shutil.which(c) or c
    for d in ("/opt/pw-browsers",):   # Playwright-Browser (Entwicklungs-Container)
        for root, _, files in os.walk(d):
            if "chrome" in files and root.endswith("chrome-linux64"):
                return os.path.join(root, "chrome")
    sys.exit("Kein Chrome/Chromium gefunden (Umgebungsvariable CHROME setzen)")


def shot(gb, n, out_png, env):
    bmp = out_png[:-4] + ".bmp"
    subprocess.run([gb, "--shot", bmp, str(n)], cwd=ROOT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60)
    if not os.path.isfile(bmp):
        print(f"  Screenshot {n} fehlgeschlagen", file=sys.stderr)
        return False
    Image.open(bmp).convert("RGB").save(out_png, optimize=True)
    os.remove(bmp)
    return True


def fix_lists(md):
    """Das Handbuch rueckt verschachtelte Listen mit 2 Leerzeichen ein, Python-Markdown braucht 4.
    Eingerueckte ```-Bloecke (in Listen) werden zu eingerueckten Code-Bloecken (die versteht es in Listen)."""
    out, fence, fenceInd = [], False, 0
    for line in md.split("\n"):
        st = line.lstrip()
        ind = len(line) - len(st)
        if st.startswith("```"):
            if not fence:
                fence, fenceInd = True, ind
                if ind:
                    out.append("")
                    continue
            else:
                fence = False
                if fenceInd:
                    out.append("")
                    continue
            out.append(line)
            continue
        if fence:
            out.append(" " * (fenceInd * 2 + 4) + line[fenceInd:] if fenceInd else line)
            continue
        if ind == 0 and re.match(r"([*-]|\d+\.) ", st) and out and out[-1].strip() and not re.match(r"\s*([*-]|\d+\.) |\s", out[-1]):
            out.append("")   # Liste direkt nach einem Absatz: Python-Markdown braucht eine Leerzeile davor
        if ind and ind % 2 == 0 and ind <= 8:
            line = " " * (ind * 2) + st
        elif ind == 3:   # "1.  Text" - Fortsetzung eines nummerierten Punkts
            line = " " * 4 + st
        out.append(line)
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--groovebox", default=os.path.join(ROOT, "groovebox"))
    ap.add_argument("--src", default=os.path.join(ROOT, "docs", "handbuch.md"))
    ap.add_argument("--out", default=os.path.join(ROOT, "dist", "SYNTH-CITY-Handbuch.pdf"))
    ap.add_argument("--html", action="store_true", help="HTML neben dem PDF behalten (zum Ansehen)")
    a = ap.parse_args()

    work = tempfile.mkdtemp(prefix="handbuch-")
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", GB_NOSPLASH="1")
    md = open(a.src, encoding="utf-8").read()
    md = re.sub(r"^# .*\n", "", md, count=1)   # Titel steht auf der Titelseite

    # Screenshots erzeugen
    shots = sorted(set(re.findall(r"\(shot:(\d+)\)", md)), key=int)
    print(f"Screenshots: {', '.join(shots)}")
    ok = {n for n in shots if shot(a.groovebox, n, os.path.join(work, f"shot-{n}.png"), env)}
    cover_ok = shot(a.groovebox, 40, os.path.join(work, "cover.png"), env)

    body = markdown.markdown(fix_lists(md), extensions=["tables", "fenced_code", "toc", "sane_lists", "attr_list"],
                             extension_configs={"toc": {"toc_depth": "2-2", "permalink": False}})
    # Bilder als Abbildungen mit Unterschrift
    def fig(m):
        alt, n = html.unescape(m.group(1)), m.group(2)
        if n not in ok:
            return ""
        return f'<figure><img src="shot-{n}.png" alt="{html.escape(alt)}"><figcaption>{html.escape(alt)}</figcaption></figure>'
    body = re.sub(r'<p><img alt="([^"]*)" src="shot:(\d+)" ?/?></p>', fig, body)
    body = re.sub(r'<img alt="([^"]*)" src="shot:(\d+)" ?/?>', fig, body)

    # Inhaltsverzeichnis aus den H2-Ueberschriften
    toc = "".join(f'<li><a href="#{i}">{t}</a></li>' for i, t in re.findall(r'<h2 id="([^"]+)">(.*?)</h2>', body))

    rev = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    date = datetime.date.today().strftime("%d.%m.%Y")
    css = open(os.path.join(HERE, "style.css"), encoding="utf-8").read()
    cover_img = '<img class="cover-img" src="cover.png" alt="">' if cover_ok else ""
    page = f"""<!doctype html><html lang="de"><head><meta charset="utf-8"><title>SYNTH CITY Handbuch</title>
<style>{css}</style></head><body>
<section class="cover">
  <div class="cover-top"><div class="kicker">GROOVEBOX FÜR DEN R36S</div>
  <h1 class="cover-title">SYNTH CITY</h1><div class="cover-sub">Handbuch</div></div>
  {cover_img}
  <div class="cover-meta">Version {rev or "dev"} · {date}</div>
</section>
<section class="toc"><h2 class="toc-title">Inhalt</h2><ol>{toc}</ol></section>
<main>{body}</main>
</body></html>"""
    hp = os.path.join(work, "handbuch.html")
    open(hp, "w", encoding="utf-8").write(page)

    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    chrome = find_chrome()
    cmd = [chrome, "--headless", "--no-sandbox", "--disable-gpu", "--no-pdf-header-footer",
           f"--print-to-pdf={os.path.abspath(a.out)}", "file://" + hp]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    if not os.path.isfile(a.out):
        print(r.stdout, r.stderr, file=sys.stderr)
        sys.exit("PDF wurde nicht erzeugt")
    if a.html:
        d = os.path.splitext(a.out)[0] + "-html"
        shutil.rmtree(d, ignore_errors=True); shutil.copytree(work, d)
        print(f"HTML: {d}/handbuch.html")
    shutil.rmtree(work, ignore_errors=True)
    print(f"PDF: {a.out} ({os.path.getsize(a.out) // 1024} KB)")


if __name__ == "__main__":
    main()
