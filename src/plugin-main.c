/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>

#include "me-bank.h"
#include "me-dock.h"
#include "me-multiview.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	me_bank_register_source();
	obs_log(LOG_INFO, "Multi-M/E plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

/* Dock erst nach dem Laden registrieren: Frontend + Qt-Hauptthread sind bereit. */
void obs_module_post_load(void)
{
	me_dock_register();
	me_multiview_register();
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
