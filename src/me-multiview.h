/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <mail@paulloth.de>

GPL v2+ (see plugin-main.c).
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the "Multi-M/E Multiview" menu (Tools) to open a PGM/PVW multiview
 * per M/E bank as a window or fullscreen. From obs_module_post_load(). */
void me_multiview_register(void);

#ifdef __cplusplus
}
#endif
