/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <mail@paulloth.de>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

/*
 * me-websocket.c — obs-websocket vendor "multi-me".
 *
 * Provides custom requests that WebSocket clients (e.g. Bitfocus Companion via
 * CallVendorRequest) can trigger to control the M/E banks. The requests hook
 * into the proc handlers of the bank sources.
 *
 * Requests (vendorName = "multi-me"):
 *   set_preview        { bank, scene } -> scene into the bank's preview
 *   set_preview_index  { bank, input } -> N-th bus scene (1-based) into preview
 *   set_program        { bank, scene } -> scene straight to program (hard cut)
 *   set_program_index  { bank, input } -> N-th bus scene (1-based) to program
 *   cut          { bank }            -> Cut (PVW -> PGM)
 *   auto         { bank }            -> Auto-Take (transition)
 *   set_transition { bank, kind }    -> set the auto transition type
 *   set_duration   { bank, ms }      -> set the auto transition duration (ms)
 *   get_banks    { }                 -> list of all banks
 *                                       [{name, uuid, hotkey_cut, hotkey_auto,
 *                                         hotkeys_preview:[{input, hotkey}]}]
 *   get_state    { bank }            -> { program, preview, in_transition, kind,
 *                                         duration, recording, rec_file }
 *   start_record { bank }            -> start this bank's file recording
 *   stop_record  { bank }            -> stop this bank's file recording
 *
 * "bank" accepts a UUID, a source name, or "#N" (the N-th bank, 1-based) — the
 * latter lets a generic control surface address banks without knowing names.
 *
 * The binding to obs-websocket goes through its globally registered procs
 * (stable API): "obs_websocket_api_get_ph" -> returns the proc handler, on
 * which "vendor_register" and "vendor_request_register" are called.
 */

#include <obs-module.h>
#include <plugin-support.h>
#include <string.h>
#include <stdio.h>  /* snprintf */
#include <stdlib.h> /* atoi */

#include "me-websocket.h"
#include "me-bank.h"   /* hotkey naming scheme (ME_HOTKEY_*_FMT, ME_PVW_SLOTS) */
#include "me-scenes.h" /* me_scenes_enum (bus order for state events) */

typedef void (*ws_request_cb)(obs_data_t *request_data, obs_data_t *response_data, void *priv_data);

struct ws_request_callback {
	ws_request_cb callback;
	void *priv_data;
};

/* ---- Minimal obs-websocket binding via procs ---------------------------- */

static proc_handler_t *ws_get_ph(void)
{
	proc_handler_t *global_ph = obs_get_proc_handler();
	if (!global_ph)
		return NULL;
	calldata_t cd;
	calldata_init(&cd);
	proc_handler_call(global_ph, "obs_websocket_api_get_ph", &cd);
	proc_handler_t *ph = (proc_handler_t *)calldata_ptr(&cd, "ph");
	calldata_free(&cd);
	return ph;
}

static void *ws_register_vendor(proc_handler_t *ph, const char *name)
{
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "name", name);
	proc_handler_call(ph, "vendor_register", &cd);
	void *vendor = calldata_ptr(&cd, "vendor");
	calldata_free(&cd);
	return vendor;
}

static bool ws_register_request(proc_handler_t *ph, void *vendor, const char *type, ws_request_cb cb)
{
	struct ws_request_callback rcb = {cb, NULL};
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_ptr(&cd, "vendor", vendor);
	calldata_set_string(&cd, "type", type);
	calldata_set_ptr(&cd, "callback", &rcb);
	proc_handler_call(ph, "vendor_request_register", &cd);
	bool ok = calldata_bool(&cd, "success");
	calldata_free(&cd);
	return ok;
}

/* ---- Helpers ------------------------------------------------------------ */

/* Find the n-th (1-based) multi_me_bank source in enumeration order. */
struct nth_bank_ctx {
	int target;
	int cur;
	obs_source_t *found; /* with ref */
};

static bool nth_bank_cb(void *p, obs_source_t *src)
{
	if (strcmp(obs_source_get_id(src), "multi_me_bank") != 0)
		return true;
	struct nth_bank_ctx *c = p;
	if (++c->cur == c->target) {
		c->found = obs_source_get_ref(src);
		return false;
	}
	return true;
}

/* Resolve a bank from the "bank" field: "#N" = the N-th bank, otherwise a UUID
 * or source name. "#N" lets a generic control surface address banks without
 * knowing their names. */
static obs_source_t *resolve_bank(obs_data_t *req)
{
	const char *bank = obs_data_get_string(req, "bank");
	if (!bank || !*bank)
		return NULL;

	if (bank[0] == '#') {
		int idx = atoi(bank + 1);
		if (idx < 1)
			return NULL;
		struct nth_bank_ctx c = {idx, 0, NULL};
		obs_enum_sources(nth_bank_cb, &c);
		return c.found;
	}

	obs_source_t *s = obs_get_source_by_uuid(bank);
	if (!s)
		s = obs_get_source_by_name(bank);
	if (s && strcmp(obs_source_get_id(s), "multi_me_bank") != 0) {
		obs_source_release(s);
		s = NULL;
	}
	return s;
}

static void bank_call_scene(obs_source_t *bank, const char *proc, const char *scene)
{
	calldata_t cd;
	calldata_init(&cd);
	if (scene)
		calldata_set_string(&cd, "scene", scene);
	proc_handler_call(obs_source_get_proc_handler(bank), proc, &cd);
	calldata_free(&cd);
}

static void fail(obs_data_t *res, const char *msg)
{
	obs_data_set_bool(res, "success", false);
	obs_data_set_string(res, "error", msg);
}

/* ---- Request handlers --------------------------------------------------- */

static void req_set_preview(obs_data_t *req, obs_data_t *res, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *b = resolve_bank(req);
	if (!b) {
		fail(res, "bank not found");
		return;
	}
	bank_call_scene(b, "set_preview", obs_data_get_string(req, "scene"));
	obs_data_set_bool(res, "success", true);
	obs_source_release(b);
}

static void req_set_preview_index(obs_data_t *req, obs_data_t *res, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *b = resolve_bank(req);
	if (!b) {
		fail(res, "bank not found");
		return;
	}
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_int(&cd, "input", obs_data_get_int(req, "input"));
	proc_handler_call(obs_source_get_proc_handler(b), "set_preview_index", &cd);
	calldata_free(&cd);
	obs_data_set_bool(res, "success", true);
	obs_source_release(b);
}

static void req_set_program(obs_data_t *req, obs_data_t *res, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *b = resolve_bank(req);
	if (!b) {
		fail(res, "bank not found");
		return;
	}
	bank_call_scene(b, "set_program", obs_data_get_string(req, "scene"));
	obs_data_set_bool(res, "success", true);
	obs_source_release(b);
}

static void req_set_program_index(obs_data_t *req, obs_data_t *res, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *b = resolve_bank(req);
	if (!b) {
		fail(res, "bank not found");
		return;
	}
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_int(&cd, "input", obs_data_get_int(req, "input"));
	proc_handler_call(obs_source_get_proc_handler(b), "set_program_index", &cd);
	calldata_free(&cd);
	obs_data_set_bool(res, "success", true);
	obs_source_release(b);
}

static void req_cut(obs_data_t *req, obs_data_t *res, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *b = resolve_bank(req);
	if (!b) {
		fail(res, "bank not found");
		return;
	}
	bank_call_scene(b, "cut", NULL);
	obs_data_set_bool(res, "success", true);
	obs_source_release(b);
}

static void req_auto(obs_data_t *req, obs_data_t *res, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *b = resolve_bank(req);
	if (!b) {
		fail(res, "bank not found");
		return;
	}
	bank_call_scene(b, "auto_take", NULL);
	obs_data_set_bool(res, "success", true);
	obs_source_release(b);
}

static void req_start_record(obs_data_t *req, obs_data_t *res, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *b = resolve_bank(req);
	if (!b) {
		fail(res, "bank not found");
		return;
	}
	calldata_t cd;
	calldata_init(&cd);
	proc_handler_call(obs_source_get_proc_handler(b), "start_record", &cd);
	obs_data_set_bool(res, "recording", calldata_bool(&cd, "recording"));
	obs_data_set_bool(res, "success", true);
	calldata_free(&cd);
	obs_source_release(b);
}

static void req_stop_record(obs_data_t *req, obs_data_t *res, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *b = resolve_bank(req);
	if (!b) {
		fail(res, "bank not found");
		return;
	}
	calldata_t cd;
	calldata_init(&cd);
	proc_handler_call(obs_source_get_proc_handler(b), "stop_record", &cd);
	obs_data_set_bool(res, "recording", calldata_bool(&cd, "recording"));
	obs_data_set_bool(res, "success", true);
	calldata_free(&cd);
	obs_source_release(b);
}

static void req_set_transition(obs_data_t *req, obs_data_t *res, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *b = resolve_bank(req);
	if (!b) {
		fail(res, "bank not found");
		return;
	}
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "kind", obs_data_get_string(req, "kind"));
	proc_handler_call(obs_source_get_proc_handler(b), "set_transition", &cd);
	calldata_free(&cd);
	obs_data_set_bool(res, "success", true);
	obs_source_release(b);
}

static void req_set_duration(obs_data_t *req, obs_data_t *res, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *b = resolve_bank(req);
	if (!b) {
		fail(res, "bank not found");
		return;
	}
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_int(&cd, "ms", obs_data_get_int(req, "ms"));
	proc_handler_call(obs_source_get_proc_handler(b), "set_duration", &cd);
	calldata_free(&cd);
	obs_data_set_bool(res, "success", true);
	obs_source_release(b);
}

static bool enum_banks_cb(void *param, obs_source_t *src)
{
	if (strcmp(obs_source_get_id(src), "multi_me_bank") == 0) {
		obs_data_array_t *arr = param;
		const char *uuid = obs_source_get_uuid(src);
		if (!uuid)
			uuid = "";
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "name", obs_source_get_name(src));
		obs_data_set_string(item, "uuid", uuid);

		/* Include ready-to-use hotkey IDs so they can be copied straight into
		 * Companion's "Trigger Hotkey by ID" (same scheme as me-bank-source.c). */
		char buf[200];
		snprintf(buf, sizeof(buf), ME_HOTKEY_CUT_FMT, uuid);
		obs_data_set_string(item, "hotkey_cut", buf);
		snprintf(buf, sizeof(buf), ME_HOTKEY_AUTO_FMT, uuid);
		obs_data_set_string(item, "hotkey_auto", buf);

		obs_data_array_t *pvw = obs_data_array_create();
		for (int i = 0; i < ME_PVW_SLOTS; i++) {
			snprintf(buf, sizeof(buf), ME_HOTKEY_PVW_FMT, uuid, i + 1);
			obs_data_t *h = obs_data_create();
			obs_data_set_int(h, "input", i + 1);
			obs_data_set_string(h, "hotkey", buf);
			obs_data_array_push_back(pvw, h);
			obs_data_release(h);
		}
		obs_data_set_array(item, "hotkeys_preview", pvw);
		obs_data_array_release(pvw);

		obs_data_array_push_back(arr, item);
		obs_data_release(item);
	}
	return true;
}

static void req_get_banks(obs_data_t *req, obs_data_t *res, void *priv)
{
	UNUSED_PARAMETER(req);
	UNUSED_PARAMETER(priv);
	obs_data_array_t *arr = obs_data_array_create();
	obs_enum_sources(enum_banks_cb, arr);
	obs_data_set_array(res, "banks", arr);
	obs_data_array_release(arr);
	obs_data_set_bool(res, "success", true);
}

static void req_get_state(obs_data_t *req, obs_data_t *res, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *b = resolve_bank(req);
	if (!b) {
		fail(res, "bank not found");
		return;
	}
	calldata_t cd;
	calldata_init(&cd);
	proc_handler_call(obs_source_get_proc_handler(b), "get_state", &cd);
	obs_data_set_string(res, "program", calldata_string(&cd, "program"));
	obs_data_set_string(res, "preview", calldata_string(&cd, "preview"));
	obs_data_set_bool(res, "in_transition", calldata_bool(&cd, "in_transition"));
	obs_data_set_string(res, "kind", calldata_string(&cd, "kind"));
	obs_data_set_int(res, "duration", calldata_int(&cd, "duration"));
	obs_data_set_bool(res, "recording", calldata_bool(&cd, "recording"));
	obs_data_set_string(res, "rec_file", calldata_string(&cd, "rec_file"));
	calldata_free(&cd);
	obs_data_set_bool(res, "success", true);
	obs_source_release(b);
}

/* ---- Vendor events (state tally) ---------------------------------------- */
/*
 * On every bank state change we emit a per-bank event "state_#<N>" (N = the
 * bank's 1-based position, same order as the "#N" addressing) carrying the
 * current preview/program bus input (1-based, 0 = none) and recording flag.
 * Companion's "VendorEvent" feedback latches on the last matching event, so a
 * page can light the live PVW/PGM input of the selected bank.
 */

static void *g_vendor = NULL;
static proc_handler_t *g_api_ph = NULL;

struct pos_ctx {
	const char *uuid;
	int cur;
	int pos;
};
static bool pos_cb(void *p, obs_source_t *src)
{
	if (strcmp(obs_source_get_id(src), "multi_me_bank") != 0)
		return true;
	struct pos_ctx *c = p;
	c->cur++;
	if (c->uuid && strcmp(obs_source_get_uuid(src), c->uuid) == 0) {
		c->pos = c->cur;
		return false;
	}
	return true;
}

struct sidx_ctx {
	const char *name;
	int cur;
	int found;
};
static bool sidx_cb(void *p, const char *name, obs_source_t *scene)
{
	UNUSED_PARAMETER(scene);
	struct sidx_ctx *c = p;
	c->cur++;
	if (c->name && strcmp(name, c->name) == 0) {
		c->found = c->cur;
		return false;
	}
	return true;
}

/* Runs on the UI thread (obs_frontend_* / scene enumeration are UI-thread). */
static void emit_task(void *param)
{
	obs_source_t *bank = param;
	if (g_vendor && g_api_ph) {
		struct pos_ctx pc = {obs_source_get_uuid(bank), 0, 0};
		obs_enum_sources(pos_cb, &pc);
		if (pc.pos >= 1) {
			calldata_t st;
			calldata_init(&st);
			proc_handler_call(obs_source_get_proc_handler(bank), "get_state", &st);
			const char *program = calldata_string(&st, "program");
			const char *preview = calldata_string(&st, "preview");
			bool rec = calldata_bool(&st, "recording");

			struct sidx_ctx pv = {preview, 0, 0};
			struct sidx_ctx pg = {program, 0, 0};
			if (preview && *preview)
				me_scenes_enum(pc.uuid, sidx_cb, &pv);
			if (program && *program)
				me_scenes_enum(pc.uuid, sidx_cb, &pg);
			calldata_free(&st);

			obs_data_t *data = obs_data_create();
			char b[16];
			snprintf(b, sizeof(b), "#%d", pc.pos);
			obs_data_set_string(data, "idx", b);
			snprintf(b, sizeof(b), "%d", pv.found);
			obs_data_set_string(data, "pvw_in", b);
			snprintf(b, sizeof(b), "%d", pg.found);
			obs_data_set_string(data, "pgm_in", b);
			obs_data_set_string(data, "rec", rec ? "1" : "0");

			/* Fixed event type: the OBS Companion module's VendorEvent feedback
			 * does NOT expand variables in its fields, so it can't filter by the
			 * selected bank. A fixed "state" type lets literal feedbacks match;
			 * the tally then follows the most recently changed bank. */
			calldata_t cd;
			calldata_init(&cd);
			calldata_set_ptr(&cd, "vendor", g_vendor);
			calldata_set_string(&cd, "type", "state");
			calldata_set_ptr(&cd, "data", data);
			proc_handler_call(g_api_ph, "vendor_event_emit", &cd);
			calldata_free(&cd);
			obs_data_release(data);
		}
	}
	obs_source_release(bank);
}

void me_websocket_emit_bank_state(obs_source_t *bank)
{
	if (!g_vendor || !bank)
		return;
	obs_source_t *ref = obs_source_get_ref(bank);
	if (!ref)
		return;
	obs_queue_task(OBS_TASK_UI, emit_task, ref, false);
}

/* ---- Registration -------------------------------------------------------- */

void me_websocket_register(void)
{
	proc_handler_t *ph = ws_get_ph();
	if (!ph) {
		obs_log(LOG_INFO, "obs-websocket not present; vendor 'multi-me' not registered");
		return;
	}
	void *vendor = ws_register_vendor(ph, "multi-me");
	if (!vendor) {
		obs_log(LOG_WARNING, "failed to register obs-websocket vendor 'multi-me'");
		return;
	}
	g_api_ph = ph;
	g_vendor = vendor;
	ws_register_request(ph, vendor, "set_preview", req_set_preview);
	ws_register_request(ph, vendor, "set_preview_index", req_set_preview_index);
	ws_register_request(ph, vendor, "set_program", req_set_program);
	ws_register_request(ph, vendor, "set_program_index", req_set_program_index);
	ws_register_request(ph, vendor, "cut", req_cut);
	ws_register_request(ph, vendor, "auto", req_auto);
	ws_register_request(ph, vendor, "set_transition", req_set_transition);
	ws_register_request(ph, vendor, "set_duration", req_set_duration);
	ws_register_request(ph, vendor, "get_banks", req_get_banks);
	ws_register_request(ph, vendor, "get_state", req_get_state);
	ws_register_request(ph, vendor, "start_record", req_start_record);
	ws_register_request(ph, vendor, "stop_record", req_stop_record);
	obs_log(LOG_INFO, "obs-websocket vendor 'multi-me' registered (12 requests)");
}
