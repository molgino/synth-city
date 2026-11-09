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
    if "$@" >"$log" 2>&1; then echo "OK ($((SECONDS - t0)) s)"; NAMES+=("$name"); RESULTS+=("OK")
    else echo "FEHLER ($((SECONDS - t0)) s) - Log: $log"; NAMES+=("$name"); RESULTS+=("FEHLER"); fail=1; tail -n 25 "$log" | sed 's/^/    /'; fi
}

[ -d samples ] || python3 tools/gensamples.py samples >/dev/null 2>&1   # Test-Samples (nicht im Repo)
stage "Bauen"                                  build.log    make -j"$(nproc)"
stage "Engine-Tests (Klang, Sequencer, Dateien)" unit.log    make test
stage "Bedien-Szenarien (UI-Simulation)"       selftest.log ./groovebox --selftest 20
stage "Tasten-Matrix (jede Taste, jede Seite)"   keymatrix.log ./groovebox --keymatrix tests/keymatrix.golden
stage "Fernbedienung im Browser (Verbindungen)" remote.log  python3 tests/scenarios/remote_scenarios.py ./groovebox
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
