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

// g_weaponLoad.cpp
// fills in memory struct with ext_dat\weapons.dat

// this is excluded from PCH usage 'cos it looks kinda scary to me, being game and ui.... -Ste
#include "g_local.h"
#include "g_weaponLoad.h"

// FIXME :What are the right values?
#define MAX_FIRETIME 10000
#define MAX_RANGE 10000
#define MAX_AMMO_STORAGE 1000
#define MAX_BARREL_COUNT 4

static void ParseInt(const char** holdBuf, int* dest)
{
	int		tokenInt;

	if (COM_ParseInt(holdBuf, &tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	*dest = tokenInt;
}

static void ParseIntWithLims(const char **holdBuf, int* dest, int min, int max, char* identifier)
{
	int		tokenInt;

	if ( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < min ) || (tokenInt > max ))
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad %s in external weapon data '%d'\n", identifier, tokenInt);
		return;
	}

	*dest = tokenInt;
}

static void ParseFlt(const char** holdBuf, float* dest)
{
	float	tokenFlt;

	if (COM_ParseFloat(holdBuf, &tokenFlt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	*dest = tokenFlt;
}

//--------------------------------------------
void ParseStr(const char **holdBuf, char* dest, int maxLen, char* identifier)
{
	int len;
	const char*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > maxLen)
	{
		len = maxLen;
		gi.Printf(S_COLOR_YELLOW"WARNING: %s too long in external WEAPONS.DAT '%s'\n", identifier, tokenStr);
	}

	Q_strncpyz(dest,tokenStr,len);
}

//--------------------------------------------

void WPN_FuncSkip(const char** holdBuf)
{
	SkipRestOfLine(holdBuf);
}

//--------------------------------------------
void WPN_WeaponClass(const char **holdBuf)
{
	int i;
	char weaponClass[32];
	int weaponNum;

	ParseStr(holdBuf, weaponClass, 32, "weaponclass");

	for (i = 0;i < numHcWeaponIndexes;i++)
	{
		if (!Q_stricmp(weaponClass, _weaponIndexes[i].weaponClass))
		{
			weaponNum = _weaponIndexes[i].index;
			break;
		}
	}
	if (i == numHcWeaponIndexes)	// !Find parameter
	{
		if (weaponCount >= MAX_WEAPONS) {
			weaponNum = 0;
			gi.Printf(S_COLOR_YELLOW"WARNING: too many weapons in external weapon data(%d); Parsing '%s'\n", MAX_WEAPONS, weaponClass);
		}
		weaponNum = weaponCount;
		weaponCount++;
	}

	wpnParms.weaponNum = weaponNum;

	Q_strncpyz(weaponData[weaponNum].classname, weaponClass, 32);

	weaponIndexes[weaponNum].index = weaponNum;
}


//--------------------------------------------
void WPN_WeaponModel(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].weaponMdl, 64, "weaponMdl");
}

//--------------------------------------------
void WPN_WeaponIcon(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].weaponIcon, 64, "weaponIcon");
}

//--------------------------------------------
void WPN_AmmoType(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].ammoIndex, AMMO_NONE, AMMO_MAX - 1, "Ammotype");
}

//--------------------------------------------
void WPN_AmmoLowCnt(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].ammoLow, 0, 200, "Ammolowcount");
	// FIXME :What are the right values?
}

//--------------------------------------------
void WPN_FiringSnd(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[0].firingSnd, 64, "firingSnd");
}

//--------------------------------------------
void WPN_AltFiringSnd( const char **holdBuf )
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[1].firingSnd, 64, "altFiringSnd");
}

//--------------------------------------------
void WPN_StopSnd( const char **holdBuf )
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].stopSnd, 64, "stopSnd");
}

//--------------------------------------------
void WPN_ChargeSnd(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[0].chargeSnd, 64, "chargeSnd");
}

//--------------------------------------------
void WPN_AltChargeSnd(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[1].chargeSnd, 64, "altChargeSnd");
}

//--------------------------------------------
void WPN_SelectSnd( const char **holdBuf )
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].selectSnd, 64, "selectSnd");
}

//--------------------------------------------
void WPN_FireTime(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[0].fireTime, 0, MAX_FIRETIME, "Firetime");
}

//--------------------------------------------
void WPN_Range(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[0].range, 0, MAX_RANGE, "Range");
}

//--------------------------------------------
void WPN_EnergyPerShot(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[0].energyPerShot, 0, MAX_AMMO_STORAGE, "EnergyPerShot");
}

//--------------------------------------------
void WPN_AltFireTime(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[1].fireTime, 0, MAX_FIRETIME, "altFireTime");
}

//--------------------------------------------
void WPN_AltRange(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[1].range, 0, MAX_RANGE, "altRange");
}

//--------------------------------------------
void WPN_AltEnergyPerShot(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[1].energyPerShot, 0, MAX_AMMO_STORAGE, "altEnergyPerShot");
}

//--------------------------------------------
void WPN_Ammo(const char **holdBuf)
{
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	if (!Q_stricmp(tokenStr,"AMMO_NONE"))
		wpnParms.ammoNum = AMMO_NONE;
	else if (!Q_stricmp(tokenStr,"AMMO_FORCE"))
		wpnParms.ammoNum = AMMO_FORCE;
	else if (!Q_stricmp(tokenStr,"AMMO_BLASTER"))
		wpnParms.ammoNum = AMMO_BLASTER;
	else if (!Q_stricmp(tokenStr,"AMMO_POWERCELL"))
		wpnParms.ammoNum = AMMO_POWERCELL;
	else if (!Q_stricmp(tokenStr,"AMMO_METAL_BOLTS"))
		wpnParms.ammoNum = AMMO_METAL_BOLTS;
	else if (!Q_stricmp(tokenStr,"AMMO_ROCKETS"))
		wpnParms.ammoNum = AMMO_ROCKETS;
	else if (!Q_stricmp(tokenStr,"AMMO_EMPLACED"))
		wpnParms.ammoNum = AMMO_EMPLACED;
	else if (!Q_stricmp(tokenStr,"AMMO_THERMAL"))
		wpnParms.ammoNum = AMMO_THERMAL;
	else if (!Q_stricmp(tokenStr,"AMMO_TRIPMINE"))
		wpnParms.ammoNum = AMMO_TRIPMINE;
	else if (!Q_stricmp(tokenStr,"AMMO_DETPACK"))
		wpnParms.ammoNum = AMMO_DETPACK;
	else
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad ammotype in external weapon data '%s'\n", tokenStr);
		wpnParms.ammoNum = 0;
	}
}

//--------------------------------------------
void WPN_AmmoIcon(const char **holdBuf)
{
	ParseStr(holdBuf, ammoData[wpnParms.ammoNum].icon, 64, "ammoicon");
}

//--------------------------------------------
void WPN_AmmoMax(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &ammoData[wpnParms.ammoNum].max, 0, MAX_AMMO_STORAGE, "Ammo Max");
}

//--------------------------------------------
void WPN_BarrelCount(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].numBarrels, 0, MAX_BARREL_COUNT, "Barrel Count");
}

//--------------------------------------------
static void WP_ParseWeaponParms(const char **holdBuf)
{
	const char	*token;
	size_t	i;

	while (holdBuf)
	{
		token = COM_ParseExt( holdBuf, qtrue );

		if (!Q_stricmp( token, "}" ))	// End of data for this weapon
			break;
		// Loop through possible parameters
		for (i=0;i<numWpnParms;++i)
		{
			if (!Q_stricmp(token,WpnParms[i].parmName))
			{
				WpnParms[i].func(holdBuf);
				break;
			}
		}

		if (i < numWpnParms)	// Find parameter???
		{
			continue;
		}
		Com_Printf("^3WARNING: bad parameter in external weapon data '%s'\n", token);
	}
}

//--------------------------------------------
void WPN_MissileName(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[0].missileMdl, 64, "MissileName");
}

//--------------------------------------------
void WPN_AltMissileName(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[1].missileMdl, 64, "AltMissileName");
}

//--------------------------------------------
void WPN_MissileHitSound(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[0].missileHitSound, 64, "MissileHitSound");
}

//--------------------------------------------
void WPN_AltMissileHitSound(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[1].missileHitSound, 64, "AltMissileHitSound");
}

//--------------------------------------------
void WPN_MissileSound(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[0].missileSound, 64, "MissileSound");
}

//--------------------------------------------
void WPN_AltMissileSound(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[1].missileSound, 64, "AltMissileSound");
}

//--------------------------------------------
void WPN_MissileLightColor(const char **holdBuf)
{
	int i;
	float	tokenFlt;

	for (i=0;i<3;++i)
	{
		if ( COM_ParseFloat(holdBuf,&tokenFlt))
		{
			SkipRestOfLine(holdBuf);
			continue;
		}

		if ((tokenFlt < 0) || (tokenFlt > 1 ))
		{
			gi.Printf(S_COLOR_YELLOW"WARNING: bad missilelightcolor in external weapon data '%f'\n", tokenFlt);
			continue;
		}
		weaponData[wpnParms.weaponNum].attackData[0].missileDlightColor[i] = tokenFlt;
	}

}

//--------------------------------------------
void WPN_AltMissileLightColor(const char **holdBuf)
{
	int i;
	float	tokenFlt;

	for (i=0;i<3;++i)
	{
		if ( COM_ParseFloat(holdBuf,&tokenFlt))
		{
			SkipRestOfLine(holdBuf);
			continue;
		}

		if ((tokenFlt < 0) || (tokenFlt > 1 ))
		{
			gi.Printf(S_COLOR_YELLOW"WARNING: bad altmissilelightcolor in external weapon data '%f'\n", tokenFlt);
			continue;
		}
		weaponData[wpnParms.weaponNum].attackData[1].missileDlightColor[i] = tokenFlt;
	}

}


//--------------------------------------------
void WPN_MissileLight(const char **holdBuf)
{
	float	tokenFlt;

	if ( COM_ParseFloat(holdBuf,&tokenFlt))
	{
		SkipRestOfLine(holdBuf);
	}

	if ((tokenFlt < 0) || (tokenFlt > 255 )) // FIXME :What are the right values?
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad missilelight in external weapon data '%f'\n", tokenFlt);
	}
	weaponData[wpnParms.weaponNum].attackData[0].missileDlight = tokenFlt;
}

//--------------------------------------------
void WPN_AltMissileLight(const char **holdBuf)
{
	float	tokenFlt;

	if ( COM_ParseFloat(holdBuf,&tokenFlt))
	{
		SkipRestOfLine(holdBuf);
	}

	if ((tokenFlt < 0) || (tokenFlt > 255 )) // FIXME :What are the right values?
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad altmissilelight in external weapon data '%f'\n", tokenFlt);
	}
	weaponData[wpnParms.weaponNum].attackData[1].missileDlight = tokenFlt;
}


//--------------------------------------------
void WPN_FuncName(const char **holdBuf)
{
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}
	size_t len = strlen(tokenStr);

	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: FuncName '%s' too long in external WEAPONS.DAT\n", tokenStr);
	}

	for ( func_t* s=funcs ; s->name ; s++ ) {
		if ( !Q_stricmp(s->name, tokenStr) ) {
			// found it
			weaponData[wpnParms.weaponNum].attackData[0].missileFunc = (void*)s->func;
			return;
		}
	}
	gi.Printf(S_COLOR_YELLOW"WARNING: FuncName '%s' in external WEAPONS.DAT does not exist\n", tokenStr);
}


//--------------------------------------------
void WPN_AltFuncName(const char **holdBuf)
{
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	size_t len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: AltFuncName '%s' too long in external WEAPONS.DAT\n", tokenStr);
	}

	for ( func_t* s=funcs ; s->name ; s++ ) {
		if ( !Q_stricmp(s->name, tokenStr) ) {
			// found it
			weaponData[wpnParms.weaponNum].attackData[1].missileFunc = (void*)s->func;
			return;
		}
	}
	gi.Printf(S_COLOR_YELLOW"WARNING: AltFuncName %s in external WEAPONS.DAT does not exist\n", tokenStr);
}

//--------------------------------------------
void WPN_MuzzleEffect(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[0].mMuzzleEffect, 64, "MuzzleEffect");
	G_EffectIndex( weaponData[wpnParms.weaponNum].attackData[0].mMuzzleEffect );
}

//--------------------------------------------
void WPN_AltMuzzleEffect(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[1].mMuzzleEffect, 64, "AltMuzzleEffect");
	G_EffectIndex( weaponData[wpnParms.weaponNum].attackData[1].mMuzzleEffect );
}

//--------------------------------------------
void WPN_ProjectileEffect(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[0].projectileEffect, 64, "projectileEffect");
	G_EffectIndex( weaponData[wpnParms.weaponNum].attackData[0].projectileEffect );
}

//--------------------------------------------
void WPN_AltProjectileEffect(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[1].projectileEffect, 64, "alt_projectileEffect");
	G_EffectIndex( weaponData[wpnParms.weaponNum].attackData[1].projectileEffect );
}
//--------------------------------------------
void WPN_ExplosionEffect(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].explosionEffect, 64, "alt_projectileEffect");
	G_EffectIndex( weaponData[wpnParms.weaponNum].explosionEffect );
}
//--------------------------------------------
void WPN_ShockwaveEffect(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].shockwaveEffect, 64, "alt_projectileEffect");
	G_EffectIndex( weaponData[wpnParms.weaponNum].shockwaveEffect );
}

//--------------------------------------------
void WPN_TertiaryMuzzleEffect(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].mTertiaryMuzzleEffect, 64, "TertiaryMuzzleEffect");
	G_EffectIndex( weaponData[wpnParms.weaponNum].mTertiaryMuzzleEffect );
}

//--------------------------------------------
void WPN_ChargeMuzzleEffect(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].chargeMuzzleShader, 64, "chargeMuzzleEffect");
	G_EffectIndex( weaponData[wpnParms.weaponNum].chargeMuzzleShader );
}

//--------------------------------------------

void WPN_Damage(const char **holdBuf)
{
	ParseInt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[0].damage);
	weaponData[wpnParms.weaponNum].defaultDamage = weaponData[wpnParms.weaponNum].attackData[0].damage;
}

//--------------------------------------------

void WPN_AltDamage(const char **holdBuf)
{
	ParseInt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[1].damage);
}

//--------------------------------------------

void WPN_SplashDamage(const char **holdBuf)
{
	ParseInt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[0].splashDamage);
}

//--------------------------------------------

void WPN_SplashRadius(const char **holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[0].splashRadius);
}

//--------------------------------------------

void WPN_AltSplashDamage(const char **holdBuf)
{
	ParseInt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[1].splashDamage);
}

//--------------------------------------------

void WPN_AltSplashRadius(const char **holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[1].splashRadius);
}

//--------------------------------------------
void WPN_TertiaryEnergyPerShot(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].tertiaryEnergyPerShot, 0, MAX_AMMO_STORAGE, "tertiaryEnergyPerShot");
}

//--------------------------------------------
void WPN_TertiaryFireTime(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].tertiaryFireTime, 0, MAX_FIRETIME, "tertiaryFireTime");
}

//--------------------------------------------
void WPN_TertiaryRange(const char **holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].tertiaryRange, 0, MAX_RANGE, "tertiaryRange");
}

//--------------------------------------------
void WPN_ScopeType(const char **holdBuf)
{
    int        tokenInt;

    if ( COM_ParseInt(holdBuf,&tokenInt))
    {
        SkipRestOfLine(holdBuf);
        return;
    }

	// This is for cg.zoommode.
	tokenInt += 3;

    if ((tokenInt < ST_A280) || (tokenInt > ST_E5 ))
    {
        gi.Printf(S_COLOR_YELLOW"WARNING: bad scopeType in external weapon data '%d'\n", tokenInt);
        return;
    }
    weaponData[wpnParms.weaponNum].scopeType = tokenInt;
}

//--------------------------------------------
void WPN_MainFireOptions(const char **holdBuf)
{
	int i;
	int tokenInt;

	for (i = 0; i < 3; i++)
	{
		if (COM_ParseInt(holdBuf, &tokenInt))
		{
			SkipRestOfLine(holdBuf);
			continue;
		}

		if ((tokenInt < 0) || (tokenInt > 10000 ))
		{
			gi.Printf(S_COLOR_YELLOW"WARNING: bad mainfireopt in external weapon data '%d'\n", tokenInt);
			continue;
		}

		weaponData[wpnParms.weaponNum].mainFireOpt[i] = tokenInt;
	}
}

//--------------------------------------------
void WPN_AltFireOptions(const char **holdBuf)
{
	int i;
	int tokenInt;

	for (i = 0; i < 3; i++)
	{
		if (COM_ParseInt(holdBuf, &tokenInt))
		{
			SkipRestOfLine(holdBuf);
			continue;
		}

		if ((tokenInt < 0) || (tokenInt > 10000 ))
		{
			gi.Printf(S_COLOR_YELLOW"WARNING: bad altfireopt in external weapon data '%d'\n", tokenInt);
			continue;
		}

		weaponData[wpnParms.weaponNum].altFireOpt[i] = tokenInt;
	}
}

//--------------------------------------------
void WPN_TertiaryFireOptions(const char **holdBuf)
{
	int i;
	int tokenInt;

	for (i = 0; i < 3; i++)
	{
		if (COM_ParseInt(holdBuf, &tokenInt))
		{
			SkipRestOfLine(holdBuf);
			continue;
		}

		if ((tokenInt < 0) || (tokenInt > 10000 ))
		{
			gi.Printf(S_COLOR_YELLOW"WARNING: bad tertiaryFireOpt in external weapon data '%d'\n", tokenInt);
			continue;
		}

		weaponData[wpnParms.weaponNum].tertiaryFireOpt[i] = tokenInt;
	}
}

//--------------------------------------------
void WPN_WeaponModel2(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].weaponMdl2, 64, "weaponMdl2");
}

//--------------------------------------------
void WPN_BaseWeapon(const char **holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].baseclass, 32, "Base weapon");
}

//--------------------------------------------

void WPN_Velocity(const char** holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[0].mVelocity);
}

//--------------------------------------------

void WPN_AltVelocity(const char** holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[1].mVelocity);
}

//--------------------------------------------

void WPN_Spread(const char** holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[0].spread);
}

//--------------------------------------------

void WPN_AltSpread(const char** holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[1].spread);
}

//--------------------------------------------

void WPN_PlayerUsable(const char** holdBuf)
{
	int tokenInt;

	if (COM_ParseInt(holdBuf, &tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].playerUsable = (qboolean) tokenInt;
}

//--------------------------------------------

void WPN_WeaponCategory(const char** holdBuf)
{
	const char* tokenStr;
	weaponCategory_t weaponCategory = WC_NONE;

	if (COM_ParseString(holdBuf, &tokenStr))
	{
		return;
	}

	if (!Q_stricmp(tokenStr, "WC_NONE")) {
		weaponCategory = WC_NONE;
	}
	else if (!Q_stricmp(tokenStr, "WC_PISTOL")) {
		weaponCategory = WC_PISTOL;
	}
	else if (!Q_stricmp(tokenStr, "WC_LIGHT")) {
		weaponCategory = WC_LIGHT;
	}
	else if (!Q_stricmp(tokenStr, "WC_HEAVY")) {
		weaponCategory = WC_HEAVY;
	}
	else if (!Q_stricmp(tokenStr, "WC_GRENADE")) {
		weaponCategory = WC_EXPLOSIVE;
	}
	else if (!Q_stricmp(tokenStr, "WC_EXPLOSIVE")) {
		weaponCategory = WC_EXPLOSIVE;
	}
	else if (!Q_stricmp(tokenStr, "WC_MINIGUN")) {
		weaponCategory = WC_MINIGUN;
	}
	else {
		weaponCategory = WC_NONE;
		gi.Printf(S_COLOR_YELLOW"WARNING: Invalid value %s for WeaponBucket in external WEAPONS.DAT\n", tokenStr);
	}
	weaponData[wpnParms.weaponNum].weaponCategory = weaponCategory;
}

void WPN_WeaponBucket(const char** holdBuf)
{
	const char* tokenStr;
	weaponBucket_t weaponBucket = WB_OTHERS;

	if (COM_ParseString(holdBuf, &tokenStr))
	{
		return;
	}

	if (!Q_stricmp(tokenStr, "WB_PISTOLS")) {
		weaponBucket = WB_PISTOLS;
	}
	else if (!Q_stricmp(tokenStr, "WB_BLASTERS")) {
		weaponBucket = WB_BLASTERS;
	}
	else if (!Q_stricmp(tokenStr, "WB_SPECIALISTS")) {
		weaponBucket = WB_SPECIALISTS;
	}
	else if (!Q_stricmp(tokenStr, "WB_HEAVY_WEAPONS")) {
		weaponBucket = WB_HEAVY_WEAPONS;
	}
	else if (!Q_stricmp(tokenStr, "WB_MELEE")) {
		weaponBucket = WB_MELEE;
	}
	else if (!Q_stricmp(tokenStr, "WB_THROWABLES")) {
		weaponBucket = WB_THROWABLES;
	}
	else if (!Q_stricmp(tokenStr, "WB_OTHERS")) {
		weaponBucket = WB_OTHERS;
	}
	else {
		gi.Printf(S_COLOR_YELLOW"WARNING: Invalid value %s for WeaponCategory in external WEAPONS.DAT\n", tokenStr);
	}
	weaponData[wpnParms.weaponNum].weaponBucket = weaponBucket;
}

//--------------------------------------------
static void WP_ParseParms(const char *buffer)
{
	const char	*holdBuf;
	const char	*token;

	holdBuf = buffer;
	COM_BeginParseSession();

	while ( holdBuf )
	{
		token = COM_ParseExt( &holdBuf, qtrue );

		if ( !Q_stricmp( token, "{" ) )
		{
			WP_ParseWeaponParms(&holdBuf);
		}

	}

	COM_EndParseSession(  );

}

//--------------------------------------------
void WP_LoadWeaponParms (void)
{
	char *buffer;
	int len;

	len = gi.FS_ReadFile("ext_data/weapons.dat",(void **) &buffer);

	if (len == -1)
	{
		Com_Error(ERR_FATAL,"Cannot find ext_data/weapons.dat!\n");
	}

	// initialise the data area
	memset(weaponData, 0, sizeof(weaponData));
	memset(weaponIndexes, 0, sizeof(weaponIndexes));

	// initialise the weapon Count;
	weaponCount = numHcWeaponIndexes;


	// put in the default values, because backwards compatibility is awesome!
	for(int i = 0; i <  numHcWeaponIndexes; i++)
	{
		weaponData[i].attackData[0].damage = defaultDamage[i];
		weaponData[i].defaultDamage = defaultDamage[i];
		weaponData[i].attackData[1].damage = defaultAltDamage[i];
		weaponData[i].attackData[0].splashDamage = defaultSplashDamage[i];
		weaponData[i].attackData[1].splashDamage = defaultAltSplashDamage[i];
		weaponData[i].attackData[0].splashRadius = defaultSplashRadius[i];
		weaponData[i].attackData[1].splashRadius = defaultAltSplashRadius[i];
		weaponData[i].playerUsable = defaultPlayerUsable[i];
		weaponData[i].attackData[0].mVelocity = defaultsWeaponSpeed[i][0];
		weaponData[i].attackData[1].mVelocity = defaultsWeaponSpeed[i][1];
		weaponData[i].attackData[0].spread = defaultsWeaponSpread[i][0];
		weaponData[i].attackData[1].spread = defaultsWeaponSpread[i][1];
		weaponData[i].weaponCategory = defaultWeaponType[i];
		weaponData[i].weaponBucket = defaultWeaponBucket[i];
		strcpy(weaponData[i].classname, _weaponIndexes[i].weaponClass);
	}
	//put in the qunset flag for playerUsable since 0 = false;
	for (int i = numHcWeaponIndexes; i < MAX_WEAPONS ; i++) {
		weaponData[i].playerUsable = qunset;
		weaponData[i].weaponCategory = WC_NONE;
		weaponData[i].weaponBucket = WB_UNSET;

	}
	WP_ParseParms(buffer);

	gi.FS_FreeFile( buffer );	//let go of the buffer

	//Read files containing Dynamics Weapons
	char*	holdChar;
	char	weaponFileList[2048];			//	The list of file names read in
	int		fileCount;
	int		fileNameSize;

	//Read all the externals file
	fileCount = gi.FS_GetFileList("ext_data/", ".wpn", weaponFileList, sizeof(weaponFileList));

	holdChar = weaponFileList;

	Com_Printf("Found %d External files\n", fileCount);
	for (int i = 0; i < fileCount; i++, holdChar += fileNameSize + 1)
	{
		fileNameSize = strlen(holdChar);

		Com_Printf( "Parsing %s\n", holdChar );
		char* fileBuffer;
		int fileLen;

		fileLen = gi.FS_ReadFile(va("ext_data/%s",holdChar), (void**)&fileBuffer);

		if (fileLen == -1)
		{
			Com_Printf("Error reading file\n");
		}
		else
		{
			WP_ParseParms(fileBuffer);

			gi.FS_FreeFile(fileBuffer);	//let go of the buffer
		}
	}

	//Get unset data from each weapon which is a copy of another;
	for (int i = 0; i <  weaponCount; i++) {
		if (!weaponData[i].baseclass[0]) {
			continue;
		}
		bool found = false;
		for (int j = 0; j < weaponCount; j++) {
			if (i == j || Q_stricmp(weaponData[i].baseclass,weaponData[j].classname)) {
				continue;
			}
			found = true;
			//Copying the weapon Strings from one to another
			if (weaponData[i].weaponMdl[0] == 0) {
				strcpy(weaponData[i].weaponMdl,weaponData[j].weaponMdl);
			}
			if (weaponData[i].stopSnd[0] == 0) {
				strcpy(weaponData[i].stopSnd,weaponData[j].stopSnd);
			}
			if (weaponData[i].selectSnd[0] == 0) {
				strcpy(weaponData[i].selectSnd,weaponData[j].selectSnd);
			}
			if(weaponData[i].weaponIcon[0] == 0){
				strcpy(weaponData[i].weaponIcon, weaponData[j].weaponIcon);
			}
			if(weaponData[i].mTertiaryMuzzleEffect[0] == 0){
				strcpy(weaponData[i].mTertiaryMuzzleEffect, weaponData[j].mTertiaryMuzzleEffect);
			}
			if(weaponData[i].chargeMuzzleShader[0] == 0){
				strcpy(weaponData[i].chargeMuzzleShader, weaponData[j].chargeMuzzleShader);
			}
			if(weaponData[i].weaponMdl2[0] == 0){
				strcpy(weaponData[i].weaponMdl2, weaponData[j].weaponMdl2);
			}

            //Copying the int arrays
			if(weaponData[i].mainFireOpt[0] == 0 && weaponData[i].mainFireOpt[1] == 0 && weaponData[i].mainFireOpt[2] == 0){
			    weaponData[i].mainFireOpt[0] = weaponData[j].mainFireOpt[0];
			    weaponData[i].mainFireOpt[1] = weaponData[j].mainFireOpt[1];
			    weaponData[i].mainFireOpt[2] = weaponData[j].mainFireOpt[2];
			}

			if(weaponData[i].altFireOpt[0] == 0 && weaponData[i].altFireOpt[1] == 0 && weaponData[i].altFireOpt[2] == 0){
			    weaponData[i].altFireOpt[0] = weaponData[j].altFireOpt[0];
			    weaponData[i].altFireOpt[1] = weaponData[j].altFireOpt[1];
			    weaponData[i].altFireOpt[2] = weaponData[j].altFireOpt[2];
			}

			if(weaponData[i].tertiaryFireOpt[0] == 0 && weaponData[i].tertiaryFireOpt[1] == 0 && weaponData[i].tertiaryFireOpt[2] == 0){
			    weaponData[i].tertiaryFireOpt[0] = weaponData[j].tertiaryFireOpt[0];
			    weaponData[i].tertiaryFireOpt[1] = weaponData[j].tertiaryFireOpt[1];
			    weaponData[i].tertiaryFireOpt[2] = weaponData[j].tertiaryFireOpt[2];
			}
			//Copiyng raw values.
			weaponData[i].ammoIndex = weaponData[i].ammoIndex == 0 ? weaponData[j].ammoIndex : weaponData[i].ammoIndex ;
			weaponData[i].ammoLow = weaponData[i].ammoLow == 0 ? weaponData[j].ammoLow : weaponData[i].ammoLow ;

			weaponData[i].numBarrels = weaponData[i].numBarrels == 0 ? weaponData[j].numBarrels : weaponData[i].numBarrels;

			weaponData[i].mTertiaryMuzzleEffectID = weaponData[i].mTertiaryMuzzleEffectID == 0 ? weaponData[j].mTertiaryMuzzleEffectID : weaponData[i].mTertiaryMuzzleEffectID;
			weaponData[i].chargeMuzzleShaderID = weaponData[i].chargeMuzzleShaderID == 0 ? weaponData[j].chargeMuzzleShaderID : weaponData[i].chargeMuzzleShaderID;

			weaponData[i].defaultDamage = weaponData[i].defaultDamage == 0 ? weaponData[j].defaultDamage : weaponData[i].defaultDamage;
			for (int k = 0; k < 2; k++) {
				weaponData[i].attackData[k].energyPerShot = weaponData[i].attackData[k].energyPerShot == 0 ? weaponData[j].attackData[k].energyPerShot : weaponData[i].attackData[k].energyPerShot ;
				weaponData[i].attackData[k].fireTime = weaponData[i].attackData[k].fireTime == 0 ? weaponData[j].attackData[k].fireTime : weaponData[i].attackData[k].fireTime ;
				weaponData[i].attackData[k].range = weaponData[i].attackData[k].range == 0 ? weaponData[j].attackData[k].range : weaponData[i].attackData[k].range ;
				weaponData[i].attackData[k].damage = weaponData[i].attackData[k].damage == 0 ? weaponData[j].attackData[k].damage : weaponData[i].attackData[k].damage;
				weaponData[i].attackData[k].splashDamage = weaponData[i].attackData[k].splashDamage == 0 ? weaponData[j].attackData[k].splashDamage : weaponData[i].attackData[k].splashDamage;
				weaponData[i].attackData[k].splashRadius = weaponData[i].attackData[k].splashRadius == 0 ? weaponData[j].attackData[k].splashRadius : weaponData[i].attackData[k].splashRadius;
				weaponData[i].attackData[k].mVelocity = weaponData[i].attackData[k].mVelocity == 0 ? weaponData[j].attackData[k].mVelocity : weaponData[i].attackData[k].mVelocity;
				weaponData[i].attackData[k].spread = weaponData[i].attackData[k].spread == 0 ? weaponData[j].attackData[k].spread : weaponData[i].attackData[k].spread;

				if (weaponData[i].attackData[k].firingSnd[0] == 0) {
					strcpy(weaponData[i].attackData[k].firingSnd,weaponData[j].attackData[k].firingSnd);
				}
				if (weaponData[i].attackData[k].chargeSnd[0] == 0) {
					strcpy(weaponData[i].attackData[k].chargeSnd,weaponData[j].attackData[k].chargeSnd);
				}

				if(weaponData[i].attackData[k].missileMdl[0] == 0){
					strcpy(weaponData[i].attackData[k].missileMdl, weaponData[j].attackData[k].missileMdl);
				}
				if(weaponData[i].attackData[k].missileSound[0] == 0){
					strcpy(weaponData[i].attackData[k].missileSound, weaponData[j].attackData[k].missileSound);
				}
				if(weaponData[i].attackData[k].missileHitSound[0] == 0){
					strcpy(weaponData[i].attackData[k].missileHitSound, weaponData[j].attackData[k].missileHitSound);
				}

				if (weaponData[i].attackData[k].projectileEffect[0] == 0) {
					strcpy(weaponData[i].attackData[k].projectileEffect, weaponData[j].attackData[k].projectileEffect);
				}
				if(weaponData[i].attackData[k].mMuzzleEffect[0] == 0){
					strcpy(weaponData[i].attackData[k].mMuzzleEffect, weaponData[j].attackData[k].mMuzzleEffect);
				}
				weaponData[i].attackData[k].mMuzzleEffectID = weaponData[i].attackData[k].mMuzzleEffectID == 0 ? weaponData[j].attackData[k].mMuzzleEffectID : weaponData[i].attackData[k].mMuzzleEffectID;

				//copying weapon missile trail Function pointers
				weaponData[i].attackData[k].missileFunc = weaponData[i].attackData[k].missileFunc == 0 ? weaponData[j].attackData[k].missileFunc : weaponData[i].attackData[k].missileFunc;

				weaponData[i].attackData[k].missileDlight = weaponData[i].attackData[k].missileDlight == 0 ? weaponData[j].attackData[k].missileDlight : weaponData[i].attackData[k].missileDlight ;
				//Copying vectors
				if (weaponData[i].attackData[k].missileDlightColor[0] == 0 && weaponData[i].attackData[k].missileDlightColor[1] == 0 && weaponData[i].attackData[k].missileDlightColor[2] == 0) {
					VectorCopy(weaponData[j].attackData[k].missileDlightColor, weaponData[i].attackData[k].missileDlightColor);
				}
			}

			weaponData[i].tertiaryEnergyPerShot = weaponData[i].tertiaryEnergyPerShot == 0 ? weaponData[j].tertiaryEnergyPerShot : weaponData[i].tertiaryEnergyPerShot;
			weaponData[i].tertiaryFireTime = weaponData[i].tertiaryFireTime == 0 ? weaponData[j].tertiaryFireTime : weaponData[i].tertiaryFireTime;
			weaponData[i].tertiaryRange = weaponData[i].tertiaryRange == 0 ? weaponData[j].tertiaryRange : weaponData[i].tertiaryRange;

			weaponData[i].scopeType = weaponData[i].scopeType == 0 ? weaponData[j].scopeType : weaponData[i].scopeType;

			weaponData[i].secondaryMdl = weaponData[i].secondaryMdl == 0 ? weaponData[j].secondaryMdl : weaponData[i].secondaryMdl;
			weaponData[i].playerUsable = weaponData[i].playerUsable == qunset ? weaponData[j].playerUsable : weaponData[i].playerUsable;
			weaponData[i].weaponCategory = weaponData[i].weaponCategory == WC_NONE ? weaponData[j].weaponCategory : weaponData[i].weaponCategory;
			weaponData[i].weaponBucket = weaponData[i].weaponBucket == 0 ? weaponData[j].weaponBucket : weaponData[i].weaponBucket;
			weaponData[i].baseWeaponNum = j;

			//The first one is the good one...
			break;
		}
		if(!found){
		    gi.Printf(S_COLOR_YELLOW"WARNING: Weapon '%s' is marked as alternate of base weapon '%s' but base weapon was not found\n",weaponData[i].classname,weaponData[i].baseclass);
		}
	}
	//Sort weapons in buckets
	int buckets[-WB_OTHERS][MAX_WEAPONS];
	//Init the data to -1
	memset(&buckets, -1, sizeof(buckets));
	memset(&weaponBuckets, -1, sizeof(weaponBuckets));
	//Naive initialisation of the buckets
	for (int i = 0; i < weaponCount;i++)
	{
		int bucketIndex = (-weaponData[i].weaponBucket) - 1;
		int weaponIndex = 0;
		while (buckets[bucketIndex][weaponIndex] != -1)
		{
			weaponIndex++;
		}
		buckets[bucketIndex][weaponIndex] = i;
	}
	//Now merge the buckets in one array
	int gi = 0;
	for (int i = 0; i < (-WB_OTHERS);i++)
	{
		weaponBuckets[gi] = (-i - 1);
		gi++;
		int j;
		for (j = 0; buckets[i][j] != -1; j++) {
			weaponBuckets[gi] = buckets[i][j];
			gi++;
		}
	}
	Com_Printf(S_COLOR_CYAN"WeaponBuckets[");
	for (int i = 0; i < (MAX_WEAPONS - WB_OTHERS); i++) {
		Com_Printf(S_COLOR_CYAN"%d,", weaponBuckets[i]);
	}
	Com_Printf(S_COLOR_CYAN"]\n");
}
