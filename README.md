## Caution! Vibe Coded

# 2ME — Second Mix/Effects for OBS

Ein OBS-Plugin, das OBS um zusätzliche **Mischebenen (M/E-Bänke)** erweitert — wie bei
einem Hardware-Bildmischer (ATEM / TriCaster / Ross / Sony). Jede Bank hat einen eigenen
**Program-/Preview-Bus** mit **Cut/Auto-Übergängen** und wird über ein **Mischpult-Dock**
bedient.

> **Status: Beta / Work in Progress.** Reine Video-Mischung (kein eigener Audio-Mix —
> Audio läuft weiter über den normalen OBS-Mixer). Entwickelt/getestet mit **OBS 31 / Qt 6.8**
> (benötigt OBS 30+).

OBS' Haupt-Canvas + Studio-Modus ist faktisch **M/E 1** — dieses Plugin liefert **M/E 2 … n**.

## Features

- **M/E-Bank als Quelle** („2ME — Mix/Effects (Re-entry)"): jede Instanz ist eine eigene
  Mischebene; ihr Ausgang lässt sich wie jede andere Quelle in Hauptszenen platzieren
  („Re-entry").
- **Program-/Preview-Bus** aus OBS-Szenen, **CUT** (sofort) und **AUTO** (Übergang); beim
  Take tauschen PGM/PVW (Ping-Pong wie am Pult).
- **Übergänge**: Fade / Swipe / Slide mit einstellbarer Dauer.
- **Mischpult-Dock**: Bank-Auswahl, **PGM/PVW-Tally** (rot/grün), Preview-Bus-Buttons,
  CUT/AUTO, Übergangstyp + Dauer. Bank-/Szenenliste aktualisiert sich automatisch; die
  zuletzt gewählte Bank wird gemerkt.
- **Hotkeys** je Bank (Einstellungen → Hotkeys: „2ME: Cut" / „2ME: Auto/Take").
- **Mehrere Bänke** = einfach mehrere Quell-Instanzen einfügen.

## Installation (Beta)

### Windows
1. Das Plugin-Zip entpacken.
2. Den Ordner `obs-2me` in das OBS-Plugin-Verzeichnis kopieren, sodass die Struktur so aussieht:
   ```
   %APPDATA%\obs-studio\plugins\obs-2me\bin\64bit\obs-2me.dll
   %APPDATA%\obs-studio\plugins\obs-2me\data\...
   ```
   Tipp: `%APPDATA%` in die Explorer-Adressleiste eingeben → landet in
   `C:\Users\<Name>\AppData\Roaming`. (Kein Administrator nötig.)
3. OBS starten. Dock über **Docks → 2ME** einblenden.

### macOS
`obs-2me.plugin` nach `~/Library/Application Support/obs-studio/plugins/` kopieren, OBS starten.

## Benutzung

1. In einer Szene **Quelle hinzufügen → „2ME — Mix/Effects (Re-entry)"**.
2. **Docks → 2ME** öffnen und im Dock die Bank wählen.
3. Im **Preview-Bus** eine Szene anklicken (= PVW), dann **CUT** oder **AUTO** — der Bank-
   Ausgang (die Re-entry-Quelle in deiner Hauptszene) schaltet entsprechend.

> ⚠️ PGM/PVW **nicht** auf die Szene legen, die die 2ME-Quelle selbst enthält → Feedback-Schleife.

## Aus dem Quellcode bauen

Das Projekt basiert auf dem [OBS Plugin Template](https://github.com/obsproject/obs-plugintemplate).

- **Windows / macOS / Linux (offiziell):** über GitHub Actions (`.github/workflows`) oder lokal
  mit dem Template-Build (CMake; Windows: Visual Studio 2022, macOS: Xcode).
- **macOS schnell, ohne volles Xcode:** `./dev/build.sh` (isolierter Ninja-Dev-Build) —
  siehe [dev/README.md](dev/README.md).

Projektziele, Architektur und Fortschritt: siehe [PROJEKT.md](PROJEKT.md).

## Lizenz

GPL v2 or later — siehe [LICENSE](LICENSE).
