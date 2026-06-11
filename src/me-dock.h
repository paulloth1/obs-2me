/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <mail@paulloth.de>

GPL v2+ (see plugin-main.c).
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the Multi-M/E dock in the OBS frontend. Call from
 * obs_module_post_load() (frontend + Qt main thread are ready by then). */
void me_dock_register(void);

#ifdef __cplusplus
}
#endif
