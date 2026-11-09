#!/usr/bin/env bash
# Alle Tests auf Knopfdruck: bauen, Engine-Tests, Bedien-Szenarien (UI-Simulation), Fernbedienung (Browser-Verbindung).
# Aufruf:  tools/test-all.sh            (schnell, ~1 min)
#          Tasten-Matrix nach gewollter Aenderung neu abnehmen: ./groovebox --keymatrix tests/keymatrix.golden --update  (git diff ansehen!)
#          tools/test-all.sh --voll     (zusaetzlich mit Sanitizern = findet Speicherfehler, dauert laenger)
# Ergebnis: Zusammenfassung auf der Konsole + dist/testbericht.md (auf GitHub auch in der Job-Zusammenfassung).
# Exit-Code 0 = alles gruen.
set -u
cd "$(dirname "$0")/.."
VOLL=0; [ "${1:-}" = "--voll" ] && VOLL=1
mkdir -p dist
LOGDIR=$(mktemp -d)
REPORT=dist/testbericht.md
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
declare -a NAMES RESULTS
fail=0

stage() {   # stage "Name" logdatei befehl...
    local name="$1" log="$LOGDIR/$2"; shift 2
    printf '%-46s' "$name ..."
    local t0=$SECONDS
    # Zeitlimit: haengt eine Stufe, ist das ein Fehler (statt ewig zu warten)
    if timeout 900 "$@" >"$log" 2>&1; then echo "OK ($((SECONDS - t0)) s)"; NAMES+=("$name"); RESULTS+=("OK")
    else echo "FEHLER ($((SECONDS - t0)) s) - Log: $log"; NAMES+=("$name"); RESULTS+=("FEHLER"); fail=1; tail -n 25 "$log" | sed 's/^/    /'; fi
}

[ -d samples ] || python3 tools/gensamples.py samples >/dev/null 2>&1   # Test-Samples (nicht im Repo)
stage "Bauen"                                  build.log    make -j"$(nproc)"
stage "Engine-Tests (Klang, Sequencer, Dateien)" unit.log    make test
stage "Bedien-Szenarien (UI-Simulation)"       selftest.log ./groovebox --selftest 20
stage "Tasten-Matrix (jede Taste, jede Seite)"   keymatrix.log ./groovebox --keymatrix tests/keymatrix.golden
stage "Webserver (Upload, Explorer, Sync-API)"     webapi.log  python3 tests/scenarios/web_api_scenarios.py ./groovebox
stage "Fernbedienung im Browser (Verbindungen)" remote.log  python3 tests/scenarios/remote_scenarios.py ./groovebox
if [ -f dist/webapp/index.html ] && node -e "require('playwright')" 2>/dev/null; then
    stage "Handy-App (echte Taps im Browser)"  app.log      node tests/app/app_scenarios.js dist/webapp
else
    echo "Handy-App: uebersprungen (erst tools/webapp/build.py; laeuft immer auf GitHub)"; touch "$LOGDIR/app.log"
fi
if [ $VOLL = 1 ]; then
    stage "Engine-Tests mit Sanitizern"        unit-san.log make test SAN=1
    stage "Mehrkern-Audio (ThreadSanitizer)"   tsan.log     make test-tsan
fi

{
    echo "# Testbericht SYNTH CITY"
    echo
    echo "Stand: $(git log -1 --format='%h %s' 2>/dev/null) – $(date '+%Y-%m-%d %H:%M')"
    echo
    echo "| Stufe | Ergebnis |"
    echo "|---|---|"
    for i in "${!NAMES[@]}"; do echo "| ${NAMES[$i]} | ${RESULTS[$i]} |"; done
    echo
    echo "## Bedien-Szenarien"
    echo
    sed -n '/^SZENARIEN/,/UI-Pruefungen/p' "$LOGDIR/selftest.log" | grep -E '^\s+(OK|FEHLER)' | sed -E 's/^\s+OK\s+/- ✅ /; s/^\s+FEHLER\s+/- ❌ /'
    grep -E '^\s+FEHLER \[' "$LOGDIR/selftest.log" | sed 's/^ */    - /'
    echo
    echo "## Tasten-Matrix"
    echo
    grep -E "^Tasten-Matrix:|^(OK|FEHLGESCHLAGEN):" "$LOGDIR/keymatrix.log" | sed 's/^/    /'
    grep -E "ABWEICHUNG|Soll:|Ist:|NEU |REGEL" "$LOGDIR/keymatrix.log" | head -60 | sed 's/^/    /'
    echo
    echo "## Abdeckung"
    echo
    echo "| Bereich | Fälle |"
    echo "|---|---|"
    G=tests/keymatrix.golden
    echo "| Jede Seite × jede Taste/Kombination | $(grep -c '^Seite ' $G) |"
    echo "| Quell-Seiten je Engine (13 Engines) | $(grep -cE '^(SRC|SR2|SR3|SR4|SR5|OSC|OP[0-9]?)[^|]* auf ' $G) |"
    echo "| Fenster (Karte, PAGE-Menü, Listen, Tastatur, Abfrage, Editoren) | $(grep -cE '^(Seitenkarte|PAGE-Menue|Liste |Tastatur|Abfrage|Sample-Editor|Wavetable-Editor)' $G) |"
    echo "| Jeder SET-Eintrag | $(grep -c '^SET ' $G) |"
    echo "| PRJ-Felder, Song-Spalten, Mixer-Zeilen, Sampler-Regler | $(grep -cE '^(PRJ|SONG|MIX|SAMPLER) ' $G) |"
    echo "| Sonderlagen (Lock-Modus, spielt, Page 2, Ablagen voll, Undo) | $(grep -cE '^TRIG ' $G) |"
    echo "| **Tasten-Matrix gesamt** | **$(grep -vc '^#' $G)** |"
    echo
    echo "## Webserver"
    echo
    grep -E '^(OK|FEHLER)' "$LOGDIR/webapi.log" | sed -E 's/^OK +/- ✅ /; s/^FEHLER /- ❌ /'
    echo
    echo "## Handy-App"
    echo
    grep -E '^(OK|FEHLER)' "$LOGDIR/app.log" | sed -E 's/^OK +/- ✅ /; s/^FEHLER /- ❌ /'
    echo
    echo "## Fernbedienung (Browser)"
    echo
    grep -E '^(OK|FEHLER)' "$LOGDIR/remote.log" | sed -E 's/^OK +/- ✅ /; s/^FEHLER /- ❌ /'
    echo
    echo "## Engine-Tests"
    echo
    tail -n 1 "$LOGDIR/unit.log" | sed 's/^/    /'
    grep -E 'FEHLER' "$LOGDIR/unit.log" | head -20 | sed 's/^/    /'
} > "$REPORT"
[ -n "${GITHUB_STEP_SUMMARY:-}" ] && cat "$REPORT" >> "$GITHUB_STEP_SUMMARY"
echo
echo "Bericht: $REPORT"
[ $fail = 0 ] && echo "ALLES GRUEN" || echo "ES GIBT FEHLER (siehe oben)"
exit $fail
