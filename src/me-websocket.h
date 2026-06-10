/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

GPL v2+ (siehe plugin-main.c).
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Registriert den obs-websocket-Vendor "multi-me" (für Bitfocus Companion &
 * andere WebSocket-Clients). No-op, falls obs-websocket nicht geladen ist.
 * Aus obs_module_post_load() aufrufen (nach dem Laden aller Module). */
void me_websocket_register(void);

#ifdef __cplusplus
}
#endif
