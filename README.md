# SYNTH CITY – a groovebox for the R36S

[![Ko-fi](https://img.shields.io/badge/Ko--fi-noxend-FF5E5B?logo=ko-fi&logoColor=white)](https://ko-fi.com/noxend) [![Licence: MIT](https://img.shields.io/badge/Licence-MIT-blue)](LICENSE)

A pocket groovebox: step sequencer with parameter locks, 15 sound engines, a sampler with chops and time-warp, 3 effect buses and a USB audio interface – on the R36S handheld (its own SYNTH CITY OS or ArkOS 2.0; it also runs on the Linux desktop and as a phone/browser app). Neo-noir look in neon magenta and cyan.

> **How this was made:** SYNTH CITY is vibecoded. It was built with an AI coding assistant (Claude) and tested on real R36S hardware by the author. Reviews and fixes from people who know DSP, embedded Linux or C++ are very welcome.

## 📖 Manual

**[SYNTH-CITY-Handbuch.pdf](https://github.com/molgino/synth-city/releases/download/nightly/SYNTH-CITY-Handbuch.pdf)** (German) – with pictures, generated on every build from [docs/handbuch.md](docs/handbuch.md) (test branches: in their own test release).
Technical data: [docs/SPECS.md](docs/SPECS.md).

## At a glance

* **8 tracks**, 128 patterns in 8 banks (A–H) with up to 128 steps, parameter locks, trigless locks, trig conditions, microtiming, Euclidean and random generators, arpeggiator, chords and up to four notes per step
* **15 engines**: kick, snare, hat, clap, tom, rim, shaker, percussion, analogue synth, FM (4 operators), wavetable (also your own or hand-drawn), sample player, chopper, granulator and drone – with **time-warp** for samples
* **Modulation**: 3 envelopes and 3 LFOs per track with 3 destinations each
* **Effects**: insert per track, 3 freely assignable effect buses (delay, tape, reverb, spring, chorus, shimmer, cloud), compressor with sidechain
* **Sampler**: record from USB, the master, a single track or the built-in mic/input, chops (even or on transients), straight into a track
* **Song mode** as a clip matrix with WAV export, LIVE page with mutes, FILL and recording
* **USB**: audio interface (up to 18 channels) and MIDI to computer/phone, USB host for keyboards/DACs, remote control, touch interface and audio in the browser
* **English and German** user interface

## Quick start

1. **SYNTH CITY OS**: write the image from the release *os-nightly* to a microSD card – or **ArkOS**: copy `r36s-groovebox-arkos.zip` from the release *nightly* into `ports/`.
2. Start it: you begin with an empty project. **A** sets a trig, **START** plays. The demo song "Neon Nights" is on the **PRJ** page → "DEMO SONG".
3. **L2/R2** = view, **▲▼** = control, **L1/R1** = track, **X/Y** = value, **tap SELECT** = MORE menu, **hold SELECT** = key help, **SELECT + B** = undo.

Everything else is in the manual.

## Contributing

SYNTH CITY is open source and contributions are very welcome. Please report bugs and ideas as an [issue](../../issues/new/choose) and send code as a pull request against `main`; [CONTRIBUTING.md](CONTRIBUTING.md) explains how and which tests need to pass. You can help without coding too: test on your R36S (or a clone) and report back.

## Support

SYNTH CITY is free and will stay free. If you enjoy the groovebox, a coffee is much appreciated: **[ko-fi.com/noxend](https://ko-fi.com/noxend)**

## Licence

The SYNTH CITY source code is under the [MIT licence](LICENSE). The SYNTH CITY OS image also contains third-party components with their own licences (Linux kernel and U-Boot under GPL-2.0, Debian packages, SDL2, ALSA …); see [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md) for the overview and the obligations when passing the image on. R36S and other devices mentioned are names of their manufacturers; no affiliation.

## For developers

### Automatic build (GitHub Actions)

Every push to `main` builds the program for the R36S and publishes the package as the **nightly** release. That is what `deploy.ps1` fetches.
A tag such as `v1.0` creates a fixed version release.

### Building it yourself

```bash
# Desktop (for testing)
sudo apt install libsdl2-dev libasound2-dev && make && ./groovebox --windowed

# Cross build for the R36S (on x86 Linux) and package
sudo apt install g++-aarch64-linux-gnu curl zip && ./tools/cross-build.sh && ./tools/package.sh
```

The cross build downloads a small Ubuntu 18.04 arm64 sysroot (glibc 2.27, SDL 2.0.8 headers) into `.sysroot`.
That way the program runs on ArkOS and uses its own SDL2 with KMS/DRM there.
The code deliberately avoids libstdc++ and only depends on libSDL2, libasound, libm and libc, all of which ArkOS ships.

More command-line options:

* `--render out.wav 8`: render the current project offline to WAV.
* `--shot picture.bmp [page]`: take a screenshot.
* `--buffer N`: override the buffer size from the SYS page.

### Layout

* `src/engine.*`: sequencer (sample-accurate, swing, retrigs, polymeter), voices, effect buses, DJ filter, limiter
* `src/params.h`: all parameter definitions and value displays
* `src/samples.*`: WAV loader and sample library
* `src/midi.*`: MIDI thread (ALSA sequencer with auto-connect or rawmidi) and lock-free queues to the audio thread
* `src/input.*`: gamepad with learnable mapping, SDL game controller and keyboard
* `src/main.cpp`: user interface (640×480) and controls
* `src/font.h`: bitmap font, generated by `tools/genfont.py` from DejaVu Sans Mono
