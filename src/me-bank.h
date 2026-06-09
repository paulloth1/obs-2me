/*
2ME — Second Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Registriert den Quelltyp "2me_bank_output" (eine M/E-Mischebene als
 * Re-entry-Quelle). In obs_module_load() aufrufen. */
void me_bank_register_source(void);

#ifdef __cplusplus
}
#endif
