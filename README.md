# UBMaster — by unique & MELO

Mastering-Suite in einem Plugin · UNIQUEBABY-Software-Familie neben [UBGrade](https://ubgrade.uniquebaby.de) und [UBTune](https://ubtune.uniquebaby.de) · [uniquebaby.de](https://uniquebaby.de)

**Webseite & Download:** https://ubmaster.uniquebaby.de · [Releases](https://github.com/gelounique/ubmaster/releases/latest)

## Signalkette
| # | Modul | Was es macht |
|---|---|---|
| 1 | **Master-EQ** | 6 Bänder (Low Cut 24 dB/Okt, Low Shelf, 2 Bells, High Shelf, High Cut) · Modi **Global / Mid-Side / Links-Rechts**, jedes Band auf beide, Mid/Links oder Side/Rechts · Punkte im Graph ziehen, Mausrad = Q, Doppelklick = an/aus, Rechtsklick = Kanal |
| 2 | **Tape-Sim** | Band-Sättigung (2× Oversampling) **nur im gewählten Frequenzbereich** – Drive, Character (gerade Obertöne), Mix. Arbeitet auf dem Verzerrungs-Residuum → Drive 0 ist bit-transparent, kein Crossover-Phasengang |
| 3 | **Transient** | Adaptiver, spektraler Transienten-Prozessor (STFT, 48 log-Bänder): Attacks pro Frequenzband **cutten oder boosten**, Amount, Sensitivity, Speed, Frequenzbereich, Mix, **Delta** (nur die Änderung hören) |
| 4 | **Stereo-Tool** | Breite in **Low / Mid / High** (0–200 %), komplementäre Bänder (100 % = bit-identisch), Vectorscope + Korrelation |
| 5 | **Exciter** | Obertöne ab wählbarer Frequenz, Tube (gerade) ↔ Tape (ungerade), 2× Oversampling |
| 6 | **Limiter** | True-Peak-Brickwall, 1,5 ms Lookahead, **8×-Peak-Erkennung**, Gain, Ceiling, Release, GR-Verlauf |
| 7 | **Loudness** | ITU-R BS.1770-4 / EBU R128: Integrated (gated), Short-term, Momentary, True Peak, LRA · Modi **Spotify (−14) · Apple Music (−16) · YouTube (−14) · SoundCloud (−14) · Custom** · Empfehlungsbox + **„Auf Ziel bringen“** |

Latenz: ~12 ms (Transient-STFT + Limiter-Lookahead), wird der DAW gemeldet (PDC).

## Formate
- **Windows 10/11 x64:** VST3 + Standalone — `UBMaster-Windows-Setup.exe` (Installer + Deinstaller)
- **macOS 10.13+ Universal:** VST3 + AU + Standalone — `UBMaster-macOS.dmg` (.pkg + Deinstaller)
- **AAX (Pro Tools):** im Build vorbereitet. Schaltet sich in der CI automatisch ein, sobald die Secrets `AAX_SDK_URL` (Avid AAX SDK) und für die Signatur `PACE_USERNAME`, `PACE_PASSWORD`, `PACE_WCGUID` gesetzt sind. Ohne PACE-Signatur lädt Pro Tools AAX nur in Developer-Builds, deshalb wird unsigniertes AAX nicht ausgeliefert.

## Struktur
- `plugin/source/dsp/` — EQ, Saturation (Tape + Exciter), Transient (STFT), Dynamics (Stereo, True-Peak, Limiter), Loudness (BS.1770)
- `plugin/source/ui/` — Theme (Tiffany #81D8D0, Instrument Sans + JetBrains Mono), Loudness Halo, Panels, Widgets
- `plugin/source/Advice.h` — Empfehlungs-Logik
- `branding/` + `tools/make_icon.py` — Icon „Loudness Halo“ (PNG, .ico, .icns)
- `installer/` — Inno Setup (Windows), pkgbuild/productbuild + DMG + Deinstaller (macOS)
- `web/` — ubmaster.uniquebaby.de (GitHub Pages, Branch `gh-pages`)
- `integration/uniquebaby.de/` — Software-Karte + Nav-Link für die Hauptseite
- `tests/dsp_test.cpp` — Transparenz, Latenz, True Peak vs. analytische Werte, BS.1770-Genauigkeit, LRA, EQ, Tape-Spektrum, Transient Cut/Boost, Stereo, Exciter, „Auf Ziel bringen“, Mono, State, Advice, CPU
- `tests/snapshot.cpp` — rendert die Oberfläche als PNG (Webseiten-Screenshots)

## Build
```
cmake -S . -B build                      # holt JUCE 8 automatisch (oder -DJUCE_PATH=...)
cmake --build build --config Release
cmake -S . -B build -DUBMASTER_BUILD_TESTS=ON && cmake --build build --target UBMasterTests && ./build/UBMasterTests_artefacts/Release/UBMasterTests
```
CI: `.github/workflows/build.yml` — Tests + pluginval (Linux), Build + pluginval (Strictness 10) + auval + Install/Launch/Uninstall-Test auf Windows und macOS, danach Release `v<Version aus CMakeLists.txt>`. Version hochzählen → neues Release.

Alle genannten Fremd-Produktnamen auf der Webseite sind Marken ihrer jeweiligen Inhaber; UBMaster steht in keiner Verbindung zu diesen Herstellern.

© 2026 unique & MELO · UNIQUEBABY — alle Rechte vorbehalten.
