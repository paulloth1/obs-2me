/*
2ME — Second Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

/*
 * me-bank-source.c — Eine M/E-Mischebene ("Bank") als OBS-Quelltyp.
 *
 * Jede Instanz dieser Quelle ist eine eigenständige Mischebene mit eigenem
 * Program-/Preview-Bus und eigener Übergangslogik (Cut / Auto-Fade). Intern
 * hält die Bank eine private Transition-Source (den A/B-Mischer). Der Ausgang
 * dieser Quelle ist das gemischte Bild und kann wie jede Quelle in Hauptszenen
 * platziert werden ("Re-entry").
 *
 * Bus-Quellen sind OBS-Szenen (per Properties wählbar). Geschaltet wird per
 * OBS-Hotkeys (Cut / Auto). Audio: bewusst keins (video-only) — Audio-Routing
 * bleibt im normalen OBS-Mixer.
 */

#include <obs-module.h>
#include <util/platform.h>
#include <pthread.h>
#include <string.h>

#include "me-bank.h"

struct me_bank {
	obs_source_t *source;     /* unsere eigene Quelle (Parent)            */
	obs_source_t *transition; /* privater A/B-Mischer                     */
	char *transition_kind;    /* aktueller Transition-Typ (z. B. fade_…)  */
	uint32_t cx, cy;          /* Ausgabegröße (= Haupt-Canvas)            */
	uint32_t duration_ms;     /* Auto-Übergangsdauer                      */

	pthread_mutex_t mutex;    /* schützt pgm/pvw/in_transition            */
	obs_weak_source_t *pgm;   /* aktuell auf Program                      */
	obs_weak_source_t *pvw;   /* aktuell auf Preview                      */
	bool in_transition;
	uint64_t transition_end_ns; /* Wall-Clock-Ende des Auto-Übergangs     */

	obs_hotkey_id cut_hotkey;
	obs_hotkey_id auto_hotkey;
};

/* ---------------------------------------------------------------- Helpers -- */

/* Szene per Name auflösen und als Weak-Ref in *slot ablegen. */
static void set_weak_by_name(obs_weak_source_t **slot, const char *name)
{
	obs_weak_source_release(*slot);
	*slot = NULL;
	if (name && *name) {
		obs_source_t *s = obs_get_source_by_name(name);
		if (s) {
			*slot = obs_source_get_weak_source(s);
			obs_source_release(s);
		}
	}
}

/* -------------------------------------------------------------- Take-Logik - */

/* Auto-Übergang fertig: nur das in_transition-Flag löschen
 * (der Bus-Swap passiert bereits beim Auslösen des Takes). */
static void me_bank_transition_stopped(void *data, calldata_t *cd)
{
	struct me_bank *b = data;
	UNUSED_PARAMETER(cd);
	pthread_mutex_lock(&b->mutex);
	b->in_transition = false;
	pthread_mutex_unlock(&b->mutex);
}

/* Transition-Source vom gewünschten Typ sicherstellen (neu erzeugen, falls Typ
 * gewechselt). Überträgt Aktiv-/Sichtbar-Zustand und zeigt aktuelles PGM. */
static void bank_apply_transition_kind(struct me_bank *b, const char *kind)
{
	if (!kind || !*kind)
		kind = "fade_transition";
	if (b->transition && b->transition_kind && strcmp(b->transition_kind, kind) == 0)
		return;

	obs_source_t *new_tr = obs_source_create_private(kind, "2me transition", NULL);
	if (!new_tr)
		return;

	obs_transition_set_size(new_tr, b->cx, b->cy);
	signal_handler_connect(obs_source_get_signal_handler(new_tr), "transition_stop",
			       me_bank_transition_stopped, b);

	obs_source_t *pgm = obs_weak_source_get_source(b->pgm);
	if (pgm) {
		obs_transition_set(new_tr, pgm);
		obs_source_release(pgm);
	}
	if (obs_source_active(b->source))
		obs_source_inc_active(new_tr);
	if (obs_source_showing(b->source))
		obs_source_inc_showing(new_tr);

	obs_source_t *old = b->transition;
	b->transition = new_tr;
	if (old) {
		signal_handler_disconnect(obs_source_get_signal_handler(old), "transition_stop",
					  me_bank_transition_stopped, b);
		obs_source_release(old);
	}
	bfree(b->transition_kind);
	b->transition_kind = bstrdup(kind);
}

/* CUT: PVW sofort auf PGM. */
static void bank_cut(struct me_bank *b)
{
	pthread_mutex_lock(&b->mutex);
	obs_source_t *dest = obs_weak_source_get_source(b->pvw);
	if (dest) {
		obs_weak_source_t *tmp = b->pgm; /* PVW wird neues PGM */
		b->pgm = b->pvw;
		b->pvw = tmp;
		b->in_transition = false;
	}
	pthread_mutex_unlock(&b->mutex);

	if (dest) {
		obs_transition_set(b->transition, dest);
		obs_source_release(dest);
	}
}

/* AUTO/Take: zeitgesteuerter Übergang PVW -> PGM. */
static void bank_auto(struct me_bank *b)
{
	pthread_mutex_lock(&b->mutex);
	obs_source_t *dest = NULL;
	if (!b->in_transition) {
		dest = obs_weak_source_get_source(b->pvw);
		if (dest) {
			obs_weak_source_t *tmp = b->pgm; /* PVW wird neues PGM */
			b->pgm = b->pvw;
			b->pvw = tmp;
			b->in_transition = true;
			b->transition_end_ns = os_gettime_ns() +
					       (uint64_t)b->duration_ms * 1000000ULL;
		}
	}
	pthread_mutex_unlock(&b->mutex);

	/* obs_transition_start außerhalb des Locks aufrufen: bei 0 ms feuert
	 * "transition_stop" synchron -> würde sonst den Mutex doppelt nehmen. */
	if (dest) {
		obs_transition_start(b->transition, OBS_TRANSITION_MODE_AUTO,
				     b->duration_ms, dest);
		obs_source_release(dest);
	}
}

/* ---- Proc-Handler-Schnittstelle (Brücke zum Qt-Dock) ---- */
static void me_bank_proc_cut(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(cd);
	bank_cut((struct me_bank *)data);
}

static void me_bank_proc_auto(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(cd);
	bank_auto((struct me_bank *)data);
}

static void me_bank_proc_set_preview(void *data, calldata_t *cd)
{
	struct me_bank *b = data;
	const char *scene = calldata_string(cd, "scene");
	pthread_mutex_lock(&b->mutex);
	set_weak_by_name(&b->pvw, scene);
	pthread_mutex_unlock(&b->mutex);
	/* in den Settings persistieren (überlebt Reload), ohne update() auszulösen */
	obs_data_t *s = obs_source_get_settings(b->source);
	obs_data_set_string(s, "pvw_scene", scene ? scene : "");
	obs_data_release(s);
}

static void me_bank_proc_set_transition(void *data, calldata_t *cd)
{
	struct me_bank *b = data;
	const char *kind = calldata_string(cd, "kind");
	if (!kind || !*kind)
		return;
	bank_apply_transition_kind(b, kind);
	obs_data_t *s = obs_source_get_settings(b->source);
	obs_data_set_string(s, "transition_kind", kind);
	obs_data_release(s);
}

static void me_bank_proc_set_duration(void *data, calldata_t *cd)
{
	struct me_bank *b = data;
	long long ms = calldata_int(cd, "ms");
	if (ms < 0)
		ms = 0;
	b->duration_ms = (uint32_t)ms;
	obs_data_t *s = obs_source_get_settings(b->source);
	obs_data_set_int(s, "duration_ms", ms);
	obs_data_release(s);
}

static void me_bank_proc_get_state(void *data, calldata_t *cd)
{
	struct me_bank *b = data;
	pthread_mutex_lock(&b->mutex);
	obs_source_t *pgm = obs_weak_source_get_source(b->pgm);
	obs_source_t *pvw = obs_weak_source_get_source(b->pvw);
	bool intr = b->in_transition;
	pthread_mutex_unlock(&b->mutex);
	calldata_set_string(cd, "program", pgm ? obs_source_get_name(pgm) : "");
	calldata_set_string(cd, "preview", pvw ? obs_source_get_name(pvw) : "");
	calldata_set_bool(cd, "in_transition", intr);
	calldata_set_string(cd, "kind", b->transition_kind ? b->transition_kind : "fade_transition");
	calldata_set_int(cd, "duration", (long long)b->duration_ms);
	obs_source_release(pgm);
	obs_source_release(pvw);
}

static void me_bank_hotkey_cut(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (pressed)
		bank_cut((struct me_bank *)data);
}

static void me_bank_hotkey_auto(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (pressed)
		bank_auto((struct me_bank *)data);
}

/* Buttons in den Eigenschaften (priv = unser me_bank via add_button2). */
static bool me_bank_cut_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	if (data)
		bank_cut((struct me_bank *)data);
	return false;
}

static bool me_bank_auto_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	if (data)
		bank_auto((struct me_bank *)data);
	return false;
}

/* ----------------------------------------------------- Quelle: Lebenszyklus - */

static const char *me_bank_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "2ME \xe2\x80\x94 Mix/Effects (Re-entry)";
}

static void me_bank_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "transition_kind", "fade_transition");
	obs_data_set_default_int(settings, "duration_ms", 300);
}

static void me_bank_update(void *data, obs_data_t *settings)
{
	struct me_bank *b = data;

	const char *kind = obs_data_get_string(settings, "transition_kind");
	if (!kind || !*kind)
		kind = "fade_transition";
	b->duration_ms = (uint32_t)obs_data_get_int(settings, "duration_ms");

	pthread_mutex_lock(&b->mutex);
	set_weak_by_name(&b->pgm, obs_data_get_string(settings, "pgm_scene"));
	set_weak_by_name(&b->pvw, obs_data_get_string(settings, "pvw_scene"));
	bool in_tr = b->in_transition;
	pthread_mutex_unlock(&b->mutex);

	bank_apply_transition_kind(b, kind);

	if (!in_tr) {
		/* aktuelle PGM-Szene anzeigen (z. B. nach Szenenwechsel) */
		obs_source_t *pgm = obs_weak_source_get_source(b->pgm);
		if (pgm) {
			obs_transition_set(b->transition, pgm);
			obs_source_release(pgm);
		}
	}
}

static void *me_bank_create(obs_data_t *settings, obs_source_t *source)
{
	struct me_bank *b = bzalloc(sizeof(struct me_bank));
	b->source = source;
	b->cut_hotkey = OBS_INVALID_HOTKEY_ID;
	b->auto_hotkey = OBS_INVALID_HOTKEY_ID;
	pthread_mutex_init(&b->mutex, NULL);

	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		b->cx = ovi.base_width;
		b->cy = ovi.base_height;
	} else {
		b->cx = 1920;
		b->cy = 1080;
	}

	b->cut_hotkey = obs_hotkey_register_source(source, "2me.cut", "2ME: Cut (PVW -> PGM)",
						   me_bank_hotkey_cut, b);
	b->auto_hotkey = obs_hotkey_register_source(source, "2me.auto", "2ME: Auto/Take (PVW -> PGM)",
						    me_bank_hotkey_auto, b);

	proc_handler_t *ph = obs_source_get_proc_handler(source);
	proc_handler_add(ph, "void cut()", me_bank_proc_cut, b);
	proc_handler_add(ph, "void auto_take()", me_bank_proc_auto, b);
	proc_handler_add(ph, "void set_preview(in string scene)", me_bank_proc_set_preview, b);
	proc_handler_add(ph, "void set_transition(in string kind)", me_bank_proc_set_transition, b);
	proc_handler_add(ph, "void set_duration(in int ms)", me_bank_proc_set_duration, b);
	proc_handler_add(ph,
			 "void get_state(out string program, out string preview, out bool in_transition, out string kind, out int duration)",
			 me_bank_proc_get_state, b);

	me_bank_update(b, settings);
	return b;
}

static void me_bank_destroy(void *data)
{
	struct me_bank *b = data;
	if (b->cut_hotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(b->cut_hotkey);
	if (b->auto_hotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(b->auto_hotkey);
	if (b->transition) {
		signal_handler_disconnect(obs_source_get_signal_handler(b->transition),
					  "transition_stop", me_bank_transition_stopped, b);
		obs_source_release(b->transition);
	}
	obs_weak_source_release(b->pgm);
	obs_weak_source_release(b->pvw);
	bfree(b->transition_kind);
	pthread_mutex_destroy(&b->mutex);
	bfree(b);
}

/* -------------------------------------------------- Quelle: Aktiv/Sichtbar - */

static void me_bank_activate(void *data)
{
	struct me_bank *b = data;
	if (b->transition)
		obs_source_inc_active(b->transition);
}

static void me_bank_deactivate(void *data)
{
	struct me_bank *b = data;
	if (b->transition)
		obs_source_dec_active(b->transition);
}

static void me_bank_show(void *data)
{
	struct me_bank *b = data;
	if (b->transition)
		obs_source_inc_showing(b->transition);
}

static void me_bank_hide(void *data)
{
	struct me_bank *b = data;
	if (b->transition)
		obs_source_dec_showing(b->transition);
}

/* ------------------------------------------------------ Quelle: Rendering -- */

/* in_transition zeitbasiert zurücksetzen (das transition_stop-Signal feuert bei
 * video-only-Nutzung nie, weil der Audio-Teil des Übergangs nie abschließt).
 * video_tick wird von OBS jeden Frame für jede Quelle aufgerufen. */
static void me_bank_video_tick(void *data, float seconds)
{
	struct me_bank *b = data;
	UNUSED_PARAMETER(seconds);
	if (!b->in_transition)
		return;
	pthread_mutex_lock(&b->mutex);
	if (b->in_transition && os_gettime_ns() >= b->transition_end_ns)
		b->in_transition = false;
	pthread_mutex_unlock(&b->mutex);
}

static void me_bank_video_render(void *data, gs_effect_t *effect)
{
	struct me_bank *b = data;
	UNUSED_PARAMETER(effect);
	if (b->transition)
		obs_source_video_render(b->transition);
}

static uint32_t me_bank_get_width(void *data)
{
	return ((struct me_bank *)data)->cx;
}

static uint32_t me_bank_get_height(void *data)
{
	return ((struct me_bank *)data)->cy;
}

static void me_bank_enum_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
	struct me_bank *b = data;
	if (b->transition)
		cb(b->source, b->transition, param);
}

/* ----------------------------------------------------- Quelle: Properties -- */

static bool add_scene_to_list(void *param, obs_source_t *src)
{
	if (obs_source_get_type(src) == OBS_SOURCE_TYPE_SCENE) {
		obs_property_t *list = param;
		const char *name = obs_source_get_name(src);
		if (name)
			obs_property_list_add_string(list, name, name);
	}
	return true;
}

static obs_properties_t *me_bank_get_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *pgm = obs_properties_add_list(props, "pgm_scene", "Program-Szene (PGM)",
						      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_t *pvw = obs_properties_add_list(props, "pvw_scene", "Preview-Szene (PVW)",
						      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_enum_scenes(add_scene_to_list, pgm);
	obs_enum_scenes(add_scene_to_list, pvw);

	obs_property_t *tk = obs_properties_add_list(props, "transition_kind", "Auto-Übergang",
						     OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(tk, "Fade", "fade_transition");
	obs_property_list_add_string(tk, "Swipe", "swipe_transition");
	obs_property_list_add_string(tk, "Slide", "slide_transition");

	obs_properties_add_int(props, "duration_ms", "Übergangsdauer (ms)", 0, 10000, 50);

	/* Direkt testbar: Buttons lösen den Take live aus (priv = diese Bank). */
	obs_properties_add_button2(props, "cut_btn", "CUT (sofort)", me_bank_cut_clicked, data);
	obs_properties_add_button2(props, "auto_btn", "AUTO / TAKE (Übergang)", me_bank_auto_clicked, data);

	obs_properties_add_text(props, "_hint",
				"Alternativ per Hotkey (Einstellungen → Hotkeys): "
				"\"2ME: Cut\" / \"2ME: Auto/Take\". "
				"Die \"Sichtbar/Anzeigen\"-Hotkeys sind OBS-Standard und NICHT 2ME.",
				OBS_TEXT_INFO);
	return props;
}

/* ----------------------------------------------------- Registrierung ------- */

static struct obs_source_info me_bank_info = {
	.id = "2me_bank_output",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = me_bank_get_name,
	.create = me_bank_create,
	.destroy = me_bank_destroy,
	.update = me_bank_update,
	.get_defaults = me_bank_defaults,
	.get_properties = me_bank_get_properties,
	.activate = me_bank_activate,
	.deactivate = me_bank_deactivate,
	.show = me_bank_show,
	.hide = me_bank_hide,
	.video_tick = me_bank_video_tick,
	.video_render = me_bank_video_render,
	.get_width = me_bank_get_width,
	.get_height = me_bank_get_height,
	.enum_active_sources = me_bank_enum_sources,
	.enum_all_sources = me_bank_enum_sources,
};

void me_bank_register_source(void)
{
	obs_register_source(&me_bank_info);
}
