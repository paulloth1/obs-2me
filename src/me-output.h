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

/*
 * me-output — this M/E bank's own file output.
 *
 * Records the mixed bank output via a private obs_view (its own video mix in
 * bank resolution) with its own video encoder to a file (ffmpeg_muxer). Lazy:
 * resources are created on start, torn down after stop. Video-only recording
 * (the bank is video-only) plus the OBS main audio track.
 *
 * Settings are read from the bank source's settings:
 *   rec_encoder (string, encoder id), rec_bitrate (int kbps),
 *   rec_path (string, folder), rec_format (string, container mkv/mp4/mov).
 */
typedef struct me_output me_output_t;

/* Bound to the bank source (raw pointer; the bank owns the lifetime). */
me_output_t *me_output_create(obs_source_t *bank_source);
void me_output_destroy(me_output_t *o);

/* Start reads the bank's rec_* settings; true = recording is running. */
bool me_output_start(me_output_t *o);
void me_output_stop(me_output_t *o);
bool me_output_active(me_output_t *o);

/* Path of the current/last written file ("" if none). */
const char *me_output_path(me_output_t *o);

/* Settings UI/defaults (from me_bank_get_properties / me_bank_defaults). */
void me_output_add_properties(obs_properties_t *props);
void me_output_get_defaults(obs_data_t *settings);

#ifdef __cplusplus
}
#endif
