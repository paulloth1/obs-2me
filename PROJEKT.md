# 2ME — Zweite Mischebene für OBS Studio

> Living-Document: Ziele, Architektur, Roadmap und Fortschritt für ein OBS-Plugin,
> das OBS um zusätzliche Mischebenen (M/E-Bänke) erweitert — analog zu ATEM /
> TriCaster / Ross / Sony Bildmischern.

Letzte Aktualisierung: 2026-06-09

---

## 1. Projektziel

OBS um **konfigurierbare zusätzliche Mischebenen (M/E-Bänke)** erweitern. Jede Bank
besitzt einen eigenen **Program-/Preview-Bus** mit eigener **Übergangslogik**
(Cut / Mix / Wipe …). Der Ausgang einer Bank ist sowohl als **Quelle im Hauptmix
("Re-entry")** als auch als **separater Stream-/Aufnahme-Output** nutzbar.

OBS' Haupt-Canvas + Studio-Modus ist faktisch **M/E 1**. Dieses Plugin liefert
**M/E 2 … M/E n**.

---

## 2. Festgelegte Entscheidungen (Stand 2026-06-09)

| Thema | Entscheidung | Konsequenz |
|---|---|---|
| **Funktionsumfang** | Video + Transitions, **kein** eigener Audio-Mix | Wir nutzen libobs-Transition-API nur für Video; Audio bleibt im normalen OBS-Mixer. Vereinfacht das Routing deutlich. |
| **Output-Nutzung** | **Beides**: Re-entry-Quelle **und** eigener Output | Custom-Source-Typ für Re-entry (Phase 1–2) + `obs_view`/`obs_output` für separaten Output (Phase 4). |
| **Anzahl Bänke** | **2–4 konfigurierbare Bänke** | Generisches `MEBank`-Modell + Verwaltung (anlegen/löschen/umbenennen), kein fest verdrahtetes „M/E 2". |
| **Dev-Hintergrund** | Erfahren in **C++/Qt** | Plan/Code technisch & kompakt; direkter libobs-/Qt-Zugriff. |
| **Zielplattform** | Primär macOS (Dev-Maschine); Win/Linux später via Template-CI | Plugin-Template deckt alle drei ab; wir bauen zuerst macOS grün. |
| **Bus-Quellen** | Nur OBS-**Szenen** auf PGM/PVW wählbar | Einfaches, pult-nahes Modell; jede „Eingabe" = eine Szene. |
| **Übergänge (Start)** | **Cut + Auto (Fade)**; T-Bar/Wipe/Stinger später | Deckt den Großteil ab; manuelle/komplexe Übergänge in spätere Phasen. |
| **Dock-Thumbnails** | **Später (Phase 5)** | Phase 1–4 ohne Live-Vorschau (nur Buttons/Tally); `obs_display` später. |
| **Bank-Auflösung** | Start **einheitlich** = Haupt-Canvas; pro Bank eigene Auflösung **später** (z. B. Hochformat) | `MEBank.canvas_cx/cy` ist dafür schon angelegt → kein Umbau nötig. |
| **OBS-/Qt-Version** | **OBS 30+ / Qt6**, keine Rückwärtskompatibilität | Erlaubt aktuelle APIs (`obs_frontend_add_dock_by_id` etc.) ohne Fallbacks. |
| **Audio** | **Dauerhaft kein eigener Audio-Mix** | Audio-Routing über bestehende OBS-Track-/Kanal-Zuordnung (welche Kanäle → Stream/Recording bzw. welcher M/E). |

Offen / später zu entscheiden → siehe §7.

---

## 3. Machbarkeitsanalyse & Architektur-Konzept

### 3.1 Kernidee: Transition = Source

In OBS sind **Transitions selbst Sources**, die zwischen zwei Kind-Sources (A/B)
mischen. Genau so funktioniert der Studio-Modus intern. Eine M/E-Bank modellieren
wir daher als **eine Transition-Source** + Zustandsverwaltung (welche Szene ist
PGM, welche PVW).

- **Cut**:  `obs_transition_set(transition, pvw_scene)` → sofortiges Umschalten
- **Auto/Take**: `obs_transition_start(transition, OBS_TRANSITION_MODE_AUTO, dur_ms, pvw_scene)`
- Nach dem Take werden die Zeiger **PGM ⇄ PVW getauscht** (Bus-Swap wie am echten Pult).

### 3.2 Datenmodell

```
MEBank
 ├─ id / name              ("M/E 2", "M/E 3", …)
 ├─ obs_source_t* transition   // der eigentliche Mischer (A/B), referenzgehalten
 ├─ obs_weak_source_t* pgm     // aktuell auf Program
 ├─ obs_weak_source_t* pvw     // aktuell auf Preview
 ├─ transition_kind            // "cut_transition","fade_transition","swipe_transition",…
 ├─ duration_ms
 ├─ canvas_cx / canvas_cy      // Default = OBS Base-Canvas
 └─ bus[]                      // wählbare OBS-Szenen für PGM/PVW
```

`BankManager` (Singleton im Modul): hält `std::vector<MEBank>`, kümmert sich um
Lebenszyklus, Persistenz, Thread-Sicherheit (Mutex), und stellt die API für die
UI bereit.

### 3.3 Re-entry: Custom-Source-Typ

Damit der Bank-Ausgang als Quelle im Hauptmix erscheint, registrieren wir einen
eigenen Source-Typ:

```c
struct obs_source_info reentry_info = {
    .id            = "2me_bank_output",
    .type          = OBS_SOURCE_TYPE_INPUT,
    .output_flags  = OBS_SOURCE_VIDEO | OBS_SOURCE_COMPOSITE,
    .get_name      = ...,
    .create/destroy= ...,
    .video_render  = reentry_render,        // → obs_source_video_render(bank->transition)
    .get_width/height = ... ,               // → bank canvas size
    .enum_active_sources = ...,             // → callback(bank->transition)  (Tally/Activation!)
    .get_properties = ...,                  // Property: welche Bank?
};
```

- `OBS_SOURCE_COMPOSITE` + `enum_active_sources` sorgen dafür, dass OBS die
  Kind-Quellen korrekt aktiviert (Tally/Showing) und keine Audio-Doppelzählung
  passiert.
- Erscheint in „Quelle hinzufügen" → Nutzer zieht **„M/E 2 Ausgang"** in eine
  Hauptszene. Das ist das Re-entry.

### 3.4 Eigener Output (separater Stream/Aufnahme) — Phase 4

```c
obs_view_t*  view  = obs_view_create();
obs_view_set_source(view, 0, bank->transition);
video_t*     video = obs_view_add2(view, &ovi);   // eigener Video-Mix @ Bank-Canvas
// → obs_output_t mit eigenem Encoder (x264/nvenc) + Service (Stream) oder File (Record)
```

Schwergewichtiger Teil (eigene Encoder-Instanz, Settings-UI) → bewusst späte Phase.

### 3.5 UI (Qt-Dock)

Registrierung über die **aktuelle** Frontend-API (OBS 30+):
`obs_frontend_add_dock_by_id(id, title, QWidget*)` — **nicht** das veraltete
`obs_frontend_add_dock(QDockWidget*)`. *(Exakte Signatur/Rückgabewert gegen
`obs-frontend-api.h` der lokalen Installation prüfen.)*

Ein Dock mit Bank-Auswahl (Tabs/Combo). Pro Bank:
- **Preview-Bus**: Buttons/Combo der wählbaren Szenen (grün = PVW)
- **Program-Bus**: Anzeige aktive Szene (rot = PGM)
- **Transition**: Typ-Combo + Dauer-Spinbox
- **CUT** / **AUTO** Buttons, optional **T-Bar** (QSlider → `obs_transition_set_manual_*` falls genutzt)
- Optional später: Live-Thumbnails von PGM/PVW via `obs_display`/`obs_view` in Qt

### 3.6 Persistenz

Bindung an die **Scene-Collection** (Bänke referenzieren Szenen):
`obs_frontend_add_save_callback(save_cb, ctx)` → bei Save Bank-Config in das
Scene-Collection-`obs_data_t` schreiben (Array), bei Load Bänke rekonstruieren.
Frontend-Events `OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED` /
`...CLEANUP` beachten.

### 3.7 Architektur-Skizze

```
            ┌────────────────────────── M/E 2 (MEBank) ──────────────────────────┐
   Szene A ─┤                                                                     │
   Szene B ─┤  Bus  ──►  obs_transition  ──►  Bank-Ausgang (Video)                │
   Szene C ─┤            (Cut/Auto/Wipe)        │            │                    │
            └───────────────────────────────────┼────────────┼────────────────────┘
                                                 │            │
                       ┌─────────────────────────┘            └────────────────────┐
                       ▼ Re-entry (Custom Source)                ▼ eigener Output (Phase 4)
            ┌───────────────────────────┐              ┌──────────────────────────────┐
            │  Hauptszene (M/E 1 / PGM) │              │ obs_view → obs_output         │
            │  enthält „M/E 2 Ausgang"  │              │ eigener Stream / Aufnahme     │
            └───────────────────────────┘              └──────────────────────────────┘
```

### 3.8 Take-Ablauf (Cut / Auto) & Bus-Swap

Zustand pro Bank: `pgm` (weak), `pvw` (weak), `in_transition` (bool).
Bank-Anlegen: `obs_transition_set(transition, pgm)` (PGM sofort aktiv).

- **PVW wählen**: nur `pvw`-Zeiger setzen + Tally aktualisieren — **kein** Transition-Call.
- **CUT**: `obs_transition_set(transition, pvw)` → `swap(pgm, pvw)`, `in_transition=false`.
- **AUTO (Take)**: läuft bereits ein Übergang → ignorieren (Policy). Sonst
  `obs_transition_start(transition, OBS_TRANSITION_MODE_AUTO, duration_ms, pvw)`,
  `in_transition=true`.
- **Übergang fertig**: nicht pollen, sondern auf das Signal `"transition_stop"`
  des Transition-Signal-Handlers verbinden (`signal_handler_connect`) → dort
  `swap(pgm, pvw)`, `in_transition=false`.

Bus-Swap = nur Zeigertausch; das gemischte Bild liegt nach Abschluss korrekt auf PGM.

### 3.9 Vorgeschlagene Projektstruktur (aus Plugin-Template)

```
2me/
├─ CMakeLists.txt            (Template)
├─ buildspec.json            (Template: OBS-Dep-Versionen)
├─ src/
│  ├─ plugin-main.cpp        obs_module_load/unload, Registrierungen
│  ├─ me-bank.{hpp,cpp}      MEBank: Transition + PGM/PVW + Take-Logik
│  ├─ bank-manager.{hpp,cpp} Verwaltung, Persistenz, Mutex
│  ├─ reentry-source.cpp     Custom-Source "2me_bank_output"
│  ├─ ui/
│  │  ├─ dock.{hpp,cpp}      QWidget-Dock + Bank-Tabs
│  │  └─ bank-panel.{hpp,cpp} PGM/PVW-Busse, CUT/AUTO, Fade-Dauer
│  └─ output/                (Phase 4) obs_view + obs_output je Bank
└─ data/locale/en-US.ini     UI-Strings
```

---

## 4. Relevante libobs / Frontend APIs (Referenz-Checkliste)

> Signaturen vor Nutzung gegen lokale Header verifizieren (Version: `obs --version`).

- **Modul**: `OBS_DECLARE_MODULE`, `obs_module_load/unload`, `obs_register_source`
- **Transition**: `obs_source_create("fade_transition"/"cut_transition"/"swipe_transition"/"slide_transition"/"obs_stinger_transition", …)`,
  `obs_transition_start`, `obs_transition_set`, `obs_transition_set_size`,
  `obs_transition_set_scale_type`, `obs_transition_set_alignment`,
  `obs_transition_get_time`, `obs_transition_enable_fixed`, `obs_transition_clear`,
  Enum `OBS_TRANSITION_MODE_AUTO`
- **Source/Rendering**: `obs_source_info` (`OBS_SOURCE_VIDEO|OBS_SOURCE_COMPOSITE`),
  `obs_source_video_render`, `enum_active_sources`, `obs_source_get_ref`,
  `obs_get_weak_source`/`obs_weak_source_get_source`
- **Szenen**: `obs_scene_create`/`obs_get_source_by_name`, `obs_frontend_get_scenes`
- **Eigener Output (Phase 4)**: `obs_view_create`, `obs_view_set_source`,
  `obs_view_add2`, `obs_output_create`, `obs_encoder_*`, `obs_output_set_video_encoder`
- **Frontend/UI**: `obs_frontend_add_dock_by_id`, `obs_frontend_add_event_callback`,
  `obs_frontend_add_save_callback`, `obs_frontend_get_current_scene_collection`
- **Threading**: `obs_queue_task(OBS_TASK_GRAPHICS, …)` für Graphics-Thread-Arbeit

---

## 5. Roadmap (Phasen & Meilensteine)

- [x] **Phase 0 — Setup & Skelett** ✅ (2026-06-09)
      Template nach `2ME/` gespiegelt, auf `obs-2me` konfiguriert, git-Repo. Isolierter
      `dev/`-Ninja-Build (ohne volles Xcode) grün; `obs-2me.plugin` lädt in OBS 31.0.2
      (Log: „[obs-2me] 2ME plugin loaded successfully"). *(Leeres Dock → Phase 2, da Qt
      nötig.)* *DoD erfüllt: Plugin baut + lädt + loggt.*
- [x] **Phase 1 — Single-Bank-Core (Walking Skeleton)** — Code fertig, lädt (2026-06-09)
      Quelltyp `2me_bank_output` (src/me-bank-source.c): jede Instanz = eine Bank mit
      privater Transition-Source, PGM/PVW als OBS-Szenen (Properties), Cut/Auto via
      OBS-Hotkeys, Re-entry über Platzierung in Hauptszene. Aktiv-/Sichtbar-Propagation
      an Bank-Szenen via `obs_source_inc/dec_active/showing`. Lädt in OBS 31.0.2 ohne
      Fehler. *Offen: manueller Funktionstest (Bild schaltet) durch Nutzer.*
- [x] **Phase 2 — Dock-UI** — Build grün & lädt (2026-06-09)
      Qt-Dock ([src/me-dock.cpp](src/me-dock.cpp)) via obs-deps Qt6 6.8.3 **ohne Xcode**:
      Bank-Auswahl, PGM/PVW-Tally (rot/grün), Preview-Bus (Szenen-Buttons), CUT/AUTO.
      Steuert Bänke über `proc_handler` (cut/auto_take/set_preview/get_state).
      *(T-Bar/weitere Übergänge → Phase 5.)* *Offen: interaktiver Nutzertest.*
- [ ] **Phase 3 — Mehrere Bänke + Persistenz**
      2–4 Bänke anlegen/löschen/umbenennen; Speichern/Laden pro Scene-Collection.
      *DoD: Bänke überleben OBS-Neustart & Collection-Wechsel.*
- [ ] **Phase 4 — Eigener Output**
      `obs_view`+`obs_output` je Bank für separaten Stream/Aufnahme inkl. Encoder-
      Settings. *DoD: Bank-Ausgang als eigene Aufnahme/Stream.*
- [ ] **Phase 5 — Politur**
      Hotkeys, Stinger/Wipe-Set, Live-Thumbnails im Dock, Win/Linux-CI-Builds,
      Doku/README. *DoD: Release-Build für alle Plattformen.*

### 5.1 Phase 1 — Detailaufgaben (Walking Skeleton)

- [ ] `MEBank`: Transition-Source erzeugen (`fade_transition`), `obs_transition_set_size` = Canvas.
- [ ] Zwei Test-Szenen als PGM/PVW verdrahten, initial `obs_transition_set(transition, pgm)`.
- [ ] Take-Logik (Cut/Auto) + Bus-Swap über `"transition_stop"`-Signal (siehe §3.8).
- [ ] Custom-Source `2me_bank_output`: `video_render` → `obs_source_video_render(transition)`,
      plus `enum_active_sources` und `get_width/height`.
- [ ] Registrierung in `obs_module_load`; eine Bank global instanziieren.
- [ ] Manueller Test: Re-entry-Quelle in Hauptszene ziehen, Cut/Auto per Test-Hotkey.

---

## 6. Risiken & Knackpunkte

1. **Feedback-Schleifen**: Re-entry-Quelle, die in einer Szene des eigenen PGM-Bus
   liegt → Rekursion. Mitigation: OBS-Zyklenerkennung
   (`obs_source_add_active_child`) + eigener Guard in `enum_active_sources`.
2. **Audio-Doppelzählung**: Quellen in Bank-Szenen, die auch im Hauptmix aktiv
   sind, könnten doppelt zählen. Da „kein Audio-Mix" gewählt: Bank rein video,
   `enum_active_sources` sauber halten; Audio-Flags der Re-entry-Source vermeiden.
3. **Thread-Sicherheit**: Render läuft im Graphics-Thread, UI im Qt-Thread.
   Bank-Liste per Mutex schützen, Source-Zeiger via (weak) Refs stabil halten.
4. **API-Drift**: Dock-/Transition-API hat sich über OBS-Versionen geändert.
   Signaturen gegen installierte Header prüfen; min. unterstützte OBS-Version
   festlegen (Vorschlag: OBS 30+).
5. **Persistenz-Timing**: Bänke referenzieren Szenen, die beim Load evtl. noch
   nicht existieren. Lade-Reihenfolge / verzögerte Auflösung beachten.

---

## 7. Offene Fragen (zu klären)

- [x] **Bank-Auflösung** → Start einheitlich (Haupt-Canvas); pro Bank eigene
  Auflösung später (z. B. Hochformat). `MEBank.canvas_cx/cy` schon vorgesehen.
- [x] **Min. OBS-Version / Qt** → OBS 30+, Qt6; keine Rückwärtskompatibilität.
- [x] **Audio** → dauerhaft kein eigener Audio-Mix; Routing über bestehende
  OBS-Track-/Kanal-Zuordnung.
- [ ] **T-Bar / Wipe / Stinger**: ab welcher Phase nachrüsten? (aktuell Phase 5)

---

## 8. Fortschrittslog

- **2026-06-09** — Projekt initialisiert. Machbarkeit analysiert (Transition-als-
  Source-Ansatz), Kern-Entscheidungen getroffen (Video-only, beide Outputs, 2–4
  Bänke, C++/Qt). Architektur-Konzept + Roadmap dokumentiert. Nächster Schritt:
  Phase 0 (Plugin-Template-Setup).
- **2026-06-09** — Detailentscheidungen ergänzt: Bus = nur OBS-Szenen; Start mit
  Cut + Auto (Fade); Dock-Thumbnails erst Phase 5. Doku verfeinert: Take-State-
  Machine (§3.8), Projektstruktur (§3.9), Phase-1-Detailaufgaben (§5.1). Kein Code
  bis Freigabe.
- **2026-06-09** — Restliche §7-Punkte entschieden: Auflösung einheitlich (Haupt-
  Canvas), pro Bank später (Hochformat); OBS 30+/Qt6 ohne Rückwärtskompatibilität;
  Audio dauerhaft ohne eigenen Mix. **Phase 0 gestartet** (Projektstruktur anlegen).
- **2026-06-09** — **Phase 0 abgeschlossen.** Build-Weg gewählt: schneller, isolierter
  `dev/`-Ninja-Build **ohne volles Xcode** (libobs-Header aus obs-studio 31.0.2 +
  `-undefined dynamic_lookup`; `.plugin`-Bundle wird ad-hoc signiert und nach
  `~/Library/.../obs-studio/plugins/` installiert; Build via `./dev/build.sh`). Der
  offizielle Xcode-Template-Build im Root bleibt für CI/Release erhalten.
  `obs-2me.plugin` lädt verifiziert in OBS 31.0.2. Offen: leere `en-US.ini` noch nicht
  ins Bundle gepackt → harmlose Locale-Warnung, mit `data/`-Bundling beheben.
- **2026-06-09** — **Phase 1 implementiert.** Neuer Quelltyp `2me_bank_output`
  ([src/me-bank-source.c](src/me-bank-source.c)): jede Instanz = eine M/E-Bank mit
  privater Transition-Source, PGM/PVW-Auswahl (OBS-Szenen via Properties), Cut/Auto
  über OBS-Hotkeys („2ME: Cut" / „2ME: Auto/Take"), Take swappt PGM/PVW. Aktiv-/
  Sichtbar-Propagation an Bank-Szenen via `obs_source_inc/dec_active/showing`.
  Video-only. Mehrere Bänke = mehrere Instanzen (Basis für Phase 3). Baut & lädt
  verifiziert in OBS 31.0.2; manueller Funktionstest steht aus.
- **2026-06-09** — Bugfix: PGM/PVW-Dropdowns waren leer, weil `obs_enum_sources` nur
  Input-Quellen liefert. Auf `obs_enum_scenes` umgestellt → Szenen werden gelistet.
  Neu gebaut & geladen.
- **2026-06-09** — Crash beim Neustart analysiert: **nicht** das Plugin (Crash-Report
  ohne 2ME-Symbole; Ursache war mein zu aggressives Neustart-Skript mit `pkill` während
  OBS herunterfuhr). Künftig nur sauberer Neustart. Cut/Auto zusätzlich als **Buttons in
  den Quellen-Eigenschaften** (`obs_properties_add_button2`, priv = Bank) → live testbar,
  unabhängig von Hotkeys. Klarstellung: „Sichtbar/Anzeigen"-Hotkeys sind OBS-Standard pro
  Szenen-Element, nicht 2ME (unsere heißen „2ME: Cut" / „2ME: Auto/Take").
- **2026-06-09** — Bugfix „Auto nur einmal": `transition_stop` feuert bei video-only-
  Nutzung **nie** (der Audio-Teil des Übergangs schließt nie ab → siehe
  obs-source-transition.c:670). Lösung: `in_transition` **zeitbasiert** in `video_tick`
  zurücksetzen (`os_gettime_ns` + Dauer), unabhängig vom Signal. Wiederholtes Auto läuft;
  PGM/PVW tauschen beim Take (Ping-Pong wie am echten Pult). Funktionstest durch Nutzer ✅.
- **2026-06-09** — **Phase 2a: Qt-Toolchain im dev-Build.** OBS-Laufzeit-Qt ist **6.8.3**
  (OBS wurde aktualisiert). obs-deps **Qt6 6.8.3** nach `.deps/` geladen. Drei macOS/CLT-
  Hürden gelöst: (1) `-isysroot` via `xcrun --show-sdk-path` setzen; (2) libc++-Header
  liegen nur im SDK → `-isystem <SDK>/usr/include/c++/v1` für C++ ergänzen; (3) Qt **nicht
  linken** (zieht das in neuen SDKs entfernte **AGL**-Framework nach) — nur Header/MOC
  nutzen, Qt-Symbole via `dynamic_lookup` aus OBS auflösen. Minimaler Dock
  ([src/me-dock.cpp](src/me-dock.cpp), `obs_frontend_add_dock_by_id`, registriert in
  `obs_module_post_load`) baut grün; `otool -L` zeigt nur libc++/libSystem.
- **2026-06-09** — **Phase 2b: volles Dock-UI.** [src/me-dock.cpp](src/me-dock.cpp):
  Bank-Auswahl (alle `2me_bank_output`-Quellen), PGM/PVW-Tally (rot/grün, 200 ms-Timer),
  Preview-Bus (Button je Szene → `set_preview`), CUT/AUTO. C↔C++-Brücke über `proc_handler`
  auf der Bank (`cut`/`auto_take`/`set_preview`/`get_state`). Lädt verifiziert
  („dock registered: ok"), kein Crash. Nebenbefund: andere Dritt-Plugins scheitern auf dem
  aktualisierten OBS an der **entfernten** `obs_frontend_add_dock` — wir nutzen korrekt
  `obs_frontend_add_dock_by_id`. Offen: interaktiver Nutzertest.
- **2026-06-09** — **Dock-Verbesserungen.** (1) **Auto-Refresh**: 200-ms-Poll im Tally-
  Timer erkennt neue/entfernte Bänke & Szenen (Signatur-Vergleich) und baut Dropdown +
  Preview-Bus neu → Bänke erscheinen von selbst (↻ bleibt als manuelle Option). (2) **Gewählte
  Bank gemerkt** in der OBS-User-Config (`obs_frontend_get_user_config`). (3) **Persistenz**:
  PVW-Auswahl, Übergangstyp & -dauer werden in die Source-Settings geschrieben (überleben
  Reload). (4) **Übergangstyp (Fade/Swipe/Slide) + Dauer** als Dock-Controls über neue Procs
  `set_transition`/`set_duration`; `get_state` liefert zusätzlich `kind`+`duration`.
  Transition-Erzeugung in `bank_apply_transition_kind()` refaktoriert. Baut & lädt verifiziert.

---

## 9. Referenzen

- Plugin-Template: https://github.com/obsproject/obs-plugintemplate
- Quick-Start: https://github.com/obsproject/obs-plugintemplate/wiki/Quick-Start-Guide
- OBS API-Doku: https://docs.obsproject.com/
- Frontend-API: https://docs.obsproject.com/reference-frontend-api
- Sources/Transitions: https://docs.obsproject.com/reference-sources
