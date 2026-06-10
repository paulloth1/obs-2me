/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

GPL v2+ (siehe plugin-main.c).
*/

#pragma once

#include <obs.h>

/* Callback-Typ außerhalb von extern "C" (damit C++-Lambdas passen). */
typedef bool (*me_scene_cb)(void *param, const char *name, obs_source_t *scene);

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Zählt die Szenen in OBS-UI-Reihenfolge auf (obs_frontend_get_scenes) und
 * filtert dabei zentral:
 *   - Szenen mit privater Einstellung show_in_multiview == false werden
 *     übersprungen (gleiche Logik wie die native OBS-Multiview).
 *   - Ist exclude_bank_uuid gesetzt, werden Szenen übersprungen, die diese
 *     Bank-Quelle (direkt oder verschachtelt) enthalten — Feedback-Schutz.
 *
 * cb(param, name, scene) wird je Szene aufgerufen; Rückgabe false bricht ab.
 * Die übergebene scene-Quelle ist nur während des Callbacks gültig.
 */
void me_scenes_enum(const char *exclude_bank_uuid, me_scene_cb cb, void *param);

#ifdef __cplusplus
}
#endif
