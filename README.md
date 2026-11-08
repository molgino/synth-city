# SYNTH CITY – Groovebox für den R36S

Groovebox für die Hosentasche: Step-Sequencer mit Parameter-Locks, 13 Klangerzeuger, Sampler mit Chops und Time-Warp, 3 Effekt-Busse und ein USB-Audio-Interface – auf dem Handheld R36S (eigenes SYNTH CITY OS oder ArkOS 2.0; läuft auch auf dem Linux-Desktop). Neo-Noir-Optik in Neon-Magenta und Cyan.

## 📖 Handbuch

**[SYNTH-CITY-Handbuch.pdf](https://github.com/molgino/synth-city/releases/download/nightly/SYNTH-CITY-Handbuch.pdf)** – mit Bildern, wird bei jedem Build automatisch aus [docs/handbuch.md](docs/handbuch.md) erzeugt (Test-Branches: im jeweiligen Test-Release).
Technische Daten: [docs/SPECS.md](docs/SPECS.md).

## Auf einen Blick

* **8 Spuren**, 16 Patterns mit bis zu 128 Steps, Parameter-Locks, Trigless Locks, Trig-Bedingungen, Mikrotiming, Euklid- und Zufalls-Generator, Arpeggiator und Akkorde
* **13 Engines**: Kick, Snare, Hat, Clap, Tom, Rim, Shaker, Analog-Synth, FM (4 Operatoren), Wavetable (auch eigene/gezeichnete), Sample-Player, Chopper, Granulator – mit **Time-Warp** für Samples
* **Modulation**: 3 Hüllkurven und 3 LFOs je Spur mit je 3 Zielen
* **Effekte**: Insert je Spur, 3 frei wählbare Effekt-Busse (Delay, Tape, Reverb, Spring, Chorus), Kompressor mit Sidechain
* **Sampler**: Aufnahme von USB, Master oder einzelner Spur, Chops (gleich oder an Transienten), direkt in die Spur
* **Song-Modus** als Clip-Matrix mit WAV-Export, LIVE-Seite mit Mutes, FILL und Aufnahme
* **USB**: Audio-Interface (bis 18 Kanäle) und MIDI zum PC/Handy, USB-Host für Keyboards/DACs, Fernbedienung und Ton im Browser

## Schnellstart

1. **Groovebox-OS**: Image aus dem Release *os-nightly* auf eine microSD-Karte schreiben – oder **ArkOS**: `r36s-groovebox-arkos.zip` aus dem Release *nightly* nach `ports/` kopieren.
2. Starten: Der Demo-Song „Neon Nights“ ist geladen, **START** spielt ihn ab.
3. **SELECT antippen** = Seitenkarte, **L2/R2** = Seite, **L1/R1** = Regler, **X/Y** = Wert, **SELECT halten** = Hilfe.

Alles Weitere steht im Handbuch.

## Für Entwickler

### Automatischer Build (GitHub Actions)

Jeder Push auf `main` baut das Programm für den R36S und legt das Paket als Release **nightly** ab. Genau das holt `deploy.ps1`.
Ein Tag wie `v1.0` erzeugt ein festes Versions-Release.

### Selbst bauen

```bash
# Desktop (zum Testen)
sudo apt install libsdl2-dev libasound2-dev && make && ./groovebox --windowed

# Cross-Build für den R36S (auf x86-Linux) und Paket
sudo apt install g++-aarch64-linux-gnu curl zip && ./tools/cross-build.sh && ./tools/package.sh
```

Der Cross-Build lädt einen kleinen Ubuntu-18.04-arm64-Sysroot (glibc 2.27, SDL 2.0.8 Header) nach `.sysroot`.
Dadurch läuft das Programm auf ArkOS und nutzt dort dessen eigene SDL2 mit KMS/DRM.
Der Code benutzt bewusst keine libstdc++ und hängt nur von libSDL2, libasound, libm und libc ab. All diese Bibliotheken bringt ArkOS mit.

Weitere Kommandozeilen-Optionen:

* `--render out.wav 8`: Das aktuelle Projekt offline als WAV rendern.
* `--shot bild.bmp [seite]`: einen Screenshot erzeugen.
* `--buffer N`: die Puffergröße der SYS-Seite übersteuern.

### Aufbau

* `src/engine.*`: Sequencer (sampelgenau in 16-Sample-Blöcken, Swing, Retrigs, Polymetrie), Stimmen, Delay, Freeverb-Reverb, DJ-Filter, Limiter
* `src/params.h`: alle Parameterdefinitionen und Wertanzeigen
* `src/samples.*`: WAV-Lader und Sample-Bibliothek
* `src/midi.*`: MIDI-Thread (ALSA-Sequencer mit automatischem Verbinden bzw. rawmidi) und lock-freie Queues zum Audio-Thread
* `src/input.*`: Gamepad mit lernbarer Belegung, SDL-GameController und Tastatur
* `src/main.cpp`: Oberfläche (640×480) und Bedienlogik
* `src/font.h`: Bitmap-Schrift, erzeugt mit `tools/genfont.py` aus DejaVu Sans Mono
