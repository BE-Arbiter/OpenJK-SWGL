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
File for default fire behavior

===========================================================================
*/

#include "g_local.h"
#include "b_local.h"
#include "g_functions.h"
#include "wp_saber.h"
#include "w_local.h"
#include "../cgame/cg_local.h"


void WP_SetMethodOfDeath(gentity_t *missile,int weaponNum, qboolean altFire) 
{
	int baseWeapon = weaponData[weaponNum].baseWeaponNum ? weaponData[weaponNum].baseWeaponNum : weaponNum;
	switch (baseWeapon) {
		case WP_BRYAR_PISTOL:
		case WP_BLASTER_PISTOL:
			missile->methodOfDeath = altFire ? MOD_BRYAR_ALT : MOD_BRYAR;
			return;
		case WP_BOWCASTER:
			missile->methodOfDeath = altFire ? MOD_BOWCASTER_ALT : MOD_BOWCASTER;
			return;
		case WP_BLASTER : 
			missile->methodOfDeath = altFire ? MOD_BLASTER_ALT : MOD_BLASTER;
			return;
		default:
			missile->methodOfDeath = altFire ? MOD_BLASTER_ALT : MOD_BLASTER;
	}
}

int WP_GetNpcVelocity(gentity_t* ent) {
	//Player and those NPC fire like... really fast
	if (ent->s.number == 0
		|| ent->client->NPC_class == CLASS_BOBAFETT
		|| ent->client->NPC_class == CLASS_MANDALORIAN
		|| ent->client->NPC_class == CLASS_JANGO) {
		return 1;
	}
	if (g_spskill->integer < 2)
	{
		return BLASTER_NPC_VEL_CUT;
	}
	else
	{
		return BLASTER_NPC_HARD_VEL_CUT;
	}
	return 1;
}

int WP_GetNpcDamage(gentity_t *ent) {
	// Player and those NPC do Normal Damage
	if (ent->s.number == 0
		|| ent->client->NPC_class == CLASS_BOBAFETT
		|| ent->client->NPC_class == CLASS_MANDALORIAN
		|| ent->client->NPC_class == CLASS_JANGO) {
		return -1;
	}
	int baseWeaponNum = weaponData[ent->s.weapon].baseWeaponNum ? weaponData[ent->s.weapon].baseWeaponNum : ent->s.weapon;
	/* Blaster Damage*/
	if (baseWeaponNum == WP_BLASTER && g_spskill->integer == 0)
	{
		return BLASTER_NPC_DAMAGE_EASY;
	}
	if (baseWeaponNum == WP_BLASTER && g_spskill->integer == 1)
	{
		return  BLASTER_NPC_DAMAGE_NORMAL;
	}
	if(baseWeaponNum == WP_BLASTER)
	{
		return BLASTER_NPC_DAMAGE_HARD;
	}
}

//---------------------------------------------------------
void WP_FireGenericBlasterMissile(gentity_t* ent, vec3_t start, vec3_t dir,weaponAttackData_t* attackData, qboolean altFire,int forcedVelocity = -1)
//---------------------------------------------------------
{
	int velocity = attackData->mVelocity;
	int	damage = attackData->damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		velocity *= WP_GetNpcVelocity(ent);
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	//ClassName Seems to be unimportant, so left as "blaster_proj" for performance reason
	missile->classname = "blaster_proj";
	missile->s.weapon = ent->s.weapon;


	//If charged attack
	if (attackData->firingLogic == FL_BLASTER_CHARGED)
	{
		int count = (level.time - ent->client->ps.weaponChargeTime) / attackData->chargeUnitTime;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > attackData->maxChargeUnits)
		{
			count = attackData->maxChargeUnits;
		}

		damage *= count;
		missile->count = count; // this will get used in the projectile rendering code to make a beefier effect
	}

	int npcDamage = WP_GetNpcDamage(ent);

	missile->damage = npcDamage != -1 ? npcDamage : damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->splashDamage = attackData->splashDamage;
	missile->splashRadius = attackData->splashRadius;

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	if (attackData->bounceWall) {
		missile->s.eFlags |= EF_BOUNCE;
	}
	missile->bounceCount = attackData->bounceCount;
}

//---------------------------------------------------------
void WP_FireGenericBlaster(gentity_t* ent, weaponAttackData_t* attackData, qboolean altFire)
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	vec3_t	dir, angs;

	
	vectoangles(forwardVec, angs);
	/* Calculate Spread, If we are a vehicle or we have Sense 2, no spread*/
	if (!ent->client || !ent->client->NPC_class == CLASS_VEHICLE
		|| !(ent->client->ps.forcePowersActive & (1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{
		// Troopers use their aim values as well as the gun's inherent inaccuracy
		// so check for all classes of stormtroopers and anyone else that has aim error
		if (ent->client && ent->NPC &&
			(
				ent->client->NPC_class == CLASS_STORMTROOPER
				|| ent->client->NPC_class == CLASS_SWAMPTROOPER 
				|| ent->client->NPC_class == CLASS_IMPWORKER
				)
			)
		{
			angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (BLASTER_NPC_SPREAD + (6 - ent->NPC->currentAim) * 0.25f));//was 0.5f
			angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (BLASTER_NPC_SPREAD + (6 - ent->NPC->currentAim) * 0.25f));//was 0.5f
		}
		else
		{
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * attackData->spread;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * attackData->spread;
		}
	}
	AngleVectors(angs, dir, NULL, NULL);

	WP_FireBlasterMissile(ent, muzzle, dir, altFire);
	//If it's a charged attack with dual pistol, fire a second projectile at muzzle 2
	if (attackData->firingLogic == FL_BLASTER_CHARGED && wpnData->weaponCategory == WC_PISTOL && ent->weaponModel[1] > 0)
	{
		WP_FireBlasterMissile(ent, muzzle2, dir, altFire);
	}

	WP_SwitchPistolMuzzle(ent);
}


//---------------------------------------------------------
void WP_FireGenericBowcaster(gentity_t* ent, weaponAttackData_t* attackData, qboolean altFire)
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	vec3_t	dir, angs, start;
	gentity_t* missile;
	int vel,wpnCount = 1,count;

	count = (level.time - ent->client->ps.weaponChargeTime) / attackData->chargeUnitTime;

	if (count < 1)
	{
		count = 1;
	}
	else if (count > attackData->maxChargeUnits)
	{
		count = attackData->maxChargeUnits;
	}

	if (!(count & 1))
	{
		// if we aren't odd, knock us down a level
		count--;
	}

	//Check if we have a twinned shot
	if (attackData->firingLogic == FL_BOWCASTER && wpnData->weaponCategory == WC_PISTOL && ent->weaponModel[1] > 0) {
		wpnCount = 2;
	}

	//Prepare the first shot
	VectorCopy(muzzle, start);
	for (int j = 0; j < wpnCount; j++)
	{
		//make sure our start point isn't on the other side of a wall
		WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);
		WP_MissileTargetHint(ent, muzzle, forwardVec);
		for (int i = 0; i < count; i++)
		{
			// create a range of different velocities
			vel = attackData->mVelocity * (Q_flrand(-1.0f, 1.0f) * BOWCASTER_VEL_RANGE + 1.0f);

			vectoangles(forwardVec, angs);

			if (!(ent->client->ps.forcePowersActive & (1 << FP_SEE))
				|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2
				)
			{
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * attackData->spread * 0.2f;
				angs[YAW] += ((i + 0.5f) * attackData->spread - count * 0.5f * attackData->spread);
				if (ent->NPC)
				{
					angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (BLASTER_NPC_SPREAD + (6 - ent->NPC->currentAim) * 0.25f));
					angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (BLASTER_NPC_SPREAD + (6 - ent->NPC->currentAim) * 0.25f));
				}
			}

			AngleVectors(angs, dir, NULL, NULL);


			missile = CreateMissile(muzzle, dir, vel, 10000, ent);

			missile->classname = "bowcaster_proj";
			missile->s.weapon = ent->s.weapon;

			VectorSet(missile->maxs, BOWCASTER_SIZE, BOWCASTER_SIZE, BOWCASTER_SIZE);
			VectorScale(missile->maxs, -1, missile->mins);

			WP_SetMethodOfDeath(missile, ent->s.weapon, altFire);

			missile->damage = attackData->damage;
			missile->dflags = DAMAGE_DEATH_KNOCKBACK;
			missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
			missile->splashDamage = attackData->splashDamage;
			missile->splashRadius = attackData->splashRadius;


			if (attackData->bounceWall) {
				missile->s.eFlags |= EF_BOUNCE;
			}
			missile->bounceCount = attackData->bounceCount;

			ent->client->sess.missionStats.shotsFired++;
		}
		//Prepare the twinned projectile
		VectorCopy(muzzle2, start);
	}
	WP_SwitchPistolMuzzle(ent);
}
