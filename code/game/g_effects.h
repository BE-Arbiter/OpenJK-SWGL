/*
===========================================================================
Copyright (C) 2026 - ..., SWGL Team

This file is part of the SWGL source code.

SWGL is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// g_activeEffects.h

#include "g_local.h"

#ifndef G_ACTIVE_EFFECTS_H
#define G_ACTIVE_EFFECTS_H

/*Start an effect on an entity*/
void G_startEffect(gentity_t* ent, activeEffect_t* effect);

/*End an effect type on an entity*/
void G_endEffectType(gentity_t* ent, effectType_t type);

/*Apply active effects to an entity*/
void G_applyEffects(gentity_t* ent);


#endif
