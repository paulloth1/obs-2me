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
- **Multiview je Bank**: eigener PGM/PVW-Projektor (Fenster oder Vollbild) über **Tools →
  Multi-M/E Multiview**, mit 8 Szenen-Vorschauen (Klick = Preview, Doppelklick = Auto, Esc
  schließt) — wie die native OBS-Multiview.
- **Fernsteuerung via obs-websocket** (Vendor `multi-me`): Preview/Program setzen, CUT und
  AUTO auslösen z. B. mit **Bitfocus Companion**. Bank per Name oder UUID adressierbar.
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

### Linux
Den Inhalt des Pakets nach `~/.config/obs-studio/plugins/multi-me/` entpacken
(`bin/64bit/multi-me.so` + `data/`), OBS starten. **Siehe Einschränkungen unten.**

## Benutzung

1. In einer Szene **Quelle hinzufügen → „Multi-M/E — Mix/Effects (Re-entry)"**.
2. **Docks → Multi-M/E** öffnen und im Dock die Bank wählen.
3. Im **Preview-Bus** eine Szene anklicken (= PVW), dann **CUT** oder **AUTO** — der Bank-
   Ausgang (die Re-entry-Quelle in deiner Hauptszene) schaltet entsprechend.

> ⚠️ PGM/PVW **nicht** auf die Szene legen, die die Multi-M/E-Quelle selbst enthält → Feedback-Schleife.

### Steuerung via obs-websocket / Bitfocus Companion

obs-websocket aktivieren (**Tools → WebSocket-Servereinstellungen**). Die Bänke werden über
den Vendor `multi-me` per `CallVendorRequest` gesteuert (`bank` = Quellname **oder** UUID):

| Request | Daten | Wirkung |
| --- | --- | --- |
| `set_preview` | `{ bank, scene }` | Szene in die Vorschau der Bank |
| `set_program` | `{ bank, scene }` | Szene sofort auf Program (harter Schnitt) |
| `cut` | `{ bank }` | Cut (PVW → PGM) |
| `auto` | `{ bank }` | Auto-Take (Übergang) |
| `get_banks` | `{ }` | Liste aller Bänke `[{name, uuid}]` |
| `get_state` | `{ bank }` | `{ program, preview, in_transition, kind, duration }` |

In **Bitfocus Companion** (OBS-Studio-Modul): Aktion **Custom Vendor Request** → Vendor
`multi-me`, Request-Typ z. B. `cut`, Request-Daten `{"bank":"Meine M/E"}`. Alternativ je Bank
per **Trigger Hotkey by ID** (CUT/AUTO/„Preview Input 1…12").

📖 Vollständige Companion-Anleitung (Requests, Hotkey-IDs, Feedback/Tally): [docs/companion.md](docs/companion.md).

## Bekannte Einschränkungen

- **Linux: Multiview-Projektor rendert (noch) nicht.** Das Plugin baut und lädt unter Linux,
  und Dock, Bus, CUT/AUTO, Hotkeys sowie die WebSocket-Steuerung funktionieren. Der
  Multiview-Projektor setzt das native Fenster-Handle bisher nur für Windows und macOS — der
  X11/Wayland-Pfad fehlt, daher bleibt das Multiview-Fenster unter Linux leer. Als Workaround
  die Bank-Quelle in eine Szene legen und einen normalen OBS-Projektor verwenden.
- **Kein eigener Audio-Mix** (Design): Audio läuft weiter über den normalen OBS-Mixer.
- **Getestet** primär unter **macOS**; Windows/Linux werden gebaut, aber weniger ausgiebig
  erprobt.

## Aus dem Quellcode bauen

Das Projekt basiert auf dem [OBS Plugin Template](https://github.com/obsproject/obs-plugintemplate).

- **Windows / macOS / Linux (offiziell):** über GitHub Actions (`.github/workflows`) oder lokal
  mit dem Template-Build (CMake; Windows: Visual Studio 2022, macOS: Xcode).
- **macOS schnell, ohne volles Xcode:** `./dev/build.sh` (isolierter Ninja-Dev-Build) —
  siehe [dev/README.md](dev/README.md).

Projektziele, Architektur und Fortschritt: siehe [PROJEKT.md](PROJEKT.md).

## Lizenz

GPL v2 or later — siehe [LICENSE](LICENSE).
