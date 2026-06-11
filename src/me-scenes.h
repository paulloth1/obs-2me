/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <mail@paulloth.de>

GPL v2+ (see plugin-main.c).
*/

#pragma once

#include <obs.h>

/* Callback type outside of extern "C" (so C++ lambdas match). */
typedef bool (*me_scene_cb)(void *param, const char *name, obs_source_t *scene);

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Enumerates the scenes in OBS UI order (obs_frontend_get_scenes) and filters
 * centrally:
 *   - scenes with the private setting show_in_multiview == false are skipped
 *     (same logic as the native OBS multiview).
 *   - if exclude_bank_uuid is set, scenes that contain that bank source
 *     (directly or nested) are skipped — feedback protection.
 *
 * cb(param, name, scene) is called per scene; returning false stops. The passed
 * scene source is only valid during the callback.
 */
void me_scenes_enum(const char *exclude_bank_uuid, me_scene_cb cb, void *param);

#ifdef __cplusplus
}
#endif
