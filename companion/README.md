# Bitfocus Companion page — Multi-M/E (Stream Deck XL)

`multi-me-streamdeck-xl.companionconfig` is a ready-to-import page for a **Stream Deck XL**
(8 × 4) that controls **any of the first four M/E banks** through one selector — no scene names
or UUIDs needed. Exported from **Companion 4.3** (also attached to each plugin release).

```
Col      1        2        3        4       5      6      7        8
Row 1  M/E 2    M/E 3    M/E 4    M/E 5     ·      ·      ·        ·     ← bank selector (#1..#4)
Row 2  PGM 1    PGM 2    PGM 3    PGM 4    PGM 5  PGM 6  PGM 7    PGM 8  ← direct take (hard cut)
Row 3  PVW 1    PVW 2    PVW 3    PVW 4    PVW 5  PVW 6  PVW 7    PVW 8  ← preview select
Row 4  Fade     Swipe    200ms    400ms     ·      ·      CUT     AUTO
```

Every control button sends a `multi-me` vendor request with `"bank":"$(internal:custom_me)"`,
so it always targets the bank picked in row 1. Bank addressing uses `#N` (the N-th M/E), so the
page works on any setup regardless of how the banks are named.

## Setup

1. **Enable obs-websocket** in OBS: *Tools → WebSocket Server Settings → Enable*. Note the port
   (default `4455`) and password.
2. In Companion, add an **OBS Studio** connection with that IP / port / password (status should
   read *OK*). The Multi-M/E controls run through this normal OBS connection.
3. In OBS, add at least one **Multi-M/E — Mix/Effects (Re-entry)** source (one per M/E bank).
4. **Import the page**: Companion web UI → *Import/Export → Import* → choose
   `multi-me-streamdeck-xl.companionconfig` → import onto a free page. When asked, map it to your
   **OBS connection**.

That's it — no variables to create by hand: the page's *Set Custom Variable* actions use
**“Create if not exists”**, so `me`, `me_pvw`, `me_pgm`, `me_trans` and `me_dur` are created
automatically on first press.

## Buttons

| Row | Buttons | Action |
| --- | --- | --- |
| 1 | **M/E 2…5** | Select the active bank (`#1`…`#4`). The selected one lights **green**. |
| 2 | **PGM 1…8** | Hard-cut the N-th bus scene straight to **program**. |
| 3 | **PVW 1…8** | Load the N-th bus scene into **preview**. |
| 4 | **Fade / Swipe** | Set the auto-transition **type**. |
| 4 | **200ms / 400ms** | Set the auto-transition **duration**. |
| 4 | **CUT / AUTO** | Perform the take (cut or transition). |

The bus inputs (1…8) follow the same order as the scene buttons in the Multi-M/E dock /
the multiview tiles.

## Tally (button lighting)

| Lights | Colour | Meaning |
| --- | --- | --- |
| M/E 2…5 | green | the selected bank |
| PVW 1…8 | green | the input last sent to **preview** |
| PGM 1…8 | red | the input last sent to **program** |
| Fade / Swipe | blue | the active transition type |
| 200 / 400 ms | blue | the active duration |

> **Scope:** the PVW/PGM/transition tally mirrors what you switch **through Companion** (using
> Companion custom variables). Switching in the Multi-M/E dock or via an OBS hotkey is **not**
> mirrored, and the tally is global — after changing the **M/E selector** it may be stale until
> you press a PVW/PGM button again. (The plugin also emits `state` vendor events with
> `pvw_in`/`pgm_in`/`rec`; a real, OBS-driven tally would need a Companion module that filters
> vendor events per bank, which the current OBS module does not. See
> [../docs/companion.md](../docs/companion.md) §5.)

## Troubleshooting

- **A button does nothing:** make sure OBS was (re)started **after** installing the plugin —
  the log must show `obs-websocket vendor 'multi-me' registered (… requests)`. Re-import the
  page after any update.
- **“bank not found”:** the selector variable `me` must be `#1`…`#4`. Press an **M/E** button
  once to set it.
- **Transition buttons seem to “do nothing”:** they only set the type/duration for the **next**
  AUTO — press *Fade* / *200ms*, then a *PVW*, then *AUTO* to see the effect (the Multi-M/E dock
  also reflects the change).

Full request reference: [../docs/companion.md](../docs/companion.md).
