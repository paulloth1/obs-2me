/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <mail@paulloth.de>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

#include <obs-module.h>
#include <obs-frontend-api.h>

#include "me-scenes.h"

struct contains_ctx {
	obs_source_t *target;
	bool found;
};

static void contains_cb(obs_source_t *parent, obs_source_t *child, void *param)
{
	struct contains_ctx *c = param;
	UNUSED_PARAMETER(parent);
	if (child == c->target)
		c->found = true;
}

static bool scene_contains(obs_source_t *scene, obs_source_t *target)
{
	struct contains_ctx c = {target, false};
	obs_source_enum_full_tree(scene, contains_cb, &c);
	return c.found;
}

void me_scenes_enum(const char *exclude_bank_uuid, me_scene_cb cb, void *param)
{
	obs_source_t *bank = (exclude_bank_uuid && *exclude_bank_uuid) ? obs_get_source_by_uuid(exclude_bank_uuid)
								       : NULL;

	struct obs_frontend_source_list scenes = {0};
	obs_frontend_get_scenes(&scenes);

	for (size_t i = 0; i < scenes.sources.num; i++) {
		obs_source_t *scene = scenes.sources.array[i];

		/* Respect the native OBS multiview exclusion */
		obs_data_t *pd = obs_source_get_private_settings(scene);
		obs_data_set_default_bool(pd, "show_in_multiview", true);
		bool show = obs_data_get_bool(pd, "show_in_multiview");
		obs_data_release(pd);
		if (!show)
			continue;

		/* Feedback protection: scenes that contain the bank's own source */
		if (bank && scene_contains(scene, bank))
			continue;

		if (!cb(param, obs_source_get_name(scene), scene))
			break;
	}

	obs_frontend_source_list_free(&scenes);
	if (bank)
		obs_source_release(bank);
}
