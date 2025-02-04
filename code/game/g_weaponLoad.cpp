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

void WPN_FuncSkip( const char **holdBuf)
{
	SkipRestOfLine(holdBuf);
}

void WPN_WeaponType( const char **holdBuf)
{
	int i;
	int weaponNum;
	const char	*tokenStr;

	if (COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	for (i = 0;i < numHcWeaponIndexes;i++)
	{
		if (!Q_stricmp(tokenStr, _weaponIndexes[i].weaponClass))
		{
			weaponNum = _weaponIndexes[i].index;
			break;
		}
	}

	if (i == numHcWeaponIndexes)	// !Find parameter
	{
		if (weaponCount >= MAX_WEAPONS) {
			weaponNum = 0;
			gi.Printf(S_COLOR_YELLOW"WARNING: too many weapons in external weapon data(%d); Parsing '%s'\n", MAX_WEAPONS, tokenStr);
		}
		weaponNum = weaponCount;
		weaponCount++;
		Com_Printf(S_COLOR_CYAN"Dynamic Weapon found : %s\n", tokenStr);
	}
	
	wpnParms.weaponNum = weaponNum;

	weaponIndexes[weaponNum].index = weaponNum;
	strcpy(weaponIndexes[weaponNum].weaponClass, tokenStr);
	
}

//--------------------------------------------
void WPN_WeaponClass(const char **holdBuf)
{
	int len;
	const char	*tokenStr;

	if (COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 32)
	{
		len = 32;
		gi.Printf(S_COLOR_YELLOW"WARNING: weaponclass too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].classname,tokenStr,len);
}


//--------------------------------------------
void WPN_WeaponModel(const char **holdBuf)
{
	int len;
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: weaponMdl too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].weaponMdl,tokenStr,len);
}

//--------------------------------------------
void WPN_WeaponIcon(const char **holdBuf)
{
	int len;
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: weaponIcon too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].weaponIcon,tokenStr,len);
}

//--------------------------------------------
void WPN_AmmoType(const char **holdBuf)
{
	int		tokenInt;

	if ( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < AMMO_NONE ) || (tokenInt >= AMMO_MAX ))
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad Ammotype in external weapon data '%d'\n", tokenInt);
		return;
	}

	weaponData[wpnParms.weaponNum].ammoIndex = tokenInt;
}

//--------------------------------------------
void WPN_AmmoLowCnt(const char **holdBuf)
{
	int		tokenInt;

	if ( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < 0) || (tokenInt > 200 )) // FIXME :What are the right values?
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad Ammolowcount in external weapon data '%d'\n", tokenInt);
		return;
	}

	weaponData[wpnParms.weaponNum].ammoLow = tokenInt;
}

//--------------------------------------------
void WPN_FiringSnd(const char **holdBuf)
{
	const char	*tokenStr;
	int		len;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: firingSnd too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].firingSnd,tokenStr,len);
}

//--------------------------------------------
void WPN_AltFiringSnd( const char **holdBuf )
{
	const char	*tokenStr;
	int		len;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: altFiringSnd too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].altFiringSnd,tokenStr,len);
}

//--------------------------------------------
void WPN_StopSnd( const char **holdBuf )
{
	const char	*tokenStr;
	int		len;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: stopSnd too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].stopSnd,tokenStr,len);
}

//--------------------------------------------
void WPN_ChargeSnd(const char **holdBuf)
{
	const char	*tokenStr;
	int		len;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: chargeSnd too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].chargeSnd,tokenStr,len);
}

//--------------------------------------------
void WPN_AltChargeSnd(const char **holdBuf)
{
	const char	*tokenStr;
	int		len;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: altChargeSnd too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].altChargeSnd,tokenStr,len);
}

//--------------------------------------------
void WPN_SelectSnd( const char **holdBuf )
{
	const char	*tokenStr;
	int		len;

	if ( COM_ParseString( holdBuf,&tokenStr ))
	{
		return;
	}

	len = strlen( tokenStr );
	len++;

	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: selectSnd too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz( weaponData[wpnParms.weaponNum].selectSnd,tokenStr,len);
}

//--------------------------------------------
void WPN_FireTime(const char **holdBuf)
{
	int		tokenInt;

	if ( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < 0) || (tokenInt > 10000 )) // FIXME :What are the right values?
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad Firetime in external weapon data '%d'\n", tokenInt);
		return;
	}
	weaponData[wpnParms.weaponNum].fireTime = tokenInt;
}

//--------------------------------------------
void WPN_Range(const char **holdBuf)
{
	int		tokenInt;

	if ( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < 0) || (tokenInt > 10000 )) // FIXME :What are the right values?
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad Range in external weapon data '%d'\n", tokenInt);
		return;
	}

	weaponData[wpnParms.weaponNum].range = tokenInt;
}

//--------------------------------------------
void WPN_EnergyPerShot(const char **holdBuf)
{
	int		tokenInt;

	if ( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < 0) || (tokenInt > 1000 )) // FIXME :What are the right values?
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad EnergyPerShot in external weapon data '%d'\n", tokenInt);
		return;
	}
	weaponData[wpnParms.weaponNum].energyPerShot = tokenInt;
}

//--------------------------------------------
void WPN_AltFireTime(const char **holdBuf)
{
	int		tokenInt;

	if ( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < 0) || (tokenInt > 10000 )) // FIXME :What are the right values?
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad altFireTime in external weapon data '%d'\n", tokenInt);
		return;
	}
	weaponData[wpnParms.weaponNum].altFireTime = tokenInt;
}

//--------------------------------------------
void WPN_AltRange(const char **holdBuf)
{
	int		tokenInt;

	if ( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < 0) || (tokenInt > 10000 )) // FIXME :What are the right values?
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad AltRange in external weapon data '%d'\n", tokenInt);
		return;
	}

	weaponData[wpnParms.weaponNum].altRange = tokenInt;
}

//--------------------------------------------
void WPN_AltEnergyPerShot(const char **holdBuf)
{
	int		tokenInt;

	if ( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < 0) || (tokenInt > 1000 )) // FIXME :What are the right values?
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad AltEnergyPerShot in external weapon data '%d'\n", tokenInt);
		return;
	}
	weaponData[wpnParms.weaponNum].altEnergyPerShot = tokenInt;
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
	const char	*tokenStr;
	int		len;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: ammoicon too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(ammoData[wpnParms.ammoNum].icon,tokenStr,len);

}

//--------------------------------------------
void WPN_AmmoMax(const char **holdBuf)
{
	int		tokenInt;

	if ( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < 0) || (tokenInt > 1000 ))
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad Ammo Max in external weapon data '%d'\n", tokenInt);
		return;
	}
	ammoData[wpnParms.ammoNum].max = tokenInt;
}

//--------------------------------------------
void WPN_BarrelCount(const char **holdBuf)
{
	int		tokenInt;

	if ( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < 0) || (tokenInt > 4 ))
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad Range in external weapon data '%d'\n", tokenInt);
		return;
	}

	weaponData[wpnParms.weaponNum].numBarrels = tokenInt;
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
	int len;
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: MissileName too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].missileMdl,tokenStr,len);

}

//--------------------------------------------
void WPN_AltMissileName(const char **holdBuf)
{
	int len;
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: AltMissileName too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].alt_missileMdl,tokenStr,len);

}


//--------------------------------------------
void WPN_MissileHitSound(const char **holdBuf)
{
	int len;
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: MissileHitSound too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].missileHitSound,tokenStr,len);
}

//--------------------------------------------
void WPN_AltMissileHitSound(const char **holdBuf)
{
	int len;
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: AltMissileHitSound too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].altmissileHitSound,tokenStr,len);
}

//--------------------------------------------
void WPN_MissileSound(const char **holdBuf)
{
	int len;
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: MissileSound too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].missileSound,tokenStr,len);

}


//--------------------------------------------
void WPN_AltMissileSound(const char **holdBuf)
{
	int len;
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: AltMissileSound too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].alt_missileSound,tokenStr,len);

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
		weaponData[wpnParms.weaponNum].missileDlightColor[i] = tokenFlt;
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
		weaponData[wpnParms.weaponNum].alt_missileDlightColor[i] = tokenFlt;
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
	weaponData[wpnParms.weaponNum].missileDlight = tokenFlt;
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
	weaponData[wpnParms.weaponNum].alt_missileDlight = tokenFlt;
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
			weaponData[wpnParms.weaponNum].func = (void*)s->func;
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
			weaponData[wpnParms.weaponNum].altfunc = (void*)s->func;
			return;
		}
	}
	gi.Printf(S_COLOR_YELLOW"WARNING: AltFuncName %s in external WEAPONS.DAT does not exist\n", tokenStr);
}

//--------------------------------------------
void WPN_MuzzleEffect(const char **holdBuf)
{
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}
	size_t len = strlen( tokenStr );

	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: MuzzleEffect '%s' too long in external WEAPONS.DAT\n", tokenStr);
	}

	G_EffectIndex( tokenStr );
	Q_strncpyz(weaponData[wpnParms.weaponNum].mMuzzleEffect,tokenStr,len);
}

//--------------------------------------------
void WPN_AltMuzzleEffect(const char **holdBuf)
{
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}
	size_t len = strlen( tokenStr );

	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: AltMuzzleEffect '%s' too long in external WEAPONS.DAT\n", tokenStr);
	}

	G_EffectIndex( tokenStr );
	Q_strncpyz(weaponData[wpnParms.weaponNum].mAltMuzzleEffect,tokenStr,len);
}

//--------------------------------------------
void WPN_TertiaryMuzzleEffect(const char **holdBuf)
{
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}
	size_t len = strlen( tokenStr );

	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: TertiaryMuzzleEffect '%s' too long in external WEAPONS.DAT\n", tokenStr);
	}

	G_EffectIndex( tokenStr );
	Q_strncpyz(weaponData[wpnParms.weaponNum].mTertiaryMuzzleEffect,tokenStr,len);
}

//--------------------------------------------

void WPN_Damage(const char **holdBuf)
{
	int		tokenInt;

	if( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].damage = tokenInt;
	weaponData[wpnParms.weaponNum].defaultDamage = tokenInt;
}

//--------------------------------------------

void WPN_AltDamage(const char **holdBuf)
{
	int		tokenInt;

	if( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].altDamage = tokenInt;
}

//--------------------------------------------

void WPN_SplashDamage(const char **holdBuf)
{
	int		tokenInt;

	if( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].splashDamage = tokenInt;
}

//--------------------------------------------

void WPN_SplashRadius(const char **holdBuf)
{
	float	tokenFlt;

	if( COM_ParseFloat(holdBuf,&tokenFlt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].splashRadius = tokenFlt;
}

//--------------------------------------------

void WPN_AltSplashDamage(const char **holdBuf)
{
	int		tokenInt;

	if( COM_ParseInt(holdBuf,&tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].altSplashDamage = tokenInt;
}

//--------------------------------------------

void WPN_AltSplashRadius(const char **holdBuf)
{
	float	tokenFlt;

	if( COM_ParseFloat(holdBuf,&tokenFlt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].altSplashRadius = tokenFlt;
}

//--------------------------------------------
void WPN_TertiaryEnergyPerShot(const char **holdBuf)
{
	int		tokenInt;

	if (COM_ParseInt(holdBuf, &tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < 0) || (tokenInt > 1000))
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad tertiaryEnergyPerShot in external weapon data '%d'\n", tokenInt);
		return;
	}

	weaponData[wpnParms.weaponNum].tertiaryEnergyPerShot = tokenInt;
}

//--------------------------------------------
void WPN_TertiaryFireTime(const char **holdBuf)
{
	int		tokenInt;

	if (COM_ParseInt(holdBuf, &tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < 0) || (tokenInt > 10000))
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad tertiaryFireTime in external weapon data '%d'\n", tokenInt);
		return;
	}

	weaponData[wpnParms.weaponNum].tertiaryFireTime = tokenInt;
}

//--------------------------------------------
void WPN_TertiaryRange(const char **holdBuf)
{
	int		tokenInt;

	if (COM_ParseInt(holdBuf, &tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < 0) || (tokenInt > 10000))
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad tertiaryRange in external weapon data '%d'\n", tokenInt);
		return;
	}

	weaponData[wpnParms.weaponNum].tertiaryRange = tokenInt;
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

    if ((tokenInt < ST_A280) || (tokenInt > ST_F11D ))
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
	int len;
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 64)
	{
		len = 64;
		gi.Printf(S_COLOR_YELLOW"WARNING: weaponMdl too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].weaponMdl2,tokenStr,len);
}

//--------------------------------------------
void WPN_BaseWeapon(const char **holdBuf)
{
	int len;
	const char	*tokenStr;

	if ( COM_ParseString(holdBuf,&tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > 32)
	{
		len = 32;
		gi.Printf(S_COLOR_YELLOW"WARNING: Base weapon too long in external WEAPONS.DAT '%s'\n", tokenStr);
	}

	Q_strncpyz(weaponData[wpnParms.weaponNum].baseclass,tokenStr,len);
}

//--------------------------------------------

void WPN_Velocity(const char** holdBuf)
{
	float	tokenFlt;

	if (COM_ParseFloat(holdBuf, &tokenFlt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].mVelocity = tokenFlt;
}

//--------------------------------------------

void WPN_AltVelocity(const char** holdBuf)
{
	float	tokenFlt;

	if (COM_ParseFloat(holdBuf, &tokenFlt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].mAltVelocity = tokenFlt;
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

void WPN_IsPistol(const char** holdBuf)
{
	int tokenInt;

	if (COM_ParseInt(holdBuf, &tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].isPistol = (qboolean) tokenInt;
}

//--------------------------------------------
void WPN_DescriptionKey(const char** holdBuf)
{
	const char* tokenStr;

	if (COM_ParseString(holdBuf, &tokenStr))
	{
		return;
	}
	size_t len = strlen(tokenStr);

	len++;
	if (len > 128)
	{
		len = 128;
		gi.Printf(S_COLOR_YELLOW"WARNING: descriptionKey '%s' too long in external WEAPONS.DAT\n", tokenStr);
	}

	G_EffectIndex(tokenStr);
	Q_strncpyz(weaponData[wpnParms.weaponNum].descriptionKey, tokenStr, len);
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
		weaponData[i].damage = defaultDamage[i];
		weaponData[i].defaultDamage = defaultDamage[i];
		weaponData[i].altDamage = defaultAltDamage[i];
		weaponData[i].splashDamage = defaultSplashDamage[i];
		weaponData[i].altSplashDamage = defaultAltSplashDamage[i];
		weaponData[i].splashRadius = defaultSplashRadius[i];
		weaponData[i].altSplashRadius = defaultAltSplashRadius[i];
		weaponData[i].playerUsable = defaultPlayerUsable[i];
		weaponData[i].mVelocity = defaultsWeaponSpeed[i][0];
		weaponData[i].mAltVelocity = defaultsWeaponSpeed[i][1];
		weaponData[i].isPistol = defaultIsPistol[i];
	}
	//put in the qunset flag for playerUsable since 0 = false;
	for (int i = numHcWeaponIndexes; i < MAX_WEAPONS ; i++) {
		weaponData[i].playerUsable = qunset;
		weaponData[i].isPistol = qunset;

	}
	WP_ParseParms(buffer);

	gi.FS_FreeFile( buffer );	//let go of the buffer

	//Read files containing Dynamics Weapons
	char*	holdChar;
	char	weaponFileList[2048];			//	The list of file names read in
	int		fileCount;
	int		fileNameSize;

	//Read all the externals file
	fileCount = gi.FS_GetFileList("ext_data/ext_weapons/", ".dat", weaponFileList, sizeof(weaponFileList));

	holdChar = weaponFileList;

	Com_Printf("Found %d External files\n", fileCount);
	for (int i = 0; i < fileCount; i++, holdChar += fileNameSize + 1)
	{
		fileNameSize = strlen(holdChar);

		Com_Printf( "Parsing %s\n", holdChar );
		char* fileBuffer;
		int fileLen;

		fileLen = gi.FS_ReadFile(va("ext_data/ext_weapons/%s",holdChar), (void**)&fileBuffer);

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
			if (weaponData[i].firingSnd[0] == 0) {
				strcpy(weaponData[i].firingSnd,weaponData[j].firingSnd);
			}
			if (weaponData[i].altFiringSnd[0] == 0) {
				strcpy(weaponData[i].altFiringSnd,weaponData[j].altFiringSnd);
			}
			if (weaponData[i].stopSnd[0] == 0) {
				strcpy(weaponData[i].stopSnd,weaponData[j].stopSnd);
			}
			if (weaponData[i].chargeSnd[0] == 0) {
				strcpy(weaponData[i].chargeSnd,weaponData[j].chargeSnd);
			}
			if (weaponData[i].altChargeSnd[0] == 0) {
				strcpy(weaponData[i].altChargeSnd,weaponData[j].altChargeSnd);
			}
			if (weaponData[i].selectSnd[0] == 0) {
				strcpy(weaponData[i].selectSnd,weaponData[j].selectSnd);
			}
			if(weaponData[i].weaponIcon[0] == 0){
				strcpy(weaponData[i].weaponIcon, weaponData[j].weaponIcon);
			}
			if(weaponData[i].missileMdl[0] == 0){
				strcpy(weaponData[i].missileMdl, weaponData[j].missileMdl);
			}
			if(weaponData[i].alt_missileMdl[0] == 0){
				strcpy(weaponData[i].alt_missileMdl, weaponData[j].alt_missileMdl);
			}
			if(weaponData[i].alt_missileSound[0] == 0){
				strcpy(weaponData[i].alt_missileSound, weaponData[j].alt_missileSound);
			}
			if(weaponData[i].missileHitSound[0] == 0){
				strcpy(weaponData[i].missileHitSound, weaponData[j].missileHitSound);
			}
			if(weaponData[i].altmissileHitSound[0] == 0){
				strcpy(weaponData[i].altmissileHitSound, weaponData[j].altmissileHitSound);
			}
			if(weaponData[i].mMuzzleEffect[0] == 0){
				strcpy(weaponData[i].mMuzzleEffect, weaponData[j].mMuzzleEffect);
			}
			if(weaponData[i].mAltMuzzleEffect[0] == 0){
				strcpy(weaponData[i].mAltMuzzleEffect, weaponData[j].mAltMuzzleEffect);
			}
			if(weaponData[i].mTertiaryMuzzleEffect[0] == 0){
				strcpy(weaponData[i].mTertiaryMuzzleEffect, weaponData[j].mTertiaryMuzzleEffect);
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

			weaponData[i].energyPerShot = weaponData[i].energyPerShot == 0 ? weaponData[j].energyPerShot : weaponData[i].energyPerShot ;
			weaponData[i].fireTime = weaponData[i].fireTime == 0 ? weaponData[j].fireTime : weaponData[i].fireTime ;
			weaponData[i].range = weaponData[i].range == 0 ? weaponData[j].range : weaponData[i].range ;

			weaponData[i].altEnergyPerShot = weaponData[i].altEnergyPerShot == 0 ? weaponData[j].altEnergyPerShot : weaponData[i].altEnergyPerShot ;
			weaponData[i].altFireTime = weaponData[i].altFireTime == 0 ? weaponData[j].altFireTime : weaponData[i].altFireTime ;
			weaponData[i].altRange = weaponData[i].altRange == 0 ? weaponData[j].altRange : weaponData[i].altRange ;

			weaponData[i].numBarrels = weaponData[i].numBarrels == 0 ? weaponData[j].numBarrels : weaponData[i].numBarrels;

			weaponData[i].alt_missileDlight = weaponData[i].alt_missileDlight == 0 ? weaponData[j].alt_missileDlight : weaponData[i].alt_missileDlight ;

			weaponData[i].mMuzzleEffectID = weaponData[i].mMuzzleEffectID == 0 ? weaponData[j].mMuzzleEffectID : weaponData[i].mMuzzleEffectID;
			weaponData[i].mAltMuzzleEffectID = weaponData[i].mAltMuzzleEffectID == 0 ? weaponData[j].mAltMuzzleEffectID : weaponData[i].mAltMuzzleEffectID;
			weaponData[i].mTertiaryMuzzleEffectID = weaponData[i].mTertiaryMuzzleEffectID == 0 ? weaponData[j].mTertiaryMuzzleEffectID : weaponData[i].mTertiaryMuzzleEffectID;

			weaponData[i].damage = weaponData[i].damage == 0 ? weaponData[j].damage : weaponData[i].damage;
			weaponData[i].defaultDamage = weaponData[i].defaultDamage == 0 ? weaponData[j].defaultDamage : weaponData[i].defaultDamage;
			weaponData[i].altDamage = weaponData[i].altDamage == 0 ? weaponData[j].altDamage : weaponData[i].altDamage;
			weaponData[i].splashDamage = weaponData[i].splashDamage == 0 ? weaponData[j].splashDamage : weaponData[i].splashDamage;
			weaponData[i].altSplashDamage = weaponData[i].altSplashDamage == 0 ? weaponData[j].altSplashDamage : weaponData[i].altSplashDamage;
			weaponData[i].splashRadius = weaponData[i].splashRadius == 0 ? weaponData[j].splashRadius : weaponData[i].splashRadius ;
			weaponData[i].altSplashRadius = weaponData[i].altSplashRadius == 0 ? weaponData[j].altSplashRadius : weaponData[i].altSplashRadius ;

			weaponData[i].tertiaryEnergyPerShot = weaponData[i].tertiaryEnergyPerShot == 0 ? weaponData[j].tertiaryEnergyPerShot : weaponData[i].tertiaryEnergyPerShot;
			weaponData[i].tertiaryFireTime = weaponData[i].tertiaryFireTime == 0 ? weaponData[j].tertiaryFireTime : weaponData[i].tertiaryFireTime;
			weaponData[i].tertiaryRange = weaponData[i].tertiaryRange == 0 ? weaponData[j].tertiaryRange : weaponData[i].tertiaryRange;

			weaponData[i].scopeType = weaponData[i].scopeType == 0 ? weaponData[j].scopeType : weaponData[i].scopeType;

			weaponData[i].secondaryMdl = weaponData[i].secondaryMdl == 0 ? weaponData[j].secondaryMdl : weaponData[i].secondaryMdl;
			weaponData[i].playerUsable = weaponData[i].playerUsable == qunset ? weaponData[j].playerUsable : weaponData[j].playerUsable;
			weaponData[i].isPistol = weaponData[i].isPistol == qunset ? weaponData[j].isPistol : weaponData[j].isPistol;
			weaponData[i].mVelocity = weaponData[i].mVelocity == 0 ? weaponData[j].mVelocity : weaponData[j].mVelocity;
			weaponData[i].mAltVelocity = weaponData[i].mAltVelocity == 0 ? weaponData[j].mAltVelocity : weaponData[j].mAltVelocity;
			weaponData[i].baseWeaponNum = j;
			//copying weapon Function pointers
			weaponData[i].func = weaponData[i].func == 0 ? weaponData[j].func : weaponData[i].func;
			weaponData[i].altfunc = weaponData[i].altfunc == 0 ? weaponData[j].altfunc : weaponData[i].altfunc;


			//Copying vectors
			if (weaponData[i].missileDlightColor[0] == 0 && weaponData[i].missileDlightColor[1] == 0 && weaponData[i].missileDlightColor[2] == 0) {
				VectorCopy(weaponData[j].missileDlightColor, weaponData[i].missileDlightColor);
			}

			if (weaponData[i].alt_missileDlightColor[0] == 0 && weaponData[i].alt_missileDlightColor[1] == 0 && weaponData[i].alt_missileDlightColor[2] == 0) {
				VectorCopy(weaponData[j].alt_missileDlightColor, weaponData[i].alt_missileDlightColor);
			}
			//The first one is the good one...
			break;
		}
		if(!found){
		    gi.Printf(S_COLOR_YELLOW"WARNING: Weapon '%s' is marked as alternate of base weapon '%s' but base weapon was not found\n",weaponData[i].classname,weaponData[i].baseclass);
		}
	}
	//globals.weaponCount = weaponCount;
}
