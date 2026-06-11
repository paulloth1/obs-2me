/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <mail@paulloth.de>

GPL v2+ (see plugin-main.c).
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the obs-websocket vendor "multi-me" (for Bitfocus Companion &
 * other WebSocket clients). No-op if obs-websocket is not loaded. Call from
 * obs_module_post_load() (after all modules have loaded). */
void me_websocket_register(void);

#ifdef __cplusplus
}
#endif
