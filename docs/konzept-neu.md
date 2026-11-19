# Groovebox-Konzept: ein Kern, drei Hüllen

Stand: 19.11.2026 · Langfassung mit Zeichnungen: Konzeptdokument in Claude
(`https://claude.ai/artifact/LxXTmb5YAYAhcSYdgw8RNs`)

Dieselbe Engine und dasselbe Projekt laufen als Handheld-Gerät, als Handy-Oberfläche und als
CLAP-Plugin — das ist das eine, was SYNTH CITY fast kann und keiner der großen Hersteller anbietet.

## Kernentscheidungen

1. **Das Projekt ist ein Dokument, kein Speicherabbild.** Datei = Liste aus (stabile ID, Wert),
   unbekannte IDs überleben das Laden. SYNTH CITY schreibt `struct Project` roh und braucht darum
   bei jedem Formatwechsel Migrationscode — inzwischen 19 Formate.
2. **Parameter sind Objekte, keine Array-Indizes.** Eine Tabelle (ID, Bereich, Kurve, Einheit,
   Default, lockbar, modulierbar) erzeugt Oberfläche, Touch-Modell, Plugin-Parameter, MIDI-Learn,
   Handbuch und Tests. Heute steht dieselbe Information an vier Stellen.
3. **Zeit rechnet in Samples, nicht in Ticks.** PPQN nur für Anzeige und Sync.
4. **Die Thread-Grenze steht vor der ersten Zeile DSP.** Lock-freie Queue statt
   `SDL_LockAudioDevice`: heute hält jede Pattern-Änderung den Audio-Callback an.
5. **Machines sind ein Interface, keine `switch`-Kaskade.**

## Aufbau

Oberflächen (Gerät, Handy/Browser, CLAP→VST3/AU) kennen nur **ein Modell**; der Kern kennt keine
Oberfläche. Dazwischen die Thread-Grenze: lock-freie Queue hinunter, Zustand hinauf, kein Mutex.
Kern = Dokument + IDs, Sequencer, Scheduler (Samples), Parameter-Tabelle, darunter
Voices → Machines → Filter → Amp → FX-Busse A/B/C → Master.

## Datenmodell

Hierarchie wie gehabt: **Projekt → Pattern → Track → Step**. Ein Step trägt Note, Velocity, Länge,
Micro-Timing, Condition, Ratchet und Locks `(ParamId, Wert, slide?)`. Wirksamer Wert immer:
**Lock → Scene-Morph → Track-Grundwert**, darauf Modulation. Ein neuer Parameter ist eine Zeile
und erscheint von selbst überall.

## Sequencer (von Tag 1)

| Fertigkeit | Bringt | Warum von Anfang |
|---|---|---|
| Conditions, Wahrscheinlichkeit **als** Condition | ein Zufalls-Trig steuert eine Gruppe über `PRE` | Auswertungsreihenfolge im Scheduler |
| Slides zwischen Locks | Acid, Filterfahrten, Glides | Wert als Rampe, nicht als Stufe |
| Scenes + Morph | ein Regler fährt durch ein ganzes Setting | zweite Ebene in der Auflösungskette |
| Polymetrik je Track | 16 gegen 12 Steps | Zähler pro Track |
| Sub-Steps/Ratchets | Rolls, Flams, Triolen | mehrere Events pro Step |
| Performance-Ebene | Fill, Mute, Step-Loop, Punch-in | darf Daten nie ändern |

`PRE`/`NEI` sehen durch Trigs ohne Condition hindurch; die Uhr bleibt stehen, wo sie steht.

## Was die Hersteller richtig machen

| Gerät | Tragende Idee | Übernehmen? |
|---|---|---|
| Elektron | feste Seiten, wechselnde Bedeutung; „halten + drehen" überall | ja |
| Roland | 16 Steps immer sichtbar, Modus-Tasten darüber | ja |
| Teenage Engineering | Farbe ist die Navigation | teilweise |
| Deluge | eine Fläche, Zoomstufen statt Umschalten | ja |
| M8 / Polyend Tracker | Tracker-Spalten, blind bedienbar | nein (Lernkurve) |
| MPC / Push | Beschriftung am Rand wandert mit | ja |
| Squarp Hapax | Überblick und Detail gleichzeitig | ja |

## Bedienkonzept

Vier feste Zonen, auf jedem Schirm dieselben, nur anders gefaltet: Transport oben, Spuren links,
**Arbeitsfläche** in der Mitte (Song → Steps → Regler, eine Fläche mit Zoomstufen),
Kontext rechts, **16 Steps unten — immer sichtbar**, Hilfe darunter.

1. Jede Taste tut überall dasselbe (durch die Tastenmatrix abgenommen).
2. Eine Geste fürs Häufigste: halten + ändern = Lock.
3. Kein Menü für etwas, das man im Takt braucht.
4. Namen statt Zahlen.
5. Sofortiges Feedback.
6. Fehler sagen, was fehlt.

Farbe trägt Bedeutung (Spur, läuft, gelockt), sonst grau: Kontrast vor Buntheit.

## Technik und Tests

C++17-Kern ohne Abhängigkeiten; **DSP aus SYNTH CITY portieren, nicht neu schreiben**
(TPT/ZDF-Filter, PolyBLEP, FDN-Hall, Drum-Engines, dB-Skala). Plugin zuerst **CLAP**, VST3/AU über
`clap-wrapper`. Oberfläche als eigene Schicht gegen das Modell. Standalone über miniaudio/SDL3,
Embedded-Pfad offen.

Tests ab Tag 1: Offline-Render gegen Referenz-WAV, Determinismus, kopflose Sequencer-Szenarien,
Tasten-/Aktionsmatrix als Golden, `clap-validator` + `pluginval` in der CI. Regel: **jeder
gemeldete Fehler bekommt ein Szenario, erst rot sehen.**

## Reihenfolge

1. Kern + Datenmodell ohne Klang → ein Projekt lädt, nachdem Parameter dazukamen.
2. Eine Machine, ein Track, Audio → portierter DSP klingt bitgleich zum alten Render.
3. Sequencer vollständig → die musikalischen Entscheidungen tragen.
4. CLAP-Plugin → das Datenmodell war richtig (fast kein Extra-Code).
5. Oberfläche gegen das Modell → ein Modell trägt Touch und Gamepad.
6. Gerät (R36S, Sampler, Dateien) → der Kern passt ins CPU-Budget des A35.

Schritt 4 ist der Prüfstein: wird das Plugin mühsam, stimmt das Datenmodell nicht — und dann ist
es noch billig zu ändern.
