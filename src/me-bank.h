/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

#pragma once

/* Hotkey-Namens-Schema je Bank. Der Name (nicht die Anzeige) ist eindeutig pro
 * Bank über die Quell-UUID, damit obs-websocket / Companion "Trigger Hotkey by
 * ID" (TriggerHotkeyByName) gezielt EINE Bank schaltet. Muss in
 * me-bank-source.c (Registrierung) und me-websocket.c (get_banks-Auskunft)
 * identisch verwendet werden. %s = Quell-UUID, %d = Preview-Slot (1-basiert). */
#define ME_HOTKEY_CUT_FMT "multime.cut.%s"
#define ME_HOTKEY_AUTO_FMT "multime.auto.%s"
#define ME_HOTKEY_PVW_FMT "multime.pvw.%s.%d"

/* Feste Anzahl Preview-Bus-Hotkeys je Bank ("Preview Input 1..N"), analog zu den
 * festen Bus-Tasten eines Hardware-Mischers. Slot i schaltet die i-te Szene der
 * gefilterten Bus-Liste (me_scenes_enum) in die Vorschau der Bank. */
#define ME_PVW_SLOTS 12

#ifdef __cplusplus
extern "C" {
#endif

/* Registriert den Quelltyp "multi_me_bank" (eine M/E-Mischebene als
 * Re-entry-Quelle). In obs_module_load() aufrufen. */
void me_bank_register_source(void);

#ifdef __cplusplus
}
#endif
