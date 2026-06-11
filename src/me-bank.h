/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <mail@paulloth.de>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

#pragma once

/* Hotkey naming scheme per bank. The name (not the display) is unique per bank
 * via the source UUID, so obs-websocket / Companion "Trigger Hotkey by ID"
 * (TriggerHotkeyByName) targets exactly ONE bank. Must be used identically in
 * me-bank-source.c (registration) and me-websocket.c (get_banks reply).
 * %s = source UUID, %d = preview slot (1-based). */
#define ME_HOTKEY_CUT_FMT "multime.cut.%s"
#define ME_HOTKEY_AUTO_FMT "multime.auto.%s"
#define ME_HOTKEY_PVW_FMT "multime.pvw.%s.%d"

/* Fixed number of preview-bus hotkeys per bank ("Preview Input 1..N"), like the
 * fixed bus buttons of a hardware switcher. Slot i switches the i-th scene of
 * the filtered bus list (me_scenes_enum) into the bank's preview. */
#define ME_PVW_SLOTS 12

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the source type "multi_me_bank" (one M/E mixing layer as a
 * re-entry source). Call in obs_module_load(). */
void me_bank_register_source(void);

#ifdef __cplusplus
}
#endif
