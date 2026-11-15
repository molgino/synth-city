# Contributing to SYNTH CITY

Thanks for helping! English or German – both are welcome.

## Reporting bugs
[Open an issue](https://github.com/molgino/synth-city/issues/new/choose): where (SYNTH CITY OS, ArkOS, desktop, phone app), which build (SET → Info), steps to reproduce. Screenshots and logs (device web page → Logs) help a lot.

## Contributing code
1. Fork the repository and create a branch (new features ideally on a `ui-…` branch: CI builds a separate test release for it).
2. Build and test on the Linux desktop:
   ```sh
   sudo apt install libsdl2-dev libasound2-dev
   make && ./groovebox --windowed
   make test                      # engine tests
   ./groovebox --selftest 20      # UI scenarios
   tools/test-all.sh              # everything at once, report in dist/testbericht.md
   ```
   Before bigger changes please also run `make SAN=1` with `--selftest 60` – several bugs only showed up under the sanitizers.
3. Open a pull request against `main`. GitHub Actions builds for the R36S and runs the tests.

The project is vibecoded (built with an AI coding assistant and tested on hardware), so careful reviews of existing code are just as welcome as new features.

## Rules
- **Every reported bug gets a scenario** in the self-test that fails on the old code.
- **Project format:** structural changes bump the version and get a migration in `Engine::load`; old projects must sound the same.
- **Only append to enums** (engines, filter types, pages …).
- **Real time:** no memory allocation, no locks, no file access in the audio thread.
- **R36S cross build without libstdc++:** C headers only, no exceptions or RTTI.
- **New texts** always with `TR(de, en)`.
- **No third-party trademarks, samples or logos**; own sounds only.

By opening a pull request you agree that your contribution is licensed under the [MIT licence](LICENSE).
