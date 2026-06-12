# Controlling Multi-M/E with Bitfocus Companion

This guide explains how to remote-control Multi-M/E's mix/effects banks from
[Bitfocus Companion](https://bitfocus.io/companion) over obs-websocket (set
preview/program, trigger CUT/AUTO) and how to get a tally-style **feedback** on your
Companion buttons.

> Companion talks to OBS through the **OBS Studio module** (obs-websocket v5). The
> action/feedback names below may differ slightly between Companion versions.

---

## 1. Connect

1. In OBS: **Tools → WebSocket Server Settings → Enable WebSocket server**. Note the
   port (default `4455`) and password.
2. In Companion: add a connection of type **OBS Studio** with the OBS machine's IP, port
   and password. The status should read **OK/connected**.
3. A **Multi-M/E bank** must exist as a source in a scene (otherwise there is nothing to
   control). Note its **source name** (e.g. `M/E 2`).

---

## 2. Two ways to control it

You can drive Multi-M/E from Companion in two ways — both work and can be mixed:

| Way | Companion action | Best for |
| --- | --- | --- |
| **A) Vendor request** | *Custom Vendor Request* | Full feature set, bank by name **or** UUID, JSON data |
| **B) Hotkey by ID** | *Trigger Hotkey by ID* | Simple triggering without JSON; great for CUT/AUTO/preview keys |

---

## 3. Way A — Vendor requests (`CallVendorRequest`)

Use the **Custom Vendor Request** action in the OBS module. Always:

- **Vendor Name:** `multi-me`
- **Request Type:** one from the table
- **Request Data (JSON):** as shown

**Addressing a bank** — the `bank` field accepts:
- a **source name** (e.g. `"M/E 2"`),
- a **UUID**, or
- **`"#N"`** = the N-th bank in order (`"#1"` = first, `"#2"` = second …). This is the easiest
  way to build a **generic page** that works regardless of how the banks are named.

| Request Type | Request Data | Effect |
| --- | --- | --- |
| `set_preview` | `{"bank":"#1","scene":"Scene A"}` | Put a named scene into the bank's **preview** (PVW) |
| `set_preview_index` | `{"bank":"#1","input":3}` | Put the **N-th bus scene** (1-based) into preview |
| `set_program` | `{"bank":"#1","scene":"Scene A"}` | Put a scene **straight** to program (hard cut) |
| `cut` | `{"bank":"#1"}` | **CUT** (PVW → PGM) |
| `auto` | `{"bank":"#1"}` | **AUTO/Take** (transition) |
| `set_transition` | `{"bank":"#1","kind":"fade_transition"}` | Set the auto transition type (e.g. `fade_transition`, `wipe_transition`) |
| `set_duration` | `{"bank":"#1","ms":200}` | Set the auto transition duration (ms) |
| `start_record` / `stop_record` | `{"bank":"#1"}` | Start / stop the bank's **file recording** |
| `get_banks` | `{}` | List of all banks incl. ready-made hotkey IDs |
| `get_state` | `{"bank":"#1"}` | `{program, preview, in_transition, kind, duration, recording, rec_file}` |

> **For multiple banks, prefer vendor requests over "Trigger Hotkey by ID":** the hotkey IDs
> carry the bank UUID (long and hard to find), and the OBS module's hotkey picker only shows
> one entry per identical description. Vendor requests address each bank cleanly via `"#N"`.

---

## 4. Way B — Trigger Hotkey by ID

Each bank registers hotkeys with **unique IDs** (the long string is the bank's UUID —
required so every bank is addressable separately):

| Hotkey ID | Effect |
| --- | --- |
| `multime.cut.<uuid>` | CUT this bank |
| `multime.auto.<uuid>` | AUTO/Take this bank |
| `multime.pvw.<uuid>.1` … `.12` | **Preview Input 1…12**: put the *N*-th scene of the bus list into preview |

The **preview slots** match the order of the scene buttons in the Multi-M/E dock and the
tiles in the multiview (filtered OBS scene order). Slot 1 = first scene, slot 2 = second,
and so on.

### Finding the IDs

You don't have to assemble the UUID yourself — the **`get_banks`** vendor request returns
the ready-made IDs:

```json
{
  "banks": [
    {
      "name": "M/E 2",
      "uuid": "a1b2c3d4-…",
      "hotkey_cut": "multime.cut.a1b2c3d4-…",
      "hotkey_auto": "multime.auto.a1b2c3d4-…",
      "hotkeys_preview": [
        { "input": 1, "hotkey": "multime.pvw.a1b2c3d4-….1" },
        { "input": 2, "hotkey": "multime.pvw.a1b2c3d4-….2" }
        // … up to input 12
      ]
    }
  ]
}
```

Just copy the desired `hotkey` string into Companion's **Trigger Hotkey by ID** action.

> Tip: You can also bind the same preview hotkeys to real keys in OBS under
> **Settings → Hotkeys** (grouped under the bank's source name, "Preview Input 1…12").

---

## 5. Feedback (tally on the buttons)

Companion has **no built-in feedbacks for custom vendor requests**. You can, however,
reliably emulate a tally using Companion's own **custom variables**: the button remembers
what it switched, and a feedback colors it accordingly.

### Example: preview tally (the active preview button lights up)

1. **Create a custom variable** (Companion: *Variables → Custom Variables*), e.g.
   `me2_pvw`.
2. **Each preview button** gets **two actions**:
   - *Trigger Hotkey by ID* → `multime.pvw.<uuid>.N` (or *Custom Vendor Request*
     `set_preview`).
   - *Internal: Set Custom Variable Value* → `me2_pvw` = `N` (this button's slot number).
3. **Feedback** on each preview button:
   - *Internal: Check Custom Variable Value* → `me2_pvw` **==** `N` → background green.

Now the most recently selected preview button always lights up. For a **program tally**
(red), do the same with a variable `me2_pgm` that you set on the **CUT/AUTO** button to
the current preview value (on a take, PVW becomes PGM).

### Limitation

This mirror only reflects what was switched **through Companion**. If you switch in
parallel via the **Multi-M/E dock**, the multiview, or an OBS hotkey, Companion doesn't
know about it and the tally can be wrong. True state-synced feedback would require the
Companion OBS module to support the `multi-me` vendor directly (reading `get_state` /
vendor events), which is currently not provided. As long as you switch **only** through
Companion, the variable mirror is reliable.

---

## 6. Stream Deck XL example page (8 × 4)

A ready-made layout that fits a **Stream Deck XL** (8 columns × 4 rows) and controls **any**
of the first four banks via a selector — no scene names or UUIDs needed (all buttons address
the bank through the `#N` selector).

Create one **custom variable** `me` (its value is the selected bank, e.g. `#1`). Every control
button passes `"bank":"$(internal:custom_me)"`.

```
Col      1        2        3        4        5        6        7        8
Row 1  In 1     In 2     In 3     In 4     In 5     In 6     In 7     In 8     ← preview bus
Row 2  CUT      AUTO     Fade     Fade      —        —        —       REC
                         200 ms   400 ms                               (start/stop)
Row 3  ME 1     ME 2     ME 3     ME 4      —        —        —        —       ← bank selector
Row 4   —        —        —        —        —        —        —        —
```

Per button:

- **In 1…8** — action *Custom Vendor Request*: vendor `multi-me`, type `set_preview_index`,
  data `{"bank":"$(internal:custom_me)","input":N}`. Optional preview tally: also
  *Set Custom Variable* `me_pvw` = `N`, and a feedback `me_pvw == N` → green (mirror, see §5).
- **CUT** — `cut`, data `{"bank":"$(internal:custom_me)"}`.
- **AUTO** — `auto`, data `{"bank":"$(internal:custom_me)"}`.
- **Fade 200 ms** — two actions: `set_transition` `{"bank":"$(internal:custom_me)","kind":"fade_transition"}`
  then `set_duration` `{"bank":"$(internal:custom_me)","ms":200}`.
- **Fade 400 ms** — same with `"ms":400`.
- **REC** — a 2-step button: step 1 `start_record`, step 2 `stop_record` (both
  `{"bank":"$(internal:custom_me)"}`).
- **ME 1…4** — *Set Custom Variable* `me` = `#1` … `#4`. Feedback *Check Custom Variable*
  `me == #N` → green, so the selected bank lights up.

> A ready-to-import `.companionconfig` for this page is being prepared in
> [`companion/`](https://github.com/paulloth1/multi-me/tree/main/companion). Until then, the
> layout above can be rebuilt in a few minutes.

---

## 7. Troubleshooting

- **"bank not found"**: `bank` doesn't match the source name (case, spaces) — or the UUID
  is wrong. When in doubt, call `get_banks` and copy the name/UUID from there.
- **Hotkey does nothing**: check the ID exactly (including the UUID suffix and slot
  number). Renaming or recreating a bank does **not** change its UUID (it is stored in
  the scene collection).
- **Preview slot switches the "wrong" scene**: slots count the **filtered** bus list (OBS
  scene order, excluding scenes that contain the bank itself and scenes hidden from the
  multiview). Order = same as in the dock.
- **Multiple banks**: each bank has its own IDs (its own UUID). `get_banks` lists them
  all.
