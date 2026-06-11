/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

/*
 * me-output.c — eigener Datei-Output je Bank (siehe me-output.h).
 *
 * Pipeline beim Start:
 *   obs_view (Bank als Channel 0) -> obs_view_add2 (Video-Mix in Bank-Größe)
 *   -> obs_video_encoder -> ffmpeg_muxer-Output -> Datei.
 * Beim Stop wird asynchron finalisiert; das "stop"-Signal setzt active=false.
 * Die Ressourcen werden beim nächsten Start bzw. beim Destroy freigegeben
 * (nicht im Signal-Callback selbst -> kein Release des Outputs aus seinem
 * eigenen Callback heraus).
 */

#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <plugin-support.h>

#include <string.h>
#include <stdio.h>
#include <time.h>

#include "me-output.h"

struct me_output {
	obs_source_t *bank; /* Parent-Bank (roher Zeiger, extern gehalten) */
	obs_view_t *view;
	video_t *video;
	obs_encoder_t *encoder;  /* Video                                   */
	obs_encoder_t *aencoder; /* Audio (OBS-Hauptspur, Track 1)          */
	obs_output_t *output;
	pthread_mutex_t mutex;
	bool active;
	char path[1024];
};

/* ---- Hilfen ------------------------------------------------------------- */

/* Quellnamen in einen dateisystemtauglichen Baustein wandeln. */
static void sanitize(const char *in, char *out, size_t out_sz)
{
	size_t j = 0;
	for (size_t i = 0; in && in[i] && j + 1 < out_sz; i++) {
		char c = in[i];
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
			  c == '_';
		out[j++] = ok ? c : '_';
	}
	if (j == 0 && out_sz > 1)
		out[j++] = 'M';
	out[j] = '\0';
}

static void timestamp(char *out, size_t out_sz)
{
	time_t t = time(NULL);
	struct tm tmv;
#ifdef _WIN32
	localtime_s(&tmv, &t);
#else
	localtime_r(&t, &tmv);
#endif
	strftime(out, out_sz, "%Y-%m-%d_%H-%M-%S", &tmv);
}

static void on_output_stopped(void *data, calldata_t *cd);

/* Ressourcen freigeben (nur aufrufen, wenn NICHT mehr aktiv). Reihenfolge:
 * Output (nutzt Encoder) -> Encoder (nutzt Video) -> View/Video. */
static void release_resources(struct me_output *o)
{
	if (o->output) {
		signal_handler_disconnect(obs_output_get_signal_handler(o->output), "stop", on_output_stopped, o);
		obs_output_release(o->output);
		o->output = NULL;
	}
	if (o->encoder) {
		obs_encoder_release(o->encoder);
		o->encoder = NULL;
	}
	if (o->aencoder) {
		obs_encoder_release(o->aencoder);
		o->aencoder = NULL;
	}
	if (o->view) {
		obs_view_set_source(o->view, 0, NULL);
		obs_view_remove(o->view);
		obs_view_destroy(o->view);
		o->view = NULL;
		o->video = NULL;
	}
}

static void on_output_stopped(void *data, calldata_t *cd)
{
	struct me_output *o = data;
	long long code = calldata_int(cd, "code");
	pthread_mutex_lock(&o->mutex);
	o->active = false;
	pthread_mutex_unlock(&o->mutex);
	obs_log(LOG_INFO, "recording stopped (code %lld): %s", code, o->path);
}

/* ---- Lebenszyklus ------------------------------------------------------- */

me_output_t *me_output_create(obs_source_t *bank_source)
{
	struct me_output *o = bzalloc(sizeof(struct me_output));
	o->bank = bank_source;
	pthread_mutex_init(&o->mutex, NULL);
	return o;
}

bool me_output_start(me_output_t *o)
{
	if (!o)
		return false;
	pthread_mutex_lock(&o->mutex);
	if (o->active) {
		pthread_mutex_unlock(&o->mutex);
		return true;
	}
	release_resources(o); /* evtl. Reste eines vorigen Laufs */

	obs_data_t *s = obs_source_get_settings(o->bank);
	char *enc_id = bstrdup(obs_data_get_string(s, "rec_encoder"));
	long long bitrate = obs_data_get_int(s, "rec_bitrate");
	char *dir = bstrdup(obs_data_get_string(s, "rec_path"));
	char *fmt = bstrdup(obs_data_get_string(s, "rec_format"));
	obs_data_release(s);

	bool ok = false;

	if (!dir || !*dir) {
		obs_log(LOG_WARNING, "recording not started: no folder set (rec_path)");
		goto done;
	}

	/* Video-Mix in Bank-Auflösung (sonst Haupt-Canvas). */
	struct obs_video_info ovi;
	if (!obs_get_video_info(&ovi)) {
		obs_log(LOG_WARNING, "recording not started: no video info");
		goto done;
	}
	uint32_t w = obs_source_get_width(o->bank);
	uint32_t h = obs_source_get_height(o->bank);
	if (w && h) {
		ovi.base_width = ovi.output_width = w;
		ovi.base_height = ovi.output_height = h;
	}

	o->view = obs_view_create();
	obs_view_set_source(o->view, 0, o->bank);
	o->video = obs_view_add2(o->view, &ovi);
	if (!o->video) {
		obs_log(LOG_WARNING, "recording not started: obs_view_add2 failed");
		goto done;
	}

	if (!enc_id || !*enc_id) {
		bfree(enc_id);
		enc_id = bstrdup("obs_x264");
	}
	obs_data_t *es = obs_data_create();
	obs_data_set_int(es, "bitrate", bitrate > 0 ? bitrate : 6000);
	obs_data_set_string(es, "rate_control", "CBR");
	o->encoder = obs_video_encoder_create(enc_id, "multi_me_rec_video", es, NULL);
	obs_data_release(es);
	if (!o->encoder) {
		obs_log(LOG_WARNING, "recording not started: encoder '%s' failed", enc_id);
		goto done;
	}
	obs_encoder_set_video(o->encoder, o->video);

	/* Audio: OBS-Hauptspur (Track 1 = mixer 0) — dieselbe Quelle wie die normale
	 * OBS-Aufnahme. Gemeinsame Audiospur = Referenz zum frame-genauen Ausrichten
	 * (Sync) der beiden Aufnahmen im Schnitt. */
	obs_data_t *as = obs_data_create();
	obs_data_set_int(as, "bitrate", 160);
	o->aencoder = obs_audio_encoder_create("ffmpeg_aac", "multi_me_rec_audio", as, 0, NULL);
	obs_data_release(as);
	if (o->aencoder)
		obs_encoder_set_audio(o->aencoder, obs_get_audio());
	else
		obs_log(LOG_WARNING, "recording: AAC audio encoder failed (continuing video-only)");

	/* Zielpfad: <Ordner>/<Bankname>_<Zeitstempel>.<Container> */
	if (!fmt || !*fmt)
		fmt = bstrdup("mkv");
	char safe[256], ts[32];
	sanitize(obs_source_get_name(o->bank), safe, sizeof(safe));
	timestamp(ts, sizeof(ts));
	size_t dl = strlen(dir);
	bool slash = dl > 0 && (dir[dl - 1] == '/' || dir[dl - 1] == '\\');
	snprintf(o->path, sizeof(o->path), "%s%s%s_%s.%s", dir, slash ? "" : "/", safe, ts, fmt);

	obs_data_t *os = obs_data_create();
	obs_data_set_string(os, "path", o->path);
	o->output = obs_output_create("ffmpeg_muxer", "multi_me_rec_output", os, NULL);
	obs_data_release(os);
	if (!o->output) {
		obs_log(LOG_WARNING, "recording not started: ffmpeg_muxer create failed");
		goto done;
	}
	signal_handler_connect(obs_output_get_signal_handler(o->output), "stop", on_output_stopped, o);
	obs_output_set_video_encoder(o->output, o->encoder);
	if (o->aencoder)
		obs_output_set_audio_encoder(o->output, o->aencoder, 0);

	if (!obs_output_start(o->output)) {
		const char *err = obs_output_get_last_error(o->output);
		obs_log(LOG_WARNING, "recording start failed: %s", err ? err : "unknown");
		goto done;
	}
	o->active = true;
	ok = true;
	obs_log(LOG_INFO, "recording started: %s", o->path);

done:
	if (!ok)
		release_resources(o);
	bfree(enc_id);
	bfree(dir);
	bfree(fmt);
	pthread_mutex_unlock(&o->mutex);
	return ok;
}

void me_output_stop(me_output_t *o)
{
	if (!o)
		return;
	pthread_mutex_lock(&o->mutex);
	if (o->active && o->output)
		obs_output_stop(o->output); /* active wird im "stop"-Signal false */
	pthread_mutex_unlock(&o->mutex);
}

bool me_output_active(me_output_t *o)
{
	if (!o)
		return false;
	pthread_mutex_lock(&o->mutex);
	bool a = o->active;
	pthread_mutex_unlock(&o->mutex);
	return a;
}

const char *me_output_path(me_output_t *o)
{
	return o ? o->path : "";
}

void me_output_destroy(me_output_t *o)
{
	if (!o)
		return;
	/* "stop"-Signal trennen, BEVOR wir (force-)stoppen und freigeben, damit der
	 * Callback nicht nach dem free feuert. */
	if (o->output)
		signal_handler_disconnect(obs_output_get_signal_handler(o->output), "stop", on_output_stopped, o);

	pthread_mutex_lock(&o->mutex);
	bool act = o->active;
	o->active = false;
	pthread_mutex_unlock(&o->mutex);
	if (act && o->output)
		obs_output_force_stop(o->output);

	release_resources(o);
	pthread_mutex_destroy(&o->mutex);
	bfree(o);
}

/* ---- Properties / Defaults --------------------------------------------- */

void me_output_add_properties(obs_properties_t *props)
{
	obs_property_t *enc = obs_properties_add_list(props, "rec_encoder", "Aufnahme-Encoder", OBS_COMBO_TYPE_LIST,
						      OBS_COMBO_FORMAT_STRING);
	size_t i = 0;
	const char *id;
	while (obs_enum_encoder_types(i++, &id)) {
		if (obs_get_encoder_type(id) != OBS_ENCODER_VIDEO)
			continue;
		const char *disp = obs_encoder_get_display_name(id);
		obs_property_list_add_string(enc, disp ? disp : id, id);
	}

	obs_properties_add_int(props, "rec_bitrate", "Aufnahme-Bitrate (kbps)", 500, 100000, 500);
	obs_properties_add_path(props, "rec_path", "Aufnahme-Ordner", OBS_PATH_DIRECTORY, NULL, NULL);

	obs_property_t *f =
		obs_properties_add_list(props, "rec_format", "Container", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(f, "MKV", "mkv");
	obs_property_list_add_string(f, "MP4", "mp4");
	obs_property_list_add_string(f, "MOV", "mov");
}

void me_output_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "rec_encoder", "obs_x264");
	obs_data_set_default_int(settings, "rec_bitrate", 6000);
	obs_data_set_default_string(settings, "rec_format", "mkv");
}
