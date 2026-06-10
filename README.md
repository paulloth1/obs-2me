## Caution! Vibe Coded

# Multi-M/E — Multiple Mix/Effects for OBS

Ein OBS-Plugin, das OBS um zusätzliche **Mischebenen (M/E-Bänke)** erweitert — wie bei
einem Hardware-Bildmischer (ATEM / TriCaster / Ross / Sony). Jede Bank hat einen eigenen
**Program-/Preview-Bus** mit **Cut/Auto-Übergängen** und wird über ein **Mischpult-Dock**
bedient.

> **Status: Beta / Work in Progress.** Reine Video-Mischung (kein eigener Audio-Mix —
> Audio läuft weiter über den normalen OBS-Mixer). Entwickelt/getestet mit **OBS 31 / Qt 6.8**
> (benötigt OBS 30+).

OBS' Haupt-Canvas + Studio-Modus ist faktisch **M/E 1** — dieses Plugin liefert **M/E 2 … n**.

## Features

- **M/E-Bank als Quelle** („Multi-M/E — Mix/Effects (Re-entry)"): jede Instanz ist eine eigene
  Mischebene; ihr Ausgang lässt sich wie jede andere Quelle in Hauptszenen platzieren
  („Re-entry").
- **Program-/Preview-Bus** aus OBS-Szenen, **CUT** (sofort) und **AUTO** (Übergang); beim
  Take tauschen PGM/PVW (Ping-Pong wie am Pult).
- **Übergänge**: Fade / Swipe / Slide mit einstellbarer Dauer.
- **Mischpult-Dock**: Bank-Auswahl, **PGM/PVW-Tally** (rot/grün), Preview-Bus-Buttons,
  CUT/AUTO, Übergangstyp + Dauer. Bank-/Szenenliste aktualisiert sich automatisch; die
  zuletzt gewählte Bank wird gemerkt.
- **Hotkeys** je Bank (Einstellungen → Hotkeys: „Multi-M/E: Cut" / „Multi-M/E: Auto/Take").
- **Mehrere Bänke** = einfach mehrere Quell-Instanzen einfügen.

## Installation (Beta)

### Windows
1. Das Plugin-Zip entpacken.
2. Den Ordner `multi-me` in das OBS-Plugin-Verzeichnis kopieren, sodass die Struktur so aussieht:
   ```
   %APPDATA%\obs-studio\plugins\multi-me\bin\64bit\multi-me.dll
   %APPDATA%\obs-studio\plugins\multi-me\data\...
   ```
   Tipp: `%APPDATA%` in die Explorer-Adressleiste eingeben → landet in
   `C:\Users\<Name>\AppData\Roaming`. (Kein Administrator nötig.)
3. OBS starten. Dock über **Docks → Multi-M/E** einblenden.

### macOS
`multi-me.plugin` nach `~/Library/Application Support/obs-studio/plugins/` kopieren, OBS starten.

## Benutzung

1. In einer Szene **Quelle hinzufügen → „Multi-M/E — Mix/Effects (Re-entry)"**.
2. **Docks → Multi-M/E** öffnen und im Dock die Bank wählen.
3. Im **Preview-Bus** eine Szene anklicken (= PVW), dann **CUT** oder **AUTO** — der Bank-
   Ausgang (die Re-entry-Quelle in deiner Hauptszene) schaltet entsprechend.

> ⚠️ PGM/PVW **nicht** auf die Szene legen, die die Multi-M/E-Quelle selbst enthält → Feedback-Schleife.

## Aus dem Quellcode bauen

Das Projekt basiert auf dem [OBS Plugin Template](https://github.com/obsproject/obs-plugintemplate).

- **Windows / macOS / Linux (offiziell):** über GitHub Actions (`.github/workflows`) oder lokal
  mit dem Template-Build (CMake; Windows: Visual Studio 2022, macOS: Xcode).
- **macOS schnell, ohne volles Xcode:** `./dev/build.sh` (isolierter Ninja-Dev-Build) —
  siehe [dev/README.md](dev/README.md).

Projektziele, Architektur und Fortschritt: siehe [PROJEKT.md](PROJEKT.md).

## Lizenz

GPL v2 or later — siehe [LICENSE](LICENSE).
