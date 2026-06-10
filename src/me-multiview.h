/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

GPL v2+ (siehe plugin-main.c).
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Registriert das "Multi-M/E Multiview"-Menü (Tools) zum Öffnen einer PGM/PVW-
 * Multiview je M/E-Bank als Fenster oder Vollbild. Aus obs_module_post_load(). */
void me_multiview_register(void);

#ifdef __cplusplus
}
#endif
