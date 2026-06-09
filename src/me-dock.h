/*
2ME — Second Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

GPL v2+ (siehe plugin-main.c).
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Registriert das 2ME-Dock im OBS-Frontend. Aus obs_module_post_load() aufrufen
 * (Frontend + Qt-Hauptthread sind dann bereit). */
void me_dock_register(void);

#ifdef __cplusplus
}
#endif
