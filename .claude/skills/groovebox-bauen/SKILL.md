---
name: groovebox-bauen
description: Architektur- und Bedienregeln für Groovebox-, Sequencer- und Synth-Software in diesem Repo (und für den geplanten Nachfolger). Nutze diesen Skill, wenn Engine, Sequencer, Parameter, Projektformat, Oberfläche oder Plugin-Export entworfen oder umgebaut werden — auch bei kleinen Änderungen an Steps, Locks, Conditions, Scenes oder am Touch-Modell.
---

# Groovebox bauen: Architektur und Bedienung

Diese Regeln sind aus SYNTH CITY gelernt (was teuer war) und aus den Handbüchern von
Elektron, Roland, Teenage Engineering, Synthstrom und Squarp (was funktioniert).
Konzeptdokument: siehe `docs/konzept-neu.md`.

## Architektur — fünf Festlegungen

1. **Projekt = Dokument, kein Speicherabbild.** Datei ist eine Liste aus (stabile ID, Wert);
   unbekannte IDs überleben das Laden unverändert. `struct` roh schreiben erzwingt bei jeder
   Erweiterung eine Migration — in SYNTH CITY 19 Formate mit je eigenem Migrationscode.
2. **Parameter sind Objekte, keine Array-Indizes.** Eine Deklaration je Parameter (ID, Bereich,
   Kurve, Einheit, Textform, Default, lockbar, modulierbar). Daraus werden Oberfläche,
   Touch-Modell, Plugin-Parameterliste, MIDI-Learn, Hilfe und Tests erzeugt — nie dieselbe
   Information an zwei Stellen pflegen.
3. **Zeit rechnet in Samples.** Der Scheduler plant Events auf Sample-Positionen, PPQN ist
   Anzeige und externer Sync. Micro-Timing, Swing, Ratchets und Slides fallen dann aus der
   Architektur statt nachgerüstet zu werden.
4. **Thread-Grenze vor dem ersten DSP.** Audio liest, die Oberfläche schreibt über eine
   lock-freie SPSC-Queue, große Objekte wechseln per atomarem Zeigertausch (Freigabe im
   Hintergrund). **Nie** `SDL_LockAudioDevice` o. Ä. aus der Oberfläche: das hält den
   Audio-Callback an und erzeugt Aussetzer.
5. **Machines sind ein Interface.** Engine, Filter, Insert, Bus-Effekt an derselben schmalen
   Schnittstelle in einer Registry; eine neue Machine = eine Datei + eine Registrierung.

## Echtzeit (nicht verhandelbar)

Im Audio-Thread: keine Allokation, kein Lock, kein I/O, kein Logging, keine Exception.
Alles in `prepare(sr, maxBlock)` vorallozieren. Variable Blockgrößen annehmen (auch 1).
Parameter rampen (5–50 ms), sonst Zipper-Noise. Denormals abschalten. Bei Sampleratenwechsel
alle Koeffizienten neu rechnen, nichts auf 44,1 kHz hart codieren.

## Sequencer — was vollständig sein muss

- **Conditions** inkl. **Wahrscheinlichkeit als Condition** (nicht daneben), damit `PRE` darauf
  reagieren kann.
- **`PRE`/`NEI` sehen durch Trigs ohne Condition hindurch**, und `PRE` selbst zählt nicht als
  ausgewertet. Nur so steuert ein Zufalls-Trig eine ganze Gruppe.
- **Slides** zwischen zwei Locks (Wert als Rampe, nicht als Stufe).
- **Scenes + Morph**: zweite Werte-Ebene in der Kette `Lock → Scene → Track-Grundwert`.
- **Polymetrik** je Track (eigener Durchlaufzähler!), Sub-Steps/Ratchets.
- **Performance-Ebene ändert nie die Daten**: Fill, Mute, Step-Loop, Punch-in. Die Uhr läuft
  dabei weiter, nur die gelesene Quelle wird umgelenkt.

## Bedienung — sechs Regeln

1. **Wenige feste Zonen, die nie verschwinden** (Transport, Spuren, Arbeitsfläche, Steps, Hilfe).
2. **Eine Fläche mit Zoomstufen** statt vieler Seiten: Song → Pattern → Steps → Regler.
3. **Jede Taste tut überall dasselbe** — mit einer Tasten-/Aktionsmatrix als Golden-Datei belegt.
4. **Eine Geste für das Häufigste**: halten + ändern = Lock (Taste+Drehen, Pad+Ziehen, Klick+Schrauben).
5. **Namen statt Zahlen** (`FLT FREQ`, Dateiname des Samples), **sofortiges Feedback**,
   **Fehlermeldungen sagen, was fehlt**.
6. **Farbe trägt Bedeutung**: Spurfarbe, „läuft", „gelockt" — sonst grau. Kontrast vor Buntheit.

Für Touch zusätzlich: in Scrollbereichen ist `pointerup` **kein** verlässliches Tap-Signal
(der Browser bricht mit `pointercancel` ab) — immer zusätzlich `click` auswerten; senkrechtes
Wischen gehört dem Browser, waagerechtes dem Wert; Flächen ≥ 44 px.

## Tests

- Offline-Render gegen Referenz-WAV, Determinismus, kopflose Sequencer-Szenarien.
- Tasten-/Aktionsmatrix als Golden-Datei; Diff nach jeder Änderung **lesen**, nicht blind updaten.
- Sanitizer-Lauf (ASan/UBSan) vor jedem Push nach main.
- **Jeder gemeldete Fehler bekommt ein Szenario — und das wird erst am alten Stand rot gesehen.**
- Plugin: `clap-validator`, `pluginval` (Strictness 5+), Test in mindestens zwei Hosts.
