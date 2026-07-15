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

// g_activeEffects.cpp

#include "g_effects.h"
#include "g_shared.h"

void reinitializeEffect(activeEffect_t* effect) {
	if (!effect) {
		return;
	}
	effect->effectType = ET_DEFAULT;
	effect->parameter = 0;
	effect->endTime = 0;
	effect->inflictorIndex = -1;
	effect->lastApplied = 0;
}

qboolean isEffectEnded(activeEffect_t* effect) {
	if (!effect || effect->effectType == ET_DEFAULT) {
		return qtrue;
	}

	//Reinitialize the effect if it has ended
	if (level.time >= effect->endTime) {
		reinitializeEffect(effect);
		return qtrue;
	}

	return qfalse;
}

/* G_ApplyFire
 * @brief Applies the fire effect to the given entity.
 * @param ent The entity to apply the fire effect to.
 * @param effect The activeEffect_t structure containing the fire effect details.
 * Will calculate the damage over time (based on the effect's parameter) and apply it to the entity.
 * In this case, the parameter of the effect must be damage per second.
 */
void G_applyFire(gentity_t* ent, activeEffect_t* effect) {
	int baseDamage = effect->parameter;
	float damageRatio = (float)(level.time - effect->lastApplied) / 1000.0f;
	float damage = baseDamage * damageRatio;

	gentity_t* attacker = &g_entities[effect->inflictorIndex];

	//Aim for somewhere in the torso
	vec3_t hitloc;
	VectorCopy(ent->currentOrigin, hitloc);
	hitloc[2] += 25.0f;

	if (damage > 0) {
		G_Damage(ent, attacker, attacker, NULL, NULL, damage, DAMAGE_NO_ARMOR | DAMAGE_NO_KNOCKBACK | DAMAGE_NO_HIT_LOC | DAMAGE_IGNORE_TEAM, MOD_LAVA, HL_NONE);


		if (ent->health > 0)
		{
			G_PlayEffect(G_EffectIndex("env/fire.efx"), ent->currentOrigin);
			G_PlayEffect(G_EffectIndex("env/small_fire.efx"), hitloc);
		}
		else
		{
			G_PlayEffect(G_EffectIndex("env/small_fire.efx"), ent->currentOrigin);
		}
		//TODO, add other modifier to the entity... Like a fear effect cause "IT'S BURRRRNNNN"
	}
}

void G_applyEffects(gentity_t* ent) {
	for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
		activeEffect_t* effect = &ent->s.activeEffects[i];
		if (isEffectEnded(effect))
		{
			continue;
		}
		switch (effect->effectType) {
		case ET_FIRE:
			// Apply fire effect logic here
			break;
		default:
			reinitializeEffect(effect);
		}

	}
}