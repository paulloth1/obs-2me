# `dev/` — Schneller lokaler Build (macOS, ohne volles Xcode)

Dieser Ordner ist ein **isolierter Dev-Build** für schnelle Iteration auf Apple
Silicon. Er läuft mit **Ninja + Command Line Tools** und braucht **kein volles
Xcode**. Der offizielle, plattformübergreifende Template-Build im Repo-Root
(Xcode-Generator, CI, Signing) bleibt davon unberührt und wird später für
Releases genutzt.

## Funktionsweise

- Kompiliert die geteilten Quellen aus [`../src`](../src) gegen die **libobs-Header**.
- Linkt mit `-undefined dynamic_lookup`: libobs-Symbole werden erst beim Laden
  durch OBS aufgelöst → **kein kompiliertes libobs nötig**, nur Header.
- Baut ein `obs-2me.plugin`-Bundle, signiert es ad-hoc (`codesign -s -`) und
  kopiert es nach `~/Library/Application Support/obs-studio/plugins/`.

## Voraussetzungen (einmalig)

1. CMake + Ninja (via Homebrew): `brew install cmake ninja`
2. libobs-Header passend zur installierten OBS-Version (Standard: **31.0.2**):

   ```sh
   mkdir -p .deps && cd .deps
   curl -fL -o obs-studio-31.0.2.tar.gz \
     https://github.com/obsproject/obs-studio/archive/refs/tags/31.0.2.tar.gz
   tar xzf obs-studio-31.0.2.tar.gz
   ```

   Erwarteter Pfad: `.deps/obs-studio-31.0.2/libobs/obs-module.h`
   (Andere Version? `-DOBS_VERSION=<x.y.z>` an CMake übergeben.)

## Bauen

```sh
./dev/build.sh
```

Danach OBS starten und im Log nach `[obs-2me] 2ME plugin loaded successfully` suchen
(Hilfe › Logdateien › Aktuelles Log anzeigen).

## Grenzen

- Nur für lokale Dev-Iteration (arm64, nicht signiert/notarisiert, keine CI).
- Qt-/Frontend-API-Funktionen (Dock ab Phase 2) erfordern später zusätzlich die
  passenden Qt6-Header; dann entweder hier ergänzen oder auf den Xcode-Pfad wechseln.
