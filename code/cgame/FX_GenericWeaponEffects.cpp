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

// Heavy Repeater Weapon

#include "cg_headers.h"

#include "cg_media.h"
#include "FxScheduler.h"


/*
------------------------
FX Walls Impacts
------------------------
*/
void FX_GenericBlasterHitWall(gentity_t *gent,int weapon, vec3_t origin, vec3_t normal )
{
	weaponInfo_t* wpnInfo = &cg_weapons[weapon];
	weaponAttackInfo_t *attackData = &wpnInfo->weaponAttacksInfo[gent->attack_index];
	if (attackData->hitWallEffect) 
	{
		theFxScheduler.PlayEffect(attackData->hitWallEffect, origin, normal);
	}
	else 
	{
		theFxScheduler.PlayEffect(cgs.effects.blasterWallImpactEffect, origin, normal);
	}
}

void FX_GenericChargedBlasterHitWall(gentity_t* gent, int weapon, vec3_t origin, vec3_t normal)
{
	weaponData_t* wpnData = &weaponData[weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[gent->attack_index];

	weaponInfo_t* wpnInfo = &cg_weapons[weapon];
	weaponAttackInfo_t* attackFxData = &wpnInfo->weaponAttacksInfo[gent->attack_index];

	float power = gent->count / attackData->maxChargeUnits;

	if (power <= 0.33 && attackFxData->hitWallEffect)
	{
		theFxScheduler.PlayEffect(attackFxData->hitWallEffect, origin, normal);
		return;
	}
	if (power <= 0.33)
	{
		theFxScheduler.PlayEffect(cgs.effects.bryarWallImpactEffect, origin, normal);
		return;
	}
	if (power <= 0.66 && attackFxData->hitWallEffect2)
	{
		theFxScheduler.PlayEffect(attackFxData->hitWallEffect2, origin, normal);
		return;
	}
	if (power <= 0.66)
	{
		theFxScheduler.PlayEffect(cgs.effects.bryarWallImpactEffect2, origin, normal);
		return;
	}
	if (attackFxData->hitWallEffect3)
	{
		theFxScheduler.PlayEffect(attackFxData->hitWallEffect3, origin, normal);
		return;
	}
	theFxScheduler.PlayEffect(cgs.effects.bryarWallImpactEffect3, origin, normal);
}

void FX_GenericExplosion(gentity_t* gent, int weapon, vec3_t origin, vec3_t normal, qboolean humanoid) {
	if (weaponData[weapon].attackData[gent->attack_index].explosionEffect[0])
	{
		theFxScheduler.PlayEffect(weaponData[weapon].attackData[gent->attack_index].explosionEffect, origin, normal);
	}
	else
	{
		theFxScheduler.PlayEffect("thermal/explosion", origin, normal);
	}

	if (weaponData[weapon].attackData[gent->attack_index].shockwaveEffect[0])
	{
		theFxScheduler.PlayEffect(weaponData[weapon].attackData[gent->attack_index].shockwaveEffect, origin);
	}
	else
	{
		theFxScheduler.PlayEffect("thermal/shockwave", origin);
	}
}
/*
------------------------
FX Fleshs Impacts
------------------------
*/
void FX_GenericBlasterHitPlayer(gentity_t* gent, int weapon, vec3_t origin, vec3_t normal,gentity_t *hit,qboolean humanoid)
{
	//temporary? just testing out the damage skin stuff -rww
	/*if (hit && hit->client && hit->ghoul2.size())
	{
		CG_AddGhoul2Mark(cgs.media.bdecal_burnmark1, flrand(3.5, 4.0), origin, normal, hit->s.number,
			hit->client->ps.origin, hit->client->renderInfo.legsYaw, hit->ghoul2, hit->s.modelScale, Q_irand(10000, 13000));
	}*/

	weaponInfo_t* wpnInfo = &cg_weapons[weapon];
	weaponAttackInfo_t* attackData = &wpnInfo->weaponAttacksInfo[gent->attack_index];
	if (!humanoid && attackData->hitDroidEffect) {
		theFxScheduler.PlayEffect(attackData->hitDroidEffect, origin, normal);
	}
	else if (attackData->hitFleshEffect)
	{
		theFxScheduler.PlayEffect(attackData->hitFleshEffect, origin, normal);
	}
	else
	{
		theFxScheduler.PlayEffect(cgs.effects.blasterFleshImpactEffect, origin, normal);
	}
}


/*
------------------------
Generic Beam Effect
------------------------
*/
static vec3_t WHITE = { 1.0f,1.0f,1.0f };
static vec3_t YELLER = { 0.8f,0.7f,0.0f };
void FX_GenericBeam(vec3_t start, vec3_t end, gentity_t *gent)
{
	weaponAttackData_t* attackData = &weaponData[gent->s.weapon].attackData[gent->attack_index];
	qhandle_t shaderHandle;
	if (attackData->beamShader[0])
	{
		shaderHandle = cgi_R_RegisterShader(attackData->beamShader);
	}
	else 
	{
		shaderHandle = cgi_R_RegisterShader("gfx/effects/redLine");
	}
	vec3_t color;
	if (attackData->beamColor) {
		VectorCopy(attackData->beamColor,color);
	}
	else
	{
		VectorCopy(WHITE, color);
	}
	FX_AddLine(-1, start, end, 0.1f, 10.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		color, color, 0.0f,
		175, shaderHandle,
		0, FX_SIZE_LINEAR | FX_ALPHA_LINEAR);

	if (gent->count || attackData->firingLogic == FL_FULL_BEAM)
	{
		if (attackData->fullBeamShader[0])
		{
			shaderHandle = cgi_R_RegisterShader(attackData->fullBeamShader);
		}
		else
		{
			shaderHandle = cgi_R_RegisterShader("gfx/misc/whiteline2");
		}
		if (attackData->fullBeamColor) {
			VectorCopy(attackData->fullBeamColor, color);
		}
		else
		{
			VectorCopy(YELLER, color);
		}
		// add some beef
		FX_AddLine(-1, start, end, 0.1f, 7.0f, 0.0f,
			1.0f, 0.0f, 0.0f,
			color, color, 0.0f,
			150, shaderHandle,
			0, FX_SIZE_LINEAR | FX_ALPHA_LINEAR);
	}
}