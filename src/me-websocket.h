/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <mail@paulloth.de>

GPL v2+ (see plugin-main.c).
*/

#pragma once

#include <obs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the obs-websocket vendor "multi-me" (for Bitfocus Companion &
 * other WebSocket clients). No-op if obs-websocket is not loaded. Call from
 * obs_module_post_load() (after all modules have loaded). */
void me_websocket_register(void);

/* Emit a per-bank "state_#<N>" vendor event (preview/program input + recording)
 * so control surfaces can show a live tally. No-op if the vendor isn't
 * registered. Safe to call from any thread (marshals to the UI thread). */
void me_websocket_emit_bank_state(obs_source_t *bank);

#ifdef __cplusplus
}
#endif
