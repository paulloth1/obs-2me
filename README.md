## Caution! Vibe Coded

# Multi-M/E — Multiple Mix/Effects for OBS

An OBS plugin that adds extra **mixing layers (M/E banks)** to OBS — like a hardware video
switcher (ATEM / TriCaster / Ross / Sony). Each bank has its own **program/preview bus** with
**cut/auto transitions** and is operated through a **control-surface dock**.

> **Status: Beta / work in progress.** Video mixing only (no separate audio mix — audio keeps
> running through the normal OBS mixer). Developed/tested with **OBS 31 / Qt 6.8** (requires
> OBS 30+).

OBS' main canvas + Studio Mode is effectively **M/E 1** — this plugin provides **M/E 2 … n**.

## Features

- **M/E bank as a source** ("Multi-M/E — Mix/Effects (Re-entry)"): each instance is its own
  mixing layer; its output can be placed in main scenes like any other source ("re-entry").
- **Program/preview bus** from OBS scenes, **CUT** (immediate) and **AUTO** (transition); on a
  take PGM/PVW swap (ping-pong like at a switcher).
- **Transitions**: Fade / Swipe / Slide with adjustable duration.
- **Control-surface dock**: bank selection, **PGM/PVW tally** (red/green), preview-bus buttons,
  CUT/AUTO, transition type + duration, and a **REC** button. The bank/scene list updates
  automatically; the last selected bank is remembered.
- **Multiview per bank**: a dedicated PGM/PVW projector (window or fullscreen) via **Tools →
  Multi-M/E Multiview**, with 8 scene previews (click = preview, double-click = auto, Esc
  closes) — like the native OBS multiview.
- **Own file recording per bank**: record a bank's mixed output to its own file (separate
  encoder/bitrate), with the OBS main audio track for easy sync with the main recording.
- **Remote control via obs-websocket** (vendor `multi-me`): set preview/program, trigger CUT
  and AUTO, start/stop recording — e.g. with **Bitfocus Companion**. Bank by name or UUID.
- **Hotkeys** per bank (Settings → Hotkeys: "Multi-M/E: Cut" / "Multi-M/E: Auto/Take" and
  "Preview Input 1…12").
- **Multiple banks** = simply add more source instances.

## Installation (beta)

### Windows
1. Unzip the plugin archive.
2. Copy the `multi-me` folder into the OBS plugin directory so the structure looks like:
   ```
   %APPDATA%\obs-studio\plugins\multi-me\bin\64bit\multi-me.dll
   %APPDATA%\obs-studio\plugins\multi-me\data\...
   ```
   Tip: type `%APPDATA%` into the Explorer address bar → it lands in
   `C:\Users\<name>\AppData\Roaming`. (No administrator needed.)
3. Start OBS. Show the dock via **Docks → Multi-M/E**.

### macOS
Copy `multi-me.plugin` to `~/Library/Application Support/obs-studio/plugins/`, start OBS.

### Linux
Unpack the package contents into `~/.config/obs-studio/plugins/multi-me/`
(`bin/64bit/multi-me.so` + `data/`), start OBS. **See limitations below.**

## Usage

1. In a scene, **Add source → "Multi-M/E — Mix/Effects (Re-entry)"**.
2. Open **Docks → Multi-M/E** and select the bank in the dock.
3. Click a scene in the **preview bus** (= PVW), then **CUT** or **AUTO** — the bank output
   (the re-entry source in your main scene) switches accordingly.

### Control via obs-websocket / Bitfocus Companion

Enable obs-websocket (**Tools → WebSocket Server Settings**). The banks are controlled through
the vendor `multi-me` via `CallVendorRequest` (`bank` = source name **or** UUID):

| Request | Data | Effect |
| --- | --- | --- |
| `set_preview` | `{ bank, scene }` | scene into the bank's preview |
| `set_program` | `{ bank, scene }` | scene straight to program (hard cut) |
| `cut` | `{ bank }` | Cut (PVW → PGM) |
| `auto` | `{ bank }` | Auto-Take (transition) |
| `start_record` / `stop_record` | `{ bank }` | start/stop the bank's file recording |
| `get_banks` | `{ }` | list of all banks (incl. ready-made hotkey IDs) |
| `get_state` | `{ bank }` | `{ program, preview, in_transition, kind, duration, recording, rec_file }` |

In **Bitfocus Companion** (OBS Studio module): action **Custom Vendor Request** → vendor
`multi-me`, request type e.g. `cut`, request data `{"bank":"My M/E"}`. Alternatively, per bank
via **Trigger Hotkey by ID** (CUT/AUTO/"Preview Input 1…12").

📖 Full Companion guide (requests, hotkey IDs, feedback/tally): [docs/companion.md](docs/companion.md).

## Known limitations

- **Linux: the multiview projector needs X11 or XWayland.** It renders on X11 and XWayland
  sessions; under a **native Wayland** session the multiview window stays blank (a warning is
  logged). Everything else (dock, bus, CUT/AUTO, hotkeys, recording, WebSocket) works
  regardless. Workaround on native Wayland: place the bank source in a scene and use a normal
  OBS projector, or start OBS under XWayland.
- **No separate audio mix** (by design): audio keeps running through the normal OBS mixer.
  (The per-bank file recording does include the OBS main audio track.)
- **Tested** primarily on **macOS**; Windows/Linux are built but exercised less thoroughly.

In my Testing it could handle:
1080p Project 25fps Project
- 8 1080p Videos Playing
- 5 M/E (4x The Plugin)
- All Recording with the HEVC Hardware Encoder at 6000mpbs CBR.
- Companion triggering Auto every 2 Seconds with Different effects and Inputs on all Channels.
- It couldn't quite handle 50p with all 5 Channels on my M1 Pro Macbook Pro

## Building from source

The project is based on the [OBS Plugin Template](https://github.com/obsproject/obs-plugintemplate).

- **Windows / macOS / Linux (official):** via GitHub Actions (`.github/workflows`) or locally
  with the template build (CMake; Windows: Visual Studio 2022, macOS: Xcode).
- **macOS quick, without full Xcode:** `./dev/build.sh` (isolated Ninja dev build) — see
  [dev/README.md](dev/README.md).

Project goals, architecture and progress: see [PROJEKT.md](PROJEKT.md) (in German).

## License

GPL v2 or later — see [LICENSE](LICENSE).
