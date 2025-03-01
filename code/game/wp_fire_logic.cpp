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


qboolean is_player_scoped(gentity_t* ent)
{
	return (qboolean)((cg.zoomMode >= ST_A280 || cg.zoomMode == ST_DISRUPTOR) && ent->client->ps.clientNum == 0);
}

/*
	Set the method of death based on the weapon
*/

//DWS-TDDO : Need to dehardcode this, allowing to override... Warning that sometimes there are several MOD for a same weapon...
void WP_SetMethodOfDeath(gentity_t *missile,int weaponNum, qboolean altFire) 
{
	int baseWeapon = weaponData[weaponNum].baseWeaponNum ? weaponData[weaponNum].baseWeaponNum : weaponNum;
	switch (baseWeapon) {
		case WP_BRYAR_PISTOL:
		case WP_BLASTER_PISTOL:
		case WP_TUSKEN_RIFLE:
			missile->methodOfDeath = altFire ? MOD_BRYAR_ALT : MOD_BRYAR;
			missile->splashMethodOfDeath = altFire ? MOD_BRYAR_ALT : MOD_BRYAR;
			return;
		case WP_BOWCASTER:
			missile->methodOfDeath = altFire ? MOD_BOWCASTER_ALT : MOD_BOWCASTER;
			missile->splashMethodOfDeath = altFire ? MOD_BOWCASTER_ALT : MOD_BOWCASTER;
			return;
		case WP_DEMP2:
			missile->methodOfDeath = altFire ? MOD_DEMP2_ALT : MOD_DEMP2;
			missile->splashMethodOfDeath = altFire ? MOD_DEMP2_ALT : MOD_DEMP2;
			
		case WP_REPEATER:
			missile->methodOfDeath = altFire ? MOD_REPEATER_ALT: MOD_REPEATER;
			missile->splashMethodOfDeath = altFire ? MOD_REPEATER_ALT: MOD_REPEATER;
			return;
		case WP_NOGHRI_STICK:
			missile->methodOfDeath = MOD_BLASTER ;
			missile->splashMethodOfDeath = MOD_GAS;
			return;
		case WP_DISRUPTOR:
			missile->methodOfDeath = altFire ? MOD_SNIPER : MOD_DISRUPTOR;
			missile->splashMethodOfDeath = altFire ? MOD_SNIPER : MOD_DISRUPTOR;
			return;
		case WP_ROCKET_LAUNCHER:
			missile->methodOfDeath = altFire ? MOD_ROCKET_ALT : MOD_ROCKET;
			missile->splashMethodOfDeath = altFire ? MOD_ROCKET_ALT : MOD_ROCKET;
			return;
		case WP_BLASTER : 
		default:
			missile->methodOfDeath = altFire ? MOD_BLASTER_ALT : MOD_BLASTER;
			missile->splashMethodOfDeath = altFire ? MOD_BLASTER_ALT : MOD_BLASTER;
	}
}

/*
	Return the weapon missile velocity for NPC
*/
float WP_GetNpcVelocity(gentity_t* ent) {
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

/*
	Return the damage, taking into account Weapon Type, Entity Type & Class + Difficulty
*/
int WP_GetWeaponDamage(gentity_t* ent, weaponAttackData_t* attackData, qboolean altFire = qfalse) {
	int baseWeaponNum = weaponData[ent->s.weapon].baseWeaponNum ? weaponData[ent->s.weapon].baseWeaponNum : ent->s.weapon;
	/* Disruptor effect (All npc do reduced damage) */
	if (ent->s.number != 0 && baseWeaponNum == WP_DISRUPTOR && g_spskill->integer == 0)
	{
		return altFire ? DISRUPTOR_NPC_ALT_DAMAGE_EASY : DISRUPTOR_NPC_MAIN_DAMAGE_EASY;
	}
	if (ent->s.number != 0 && baseWeaponNum == WP_DISRUPTOR && g_spskill->integer == 1)
	{
		return altFire ? DISRUPTOR_NPC_ALT_DAMAGE_MEDIUM : DISRUPTOR_NPC_MAIN_DAMAGE_MEDIUM;
	}
	if (ent->s.number != 0 && baseWeaponNum == WP_DISRUPTOR)
	{
		return altFire ? DISRUPTOR_NPC_ALT_DAMAGE_HARD : DISRUPTOR_NPC_MAIN_DAMAGE_HARD;
	}
	// Player and those NPC do Normal Damage
	if (ent->s.number == 0
		|| ent->client->NPC_class == CLASS_BOBAFETT
		|| ent->client->NPC_class == CLASS_MANDALORIAN
		|| ent->client->NPC_class == CLASS_JANGO) {
		return attackData->damage;
	}
	//Instead of thousand of define, base the damage around a difficulty modifier
	//Could even be set in a float cvar next...
	float mult = (g_spskill->integer == 0) ? 0.3f : ((g_spskill->integer == 1) ? 0.6f : 0.9f);
	return (int) (attackData->damage * mult);
}

/*
	Return the spread for NPC if it is found, 0 otherwise
*/
float WP_GetNpcSpread(gentity_t* ent, qboolean altFire = qfalse) {
	int baseWeaponNum = weaponData[ent->s.weapon].baseWeaponNum ? weaponData[ent->s.weapon].baseWeaponNum : ent->s.weapon;
	if (baseWeaponNum == WP_BLASTER || baseWeaponNum == WP_BOWCASTER) {
		return BLASTER_NPC_SPREAD;
	}
	if (baseWeaponNum == WP_THEFIRSTORDER) {
		return F_11D_NPC_SPREAD;
	}
	return BLASTER_NPC_SPREAD;
}

void WP_ApplyLockDownOnMissile(gentity_t* ent, gentity_t* missile, qboolean alwaysLock = qfalse) {
	int	lockEntNum, lockTime;
	if (ent->NPC && ent->enemy)
	{
		lockEntNum = ent->enemy->s.number;
		lockTime = Q_irand(600, 1200);
	}
	else
	{
		lockEntNum = g_rocketLockEntNum;
		lockTime = g_rocketLockTime;
	}
	// we'll consider attempting to lock this little poochie onto some baddie.
	if ((lockEntNum > 0 || (ent->NPC && lockEntNum >= 0)) && lockEntNum < ENTITYNUM_WORLD && lockTime > 0)
	{
		// take our current lock time and divide that by 8 wedge slices to get the current lock amount
		int dif = (level.time - lockTime) / (1200.0f / 8.0f);

		if (dif < 0)
		{
			dif = 0;
		}
		else if (dif > 8)
		{
			dif = 8;
		}

		// if we are fully locked, always take on the enemy.
		//	Also give a slight advantage to higher, but not quite full charges.
		//	Finally, just give any amount of charge a very slight random chance of locking.
		if (dif == 8 || Q_flrand(0.0f, 1.0f) * dif > 2 || Q_flrand(0.0f, 1.0f) > 0.97f || alwaysLock)
		{
			missile->enemy = &g_entities[lockEntNum];

			if (missile->enemy
				&& missile->enemy->inuse)//&& DistanceSquared( missile->currentOrigin, missile->enemy->currentOrigin ) < 262144 && InFOV( missile->currentOrigin, missile->enemy->currentOrigin, missile->enemy->client->ps.viewangles, 45, 45 ) )
			{
				if (missile->enemy->client
					&& (missile->enemy->client->ps.forcePowersKnown & (1 << FP_PUSH))
					&& missile->enemy->client->ps.forcePowerLevel[FP_PUSH] > FORCE_LEVEL_0)
				{//have force push, don't flee from homing rockets
				}
				else
				{
					vec3_t dir, dir2;

					AngleVectors(missile->enemy->currentAngles, dir, NULL, NULL);
					AngleVectors(ent->client->renderInfo.eyeAngles, dir2, NULL, NULL);

					if (DotProduct(dir, dir2) < 0.0f)
					{
						G_StartFlee(missile->enemy, ent, missile->enemy->currentOrigin, AEL_DANGER_GREAT, 3000, 5000);
						if (!TIMER_Done(missile->enemy, "flee"))
						{
							TIMER_Set(missile->enemy, "rocketChasing", 500);
						}
					}
				}
			}
		}
	}

	VectorCopy(forwardVec, missile->movedir);

	missile->e_ThinkFunc = thinkF_WP_RocketThink;
	missile->random = 1.0f;
	missile->nextthink = level.time + 100;
}
//---------------------------------------------------------
void WP_FireGenericBlasterMissile(gentity_t* ent, vec3_t start, vec3_t dir,int attackIndex,int forcedVelocity = -1)
// This method fire a missile typical of a blaster, default weapons includes : 
//  - Blaster Pistol + Charged Blaster Pistol (and all variant)
//  - Blaster (Incl. Rapid fire)
//  - Reapeater Main fire
//  - BowCaster Alt Fire
// 
// Use Attack Data Firing Logic (Grenade Launcher) to determine if it sould be affected by gravity
// Use attack Data bounceWall to determine if it should bounce on wall
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
	int velocity = attackData->mVelocity;
	int	damage = WP_GetWeaponDamage(ent, attackData);

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

	gentity_t* missile = CreateMissile(start, dir, velocity, 10000, ent, attackIndex);

	//ClassName Seems to be unimportant, so left as "blaster_proj" for performance reason
	missile->classname = "blaster_proj";
	missile->s.weapon = ent->s.weapon;


	//If charged attack
	if (attackData->firingLogic == FL_BLASTER_CHARGED || attackData->firingLogic == FL_DEMP2_ALT)
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

		if (attackData->firingLogic == FL_DEMP2_ALT)
		{
			damage *= (1 + (count * (count - 1)));
			missile->e_ThinkFunc = thinkF_DEMP2_AltDetonate;
		}
		else
		{
			damage *= count;
		}
		missile->count = count; // this will get used in the projectile rendering code to make a beefier effect
	}
	//If Grenade Launcher
	else if (attackData->firingLogic == FL_GRENADE_LAUNCHER) {
		missile->s.pos.trType = TR_GRAVITY;
		missile->s.pos.trDelta[2] += 40.0f;
		missile->mass = 10;
	}
	else if (attackData->firingLogic == FL_MISSILE_AIMED) {
		WP_ApplyLockDownOnMissile(ent, missile);
	}

	if (attackData->missileSize) {
		// Make it easier to hit things
		VectorSet(missile->maxs, attackData->missileSize, attackData->missileSize, attackData->missileSize);
		VectorScale(missile->maxs, -1, missile->mins);
	}

	if (attackData->firingLogic == FL_NOGHRI) {
		missile->dflags = DAMAGE_NO_KNOCKBACK;
	}
	else {
		missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	}
	missile->damage = damage;
	missile->splashDamage = attackData->splashDamage;
	missile->splashRadius = attackData->splashRadius;

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	if (attackData->bounceWall) {
		missile->s.eFlags |= EF_BOUNCE;
	}
	missile->bounceCount = attackData->bounceCount;
}

//---------------------------------------------------------
void WP_FireGenericBlaster(gentity_t* ent, int attackIndex)
// This method handle the firing of generic blasters, see Previous WP_FireGenericBlasterMissile for more information
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
	vec3_t	dir, angs;

	
	vectoangles(forwardVec, angs);
	/* Calculate Spread, If we are a vehicle or we have Sense 2, no spread*/
	if (!ent->client || !(ent->client->NPC_class == CLASS_VEHICLE)
		|| !(ent->client->ps.forcePowersActive & (1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{
		// Some NPCs can't aim
		if (ent->client && ent->NPC && ( ent->client->NPC_class == CLASS_STORMTROOPER 
			|| ent->client->NPC_class == CLASS_SWAMPTROOPER  || ent->client->NPC_class == CLASS_IMPWORKER)
		)
		{
			float npcSpread = WP_GetNpcSpread(ent);
			angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (npcSpread + (6 - ent->NPC->currentAim) * 0.25f));
			angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (npcSpread + (6 - ent->NPC->currentAim) * 0.25f));
		}
		else
		{
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * attackData->spread;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * attackData->spread;
		}
	}

	if (is_player_scoped(ent)) {
		AngleVectors(ent->client->renderInfo.eyeAngles, forwardVec, NULL, NULL);
		WP_FireGenericBlasterMissile(ent, ent->client->renderInfo.eyePoint, forwardVec, attackIndex);
	}
	else {
		AngleVectors(angs, dir, NULL, NULL);
		WP_FireGenericBlasterMissile(ent, muzzle, dir, attackIndex);
		//If it's a charged attack with dual pistol, fire a second projectile at muzzle 2
		if (attackData->firingLogic == FL_BLASTER_CHARGED && wpnData->weaponCategory == WC_PISTOL && ent->weaponModel[1] > 0)
		{
			WP_FireGenericBlasterMissile(ent, muzzle2, dir, attackIndex);
		}
	}

	WP_SwitchPistolMuzzle(ent);
}


//---------------------------------------------------------
void WP_FireFlameThrower(gentity_t* ent, int attackIndex)
//---------------------------------------------------------
{
	trace_t		tr;
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
	vec3_t	dir, start,end;
	float range = attackData->range;

	/* Calculate the damage, it's a little bit random */
	int	damage = attackData->damage;
	
	//Get normalized direction in dir
	VectorCopy(forwardVec, dir);
	VectorNormalizeFast(dir);

	//Init start point
	VectorCopy(muzzle, start);
	VectorMA(start, range, dir, end);


	vec3_t mins, maxs;
	for (int i = 0; i < 3; i++)
	{
		mins[i] = start[i] - range;
		maxs[i] = start[i] + range;
	}
	gentity_t* entityList[MAX_GENTITIES];
	int numListedEntities = gi.EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);
	
	for(int i = 0 ; i < numListedEntities; i++){
		gentity_t *target = entityList[i];

		if (target->s.clientNum == ent->s.clientNum) {
			//Don't target oneself
			continue;
		}

		//Aim for somewhere in the torso
		vec3_t hitloc;
		VectorCopy(target->currentOrigin, hitloc);
		hitloc[2] += 25.0f;
		

        // Is the target in front of us?
		vec3_t target_dir;
		VectorSubtract(hitloc, start, target_dir);

		int dist = VectorLength(target_dir);
		if (dist == 0) {
			continue; // Just in case
		}

		VectorNormalizeFast(target_dir);
		if (DotProduct(target_dir, dir) < 0.80f) {
			continue; // We are not "in front" of the muzzle
		}

		gi.trace(&tr, start, vec3_origin, vec3_origin, hitloc, ent->s.number, MASK_FORCE_PUSH, (EG2_Collision)0, 0);//was MASK_SHOT, but changed to match above trace and crosshair trace
		if (tr.fraction < 1.0f && tr.entityNum != target->s.number)
		{//must have clear LOS
			continue;
		}

		if (target->takedamage)
		{
			G_Damage(target, ent, ent, target_dir, hitloc, damage, DAMAGE_NO_ARMOR | DAMAGE_NO_KNOCKBACK | DAMAGE_NO_HIT_LOC | DAMAGE_IGNORE_TEAM, MOD_LAVA, HL_NONE);
			// HeEeHuHahEHoHo
			if (target->health > 0)
			{
				if (attackData->hitFleshEffect[0]) 
				{
					G_PlayEffect(G_EffectIndex(attackData->hitFleshEffect), target->currentOrigin);
				}
				else
				{
					G_PlayEffect(G_EffectIndex("env/fire.efx"), target->currentOrigin);
					G_PlayEffect(G_EffectIndex("env/small_fire.efx"), hitloc);
				}
			}
			else
			{
				if (attackData->hitFleshEffect[0])
				{
					G_PlayEffect(G_EffectIndex(attackData->hitFleshEffect), target->currentOrigin);
				}
				else
				{
					G_PlayEffect(G_EffectIndex("env/small_fire.efx"), target->currentOrigin);
				}
			}
		}
	}
}
//---------------------------------------------------------
void WP_FireGenericBowcaster(gentity_t* ent, int attackIndex)
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
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
		WP_MissileTargetHint(ent, start, forwardVec);
		for (int i = 0; i < count; i++)
		{
			// create a range of different velocities
			vel = attackData->mVelocity * (Q_flrand(0.8f, 1.2f) + 1.0f);

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


			missile = CreateMissile(start, dir, vel, 10000, ent);

			missile->classname = "bowcaster_proj";
			missile->s.weapon = ent->s.weapon;

			if (attackData->missileSize) {
				// Make it easier to hit things
				VectorSet(missile->maxs, attackData->missileSize, attackData->missileSize, attackData->missileSize);
				VectorScale(missile->maxs, -1, missile->mins);
			}

			WP_SetMethodOfDeath(missile, ent->s.weapon,(qboolean)attackIndex);

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


//DWS-TODO : MOD_XXXX still hardcoded
//---------------------------------------------------------
void WP_FireGenericBeam(gentity_t* ent, int attackIndex)
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
	int			damage = attackData->damage, skip,traces = 10;
	qboolean	render_impact = qtrue;
	vec3_t		start, end;
	vec3_t		spot, dir;
	trace_t		tr;
	gentity_t* traceEnt, * tent;
	float		dist, shotDist, shotRange = 8192;
	qboolean	hitDodged = qfalse, fullCharge = qfalse;

	// The trace start will originate at the eye so we can ensure that it hits the crosshair.
	if (ent->s.number != 0)
	{
		qboolean isNpcAltdamage = qfalse;
		VectorCopy(muzzle, start);
		if (attackData->firingLogic == FL_BEAM_CHARGED) 
		{
			fullCharge = qtrue;
			traces = DISRUPTOR_ALT_TRACES;
			isNpcAltdamage = qtrue;
		}
		//DWS-TODO : Need to do something else here to remove the Disruptor NPC damage constant..
		damage = WP_GetWeaponDamage(ent, attackData,isNpcAltdamage); //We don't check for alt fire but for charged.
	}
	else if(attackData->firingLogic == FL_BEAM_CHARGED)
	{

		int count = (level.time - ent->client->ps.weaponChargeTime - 50) / attackData->chargeUnitTime;

		if (count < 1)
		{
			count = 1;
		}
		else if (count >= attackData->maxChargeUnits)
		{
			count = attackData->maxChargeUnits;
			fullCharge = qtrue;
		}

		// more powerful charges go through more things
		if (count < 3)
		{
			traces = 1;
		}
		else if (count < 6)
		{
			traces = 2;
		}
		else {
			traces = DISRUPTOR_ALT_TRACES;
		}

		damage = damage * count + attackData->damage * 0.5f; // give a boost to low charge shots
	}

	if (!ent->NPC) {
		if (is_player_scoped(ent)) {
			VectorCopy(ent->client->renderInfo.eyePoint, start);
			AngleVectors(ent->client->renderInfo.eyeAngles, forwardVec, NULL, NULL);
		}
		else {
			VectorCopy(muzzle, start);
		}
		WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);
		WP_MissileTargetHint(ent, start, forwardVec);
	}

	skip = ent->s.number;

	for (int i = 0; i < traces; i++)
	{
		VectorMA(start, shotRange, forwardVec, end);

		gi.trace(&tr, start, NULL, NULL, end, skip, MASK_SHOT, G2_COLLIDE, 10);

		if (tr.surfaceFlags & SURF_NOIMPACT)
		{
			render_impact = qfalse;
		}

		if (tr.entityNum == ent->s.number)
		{
			// should never happen, but basically we don't want to consider a hit to ourselves?
			// Get ready for an attempt to trace through another person
			VectorCopy(tr.endpos, muzzle2);
			VectorCopy(tr.endpos, start);
			skip = tr.entityNum;
			continue;
		}

		if (tr.fraction >= 1.0f)
		{
			// draw the beam but don't do anything els
			break;
		}

		traceEnt = &g_entities[tr.entityNum];

		if (traceEnt && (traceEnt->s.weapon == WP_SABER || (traceEnt->client 
			&& (
				traceEnt->client->NPC_class == CLASS_BOBAFETT 
					|| traceEnt->client->NPC_class == CLASS_MANDALORIAN 
					|| traceEnt->client->NPC_class == CLASS_JANGO
					|| traceEnt->client->NPC_class == CLASS_REBORN)
			))
		)
		{
			hitDodged = Jedi_DodgeEvasion(traceEnt, ent, &tr, HL_NONE);
			//acts like we didn't even hit him
		}
		if (!hitDodged)
		{
			if (render_impact)
			{
				if ((tr.entityNum < ENTITYNUM_WORLD && traceEnt->takedamage)
					|| !Q_stricmp(traceEnt->classname, "misc_model_breakable")
					|| traceEnt->s.eType == ET_MOVER)
				{
					// Create a simple impact type mark that doesn't last long in the world
					int effect;
					if (attackData->hitFleshEffect[0]) {
						effect = G_EffectIndex(attackData->hitFleshEffect);
					}
					else {
						effect = G_EffectIndex("disruptor/alt_hit");
					}

					G_PlayEffect(effect, tr.endpos, tr.plane.normal);

					if (traceEnt->client && LogAccuracyHit(traceEnt, ent))
					{//NOTE: hitting multiple ents can still get you over 100% accuracy
						ent->client->ps.persistant[PERS_ACCURACY_HITS]++;
					}

					int hitLoc = G_GetHitLocFromTrace(&tr, MOD_DISRUPTOR);
					if (traceEnt && traceEnt->client && traceEnt->client->NPC_class == CLASS_GALAKMECH)
					{//hehe
						G_Damage(traceEnt, ent, ent, forwardVec, tr.endpos, 10, DAMAGE_NO_KNOCKBACK | DAMAGE_NO_HIT_LOC, fullCharge ? MOD_SNIPER : MOD_DISRUPTOR, hitLoc);
						break;
					}
					G_Damage(traceEnt, ent, ent, forwardVec, tr.endpos, damage, DAMAGE_NO_KNOCKBACK | DAMAGE_NO_HIT_LOC, fullCharge ? MOD_SNIPER : MOD_DISRUPTOR, hitLoc);
					if (traceEnt->s.eType == ET_MOVER)
					{
						break;
					}
				}
				else
				{
					// we only make this mark on things that can't break or move
					tent = G_TempEntity(tr.endpos, EV_DISRUPTOR_SNIPER_MISS);
					tent->svFlags |= SVF_BROADCAST;
					VectorCopy(tr.plane.normal, tent->pos1);
					break; // hit solid, but doesn't take damage, so stop the shot...we _could_ allow it to shoot through walls, might be cool?
				}
			}
			else // not rendering impact, must be a skybox or other similar thing?
			{
				break; // don't try anymore traces
			}
		}
		VectorCopy(tr.endpos, start);
		skip = tr.entityNum;
		hitDodged = qfalse;
	}
	//just draw one solid beam all the way to the end...
	tent = G_TempEntity(tr.endpos,EV_GENERIC_BEAM);
	tent->svFlags |= SVF_BROADCAST;
	tent->count = fullCharge; // mark us so we can alter the effect
	tent->attack_index = attackIndex;
	tent->s.weapon = ent->s.weapon;

	VectorCopy(muzzle, tent->s.origin2);

	// now go along the trail and make sight events
	VectorSubtract(tr.endpos, muzzle, dir);

	shotDist = VectorNormalize(dir);

	//FIXME: if shoot *really* close to someone, the alert could be way out of their FOV
	for (dist = 0; dist < shotDist; dist += 64)
	{
		VectorMA(muzzle, dist, dir, spot);
		AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);
	}
	//FIXME: spawn a temp ent that continuously spawns sight alerts here?  And 1 sound alert to draw their attention?
	VectorMA(start, shotDist - 4, forwardVec, spot);
	AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);


	WP_SwitchPistolMuzzle(ent);
}

//---------------
//	Super Battle Droid & Droideka
//---------------
void WP_FireDroidsTwinBlasters(gentity_t* ent, int attackIndex)
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
	vec3_t	angs;
	int velocity = attackData->mVelocity;
	int damage = WP_GetWeaponDamage(ent, attackData);
	float scalers[2] = { SBD_LEFT_SHOT, SBD_RIGHT_SHOT };

	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent->client && ent->client->ps.clientNum != 0)
	{
		velocity *= WP_GetNpcVelocity(ent);
	}

	WP_TraceSetStart(ent, muzzle, vec3_origin, vec3_origin);

	if (ent->client && ent->client->NPC_class == CLASS_DROIDEKA)
	{
		if (NPC && NPC->enemy)
		{
			float enemyDist = DistanceSquared(NPC->currentOrigin, NPC->enemy->currentOrigin);
			vectoangles(forwardVec, angs);
			float baseAngle = 3.0f;
			float baseDist = 66596.9531f;

			float angle = baseAngle * (baseDist / enemyDist);
			if (angle > 14)
				angle = 14.0f;
			if (angle < 1)
				angle = 1.0f;

			if (ent->count)
				angs[YAW] += (angle * -1);
			else
				angs[YAW] += angle;

			AngleVectors(angs, forwardVec, NULL, NULL);
		}
		else
		{
			vectoangles(forwardVec, angs);
			if (ent->count)
				angs[YAW] += -2.0f;
			else
				angs[YAW] += 2.0f;

			AngleVectors(angs, forwardVec, NULL, NULL);
		}

	}

	for (int i = 0; i < 2; i++)
	{
		VectorMA(muzzle, scalers[i], vrightVec, muzzle);

		gentity_t* missile = CreateMissile(muzzle, forwardVec, velocity, 10000, ent, 0);

		missile->classname = "blaster_proj";
		missile->s.weapon = ent->s.weapon;

		missile->damage = damage;
		missile->dflags = DAMAGE_DEATH_KNOCKBACK;
		missile->methodOfDeath = MOD_SBD;
		missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
		missile->bounceCount = 8;
	}

	WP_SwitchPistolMuzzle(ent);
}


//------------------------------------------------------------------------------------
//	Grenades Functions
//------------------------------------------------------------------------------------
void WP_GrenadePlayExplosionEffect(gentity_t* ent, weaponAttackData_t *attackData) {
	if (attackData->explosionEffect && attackData->explosionEffect[0]) {
		G_PlayEffect(attackData->explosionEffect, ent->currentOrigin);
	}
	else
	{
		G_PlayEffect("thermal/explosion", ent->currentOrigin);
	}
	if (attackData->shockwaveEffect && attackData->shockwaveEffect[0]) {
		G_PlayEffect(attackData->shockwaveEffect, ent->currentOrigin);
	}
	else
	{
		G_PlayEffect("thermal/shockwave", ent->currentOrigin);
	}
}
//---------------------------------------------------------
void WP_GrenadeExplode(gentity_t* ent)
//---------------------------------------------------------
{
	if ((ent->s.eFlags & EF_HELD_BY_SAND_CREATURE))
	{
		ent->takedamage = qfalse; // don't allow double deaths!

		G_Damage(ent->activator, ent, ent->owner, vec3_origin, ent->currentOrigin, weaponData[ent->s.weapon].attackData[0].damage, 0, MOD_EXPLOSIVE);

		WP_GrenadePlayExplosionEffect(ent, &weaponData[ent->s.weapon].attackData[0]);

		G_FreeEntity(ent);
	}
	else if (!ent->count)
	{
		G_Sound(ent, G_SoundIndex("sound/weapons/thermal/warning.wav"));
		ent->count = 1;
		ent->nextthink = level.time + 800;
		ent->svFlags |= SVF_BROADCAST;//so everyone hears/sees the explosion?
	}
	else
	{
		vec3_t	pos;

		VectorSet(pos, ent->currentOrigin[0], ent->currentOrigin[1], ent->currentOrigin[2] + 8);

		ent->takedamage = qfalse; // don't allow double deaths!

		G_RadiusDamage(ent->currentOrigin, ent->owner, weaponData[ent->s.weapon].attackData[0].splashDamage, weaponData[ent->s.weapon].attackData[0].splashRadius, NULL, MOD_EXPLOSIVE_SPLASH);

		WP_GrenadePlayExplosionEffect(ent, &weaponData[ent->s.weapon].attackData[0]);

		G_FreeEntity(ent);
	}
}

//---------------------------------------------------------
qboolean WP_LobFire(gentity_t* self, vec3_t start, vec3_t target, vec3_t mins, vec3_t maxs, int clipmask,
	vec3_t velocity, qboolean tracePath, int ignoreEntNum, int enemyNum,
	float minSpeed, float maxSpeed, float idealSpeed, qboolean mustHit)
	//---------------------------------------------------------
{
	float	targetDist, shotSpeed, speedInc = 100, travelTime, impactDist, bestImpactDist = Q3_INFINITE;//fireSpeed,
	vec3_t	targetDir, shotVel, failCase = { 0.0f };
	trace_t	trace;
	trajectory_t	tr;
	qboolean	blocked;
	int		elapsedTime, skipNum, timeStep = 500, hitCount = 0, maxHits = 7;
	vec3_t	lastPos, testPos;
	gentity_t* traceEnt;

	if (!idealSpeed)
	{
		idealSpeed = 300;
	}
	else if (idealSpeed < speedInc)
	{
		idealSpeed = speedInc;
	}
	shotSpeed = idealSpeed;
	skipNum = (idealSpeed - speedInc) / speedInc;
	if (!minSpeed)
	{
		minSpeed = 100;
	}
	if (!maxSpeed)
	{
		maxSpeed = 900;
	}
	while (hitCount < maxHits)
	{
		VectorSubtract(target, start, targetDir);
		targetDist = VectorNormalize(targetDir);

		VectorScale(targetDir, shotSpeed, shotVel);
		travelTime = targetDist / shotSpeed;
		shotVel[2] += travelTime * 0.5 * g_gravity->value;

		if (!hitCount)
		{//save the first (ideal) one as the failCase (fallback value)
			if (!mustHit)
			{//default is fine as a return value
				VectorCopy(shotVel, failCase);
			}
		}

		if (tracePath)
		{//do a rough trace of the path
			blocked = qfalse;

			VectorCopy(start, tr.trBase);
			VectorCopy(shotVel, tr.trDelta);
			tr.trType = TR_GRAVITY;
			tr.trTime = level.time;
			travelTime *= 1000.0f;
			VectorCopy(start, lastPos);

			//This may be kind of wasteful, especially on long throws... use larger steps?  Divide the travelTime into a certain hard number of slices?  Trace just to apex and down?
			for (elapsedTime = timeStep; elapsedTime < floor(travelTime) + timeStep; elapsedTime += timeStep)
			{
				if ((float)elapsedTime > travelTime)
				{//cap it
					elapsedTime = floor(travelTime);
				}
				EvaluateTrajectory(&tr, level.time + elapsedTime, testPos);
				gi.trace(&trace, lastPos, mins, maxs, testPos, ignoreEntNum, clipmask, (EG2_Collision)0, 0);

				if (trace.allsolid || trace.startsolid)
				{
					blocked = qtrue;
					break;
				}
				if (trace.fraction < 1.0f)
				{//hit something
					if (trace.entityNum == enemyNum)
					{//hit the enemy, that's perfect!
						break;
					}
					else if (trace.plane.normal[2] > 0.7 && DistanceSquared(trace.endpos, target) < 4096)//hit within 64 of desired location, should be okay
					{//close enough!
						break;
					}
					else
					{//FIXME: maybe find the extents of this brush and go above or below it on next try somehow?
						impactDist = DistanceSquared(trace.endpos, target);
						if (impactDist < bestImpactDist)
						{
							bestImpactDist = impactDist;
							VectorCopy(shotVel, failCase);
						}
						blocked = qtrue;
						//see if we should store this as the failCase
						if (trace.entityNum < ENTITYNUM_WORLD)
						{//hit an ent
							traceEnt = &g_entities[trace.entityNum];
							if (traceEnt && traceEnt->takedamage && !OnSameTeam(self, traceEnt))
							{//hit something breakable, so that's okay
								//we haven't found a clear shot yet so use this as the failcase
								VectorCopy(shotVel, failCase);
							}
						}
						break;
					}
				}
				if (elapsedTime == floor(travelTime))
				{//reached end, all clear
					break;
				}
				else
				{
					//all clear, try next slice
					VectorCopy(testPos, lastPos);
				}
			}
			if (blocked)
			{//hit something, adjust speed (which will change arc)
				hitCount++;
				shotSpeed = idealSpeed + ((hitCount - skipNum) * speedInc);//from min to max (skipping ideal)
				if (hitCount >= skipNum)
				{//skip ideal since that was the first value we tested
					shotSpeed += speedInc;
				}
			}
			else
			{//made it!
				break;
			}
		}
		else
		{//no need to check the path, go with first calc
			break;
		}
	}

	if (hitCount >= maxHits)
	{//NOTE: worst case scenario, use the one that impacted closest to the target (or just use the first try...?)
		assert((failCase[0] + failCase[1] + failCase[2]) > 0.0f);
		VectorCopy(failCase, velocity);
		return qfalse;
	}
	VectorCopy(shotVel, velocity);
	return qtrue;
}

//---------------------------------------------------------
void WP_GrenadeThink(gentity_t* ent)
//---------------------------------------------------------
{
	int			count;
	qboolean	blow = qfalse;

	// Thermal detonators for the player do occasional radius checks and blow up if there are entities in the blast radius
	//	This is done so that the main fire is actually useful as an attack.  We explode anyway after delay expires.

	if ((ent->s.eFlags & EF_HELD_BY_SAND_CREATURE))
	{//blow once creature is underground (done with anim)
		//FIXME: chance of being spit out?  Especially if lots of delay left...
		ent->e_TouchFunc = touchF_NULL;//don't impact on anything
		if (!ent->activator
			|| !ent->activator->client
			|| !ent->activator->client->ps.legsAnimTimer)
		{//either something happened to the sand creature or it's done with it's attack anim
			//blow!
			ent->e_ThinkFunc = thinkF_WP_GrenadeExplode;
			ent->nextthink = level.time + Q_irand(50, 2000);
		}
		else
		{//keep checking
			ent->nextthink = level.time + TD_THINK_TIME;
		}
		return;
	}
	else if (ent->delay > level.time)
	{
		//	Finally, we force it to bounce at least once before doing the special checks, otherwise it's just too easy for the player?
		if (ent->has_bounced)
		{
			count = G_RadiusList(ent->currentOrigin, TD_TEST_RAD, ent, qtrue, ent_list);

			for (int i = 0; i < count; i++)
			{
				if (ent_list[i]->s.number == 0)
				{
					// avoid deliberately blowing up next to the player, no matter how close any enemy is..
					//	...if the delay time expires though, there is no saving the player...muwhaaa haa ha
					blow = qfalse;
					break;
				}
				else if (ent_list[i]->client
					&& ent_list[i]->client->NPC_class != CLASS_SAND_CREATURE//ignore sand creatures
					&& ent_list[i]->health > 0)
				{
					//FIXME! sometimes the ent_list order changes, so we should make sure that the player isn't anywhere in this list
					blow = qtrue;
				}
			}
		}
	}
	else
	{
		// our death time has arrived, even if nothing is near us
		blow = qtrue;
	}

	if (blow)
	{
		ent->e_ThinkFunc = thinkF_WP_GrenadeExplode;
		ent->nextthink = level.time + 50;
	}
	else
	{
		// we probably don't need to do this thinking logic very often...maybe this is fast enough?
		ent->nextthink = level.time + TD_THINK_TIME;
	}
}

//---------------------------------------------------------
gentity_t* WP_FireGrenade(gentity_t* ent, int attackIndex)
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
	gentity_t* bolt;
	vec3_t		dir, start;
	float		damageScale = 1.0f;

	VectorCopy(forwardVec, dir);
	VectorCopy(muzzle, start);

	bolt = G_Spawn();

	bolt->classname = "thermal_detonator";

	if (ent->s.number != 0)
	{
		// If not the player, cut the damage a bit so we don't get pounded on so much
		damageScale = TD_NPC_DAMAGE_CUT;
	}

	if (!attackIndex && ent->s.number == 0)
	{
		// Main fires for the players do a little bit of extra thinking
		bolt->e_ThinkFunc = thinkF_WP_GrenadeThink;
		bolt->nextthink = level.time + TD_THINK_TIME;
		bolt->delay = level.time + TD_TIME; // How long 'til she blows
	}
	else
	{
		bolt->e_ThinkFunc = thinkF_WP_GrenadeExplode;
		bolt->nextthink = level.time + TD_TIME; // How long 'til she blows
	}

	bolt->mass = 10;

	// How 'bout we give this thing a size...
	VectorSet(bolt->mins, -4.0f, -4.0f, -4.0f);
	VectorSet(bolt->maxs, 4.0f, 4.0f, 4.0f);
	bolt->clipmask = MASK_SHOT;
	bolt->clipmask &= ~CONTENTS_CORPSE;
	bolt->contents = CONTENTS_SHOTCLIP;
	bolt->takedamage = qtrue;
	bolt->health = 15;
	bolt->e_DieFunc = dieF_WP_GrenadeDie;

	WP_TraceSetStart(ent, start, bolt->mins, bolt->maxs);//make sure our start point isn't on the other side of a wall

	float chargeAmount = 1.0f; // default of full charge

	if (ent->client)
	{
		chargeAmount = level.time - ent->client->ps.weaponChargeTime;
	}

	// get charge amount
	chargeAmount = chargeAmount / (float)attackData->mVelocity;

	if (chargeAmount > 1.0f)
	{
		chargeAmount = 1.0f;
	}
	else if (chargeAmount < TD_MIN_CHARGE)
	{
		chargeAmount = TD_MIN_CHARGE;
	}

	float	thrownSpeed = attackData->mVelocity;
	const qboolean thisIsAShooter = (qboolean)!Q_stricmp("misc_weapon_shooter", ent->classname);

	if (thisIsAShooter)
	{
		if (ent->delay != 0)
		{
			thrownSpeed = ent->delay;
		}
	}

	// normal ones bounce, alt ones explode on impact
	bolt->s.pos.trType = TR_GRAVITY;
	bolt->owner = ent;
	VectorScale(dir, thrownSpeed * chargeAmount, bolt->s.pos.trDelta);

	if (ent->health > 0)
	{
		bolt->s.pos.trDelta[2] += 120;

		if ((ent->NPC || (ent->s.number && thisIsAShooter))
			&& ent->enemy)
		{//NPC or misc_weapon_shooter
			//FIXME: we're assuming he's actually facing this direction...
			vec3_t	target;

			VectorCopy(ent->enemy->currentOrigin, target);
			if (target[2] <= start[2])
			{
				vec3_t	vec;
				VectorSubtract(target, start, vec);
				VectorNormalize(vec);
				VectorMA(target, Q_flrand(0, -32), vec, target);//throw a little short
			}

			target[0] += Q_flrand(-5, 5) + (Q_flrand(-1.0f, 1.0f) * (6 - ent->NPC->currentAim) * 2);
			target[1] += Q_flrand(-5, 5) + (Q_flrand(-1.0f, 1.0f) * (6 - ent->NPC->currentAim) * 2);
			target[2] += Q_flrand(-5, 5) + (Q_flrand(-1.0f, 1.0f) * (6 - ent->NPC->currentAim) * 2);

			WP_LobFire(ent, start, target, bolt->mins, bolt->maxs, bolt->clipmask, bolt->s.pos.trDelta, qtrue, ent->s.number, ent->enemy->s.number);
		}
		else if (thisIsAShooter && ent->target && !VectorCompare(ent->pos1, vec3_origin))
		{//misc_weapon_shooter firing at a position
			WP_LobFire(ent, start, ent->pos1, bolt->mins, bolt->maxs, bolt->clipmask, bolt->s.pos.trDelta, qtrue, ent->s.number, ent->enemy->s.number);
		}
	}

	if (attackIndex)
	{
		bolt->alt_fire = qtrue;
	}
	else
	{
		bolt->s.eFlags |= EF_BOUNCE_HALF;
	}

	bolt->s.loopSound = G_SoundIndex("sound/weapons/thermal/thermloop.wav");

	bolt->damage = weaponData[ent->s.weapon].attackData[0].damage * damageScale;
	bolt->dflags = 0;
	bolt->splashDamage = weaponData[ent->s.weapon].attackData[0].splashDamage * damageScale;
	bolt->splashRadius = weaponData[ent->s.weapon].attackData[0].splashRadius;

	bolt->s.eType = ET_MISSILE;
	bolt->svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = ent->s.weapon;

	if (attackIndex)
	{
		bolt->methodOfDeath = MOD_THERMAL_ALT;
		bolt->splashMethodOfDeath = MOD_THERMAL_ALT;//? SPLASH;
	}
	else
	{
		bolt->methodOfDeath = MOD_THERMAL;
		bolt->splashMethodOfDeath = MOD_THERMAL;//? SPLASH;
	}

	bolt->s.pos.trTime = level.time;		// move a bit on the very first frame
	VectorCopy(start, bolt->s.pos.trBase);

	SnapVector(bolt->s.pos.trDelta);			// save net bandwidth
	VectorCopy(start, bolt->currentOrigin);

	VectorCopy(start, bolt->pos2);

	return bolt;
}

//---------------------------------------------------------
gentity_t* WP_DropGrenade(gentity_t* ent, int attackIndex)
//---------------------------------------------------------
{
	AngleVectors(ent->client->ps.viewangles, forwardVec, vrightVec, up);
	CalcEntitySpot(ent, SPOT_WEAPON, muzzle);
	return (WP_FireGrenade(ent, attackIndex));
}
extern void WP_GrenadeDie(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod, int dFlags, int hitLoc) {
	WP_GrenadeExplode(self);
}


//---------------------------------------------------------
void WP_RocketThink(gentity_t* ent)
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* atkData = &wpnData->attackData[ent->attack_index];
	vec3_t newdir, targetdir,
		up = { 0,0,1 }, right;
	vec3_t	org;
	float dot, dot2;

	if (ent->disconnectDebounceTime && ent->disconnectDebounceTime < level.time)
	{//time's up, we're done, remove us
		if (ent->lockCount)
		{//explode when die
			WP_ExplosiveDie(ent, ent->owner, ent->owner, 0, MOD_UNKNOWN, 0, HL_NONE);
		}
		else
		{//just remove when die
			G_FreeEntity(ent);
		}
		return;
	}
	if (ent->enemy && ent->enemy->inuse)
	{
		float vel = (ent->spawnflags & 1) ? ent->speed : atkData->mVelocity;
		float newDirMult = ent->angle ? ent->angle * 2.0f : 1.0f;
		float oldDirMult = ent->angle ? (1.0f - ent->angle) * 2.0f : 1.0f;

		if ((ent->spawnflags & 1))
		{//vehicle rocket
			if (ent->enemy->client && ent->enemy->client->NPC_class == CLASS_VEHICLE)
			{//tracking another vehicle
				if (ent->enemy->client->ps.speed + ent->speed > vel)
				{
					vel = ent->enemy->client->ps.speed + ent->speed;
				}
			}
		}

		VectorCopy(ent->enemy->currentOrigin, org);
		org[2] += (ent->enemy->mins[2] + ent->enemy->maxs[2]) * 0.5f;

		if (ent->enemy->client)
		{
			switch (ent->enemy->client->NPC_class)
			{
			case CLASS_ATST:
				org[2] += 80;
				break;
			case CLASS_MARK1:
				org[2] += 40;
				break;
			case CLASS_PROBE:
				org[2] += 60;
				break;
			default:
				break;
			}
			if (!TIMER_Done(ent->enemy, "flee"))
			{
				TIMER_Set(ent->enemy, "rocketChasing", 500);
			}
		}

		VectorSubtract(org, ent->currentOrigin, targetdir);
		VectorNormalize(targetdir);

		// Now the rocket can't do a 180 in space, so we'll limit the turn to about 45 degrees.
		dot = DotProduct(targetdir, ent->movedir);

		// a dot of 1.0 means right-on-target.
		if (dot < 0.0f)
		{
			// Go in the direction opposite, start a 180.
			CrossProduct(ent->movedir, up, right);
			dot2 = DotProduct(targetdir, right);

			if (dot2 > 0)
			{
				// Turn 45 degrees right.
				VectorMA(ent->movedir, 0.3f * newDirMult, right, newdir);
			}
			else
			{
				// Turn 45 degrees left.
				VectorMA(ent->movedir, -0.3f * newDirMult, right, newdir);
			}

			// Yeah we've adjusted horizontally, but let's split the difference vertically, so we kinda try to move towards it.
			newdir[2] = ((targetdir[2] * newDirMult) + (ent->movedir[2] * oldDirMult)) * 0.5;

			// slowing down coupled with fairly tight turns can lead us to orbit an enemy..looks bad so don't do it!
//			vel *= 0.5f;
		}
		else if (dot < 0.70f)
		{
			// Still a bit off, so we turn a bit softer
			VectorMA(ent->movedir, 0.5f * newDirMult, targetdir, newdir);
		}
		else
		{
			// getting close, so turn a bit harder
			VectorMA(ent->movedir, 0.9f * newDirMult, targetdir, newdir);
		}

		// add crazy drunkenness
		for (int i = 0; i < 3; i++)
		{
			newdir[i] += Q_flrand(-1.0f, 1.0f) * ent->random * 0.25f;
		}

		// decay the randomness
		ent->random *= 0.9f;

		if (ent->enemy->client
			&& ent->enemy->client->ps.groundEntityNum != ENTITYNUM_NONE)
		{//tracking a client who's on the ground, aim at the floor...?
			// Try to crash into the ground if we get close enough to do splash damage
			float dis = Distance(ent->currentOrigin, org);

			if (dis < 128)
			{
				// the closer we get, the more we push the rocket down, heh heh.
				newdir[2] -= (1.0f - (dis / 128.0f)) * 0.6f;
			}
		}

		VectorNormalize(newdir);

		VectorScale(newdir, vel * 0.5f, ent->s.pos.trDelta);
		VectorCopy(newdir, ent->movedir);
		SnapVector(ent->s.pos.trDelta);			// save net bandwidth
		VectorCopy(ent->currentOrigin, ent->s.pos.trBase);
		ent->s.pos.trTime = level.time;
	}

	ent->nextthink = level.time + 100;
	return;
}

// NOTE: this is 100% for the demp2 alt-fire effect, so changes to the visual effect will affect game side demp2 code
//--------------------------------------------------
void DEMP2_AltRadiusDamage(gentity_t* ent)
{
	float		frac = (level.time - ent->fx_time) / 1300.0f; // synchronize with demp2 effect
	float		dist, radius;
	gentity_t* gent;
	gentity_t* entityList[MAX_GENTITIES];
	int			numListedEntities, i, e;
	vec3_t		mins, maxs;
	vec3_t		v, dir;

	frac *= frac * frac; // yes, this is completely ridiculous...but it causes the shell to grow slowly then "explode" at the end

	radius = frac * 200.0f; // 200 is max radius...the model is aprox. 100 units tall...the fx draw code mults. this by 2.

	for (i = 0; i < 3; i++)
	{
		mins[i] = ent->currentOrigin[i] - radius;
		maxs[i] = ent->currentOrigin[i] + radius;
	}

	numListedEntities = gi.EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

	for (e = 0; e < numListedEntities; e++)
	{
		gent = entityList[e];

		if (!gent->takedamage || !gent->contents)
		{
			continue;
		}

		// find the distance from the edge of the bounding box
		for (i = 0; i < 3; i++)
		{
			if (ent->currentOrigin[i] < gent->absmin[i])
			{
				v[i] = gent->absmin[i] - ent->currentOrigin[i];
			}
			else if (ent->currentOrigin[i] > gent->absmax[i])
			{
				v[i] = ent->currentOrigin[i] - gent->absmax[i];
			}
			else
			{
				v[i] = 0;
			}
		}

		// shape is an ellipsoid, so cut vertical distance in half`
		v[2] *= 0.5f;

		dist = VectorLength(v);

		if (dist >= radius)
		{
			// shockwave hasn't hit them yet
			continue;
		}

		if (dist < ent->radius)
		{
			// shockwave has already hit this thing...
			continue;
		}

		VectorCopy(gent->currentOrigin, v);
		VectorSubtract(v, ent->currentOrigin, dir);

		// push the center of mass higher than the origin so players get knocked into the air more
		dir[2] += 12;

		G_Damage(gent, ent, ent->owner, dir, ent->currentOrigin, weaponData[ent->s.weapon].attackData[1].damage, DAMAGE_DEATH_KNOCKBACK, ent->splashMethodOfDeath);
		if (gent->takedamage && gent->client)
		{
			gent->s.powerups |= (1 << PW_SHOCKED);
			gent->client->ps.powerups[PW_SHOCKED] = level.time + 2000;
			Saboteur_Decloak(gent, Q_irand(3000, 10000));
		}
	}

	// store the last fraction so that next time around we can test against those things that fall between that last point and where the current shockwave edge is
	ent->radius = radius;

	if (frac < 1.0f)
	{
		// shock is still happening so continue letting it expand
		ent->nextthink = level.time + 50;
	}
}

//---------------------------------------------------------
void DEMP2_AltDetonate(gentity_t* ent)
//---------------------------------------------------------
{
	G_SetOrigin(ent, ent->currentOrigin);

	// start the effects, unfortunately, I wanted to do some custom things that I couldn't easily do with the fx system, so part of it uses an event and localEntities
	G_PlayEffect("demp2/altDetonate", ent->currentOrigin, ent->pos1);
	G_AddEvent(ent, EV_DEMP2_ALT_IMPACT, ent->count * 2);

	ent->fx_time = level.time;
	ent->radius = 0;
	ent->nextthink = level.time + 50;
	ent->e_ThinkFunc = thinkF_DEMP2_AltRadiusDamage;
	ent->s.eType = ET_GENERAL; // make us a missile no longer
}