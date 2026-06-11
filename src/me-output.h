/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

GPL v2+ (siehe plugin-main.c).
*/

#pragma once

#include <obs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * me-output — eigener Datei-Output je M/E-Bank.
 *
 * Nimmt den gemischten Bank-Ausgang über eine private obs_view (eigener
 * Video-Mix in Bank-Auflösung) mit eigenem Video-Encoder als Datei auf
 * (ffmpeg_muxer). Lazy: Ressourcen entstehen erst beim Start, Teardown nach
 * dem Stop. Reine Video-Aufnahme (die Bank ist video-only).
 *
 * Einstellungen werden aus den Settings der Bank-Quelle gelesen:
 *   rec_encoder (string, Encoder-ID), rec_bitrate (int kbps),
 *   rec_path (string, Ordner), rec_format (string, Container mkv/mp4/mov).
 */
typedef struct me_output me_output_t;

/* An die Bank-Quelle gebunden (roher Zeiger; Lebensdauer hält die Bank). */
me_output_t *me_output_create(obs_source_t *bank_source);
void me_output_destroy(me_output_t *o);

/* Start liest die rec_*-Settings der Bank; true = Aufnahme läuft. */
bool me_output_start(me_output_t *o);
void me_output_stop(me_output_t *o);
bool me_output_active(me_output_t *o);

/* Pfad der aktuellen/zuletzt geschriebenen Datei ("" wenn keiner). */
const char *me_output_path(me_output_t *o);

/* Settings-UI/-Defaults (aus me_bank_get_properties / me_bank_defaults). */
void me_output_add_properties(obs_properties_t *props);
void me_output_get_defaults(obs_data_t *settings);

#ifdef __cplusplus
}
#endif
