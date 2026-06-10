/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

/*
 * me-websocket.c — obs-websocket-Vendor "multi-me".
 *
 * Stellt Custom-Requests bereit, die WebSocket-Clients (z. B. Bitfocus
 * Companion via CallVendorRequest) auslösen können, um die M/E-Bänke zu
 * steuern. Die Requests hängen sich an die Proc-Handler der Bank-Quellen.
 *
 * Requests (vendorName = "multi-me"):
 *   set_preview  { bank, scene }     -> Szene in die Vorschau der Bank
 *   set_program  { bank, scene }     -> Szene sofort auf Program (harter Schnitt)
 *   cut          { bank }            -> Cut (PVW -> PGM)
 *   auto         { bank }            -> Auto-Take (Übergang)
 *   get_banks    { }                 -> Liste aller Bänke [{name, uuid}]
 *   get_state    { bank }            -> { program, preview, in_transition, kind, duration }
 *
 * "bank" akzeptiert UUID oder Quellname.
 *
 * Die Anbindung an obs-websocket erfolgt über dessen global registrierte
 * Procs (stabile API): "obs_websocket_api_get_ph" -> liefert den Proc-Handler,
 * darauf "vendor_register" und "vendor_request_register".
 */

#include <obs-module.h>
#include <plugin-support.h>
#include <string.h>

#include "me-websocket.h"

typedef void (*ws_request_cb)(obs_data_t *request_data, obs_data_t *response_data, void *priv_data);

struct ws_request_callback {
	ws_request_cb callback;
	void *priv_data;
};

/* ---- Minimale obs-websocket-Anbindung über Procs ------------------------ */

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

/* ---- Helfer ------------------------------------------------------------- */

static obs_source_t *resolve_bank(obs_data_t *req)
{
	const char *bank = obs_data_get_string(req, "bank");
	if (!bank || !*bank)
		return NULL;
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

/* ---- Request-Handler ---------------------------------------------------- */

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

static bool enum_banks_cb(void *param, obs_source_t *src)
{
	if (strcmp(obs_source_get_id(src), "multi_me_bank") == 0) {
		obs_data_array_t *arr = param;
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "name", obs_source_get_name(src));
		obs_data_set_string(item, "uuid", obs_source_get_uuid(src));
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
	calldata_free(&cd);
	obs_data_set_bool(res, "success", true);
	obs_source_release(b);
}

/* ---- Registrierung ------------------------------------------------------ */

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
	ws_register_request(ph, vendor, "set_preview", req_set_preview);
	ws_register_request(ph, vendor, "set_program", req_set_program);
	ws_register_request(ph, vendor, "cut", req_cut);
	ws_register_request(ph, vendor, "auto", req_auto);
	ws_register_request(ph, vendor, "get_banks", req_get_banks);
	ws_register_request(ph, vendor, "get_state", req_get_state);
	obs_log(LOG_INFO, "obs-websocket vendor 'multi-me' registered (6 requests)");
}
