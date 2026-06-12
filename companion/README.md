# Bitfocus Companion page — Multi-M/E (Stream Deck XL)

`multi-me-streamdeck-xl.companionconfig` is a ready-to-import page for a **Stream Deck XL**
(8 × 4) that controls **any of the first four M/E banks** through one selector — no scene names
or UUIDs needed. Exported from **Companion 4.3**.

```
Col      1        2        3        4      5    6    7        8
Row 1  M/E 2    M/E 3    M/E 4    M/E 5    ·    ·    ·        ·     ← bank selector (#1..#4)
Row 2  PGM 1    PGM 2    PGM 3    PGM 4   PGM5 PGM6 PGM 7    PGM 8  ← direct take (hard cut)
Row 3  PVW 1    PVW 2    PVW 3    PVW 4   PVW5 PVW6 PVW 7    PVW 8  ← preview select
Row 4  Fade     Swipe    200ms    400ms    ·    ·   CUT      AUTO
```

All control buttons send `multi-me` vendor requests with `"bank":"$(internal:custom_me)"`, so
they always target the bank picked in row 1.

## Setup

1. **Create a custom variable** in Companion: *Variables → Custom Variables* → add **`me`**,
   initial value **`#1`**. (The selector buttons set it to `#1`…`#4`.)
2. **Import the page**: *Import/Export → Import* → pick `multi-me-streamdeck-xl.companionconfig`
   → import onto a free page.
3. On import, Companion asks which **OBS connection** to map it to — pick your obs-websocket
   connection (the plugin's vendor lives in the normal OBS module).
4. In OBS, enable obs-websocket (*Tools → WebSocket Server Settings*) and add at least one
   Multi-M/E bank source.

The four **M/E** buttons select the active bank (the selected one lights up green). **PGM**
hard-cuts the N-th bus scene to program; **PVW** loads it to preview; **Fade/Swipe** set the
transition type and **200/400 ms** its duration; **CUT/AUTO** perform the take.

See [../docs/companion.md](../docs/companion.md) for the full request reference and the
custom-variable tally pattern.
