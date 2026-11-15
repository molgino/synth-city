# Mitmachen bei SYNTH CITY

Danke fürs Mithelfen! Deutsch oder Englisch – beides ist willkommen.

## Fehler melden
[Issue anlegen](https://github.com/molgino/synth-city/issues/new/choose): wo (SYNTH CITY OS, ArkOS, Desktop, Handy-App), welcher Build (SET → Info), Schritte zum Nachstellen. Bildschirmfotos und Logs (Web-Seite des Geräts → Logs) helfen sehr.

## Code beitragen
1. Fork anlegen, Branch erstellen (neue Funktionen gern als `ui-…`-Branch: CI baut dafür ein eigenes Test-Release).
2. Am Linux-Desktop bauen und testen:
   ```sh
   sudo apt install libsdl2-dev libasound2-dev
   make && ./groovebox --windowed
   make test                      # Engine-Tests
   ./groovebox --selftest 20      # Bedien-Szenarien
   tools/test-all.sh              # alles auf einmal, Bericht in dist/testbericht.md
   ```
   Vor größeren Änderungen bitte auch `make SAN=1` mit `--selftest 60` laufen lassen – mehrere Fehler fielen nur unter den Sanitizern auf.
3. Pull Request gegen `main`. GitHub Actions baut für den R36S und führt die Tests aus.

## Regeln
- **Jeder gemeldete Fehler bekommt ein Szenario** im Selbsttest, das am alten Stand rot ist.
- **Projektformat:** Strukturänderungen erhöhen die Version und bekommen eine Migration in `Engine::load`; alte Projekte müssen gleich klingen.
- **Enums nur hinten anfügen** (Engines, Filtertypen, Seiten …).
- **Echtzeit:** im Audio-Thread keine Speicheranforderung, keine Sperren, keine Datei-Zugriffe.
- **R36S-Cross-Build ohne libstdc++:** nur C-Header, keine Exceptions/RTTI.
- **Neue Texte** immer mit `TR(de, en)`.
- **Keine fremden Marken, Samples oder Logos**; eigene Klänge.

Mit einem Pull Request stimmst du zu, dass dein Beitrag unter der [MIT-Lizenz](LICENSE) steht.
