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

/* Effect Ending */
void endEffect(activeEffect_t* effect) {
	if (!effect) {
		return;
	}
	switch (effect->effectType) {
		case ET_FIRE:
			// Handle fire effect cleanup
			break;
		default:
			break;
	}
	effect->effectType = ET_DEFAULT;
	effect->parameter = 0;
	effect->endTime = 0;
	effect->inflictorIndex = -1;
	effect->lastApplied = 0;
}

void endEffectType(gentity_t* ent, effectType_t type) {
	if (!ent) {
		return;
	}
	for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
		activeEffect_t* effect = &ent->s.activeEffects[i];
		if (effect->effectType == type) {
			endEffect(effect);
			return;
		}
	}
}

qboolean isEffectEnded(activeEffect_t* effect) {
	if (!effect || effect->effectType == ET_DEFAULT) {
		return qtrue;
	}

	//Reinitialize the effect if it has ended
	if (level.time >= effect->endTime) {
		endEffect(effect);
		return qtrue;
	}

	return qfalse;
}
/* Ending conditions*/
const animNumber_t rollAnims[] = {
	BOTH_ROLL_F,			//# Roll forward
	BOTH_ROLL_B,			//# Roll backward
	BOTH_ROLL_L,			//# Roll left
	BOTH_ROLL_R,			//# Roll right
	BOTH_ROLL_STAB
};
/* G_FireShouldEnd

* @brief Determines if the fire effect should end for the given entity.
 * @param ent The entity to check.
 * @param effect The activeEffect_t structure containing the fire effect details.
 * @return qtrue if the fire effect should end, qfalse otherwise.
 */
qboolean G_FireShouldEnd(gentity_t* ent, activeEffect_t* effect) {
	//Should never happen
	if (!ent || !effect || effect->effectType != ET_FIRE) {
		return qtrue;
	}

	//If in water
	if (gi.totalMapContents() & (CONTENTS_WATER | CONTENTS_SLIME))
	{
		const int contents = gi.pointcontents(ent->currentOrigin, ent->s.clientNum);
		if (contents & (CONTENTS_WATER | CONTENTS_SLIME)) {
			return qtrue;
		}
	}
	//If doing roll
	if (ent->client) {
		int legsAnim = ent->client->ps.legsAnim;
		int torsoAnim = ent->client->ps.torsoAnim;
		for (int i = 0; i < sizeof(rollAnims) / sizeof(rollAnims[0]); i++) {
			if (legsAnim == rollAnims[i] || torsoAnim == rollAnims[i]) {
				return qtrue;
			}
		}
	}
	return qfalse;
}

/* G_EffectShouldEnd
 * @brief Determines if the given effect should end for the specified entity.
 * @param ent The entity to check.
 * @param effect The activeEffect_t structure containing the effect details.
 * @return qtrue if the effect should end, qfalse otherwise.
 */
qboolean G_EffectShouldEnd(gentity_t* ent, activeEffect_t* effect) {
	if (!ent || !effect) {
		return qtrue;
	}
	switch (effect->effectType) {
	case ET_FIRE:
		return G_FireShouldEnd(ent, effect);
	default:
		return qtrue;
	}
}

/* Effect ongoing */
/* G_ApplyFire
 * @brief Applies the fire effect to the given entity.
 * @param ent The entity to apply the fire effect to.
 * @param effect The activeEffect_t structure containing the fire effect details.
 * Will calculate the damage over time (based on the effect's parameter) and apply it to the entity.
 * In this case, the parameter of the effect must be damage per second.
 */
void G_applyFire(gentity_t* ent, activeEffect_t* effect) {
	float baseDamage = effect->parameter;
	float damageRatio = (float)(level.time - effect->lastApplied) / 1000.0f;
	float damage = baseDamage * damageRatio;

	gentity_t* attacker = &g_entities[effect->inflictorIndex];

	//Aim for somewhere in the torso
	vec3_t hitloc;
	VectorCopy(ent->currentOrigin, hitloc);

	if (effect->effectIndex > -1)
	{
		G_PlayEffect(effect->effectIndex, hitloc);
	}
	else if (ent->health > 0)
	{
		hitloc[2] += 25.0f;
		G_PlayEffect(G_EffectIndex("env/fire.efx"), ent->currentOrigin);
		G_PlayEffect(G_EffectIndex("env/small_fire.efx"), hitloc);
	}
	else
	{
		hitloc[2] -= 10.0f;
		G_PlayEffect(G_EffectIndex("env/small_fire.efx"), hitloc);
	}

	if (damage > 1) {
		G_Damage(ent, attacker, attacker, NULL, NULL, damage, DAMAGE_NO_ARMOR | DAMAGE_NO_KNOCKBACK | DAMAGE_NO_HIT_LOC | DAMAGE_IGNORE_TEAM, MOD_LAVA, HL_NONE);
		effect->lastApplied = level.time;
	}
}

void G_applyEffect(gentity_t* ent, activeEffect_t* effect) {
	if (!ent || !effect) {
		return;
	}
	//Check if Effect has ended
	if (isEffectEnded(effect))
	{
		return;
	}
	//Check if Effect Should End
	if (G_EffectShouldEnd(ent, effect))
	{
		endEffect(effect);
		return;
	}
	const int contents = gi.pointcontents(ent->currentOrigin, ent->s.clientNum);
	switch (effect->effectType) {
	case ET_FIRE:
		G_applyFire(ent, effect);
		break;
	default:
		endEffect(effect);
	}
}

void G_applyEffects(gentity_t* ent) {
	for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
		activeEffect_t* effect = &ent->s.activeEffects[i];
		G_applyEffect(ent, effect);
	}
}

/* G_startEffect
 * @brief Starts the given effect on the specified entity.
 * @param ent The entity to start the effect on.
 * @param effect The activeEffect_t structure containing the effect details.
 * If the effect is already active on the entity, it will update the existing effect's parameters and end time.
 * If the effect is not active, it will find an empty slot and add the new effect to the entity's active effects list.
 */
void G_startEffect(gentity_t* ent, activeEffect_t* effect) {
	if (!ent || !effect || effect->effectType == ET_DEFAULT) {
		return;
	}

	for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
		if (ent->s.activeEffects[i].effectType == effect->effectType) {
			G_applyEffect(ent, &ent->s.activeEffects[i]); //Apply the existing effect before updating
			int lastApplied = ent->s.activeEffects[i].lastApplied;
			ent->s.activeEffects[i] = *effect;
			ent->s.activeEffects[i].lastApplied = lastApplied; // Preserve the last applied time]
			return;
		}
	}


	switch (effect->effectType) {
		case ET_FIRE:
			// Handle fire effect initialization
			break;
		default:
			return; //Invalid Effect
	}
	for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
		if (isEffectEnded(&ent->s.activeEffects[i])) {
			ent->s.activeEffects[i] = *effect;
			return;
		}
	}
}