/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
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

#include "g_local.h"
#include "b_local.h"
#include "g_functions.h"
#include "wp_saber.h"
#include "w_local.h"
#include "../cgame/cg_local.h"

static qboolean is_player_scoped(gentity_t *ent)
{
	return (qboolean)(cg.zoomMode >= ST_A280 && ent->client->ps.clientNum == 0);
}

// The reason why we set alt_fire here is because
// of how "damage" and/or "velocity" will be evaluated
// in WP_FireWeaponMissile.
static void is_player_alt_firing(gentity_t *ent, weapon_t weapon_num, qboolean *alt_fire)
{
	if (ent->client->ps.clientNum == 0)
	{
		// The damage for the high powered shot gets
		// set in bg_pmove so use main damage i.e. set
		// alt_fire to false.
		if (ent->client->ps.firing_attack & TERTIARY_ATTACK
			&& weaponData[weapon_num].tertiaryFireOpt[FIRING_TYPE] == FT_HIGH_POWERED)
		{
			*alt_fire = qfalse;
		}
		// Set alt_fire to true if ALT_ATTACK is enabled (weapons
		// like the clone carbine use ALT_ATTACK) or if scoped.
		else if (ent->client->ps.firing_attack & ALT_ATTACK || cg.zoomMode >= ST_A280)
		{
			*alt_fire = qtrue;
		}
	}
}

// If you have a high powered shot switch
// the MOD to play the disintegration effect and 
// let there be no knockback so the effect can
// fully play out.
static void check_means_of_death(gentity_t *ent, gentity_t *missile, weapon_t weapon_num)
{
	if (weaponData[weapon_num].tertiaryFireOpt[FIRING_TYPE] == FT_HIGH_POWERED
		&& ent->client->ps.tertiaryMode)
	{
		missile->dflags = DAMAGE_NO_KNOCKBACK;
		missile->methodOfDeath = MOD_HIGH_POWERED_SHOT;
	}
}

//---------------
//	E-5 Carbine
//---------------

//---------------------------------------------------------
void WP_FireBattleDroidMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int velocity = E5_VELOCITY;
	int	damage = altFire ? weaponData[WP_BATTLEDROID].altDamage : weaponData[WP_BATTLEDROID].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= E5_NPC_VEL_CUT;
			}
			else
			{
				velocity *= E5_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_BATTLEDROID;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = E5_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = E5_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = E5_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		missile->methodOfDeath = MOD_BLASTER_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BLASTER;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_BATTLEDROID);
}

//---------------------------------------------------------
void WP_FireBattleDroid(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t	dir, angs;

	vectoangles(forwardVec, angs);
	is_player_alt_firing(ent, WP_BATTLEDROID, &alt_fire);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if (alt_fire)
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * E5_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * E5_ALT_SPREAD;
		}
		else
		{
			// Troopers use their aim values as well as the gun's inherent inaccuracy
			// so check for all classes of stormtroopers and anyone else that has aim error
			if (ent->client && ent->NPC &&
				(ent->client->NPC_class == CLASS_STORMTROOPER ||
				ent->client->NPC_class == CLASS_SWAMPTROOPER))
			{
				angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (E5_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));
				angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (E5_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));
			}
			else
			{
				// add some slop to the main-fire direction
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * E5_MAIN_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * E5_MAIN_SPREAD;
			}
		}
	}

	AngleVectors(angs, dir, NULL, NULL);

	// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
	WP_FireBattleDroidMissile(ent, muzzle, dir, alt_fire);
}

//---------------
//	F-11D
//---------------

//---------------------------------------------------------
void WP_FireFirstOrderMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int velocity = F_11D_VELOCITY;
	int	damage = altFire ? weaponData[WP_THEFIRSTORDER].altDamage : weaponData[WP_THEFIRSTORDER].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= F_11D_NPC_VEL_CUT;
			}
			else
			{
				velocity *= F_11D_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_THEFIRSTORDER;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = F_11D_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = F_11D_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = F_11D_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		missile->methodOfDeath = MOD_BLASTER_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BLASTER;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_THEFIRSTORDER);
}

//---------------------------------------------------------
void WP_FireFirstOrder(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t	dir, angs;
	qboolean scoped = is_player_scoped(ent);

	vectoangles(forwardVec, angs);
	is_player_alt_firing(ent, WP_THEFIRSTORDER, &alt_fire);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (scoped)
	{
		// angs[PITCH] += Q_flrand(-0.5f, 0.5f) * 0.01;
		// angs[YAW] += Q_flrand(-0.25f, 0.25f) * 0.01;

		AngleVectors(ent->client->renderInfo.eyeAngles, forwardVec, NULL, NULL);
		vectoangles(forwardVec, angs);

		angs[PITCH] += Q_flrand(-1.0f, 1.0f) * F_11D_ALT_SPREAD;
		angs[YAW] += Q_flrand(-1.0f, 1.0f) * F_11D_ALT_SPREAD;
	} 
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if (alt_fire)
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * F_11D_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * F_11D_ALT_SPREAD;
		}
		// Troopers use their aim values as well as the gun's inherent inaccuracy
		// so check for all classes of stormtroopers and anyone else that has aim error
		if (ent->client && ent->NPC &&
			(ent->client->NPC_class == CLASS_STORMTROOPER ||
			ent->client->NPC_class == CLASS_SWAMPTROOPER))
		{
			angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (F_11D_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
			angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (F_11D_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
		}
		else
		{
			// add some slop to the main-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * F_11D_MAIN_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * F_11D_MAIN_SPREAD;
		}
	}

	if (scoped)
	{
		AngleVectors(angs, forwardVec, NULL, NULL);
		WP_FireFirstOrderMissile(ent, ent->client->renderInfo.eyePoint, forwardVec, alt_fire);
	}
	else
	{
		AngleVectors(angs, dir, NULL, NULL);

		// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
		WP_FireFirstOrderMissile(ent, muzzle, dir, alt_fire);
	}
}

//---------------
//	DC-15 Carbine
//---------------

//---------------------------------------------------------
void WP_FireCloneCarbineMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int velocity = CLONECARBINE_VELOCITY;
	int	damage = altFire ? weaponData[WP_CLONECARBINE].altDamage : weaponData[WP_CLONECARBINE].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= CLONECARBINE_NPC_VEL_CUT;
			}
			else
			{
				velocity *= CLONECARBINE_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "clone_proj";
	missile->s.weapon = WP_CLONECARBINE;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = CLONECARBINE_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = CLONECARBINE_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = CLONECARBINE_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		missile->methodOfDeath = MOD_BLASTER_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BLASTER;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_CLONECARBINE);
}

//---------------------------------------------------------
void WP_FireCloneCarbine(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t	dir, angs;

	vectoangles(forwardVec, angs);
	is_player_alt_firing(ent, WP_CLONECARBINE, &alt_fire);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if (alt_fire)
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CLONECARBINE_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * CLONECARBINE_ALT_SPREAD;
		}
		else
		{
			// Troopers use their aim values as well as the gun's inherent inaccuracy
			// so check for all classes of stormtroopers and anyone else that has aim error
			if (ent->client && ent->NPC &&
				(ent->client->NPC_class == CLASS_STORMTROOPER ||
				ent->client->NPC_class == CLASS_SWAMPTROOPER))
			{
				angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (CLONECARBINE_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));
				angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (CLONECARBINE_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));
			}
			else
			{
				// add some slop to the main-fire direction
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CLONECARBINE_MAIN_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * CLONECARBINE_MAIN_SPREAD;
			}
		}
	}

	AngleVectors(angs, dir, NULL, NULL);

	// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
	WP_FireCloneCarbineMissile(ent, muzzle, dir, alt_fire);
}

//---------------
//	DH-17
//---------------

//---------------------------------------------------------
void WP_FireRebelBlasterMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int velocity = REBELBLASTER_VELOCITY;
	int	damage = altFire ? weaponData[WP_REBELBLASTER].altDamage : weaponData[WP_REBELBLASTER].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= REBELBLASTER_NPC_VEL_CUT;
			}
			else
			{
				velocity *= REBELBLASTER_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_REBELBLASTER;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = REBELBLASTER_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = REBELBLASTER_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = REBELBLASTER_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		missile->methodOfDeath = MOD_REBELBLASTER_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_REBELBLASTER;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_REBELBLASTER);
}

//---------------------------------------------------------
void WP_FireRebelBlaster(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t	dir, angs;
	qboolean scoped = is_player_scoped(ent);

	vectoangles(forwardVec, angs);
	is_player_alt_firing(ent, WP_REBELBLASTER, &alt_fire);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (scoped)
	{
		AngleVectors(ent->client->renderInfo.eyeAngles, forwardVec, NULL, NULL);
		vectoangles(forwardVec, angs);

		angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REBELBLASTER_ALT_SPREAD;
		angs[YAW] += Q_flrand(-1.0f, 1.0f) * REBELBLASTER_ALT_SPREAD;
	}
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if (alt_fire)
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REBELBLASTER_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * REBELBLASTER_ALT_SPREAD;
		}
		// Troopers use their aim values as well as the gun's inherent inaccuracy
		// so check for all classes of stormtroopers and anyone else that has aim error
		if (ent->client && ent->NPC &&
			(ent->client->NPC_class == CLASS_STORMTROOPER ||
			ent->client->NPC_class == CLASS_SWAMPTROOPER))
		{
			angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (REBELBLASTER_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
			angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (REBELBLASTER_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
		}
		else
		{
			// add some slop to the main-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REBELBLASTER_MAIN_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * REBELBLASTER_MAIN_SPREAD;
		}
	}

	if (scoped)
	{
		AngleVectors(angs, forwardVec, NULL, NULL);
		WP_FireRebelBlasterMissile(ent, ent->client->renderInfo.eyePoint, forwardVec, alt_fire);
	}
	else
	{
		AngleVectors(angs, dir, NULL, NULL);

		// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
		WP_FireRebelBlasterMissile(ent, muzzle, dir, alt_fire);
	}
}

//---------------
//	DC-15 Rifle
//---------------

//---------------------------------------------------------
void WP_FireCloneRifleMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int velocity = CLONERIFLE_VELOCITY;
	int	damage = altFire ? weaponData[WP_CLONERIFLE].altDamage : weaponData[WP_CLONERIFLE].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= CLONERIFLE_NPC_VEL_CUT;
			}
			else
			{
				velocity *= CLONERIFLE_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "clone_proj";
	missile->s.weapon = WP_CLONERIFLE;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = CLONERIFLE_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = CLONERIFLE_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = CLONERIFLE_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		missile->methodOfDeath = MOD_CLONERIFLE_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_CLONERIFLE;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_CLONERIFLE);
}

//---------------------------------------------------------
void WP_FireCloneRifle(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t	dir, angs;

	vectoangles(forwardVec, angs);
	is_player_alt_firing(ent, WP_CLONERIFLE, &alt_fire);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if (alt_fire)
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CLONERIFLE_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * CLONERIFLE_ALT_SPREAD;
		}
		else
		{
			// Troopers use their aim values as well as the gun's inherent inaccuracy
			// so check for all classes of stormtroopers and anyone else that has aim error
			if (ent->client && ent->NPC &&
				(ent->client->NPC_class == CLASS_STORMTROOPER ||
				ent->client->NPC_class == CLASS_SWAMPTROOPER))
			{
				angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (CLONERIFLE_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
				angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (CLONERIFLE_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
			}
			else
			{
				// add some slop to the main-fire direction
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CLONERIFLE_MAIN_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * CLONERIFLE_MAIN_SPREAD;
			}
		}
	}

	AngleVectors(angs, dir, NULL, NULL);

	// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
	WP_FireCloneRifleMissile(ent, muzzle, dir, alt_fire);
}

//---------------
//	DC-17
//---------------

static void WP_FireCloneCommandoBeam(gentity_t *ent)
{
	trace_t	tr;
	gentity_t *trace_ent = NULL;
	gentity_t *tent = NULL;

	vec3_t start, end, forward_acc;
	vec3_t dir, spot;

	qboolean render_impact = qtrue;
	qboolean hit_dodged = qfalse;

	int shot_dist = 0;
	int shot_range = 8192;

	// Takes the angle where your eyes are looking at and copy it.
	VectorCopy(ent->client->renderInfo.eyeAngles, forward_acc);

	// Changes the accuracy.
	forward_acc[PITCH] += Q_flrand(-0.15f, 0.15f);
	forward_acc[YAW] += Q_flrand(-0.15f, 0.15f);

	// Copy where your eyes are in space into a varible called start.
	VectorCopy(ent->client->renderInfo.eyePoint, start);
	// Takes the angle where your eyes are looking and converts it into a forward vector.
	AngleVectors(forward_acc, forwardVec, NULL, NULL );

	// Takes the starting point plus the shot_range times the forwardVec.
	// Gets the end vector.
	VectorMA(start, shot_range, forwardVec, end);

	// Shooting a ray and checking where it hits.
	gi.trace(&tr, start, NULL, NULL, end, ent->s.number, MASK_SHOT, G2_COLLIDE, 10);

	// We didn't hit anything.
	if ( tr.surfaceFlags & SURF_NOIMPACT )
	{
		render_impact = qfalse;
	}

	// The ent that you fired on.
	trace_ent = &g_entities[tr.entityNum];

	if (trace_ent && (trace_ent->s.weapon == WP_SABER
		|| (trace_ent->client && (trace_ent->client->NPC_class == CLASS_BOBAFETT
		|| trace_ent->client->NPC_class == CLASS_MANDALORIAN
		|| trace_ent->client->NPC_class == CLASS_JANGO
		|| trace_ent->client->NPC_class == CLASS_REBORN))))
		{
			// Acts like we didn't even hit him.
			hit_dodged = Jedi_DodgeEvasion(trace_ent, ent, &tr, HL_NONE);
		}

	// They didn't dodge and we hit something.
	if (!hit_dodged && render_impact)
	{
		// If the beam hits a NPC.
		if ((tr.entityNum < ENTITYNUM_WORLD && trace_ent->takedamage)
			|| !Q_stricmp(trace_ent->classname, "misc_model_breakable")
			|| trace_ent->s.eType == ET_MOVER)
		{
			G_PlayEffect(G_EffectIndex("clone/flesh_impact"), tr.endpos, tr.plane.normal);

			if (trace_ent->client && LogAccuracyHit(trace_ent, ent))
			{
				ent->client->ps.persistant[PERS_ACCURACY_HITS]++;
			}

			G_Damage(trace_ent, ent, ent, forwardVec, tr.endpos, CLONECOMMANDO_SCOPE_DAMAGE,
					DAMAGE_DEATH_KNOCKBACK, MOD_CLONECOMMANDO, G_GetHitLocFromTrace(&tr, MOD_CLONECOMMANDO));
		}
		// If the beam hits a wall.
		else
		{
			tent = G_TempEntity(tr.endpos, EV_CLONECOMMANDO_SNIPER_MISS);
			tent->svFlags |= SVF_BROADCAST;
			VectorCopy(tr.plane.normal, tent->pos1);
		}
	}

	// Play the beam effect.
	tent = G_TempEntity(tr.endpos, EV_CLONECOMMANDO_SNIPER_SHOT);
	tent->svFlags |= SVF_BROADCAST;
	VectorCopy(muzzle, tent->s.origin2);

	// Getting the difference between you and the ent you hit.
	// Getting the magnitude between you and the ent.
	shot_dist = Distance(tr.endpos, muzzle);

	// As the beam travles, alert the emenies to your location.
	for (float dist = 0; dist < shot_dist; dist += 64)
	{
		VectorMA(muzzle, dist, dir, spot);
		AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);
	}

	// If they got spawned right in front you.
	VectorMA(start, shot_dist-4, forwardVec, spot);
	AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);
}

//---------------------------------------------------------
void WP_FireCloneCommandoMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int	damage = 0;
	int velocity = altFire ? CLONECOMMANDO_ALT_VELOCITY : CLONECOMMANDO_VELOCITY;
	float kick = (ent->client->usercmd.upmove < 0.0f) ? -100.0f : -300.0f;

	if (cg.zoomMode >= ST_A280)
	{
		damage = CLONECOMMANDO_SCOPE_DAMAGE;
	}
	else if (altFire)
	{
		damage = weaponData[WP_CLONECOMMANDO].altDamage;
	}
	else
	{
		damage = weaponData[WP_CLONECOMMANDO].damage;
	}

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= CLONECOMMANDO_NPC_VEL_CUT;
			}
			else
			{
				velocity *= CLONECOMMANDO_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "clone_proj";
	missile->s.weapon = WP_CLONECOMMANDO;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = CLONECOMMANDO_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = CLONECOMMANDO_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = CLONECOMMANDO_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		VectorSet(missile->maxs, CLONECOMMANDO_ALT_SIZE, CLONECOMMANDO_ALT_SIZE, CLONECOMMANDO_ALT_SIZE);
		VectorScale(missile->maxs, -1, missile->mins);

		missile->s.pos.trType = TR_GRAVITY;
		missile->s.pos.trDelta[2] += 200.0f;
		missile->methodOfDeath = MOD_CLONECOMMANDO_ALT;
		missile->splashDamage = weaponData[WP_CLONECOMMANDO].altSplashDamage;
		missile->splashRadius = weaponData[WP_CLONECOMMANDO].altSplashRadius;

		// Shove us backwards.
		VectorMA(ent->client->ps.velocity, kick, forwardVec, ent->client->ps.velocity);
	}
	else
	{
		missile->methodOfDeath = MOD_CLONECOMMANDO;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//---------------------------------------------------------
void WP_FireCloneCommando(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	if (cg.zoomMode >= ST_A280 || ent->client->ps.clientNum != 0)
	{
		alt_fire = qfalse;
	}
	else if (ent->client->ps.firing_attack & TERTIARY_ATTACK)
	{
		alt_fire = qtrue;
	}

	vec3_t	dir, angs;

	vectoangles(forwardVec, angs);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (is_player_scoped(ent))
	{
		WP_FireCloneCommandoBeam(ent);
	}
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{
		// Troopers use their aim values as well as the gun's inherent inaccuracy
		// so check for all classes of stormtroopers and anyone else that has aim error
		if (ent->client && ent->NPC &&
			(ent->client->NPC_class == CLASS_STORMTROOPER ||
			ent->client->NPC_class == CLASS_SWAMPTROOPER))
		{
			angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (CLONECOMMANDO_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
			angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (CLONECOMMANDO_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
		}
		else
		{
			// add some slop to the main-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CLONECOMMANDO_MAIN_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * CLONECOMMANDO_MAIN_SPREAD;
		}
	}

	if (!(cg.zoomMode >= ST_A280) || ent->client->ps.clientNum != 0)
	{
		AngleVectors(angs, dir, NULL, NULL);

		// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
		WP_FireCloneCommandoMissile(ent, muzzle, dir, alt_fire);
	}
}

//---------------
//	A280
//---------------

//---------------------------------------------------------
void WP_FireRebelRifleMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int velocity = REBELRIFLE_VELOCITY;
	int	damage = altFire ? weaponData[WP_REBELRIFLE].altDamage : weaponData[WP_REBELRIFLE].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= REBELRIFLE_NPC_VEL_CUT;
			}
			else
			{
				velocity *= REBELRIFLE_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_REBELRIFLE;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = REBELRIFLE_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = REBELRIFLE_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = REBELRIFLE_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		missile->methodOfDeath = MOD_REBELRIFLE_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_REBELRIFLE;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_REBELRIFLE);
}

//---------------------------------------------------------
void WP_FireRebelRifle(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t	dir, angs;
	qboolean scoped = is_player_scoped(ent);

	vectoangles(forwardVec, angs);
	is_player_alt_firing(ent, WP_REBELRIFLE, &alt_fire);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (scoped)
	{
		AngleVectors(ent->client->renderInfo.eyeAngles, forwardVec, NULL, NULL);
		vectoangles(forwardVec, angs);

		if (ent->client->ps.firing_attack & TERTIARY_ATTACK)
		{
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_TERTIARY_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_TERTIARY_SPREAD;
		}
		else
		{
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_ALT_SPREAD;
		}
	}
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if (alt_fire)
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_ALT_SPREAD;
		}
		else
		{
			// Troopers use their aim values as well as the gun's inherent inaccuracy
			// so check for all classes of stormtroopers and anyone else that has aim error
			if (ent->client && ent->NPC &&
				(ent->client->NPC_class == CLASS_STORMTROOPER ||
				ent->client->NPC_class == CLASS_SWAMPTROOPER))
			{
				angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (REBELRIFLE_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
				angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (REBELRIFLE_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
			}
			else
			{
				// add some slop to the main-fire direction
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_MAIN_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_MAIN_SPREAD;
			}
		}
	}

	if (scoped)
	{
		AngleVectors(angs, forwardVec, NULL, NULL);
		WP_FireRebelRifleMissile(ent, ent->client->renderInfo.eyePoint, forwardVec, alt_fire);
	}
	else
	{
		AngleVectors(angs, dir, NULL, NULL);

		// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
		WP_FireRebelRifleMissile(ent, muzzle, dir, alt_fire);
	}
}

//---------------
//	LPA NN-14
//---------------

//---------------------------------------------------------
void WP_FireReyPistol( gentity_t *ent, qboolean alt_fire )
//---------------------------------------------------------
{
	vec3_t	start;
	int	damage = !alt_fire ? weaponData[WP_REY].damage : weaponData[WP_REY].altDamage;

	VectorCopy( muzzle, start );
	WP_TraceSetStart( ent, start, vec3_origin, vec3_origin );//make sure our start point isn't on the other side of a wall

	if ( !(ent->client->ps.forcePowersActive&(1<<FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2 )
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if ( ent->NPC && ent->NPC->currentAim < 5 )
		{
			vec3_t	angs;

			vectoangles( forwardVec, angs );

			if ( ent->client->NPC_class == CLASS_IMPWORKER )
			{//*sigh*, hack to make impworkers less accurate without affecteing imperial officer accuracy
				angs[PITCH] += ( Q_flrand(-1.0f, 1.0f) * (BLASTER_NPC_SPREAD+(6-ent->NPC->currentAim)*0.25f));//was 0.5f
				angs[YAW]	+= ( Q_flrand(-1.0f, 1.0f) * (BLASTER_NPC_SPREAD+(6-ent->NPC->currentAim)*0.25f));//was 0.5f
			}
			else
			{
				angs[PITCH] += ( Q_flrand(-1.0f, 1.0f) * ((5-ent->NPC->currentAim)*0.25f) );
				angs[YAW]	+= ( Q_flrand(-1.0f, 1.0f) * ((5-ent->NPC->currentAim)*0.25f) );
			}

			AngleVectors( angs, forwardVec, NULL, NULL );
		}
	}

	WP_MissileTargetHint(ent, start, forwardVec);

	gentity_t	*missile = CreateMissile( start, forwardVec, REY_VEL, 10000, ent, alt_fire );

	missile->classname = "bryar_proj";
	if ( ent->s.weapon == WP_BLASTER_PISTOL
		|| ent->s.weapon == WP_JAWA )
	{//*SIGH*... I hate our weapon system...
		missile->s.weapon = ent->s.weapon;
	}
	else
	{
		missile->s.weapon = WP_REY;
	}

	if ( alt_fire )
	{
		int count = ( level.time - ent->client->ps.weaponChargeTime ) / REY_CHARGE_UNIT;

		if ( count < 1 )
		{
			count = 1;
		}
		else if ( count > 5 )
		{
			count = 5;
		}

		damage *= count;
		missile->count = count; // this will get used in the projectile rendering code to make a beefier effect
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if ( alt_fire )
	{
		missile->methodOfDeath = MOD_REY_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_REY;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_REY);

	if ( ent->weaponModel[1] > 0 )
	{//dual pistols, toggle the muzzle point back and forth between the two pistols each time he fires
		ent->count = (ent->count)?0:1;
	}
}

//---------------
//	Westar 34
//---------------

//---------------------------------------------------------
void WP_FireJangoPistolMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int	velocity = altFire ? weaponData[ent->s.weapon].mAltVelocity : weaponData[ent->s.weapon].mVelocity;
	int	damage = altFire ? weaponData[ent->s.weapon].altDamage : weaponData[ent->s.weapon].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= JANGO_NPC_VEL_CUT;
			}
			else
			{
				velocity *= JANGO_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "blaster_proj";
	missile->s.weapon = ent->s.weapon;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = JANGO_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = JANGO_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = JANGO_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		missile->methodOfDeath = MOD_JANGO_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_JANGO;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
	
	check_means_of_death(ent, missile, WP_JANGO);

	if (ent->weaponModel[1] > 0)
	{
		ent->count = (ent->count) ? 0 : 1;
	}
}

//---------------------------------------------------------
void WP_FireJangoPistol(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t	dir, angs;

	vectoangles(forwardVec, angs);
	is_player_alt_firing(ent, WP_JANGO, &alt_fire);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if (alt_fire)
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * JANGO_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * JANGO_ALT_SPREAD;
		}
		else
		{
			// Troopers use their aim values as well as the gun's inherent inaccuracy
			// so check for all classes of stormtroopers and anyone else that has aim error
			if (ent->client && ent->NPC &&
				(ent->client->NPC_class == CLASS_STORMTROOPER ||
				ent->client->NPC_class == CLASS_SWAMPTROOPER))
			{
				angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (JANGO_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
				angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (JANGO_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
			}
			else
			{
				// add some slop to the main-fire direction
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * JANGO_MAIN_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * JANGO_MAIN_SPREAD;
			}
		}
	}

	AngleVectors(angs, dir, NULL, NULL);

	// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
	WP_FireJangoPistolMissile(ent, muzzle, dir, alt_fire);
}

//---------------
//	EE-3 Carbine Rifle
//---------------

//---------------------------------------------------------
void WP_FireBobaRifleMissile( gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire )
//---------------------------------------------------------
{
	int velocity = BOBA_VELOCITY;
	int	damage = altFire ? weaponData[WP_BOBA].altDamage : weaponData[WP_BOBA].damage;

	if ( ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE )
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if ( ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO) )
		{
			if ( g_spskill->integer < 2 )
			{
				velocity *= BOBA_NPC_VEL_CUT;
			}
			else
			{
				velocity *= BOBA_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart( ent, start, vec3_origin, vec3_origin );//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile( start, dir, velocity, 10000, ent, altFire );

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_BOBA;

	// Do the damages
	if ( ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO) )
	{
		if ( g_spskill->integer == 0 )
		{
			damage = BOBA_NPC_DAMAGE_EASY;
		}
		else if ( g_spskill->integer == 1 )
		{
			damage = BOBA_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = BOBA_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_BOBA;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_BOBA);
}

//---------------------------------------------------------
void WP_FireBobaRifle( gentity_t *ent, qboolean alt_fire )
//---------------------------------------------------------
{
	vec3_t	dir, angs;
	qboolean scoped = is_player_scoped(ent);

	vectoangles( forwardVec, angs );
	is_player_alt_firing(ent, WP_BOBA, &alt_fire);

	if ( ent->client && ent->client->NPC_class == CLASS_VEHICLE )
	{//no inherent aim screw up
	}
	else if (scoped)
	{
		AngleVectors(ent->client->renderInfo.eyeAngles, forwardVec, NULL, NULL);
		vectoangles(forwardVec, angs);

		if (ent->client->ps.firing_attack & TERTIARY_ATTACK)
		{
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BOBA_TERTIARY_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * BOBA_TERTIARY_SPREAD;
		}
		else
		{
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BOBA_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * BOBA_ALT_SPREAD;
		}
	}
	else if ( !(ent->client->ps.forcePowersActive&(1<<FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2 )
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if ( alt_fire )
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BOBA_ALT_SPREAD;
			angs[YAW]	+= Q_flrand(-1.0f, 1.0f) * BOBA_ALT_SPREAD;
		}
		else
		{
			// Troopers use their aim values as well as the gun's inherent inaccuracy
			// so check for all classes of stormtroopers and anyone else that has aim error
			if ( ent->client && ent->NPC &&
				( ent->client->NPC_class == CLASS_STORMTROOPER ||
				ent->client->NPC_class == CLASS_SWAMPTROOPER ) )
			{
				angs[PITCH] += ( Q_flrand(-1.0f, 1.0f) * (BOBA_NPC_SPREAD+(6-ent->NPC->currentAim)*0.25f));//was 0.5f
				angs[YAW]	+= ( Q_flrand(-1.0f, 1.0f) * (BOBA_NPC_SPREAD+(6-ent->NPC->currentAim)*0.25f));//was 0.5f
			}
			else
			{
				// add some slop to the main-fire direction
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BOBA_MAIN_SPREAD;
				angs[YAW]	+= Q_flrand(-1.0f, 1.0f) * BOBA_MAIN_SPREAD;
			}
		}
	}

	if (scoped)
	{
		AngleVectors(angs, forwardVec, NULL, NULL);
		WP_FireBobaRifleMissile(ent, ent->client->renderInfo.eyePoint, forwardVec, alt_fire);
	}
	else
	{
		AngleVectors( angs, dir, NULL, NULL );

		// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
		WP_FireBobaRifleMissile( ent, muzzle, dir, alt_fire );
	}
}

//---------------
//	DC-17 Hand Pistol
//---------------

//---------------------------------------------------------
void WP_FireClonePistolMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int velocity = CLONEPISTOL_VELOCITY;
	int	damage = altFire ? weaponData[WP_CLONEPISTOL].altDamage : weaponData[WP_CLONEPISTOL].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= CLONEPISTOL_NPC_VEL_CUT;
			}
			else
			{
				velocity *= CLONEPISTOL_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_CLONEPISTOL;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = CLONEPISTOL_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = CLONEPISTOL_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = CLONEPISTOL_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		missile->methodOfDeath = MOD_CLONEPISTOL_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_CLONEPISTOL;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_CLONEPISTOL);

	if (ent->weaponModel[1] > 0)
	{
		ent->count = (ent->count) ? 0 : 1;
	}
}

//---------------------------------------------------------
void WP_FireClonePistol(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t	dir, angs;

	vectoangles(forwardVec, angs);
	is_player_alt_firing(ent, WP_CLONEPISTOL, &alt_fire);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if (alt_fire)
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CLONEPISTOL_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * CLONEPISTOL_ALT_SPREAD;
		}
		else
		{
			// Troopers use their aim values as well as the gun's inherent inaccuracy
			// so check for all classes of stormtroopers and anyone else that has aim error
			if (ent->client && ent->NPC &&
				(ent->client->NPC_class == CLASS_STORMTROOPER ||
				ent->client->NPC_class == CLASS_SWAMPTROOPER))
			{
				angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (CLONEPISTOL_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
				angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (CLONEPISTOL_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
			}
			else
			{
				// add some slop to the main-fire direction
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CLONEPISTOL_MAIN_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * CLONEPISTOL_MAIN_SPREAD;
			}
		}
	}

	AngleVectors(angs, dir, NULL, NULL);

	// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
	WP_FireClonePistolMissile(ent, muzzle, dir, alt_fire);
}
