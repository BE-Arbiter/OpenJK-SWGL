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

// g_weaponLoad.h
// Contains Default Data for hardcoded weapons

/*===========================================================================
	Default (Harcoded) Indexes
===========================================================================*/
weaponIndexes_t _weaponIndexes[] = {
	{"WP_NONE",0},
	{"WP_SABER",1},
	{"WP_BLASTER_PISTOL",2},
	{"WP_BLASTER",3 },
	{"WP_DISRUPTOR",4},
	{"WP_BOWCASTER",5},
	{"WP_REPEATER",6},
	{"WP_DEMP2",7},
	{"WP_FLECHETTE",8},
	{"WP_ROCKET_LAUNCHER",9},
	{"WP_THERMAL",10},
	{"WP_TRIP_MINE",11},
	{"WP_DET_PACK",12},
	{"WP_CONCUSSION",13},
	{"WP_MELEE",14},
	{"WP_ATST_MAIN",15},
	{"WP_ATST_SIDE",16},
	{"WP_STUN_BATON",17},
	{"WP_BRYAR_PISTOL",18},
	{"WP_EMPLACED_GUN",19},
	{"WP_BOT_LASER",20},
	{"WP_TURRET",21},
	{"WP_TIE_FIGHTER",22},
	{"WP_RAPID_FIRE_CONC",23 },
	{"WP_JAWA",24},
	{"WP_TUSKEN_RIFLE",25 },
	{"WP_TUSKEN_STAFF",26},
	{"WP_SCEPTER",27},
	{"WP_NOGHRI_STICK",28},
	{"WP_BATTLEDROID",29},
	{"WP_THEFIRSTORDER", 30},
	{"WP_CLONECARBINE",31},
	{"WP_REBELBLASTER",32},
	{"WP_CLONERIFLE",33},
	{"WP_CLONECOMMANDO",34},
	{"WP_REBELRIFLE",35},
	{"WP_REY",36},
	{"WP_JANGO",37},
	{"WP_BOBA",38},
	{"WP_CLONEPISTOL",39},
};
static const size_t numHcWeaponIndexes = ARRAY_LEN(_weaponIndexes);

/*===========================================================================
	Logic Function definitions
===========================================================================*/
typedef struct {
	const char* name;
	void	(*func)(centity_t* cent, const struct weaponInfo_s* weapon);
} func_t;
// Bryar
void FX_BryarProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_BryarAltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Blaster
void FX_BlasterProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_BlasterAltFireThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Clone
void FX_CloneProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_CloneAltFireThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_CloneAltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_CloneCommandoProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);


// Bowcaster
void FX_BowcasterProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Heavy Repeater
void FX_RepeaterProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_RepeaterAltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// DEMP2
void FX_DEMP2_ProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_DEMP2_AltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Golan Arms Flechette
void FX_FlechetteProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_FlechetteAltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Personal Rocket Launcher
void FX_RocketProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_RocketAltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Concussion Rifle
void FX_ConcProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Emplaced weapon
void FX_EmplacedProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Turret weapon
void FX_TurretProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// ATST Main weapon
void FX_ATSTMainProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// ATST Side weapons
void FX_ATSTSideMainProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_ATSTSideAltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

//Tusken projectile
void FX_TuskenShotProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

//Noghri projectile
void FX_NoghriShotProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Table used to attach an extern missile function string to the actual cgame function
func_t	funcs[] = {
	{"bryar_func",			FX_BryarProjectileThink},
	{"bryar_alt_func",		FX_BryarAltProjectileThink},
	{"blaster_func",		FX_BlasterProjectileThink},
	{"blaster_alt_func",	FX_BlasterAltFireThink},
	{"bowcaster_func",		FX_BowcasterProjectileThink},
	{"repeater_func",		FX_RepeaterProjectileThink},
	{"repeater_alt_func",	FX_RepeaterAltProjectileThink},
	{"demp2_func",			FX_DEMP2_ProjectileThink},
	{"demp2_alt_func",		FX_DEMP2_AltProjectileThink},
	{"flechette_func",		FX_FlechetteProjectileThink},
	{"flechette_alt_func",	FX_FlechetteAltProjectileThink},
	{"rocket_func",			FX_RocketProjectileThink},
	{"rocket_alt_func",		FX_RocketAltProjectileThink},
	{"conc_func",			FX_ConcProjectileThink},
	{"emplaced_func",		FX_EmplacedProjectileThink},
	{"turret_func",			FX_TurretProjectileThink},
	{"atstmain_func",		FX_ATSTMainProjectileThink},
	{"atst_side_alt_func",	FX_ATSTSideAltProjectileThink},
	{"atst_side_main_func",	FX_ATSTSideMainProjectileThink},
	{"tusk_shot_func",		FX_TuskenShotProjectileThink},
	{"noghri_shot_func",	FX_NoghriShotProjectileThink},
	{ "clone_func",			FX_CloneProjectileThink},
	{ "clone_alt_func",     FX_CloneAltFireThink },
	{ "clone_pistol_alt_func", FX_CloneAltProjectileThink },
	{ "clone_commando_alt_func", FX_CloneCommandoProjectileThink },
	{NULL,					NULL}
};

/*===========================================================================
	Parameters & Parameters Function Declarations
===========================================================================*/
struct wpnParms_s
{
	int	weaponNum;	// Current weapon number
	int	ammoNum;
} wpnParms;

void WPN_Ammo(const char** holdBuf);
void WPN_AmmoIcon(const char** holdBuf);
void WPN_AmmoMax(const char** holdBuf);
void WPN_AmmoLowCnt(const char** holdBuf);
void WPN_AmmoType(const char** holdBuf);
void WPN_EnergyPerShot(const char** holdBuf);
void WPN_FireTime(const char** holdBuf);
void WPN_FiringSnd(const char** holdBuf);
void WPN_AltFiringSnd(const char** holdBuf);
void WPN_StopSnd(const char** holdBuf);
void WPN_ChargeSnd(const char** holdBuf);
void WPN_AltChargeSnd(const char** holdBuf);
void WPN_SelectSnd(const char** holdBuf);
void WPN_Range(const char** holdBuf);
void WPN_WeaponClass(const char** holdBuf);
void WPN_WeaponIcon(const char** holdBuf);
void WPN_WeaponModel(const char** holdBuf);
void WPN_WeaponType(const char** holdBuf);
void WPN_AltEnergyPerShot(const char** holdBuf);
void WPN_AltFireTime(const char** holdBuf);
void WPN_AltRange(const char** holdBuf);
void WPN_BarrelCount(const char** holdBuf);
void WPN_MissileName(const char** holdBuf);
void WPN_AltMissileName(const char** holdBuf);
void WPN_MissileSound(const char** holdBuf);
void WPN_AltMissileSound(const char** holdBuf);
void WPN_MissileLight(const char** holdBuf);
void WPN_AltMissileLight(const char** holdBuf);
void WPN_MissileLightColor(const char** holdBuf);
void WPN_AltMissileLightColor(const char** holdBuf);
void WPN_FuncName(const char** holdBuf);
void WPN_AltFuncName(const char** holdBuf);
void WPN_MissileHitSound(const char** holdBuf);
void WPN_AltMissileHitSound(const char** holdBuf);
void WPN_MuzzleEffect(const char** holdBuf);
void WPN_AltMuzzleEffect(const char** holdBuf);
void WPN_TertiaryMuzzleEffect(const char** holdBuf);
void WPN_ChargeMuzzleEffect(const char** holdBuf);
// OPENJK ADD

void WPN_Damage(const char** holdBuf);
void WPN_AltDamage(const char** holdBuf);
void WPN_SplashDamage(const char** holdBuf);
void WPN_SplashRadius(const char** holdBuf);
void WPN_AltSplashDamage(const char** holdBuf);
void WPN_AltSplashRadius(const char** holdBuf);

void WPN_TertiaryEnergyPerShot(const char** holdBuf);
void WPN_TertiaryFireTime(const char** holdBuf);
void WPN_TertiaryRange(const char** holdBuf);

void WPN_ScopeType(const char** holdBuf);

void WPN_MainFireOptions(const char** holdBuf);
void WPN_AltFireOptions(const char** holdBuf);
void WPN_TertiaryFireOptions(const char** holdBuf);

void WPN_WeaponModel2(const char** holdBuf);

void WPN_BaseWeapon(const char** holdBuf);
void WPN_Velocity(const char** holdBuf);
void WPN_AltVelocity(const char** holdBuf);
void WPN_PlayerUsable(const char** holdBuf);
void WPN_WeaponCategory(const char** holdBuf);
void WPN_DescriptionKey(const char** holdBuf);

void WPN_ProjectileEffect(const char** holdBuf);
void WPN_AltProjectileEffect(const char** holdBuf);

// Legacy weapons.dat force fields
void WPN_FuncSkip(const char** holdBuf);


typedef struct
{
	const char* parmName;
	void	(*func)(const char** holdBuf);
} wpnParms_t;

wpnParms_t WpnParms[] =
{
	{ "ammo",				WPN_Ammo },	//ammo
	{ "ammoicon",			WPN_AmmoIcon },
	{ "ammomax",			WPN_AmmoMax },
	{ "ammolowcount",		WPN_AmmoLowCnt }, //weapons
	{ "ammotype",			WPN_AmmoType },
	{ "energypershot",	WPN_EnergyPerShot },
	{ "fireTime",			WPN_FireTime },
	{ "firingsound",		WPN_FiringSnd },
	{ "altfiringsound",	WPN_AltFiringSnd },
	{ "stopsound",		WPN_StopSnd },
	{ "chargesound",		WPN_ChargeSnd },
	{ "altchargesound",	WPN_AltChargeSnd },
	{ "selectsound",		WPN_SelectSnd },
	{ "range",			WPN_Range },
	{ "weaponclass",		WPN_WeaponClass },
	{ "weaponicon",		WPN_WeaponIcon },
	{ "weaponmodel",		WPN_WeaponModel },
	{ "weapontype",		WPN_WeaponType },
	{ "altenergypershot",	WPN_AltEnergyPerShot },
	{ "altfireTime",		WPN_AltFireTime },
	{ "altrange",			WPN_AltRange },
	{ "barrelcount",		WPN_BarrelCount },
	{ "missileModel",		WPN_MissileName },
	{ "altmissileModel", 	WPN_AltMissileName },
	{ "missileSound",		WPN_MissileSound },
	{ "altmissileSound", 	WPN_AltMissileSound },
	{ "missileLight",		WPN_MissileLight },
	{ "altmissileLight", 	WPN_AltMissileLight },
	{ "missileLightColor",WPN_MissileLightColor },
	{ "altmissileLightColor",	WPN_AltMissileLightColor },
	{ "missileFuncName",		WPN_FuncName },
	{ "altmissileFuncName",	WPN_AltFuncName },
	{ "missileHitSound",		WPN_MissileHitSound },
	{ "altmissileHitSound",	WPN_AltMissileHitSound },
	{ "muzzleEffect",			WPN_MuzzleEffect },
	{ "altmuzzleEffect",		WPN_AltMuzzleEffect },
	{ "tertiarymuzzleEffect",	WPN_TertiaryMuzzleEffect },
	{ "chargeMuzzleShader",	WPN_ChargeMuzzleEffect },
	{ "projectileEffect",			WPN_ProjectileEffect },
	{ "altProjectileEffect",		WPN_AltProjectileEffect},
	// OPENJK NEW FIELDS
	{ "damage",				WPN_Damage },
	{ "altdamage",			WPN_AltDamage },
	{ "splashDamage",		WPN_SplashDamage },
	{ "splashRadius",		WPN_SplashRadius },
	{ "altSplashDamage",	WPN_AltSplashDamage },
	{ "altSplashRadius",	WPN_AltSplashRadius },
	{ "tertiaryenergypershot",	WPN_TertiaryEnergyPerShot },
	{ "tertiaryfiretime",		WPN_TertiaryFireTime },
	{ "tertiaryrange",			WPN_TertiaryRange },

	{ "scopeType",			WPN_ScopeType },

	{ "mainfireopt",		WPN_MainFireOptions},
	{ "altfireopt",			WPN_AltFireOptions},
	{ "tertiaryfireopt",	WPN_TertiaryFireOptions},

	{ "weaponmodel2",		WPN_WeaponModel2 },
	{ "baseweapon",			WPN_BaseWeapon },
	{ "velocity",			WPN_Velocity },
	{ "altVelocity",		WPN_AltVelocity },
	{ "playerUsable",		WPN_PlayerUsable },
	{ "weaponCategory",			WPN_WeaponCategory },
	{ "descriptionKey",		WPN_DescriptionKey},
	// Old legacy files contain these, so we skip them to shut up warnings
	{ "firingforce",		WPN_FuncSkip },
	{ "chargeforce",		WPN_FuncSkip },
	{ "altchargeforce",	WPN_FuncSkip },
	{ "selectforce",		WPN_FuncSkip },
};

/*===========================================================================
	Defaults Values
===========================================================================*/

float defaultsWeaponSpeed[][2] =
{
	{ 0,0 },//WP_NONE,
	{ 0,0 },//WP_SABER,				 // NOTE: lots of code assumes this is the first weapon (... which is crap) so be careful -Ste.
	{ BRYAR_PISTOL_VEL,BRYAR_PISTOL_VEL },//WP_BLASTER_PISTOL,
	{ BLASTER_VELOCITY,BLASTER_VELOCITY },//WP_BLASTER,
	{ Q3_INFINITE,Q3_INFINITE },//WP_DISRUPTOR,
	{ BOWCASTER_VELOCITY,BOWCASTER_VELOCITY },//WP_BOWCASTER,
	{ REPEATER_VELOCITY,REPEATER_ALT_VELOCITY },//WP_REPEATER,
	{ DEMP2_VELOCITY,DEMP2_ALT_RANGE },//WP_DEMP2,
	{ FLECHETTE_VEL,FLECHETTE_MINE_VEL },//WP_FLECHETTE,
	{ ROCKET_VELOCITY,ROCKET_ALT_VELOCITY },//WP_ROCKET_LAUNCHER,
	{ TD_VELOCITY,TD_ALT_VELOCITY },//WP_THERMAL,
	{ 0,0 },//WP_TRIP_MINE,
	{ 0,0 },//WP_DET_PACK,
	{ CONC_VELOCITY,Q3_INFINITE },//WP_CONCUSSION,
	{ 0,0 },//WP_MELEE,			// Any ol' melee attack
	{ 0,0 },//WP_STUN_BATON,
	{ BRYAR_PISTOL_VEL,BRYAR_PISTOL_VEL },//WP_BRYAR_PISTOL,
	{ EMPLACED_VEL,EMPLACED_VEL },//WP_EMPLACED_GUN,
	{ BRYAR_PISTOL_VEL,BRYAR_PISTOL_VEL },//WP_BOT_LASER,		// Probe droid	- Laser blast
	{ 0,0 },//WP_TURRET,			// turret guns
	{ ATST_MAIN_VEL,ATST_MAIN_VEL },//WP_ATST_MAIN,
	{ ATST_SIDE_MAIN_VELOCITY,ATST_SIDE_ALT_NPC_VELOCITY },//WP_ATST_SIDE,
	{ EMPLACED_VEL,EMPLACED_VEL },//WP_TIE_FIGHTER,
	{ EMPLACED_VEL,REPEATER_ALT_VELOCITY },//WP_RAPID_FIRE_CONC,
	{ 0,0 },//WP_JAWA,
	{ TUSKEN_RIFLE_VEL,TUSKEN_RIFLE_VEL },//WP_TUSKEN_RIFLE,
	{ 0,0 },//WP_TUSKEN_STAFF,
	{ 0,0 },//WP_SCEPTER,
	{ 0,0 },//WP_NOGHRI_STICK,
	{ E5_VELOCITY, E5_VELOCITY }, // WP_BATTLEDROID,
	{ F_11D_VELOCITY, F_11D_VELOCITY },// WP_THEFIRSTORDER,
	{ CLONECARBINE_VELOCITY, CLONECARBINE_VELOCITY },// WP_CLONECARBINE,
	{ CLONERIFLE_VELOCITY, CLONERIFLE_VELOCITY },// WP_CLONERIFLE
	{ REBELBLASTER_VELOCITY, REBELBLASTER_VELOCITY },// WP_REBELBLASTER
	{ CLONECOMMANDO_VELOCITY, CLONECOMMANDO_VELOCITY },// WP_CLONECOMMANDO
	{ REBELRIFLE_VELOCITY, REBELRIFLE_VELOCITY },// WP_REBELRIFLE
	{ REY_VEL,REY_VEL },//WP_REY,
	{ JANGO_VELOCITY, JANGO_VELOCITY },// WP_JANGO
	{ BOBA_VELOCITY, BOBA_VELOCITY },// WP_BOBA
	{ CLONEPISTOL_VELOCITY, CLONEPISTOL_VELOCITY },// WP_CLONEPISTOL
};


const char* defaultDescriptionKeys[WP_HC_NUM_WEAPONS] =
{
"",
"SABER_DESC",
"NEW_BLASTER_PISTOL_DESC",
"BLASTER_RIFLE_DESC",
"DISRUPTOR_RIFLE_DESC",
"BOWCASTER_DESC",
"HEAVYREPEATER_DESC",
"DEMP2_DESC",
"FLECHETTE_DESC",
"MERR_SONN_DESC",
"THERMAL_DETONATOR_DESC",
"TRIP_MINE_DESC",
"DET_PACK_DESC",
"CONCUSSION_DESC",
"MELEE_DESC",
"ATST_MAIN_DESC",
"ATST_SIDE_DESC",
"STUN_BATON_DESC",
"BLASTER_PISTOL_DESC",
"EMPLACED_GUN_DESC",
"BOT_LASER_DESC",
"TURRET_DESC",
"TIE_FIGHTER_DESC",
"RAPID_CONCUSSION_DESC",
"JAWA_DESC",
"TUSKEN_RIFLE_DESC",
"TUSKEN_STAFF_DESC",
"SCEPTER_DESC",
"NOGHRI_STICK_DESC",
"BATTLEDROID_DESC",
"THEFIRSTORDER_DESC",
"CLONECARBINE_DESC",
"REBELBLASTER_DESC",
"CLONERIFLE_DESC",
"CLONECOMMANDO_DESC",
"REBELRIFLE_DESC",
"REY_DESC",
"JANGO_DESC",
"BOBA_DESC",
"CLONEPISTOL_DESC",
};

const qboolean defaultPlayerUsable[] = {
	qtrue,//WP_NONE,

	// Player weapons
	qtrue,//WP_SABER,
	qtrue,//WP_BLASTER_PISTOL,	// player and NPC weapon
	qtrue,//WP_BLASTER,			// player and NPC weapon
	qtrue,//WP_DISRUPTOR,		// player and NPC weapon
	qtrue,//WP_BOWCASTER,		// NPC weapon - player can pick this up, but never starts with them
	qtrue,//WP_REPEATER,		// NPC weapon - player can pick this up, but never starts with them
	qtrue,//WP_DEMP2,			// NPC weapon - player can pick this up, but never starts with them
	qtrue,//WP_FLECHETTE,		// NPC weapon - player can pick this up, but never starts with them
	qtrue,//WP_ROCKET_LAUNCHER,	// NPC weapon - player can pick this up, but never starts with them
	qtrue,//WP_THERMAL,			// player and NPC weapon
	qtrue,//WP_TRIP_MINE,		// NPC weapon - player can pick this up, but never starts with them
	qtrue,//WP_DET_PACK,		// NPC weapon - player can pick this up, but never starts with them
	qtrue,//WP_CONCUSSION,		// NPC weapon - player can pick this up, but never starts with them

	//extras
	qtrue,//WP_MELEE,			// player and NPC weapon - Any ol' melee attack

	//when in atst
	qtrue,//WP_ATST_MAIN,
	qtrue,//WP_ATST_SIDE,

	// These can never be gotten directly by the player
	qtrue,//WP_STUN_BATON,		// stupid weapon, should remove

	//NPC weapons
	qtrue,//WP_BRYAR_PISTOL,	// NPC weapon - player can pick this up, but never starts with them

	qfalse,//WP_EMPLACED_GUN,

	qfalse,//WP_BOT_LASER,		// Probe droid	- Laser blast

	qfalse,//WP_TURRET,			// turret guns

	qfalse,//WP_TIE_FIGHTER,

	qfalse,//WP_RAPID_FIRE_CONC,

	qfalse,//WP_JAWA,
	qtrue,//WP_TUSKEN_RIFLE,
	qfalse,//WP_TUSKEN_STAFF,
	qfalse,//WP_SCEPTER,
	qtrue,//WP_NOGHRI_STICK,

	qtrue,//WP_BATTLEDROID
	qtrue,//WP_THEFIRSTORDER,
	qtrue,//WP_CLONECARBINE,
	qtrue,//WP_REBELBLASTER,
	qtrue,//WP_CLONERIFLE,
	qtrue,//WP_CLONECOMMANDO,
	qtrue,//WP_REBELRIFLE,
	qtrue,//WP_REY,
	qtrue,//WP_JANGO,
	qtrue,//WP_BOBA,
	qtrue,//WP_CLONEPISTOL
};

const weaponCategory_t defaultWeaponType[] = {
	WC_NONE,//WP_NONE,

	// Player weapons
	WC_MELEE_1H,//WP_SABER,
	WC_PISTOL,//WP_BLASTER_PISTOL,	// player and NPC weapon
	WC_LIGHT,//WP_BLASTER,			// player and NPC weapon
	WC_LIGHT,//WP_DISRUPTOR,		// player and NPC weapon
	WC_HEAVY,//WP_BOWCASTER,		// NPC weapon - player can pick this up, but never starts with them
	WC_HEAVY,//WP_REPEATER,		// NPC weapon - player can pick this up, but never starts with them
	WC_LIGHT,//WP_DEMP2,			// NPC weapon - player can pick this up, but never starts with them
	WC_HEAVY,//WP_FLECHETTE,		// NPC weapon - player can pick this up, but never starts with them
	WC_HEAVY,//WP_ROCKET_LAUNCHER,	// NPC weapon - player can pick this up, but never starts with them
	WC_GRENADE,//WP_THERMAL,			// player and NPC weapon
	WC_EXPLOSIVE,//WP_TRIP_MINE,		// NPC weapon - player can pick this up, but never starts with them
	WC_EXPLOSIVE,//WP_DET_PACK,		// NPC weapon - player can pick this up, but never starts with them
	WC_HEAVY,//WP_CONCUSSION,		// NPC weapon - player can pick this up, but never starts with them

	//extras
	WC_MELEE,//WP_MELEE,			// player and NPC weapon - Any ol' melee attack

	//when in atst
	WC_NONE,//WP_ATST_MAIN,
	WC_NONE,//WP_ATST_SIDE,

	// These can never be gotten directly by the player
	WC_STUN_BATON,//WP_STUN_BATON,		// Putting WC_MELEE_1H make it like the gaffi stick...

	//NPC weapons
	WC_PISTOL,//WP_BRYAR_PISTOL,	// NPC weapon - player can pick this up, but never starts with them

	WC_NONE,//WP_EMPLACED_GUN,

	WC_NONE,//WP_BOT_LASER,		// Probe droid	- Laser blast

	WC_NONE,//WP_TURRET,			// turret guns

	WC_NONE,//WP_TIE_FIGHTER,

	WC_HEAVY,//WP_RAPID_FIRE_CONC,

	WC_LIGHT,//WP_JAWA,
	WC_LIGHT,//WP_TUSKEN_RIFLE,
	WC_MELEE_1H,//WP_TUSKEN_STAFF,
	WC_MELEE_2H,//WP_SCEPTER,
	WC_MELEE_2H,//WP_NOGHRI_STICK,

	WC_LIGHT,//WP_BATTLEDROID
	WC_LIGHT,//WP_THEFIRSTORDER,
	WC_LIGHT,//WP_CLONECARBINE,
	WC_LIGHT,//WP_REBELBLASTER,
	WC_HEAVY,//WP_CLONERIFLE,
	WC_HEAVY,//WP_CLONECOMMANDO,
	WC_HEAVY,//WP_REBELRIFLE,
	WC_PISTOL,//WP_REY,
	WC_PISTOL,//WP_JANGO,
	WC_HEAVY,//WP_BOBA,
	WC_PISTOL,//WP_CLONEPISTOL
};

const int defaultDamage[] = {
	0,							// WP_NONE
	0,							// WP_SABER				// handled elsewhere
	BRYAR_PISTOL_DAMAGE,		// WP_BLASTER_PISTOL
	BLASTER_DAMAGE,				// WP_BLASTER
	DISRUPTOR_MAIN_DAMAGE,		// WP_DISRUPTOR
	BOWCASTER_DAMAGE,			// WP_BOWCASTER
	REPEATER_DAMAGE,			// WP_REPEATER
	DEMP2_DAMAGE,				// WP_DEMP2
	FLECHETTE_DAMAGE,			// WP_FLECHETTE
	ROCKET_DAMAGE,				// WP_ROCKET_LAUNCHER
	TD_DAMAGE,					// WP_THERMAL
	LT_DAMAGE,					// WP_TRIP_MINE
	FLECHETTE_MINE_DAMAGE,		// WP_DET_PACK			// HACK, this is what the code sez.
	CONC_DAMAGE,				// WP_CONCUSSION

	0,							// WP_MELEE				// handled by the melee attack function

	ATST_MAIN_DAMAGE,			// WP_ATST_MAIN
	ATST_SIDE_MAIN_DAMAGE,		// WP_ATST_SIDE

	STUN_BATON_DAMAGE,			// WP_STUN_BATON

	BRYAR_PISTOL_DAMAGE,		// WP_BRYAR_PISTOL
	EMPLACED_DAMAGE,			// WP_EMPLACED_GUN
	BRYAR_PISTOL_DAMAGE,		// WP_BOT_LASER
	0,							// WP_TURRET			// handled elsewhere
	EMPLACED_DAMAGE,			// WP_TIE_FIGHTER
	EMPLACED_DAMAGE,			// WP_RAPID_FIRE_CONC,

	BRYAR_PISTOL_DAMAGE,		// WP_JAWA
	0,							// WP_TUSKEN_RIFLE
	0,							// WP_TUSKEN_STAFF
	0,							// WP_SCEPTER
	0,							// WP_NOGHRI_STICK

	E5_DAMAGE,					// WP_BATTLEDROID
	F_11D_DAMAGE,				// WP_THEFIRSTORDER
	CLONECARBINE_DAMAGE,		// WP_CLONECARBINE
	REBELBLASTER_DAMAGE,		// WP_REBELBLASTER
	CLONERIFLE_DAMAGE,			// WP_CLONERIFLE
	CLONECOMMANDO_DAMAGE,		// WP_CLONECOMMANDO
	REBELRIFLE_DAMAGE,			// WP_REBELRIFLE
	REY_DAMAGE,					// WP_REY
	JANGO_DAMAGE,				// WP_JANGO
	BOBA_DAMAGE,				// WP_BOBA
	CLONEPISTOL_DAMAGE,			// WP_CLONEPISTOL
};

const int defaultAltDamage[] = {
	0,						// WP_NONE
	0,						// WP_SABER					// handled elsewhere
	BRYAR_PISTOL_DAMAGE,	// WP_BLASTER_PISTOL
	BLASTER_DAMAGE,			// WP_BLASTER
	DISRUPTOR_ALT_DAMAGE,	// WP_DISRUPTOR
	BOWCASTER_DAMAGE,		// WP_BOWCASTER
	REPEATER_ALT_DAMAGE,	// WP_REPEATER
	DEMP2_ALT_DAMAGE,		// WP_DEMP2
	FLECHETTE_ALT_DAMAGE,	// WP_FLECHETTE
	ROCKET_DAMAGE,			// WP_ROCKET_LAUNCHER
	TD_ALT_DAMAGE,			// WP_THERMAL
	LT_DAMAGE,				// WP_TRIP_MINE
	FLECHETTE_MINE_DAMAGE,	// WP_DET_PACK				// HACK, this is what the code sez.
	CONC_ALT_DAMAGE,		// WP_CONCUSION

	0,						// WP_MELEE					// handled by the melee attack function

	ATST_MAIN_DAMAGE,		// WP_ATST_MAIN
	ATST_SIDE_ALT_DAMAGE,	// WP_ATST_SIDE

	STUN_BATON_ALT_DAMAGE,	// WP_STUN_BATON

	BRYAR_PISTOL_DAMAGE,	// WP_BRYAR_PISTOL
	EMPLACED_DAMAGE,		// WP_EMPLACED_GUN
	BRYAR_PISTOL_DAMAGE,	// WP_BOT_LASER
	0,						// WP_TURRET				// handled elsewhere
	EMPLACED_DAMAGE,		// WP_TIE_FIGHTER
	0,						// WP_RAPID_FIRE_CONC		// repeater alt damage is used instead

	BRYAR_PISTOL_DAMAGE,	// WP_JAWA
	0,						// WP_TUSKEN_RIFLE
	0,						// WP_TUSKEN_STAFF
	0,						// WP_SCEPTER
	0,						// WP_NOGHRI_STICK

	E5_ALT_DAMAGE,			// WP_BATTLEDROID
	F_11D_SCOPE_DAMAGE,		// WP_THEFIRSTORDER
	CLONECARBINE_ALT_DAMAGE, // WP_CLONECARBINE
	REBELBLASTER_SCOPE_DAMAGE,// WP_REBELBLASTER
	CLONERIFLE_ALT_DAMAGE,		// WP_CLONERIFLE
	CLONECOMMANDO_ALT_DAMAGE,// WP_CLONECOMMANDO
	REBELRIFLE_SCOPE_DAMAGE,// WP_REBELRIFLE
	REY_DAMAGE,				// WP_REY
	JANGO_ALT_DAMAGE,		// WP_JANGO
	BOBA_SCOPE_DAMAGE,		// WP_BOBA
	CLONEPISTOL_ALT_DAMAGE,		// WP_CLONEPISTOL
};

const int defaultSplashDamage[] = {
	0,								// WP_NONE
	0,								// WP_SABER
	0,								// WP_BLASTER_PISTOL
	0,								// WP_BLASTER
	0,								// WP_DISRUPTOR
	BOWCASTER_SPLASH_DAMAGE,		// WP_BOWCASTER
	0,								// WP_REPEATER
	0,								// WP_DEMP2
	0,								// WP_FLECHETTE
	ROCKET_SPLASH_DAMAGE,			// WP_ROCKET_LAUNCHER
	TD_SPLASH_DAM,					// WP_THERMAL
	LT_SPLASH_DAM,					// WP_TRIP_MINE
	FLECHETTE_MINE_SPLASH_DAMAGE,	// WP_DET_PACK		// HACK, this is what the code sez.
	CONC_SPLASH_DAMAGE,				// WP_CONCUSSION

	0,								// WP_MELEE

	0,								// WP_ATST_MAIN
	ATST_SIDE_MAIN_SPLASH_DAMAGE,	// WP_ATST_SIDE

	0,								// WP_STUN_BATON

	0,								// WP_BRYAR_PISTOL
	0,								// WP_EMPLACED_GUN
	0,								// WP_BOT_LASER
	0,								// WP_TURRET
	0,								// WP_TIE_FIGHTER
	0,								// WP_RAPID_FIRE_CONC

	0,								// WP_JAWA
	0,								// WP_TUSKEN_RIFLE
	0,								// WP_TUSKEN_STAFF
	0,								// WP_SCEPTER
	0,								// WP_NOGHRI_STICK

	0,								// WP_BATTLEDROID
	0,				   				// WP_THEFIRSTORDER
	0,				   				// WP_CLONECARBINE
	0,				   				// WP_REBELBLASTER
	0,				   				// WP_CLONERIFLE
	0,				   				// WP_CLONECOMMANDO
	0,						   	 	// WP_REBELRIFLE
	0,				   				// WP_REY
	0,				   				// WP_JANGO
	0,				   				// WP_BOBA
	0,				   				// WP_CLONEPISTOL
};

const float defaultSplashRadius[] = {
	0.0f,							// WP_NONE
	0.0f,							// WP_SABER
	0.0f,							// WP_BLASTER_PISTOL
	0.0f,							// WP_BLASTER
	0.0f,							// WP_DISRUPTOR
	BOWCASTER_SPLASH_RADIUS,		// WP_BOWCASTER
	0.0f,							// WP_REPEATER
	0.0f,							// WP_DEMP2
	0.0f,							// WP_FLECHETTE
	ROCKET_SPLASH_RADIUS,			// WP_ROCKET_LAUNCHER
	TD_SPLASH_RAD,					// WP_THERMAL
	LT_SPLASH_RAD,					// WP_TRIP_MINE
	FLECHETTE_MINE_SPLASH_RADIUS,	// WP_DET_PACK		// HACK, this is what the code sez.
	CONC_SPLASH_RADIUS,				// WP_CONCUSSION

	0.0f,							// WP_MELEE

	0.0f,							// WP_ATST_MAIN
	ATST_SIDE_MAIN_SPLASH_RADIUS,	// WP_ATST_SIDE

	0.0f,							// WP_STUN_BATON

	0.0f,							// WP_BRYAR_PISTOL
	0.0f,							// WP_EMPLACED_GUN
	0.0f,							// WP_BOT_LASER
	0.0f,							// WP_TURRET
	0.0f,							// WP_TIE_FIGHTER
	0.0f,							// WP_RAPID_FIRE_CONC

	0.0f,							// WP_JAWA
	0.0f,							// WP_TUSKEN_RIFLE
	0.0f,							// WP_TUSKEN_STAFF
	0.0f,							// WP_SCEPTER
	0.0f,							// WP_NOGHRI_STICK

	0.0f,							// WP_BATTLEDROID
	0.0f,							// WP_THEFIRSTORDER
	0.0f,							// WP_CLONECARBINE
	0.0f,							// WP_REBELBLASTER
	0.0f,							// WP_CLONERIFLE
	0.0f,							// WP_CLONECOMMANDO
	0.0f,							// WP_REBELRIFLE
	0.0f,							// WP_REY
	0.0f,							// WP_JANGO
	0.0f,							// WP_BOBA
	0.0f,							// WP_CLONEPISTOL
};

const int defaultAltSplashDamage[] = {
	0,								// WP_NONE
	0,								// WP_SABER			// handled elsewhere
	0,								// WP_BLASTER_PISTOL
	0,								// WP_BLASTER
	0,								// WP_DISRUPTOR
	BOWCASTER_SPLASH_DAMAGE,		// WP_BOWCASTER
	REPEATER_ALT_SPLASH_DAMAGE,		// WP_REPEATER
	DEMP2_ALT_DAMAGE,				// WP_DEMP2
	FLECHETTE_ALT_SPLASH_DAM,		// WP_FLECHETTE
	ROCKET_SPLASH_DAMAGE,			// WP_ROCKET_LAUNCHER
	TD_ALT_SPLASH_DAM,				// WP_THERMAL
	TD_ALT_SPLASH_DAM,				// WP_TRIP_MINE
	FLECHETTE_MINE_SPLASH_DAMAGE,	// WP_DET_PACK		// HACK, this is what the code sez.
	0,								// WP_CONCUSSION

	0,								// WP_MELEE			// handled by the melee attack function

	0,								// WP_ATST_MAIN
	ATST_SIDE_ALT_SPLASH_DAMAGE,	// WP_ATST_SIDE

	0,								// WP_STUN_BATON

	0,								// WP_BRYAR_PISTOL
	0,								// WP_EMPLACED_GUN
	0,								// WP_BOT_LASER
	0,								// WP_TURRET		// handled elsewhere
	0,								// WP_TIE_FIGHTER
	0,								// WP_RAPID_FIRE_CONC

	0,								// WP_JAWA
	0,								// WP_TUSKEN_RIFLE
	0,								// WP_TUSKEN_STAFF
	0,								// WP_SCEPTER
	0,								// WP_NOGHRI_STICK

	0,								// WP_BATTLEDROID
	0,								// WP_THEFIRSTORDER
	0,								// WP_CLONECARBINE
	0,								// WP_REBELBLASTER
	0,				   				// WP_CLONERIFLE
	CLONECOMMANDO_ALT_SPLASH_DAMAGE,// WP_CLONECOMMANDO
	0,				   				// WP_REBELRIFLE
	0,				   				// WP_REY
	0,				   				// WP_JANGO
	0,				   				// WP_BOBA
	0,				   				// WP_CLONEPISTOL
};

const float defaultAltSplashRadius[] = {
	0.0f,							// WP_NONE
	0.0f,							// WP_SABER		// handled elsewhere
	0.0f,							// WP_BLASTER_PISTOL
	0.0f,							// WP_BLASTER
	0.0f,							// WP_DISRUPTOR
	BOWCASTER_SPLASH_RADIUS,		// WP_BOWCASTER
	REPEATER_ALT_SPLASH_RADIUS,		// WP_REPEATER
	DEMP2_ALT_SPLASHRADIUS,			// WP_DEMP2
	FLECHETTE_ALT_SPLASH_RAD,		// WP_FLECHETTE
	ROCKET_SPLASH_RADIUS,			// WP_ROCKET_LAUNCHER
	TD_ALT_SPLASH_RAD,				// WP_THERMAL
	LT_SPLASH_RAD,					// WP_TRIP_MINE
	FLECHETTE_ALT_SPLASH_RAD,		// WP_DET_PACK		// HACK, this is what the code sez.
	0.0f,							// WP_CONCUSSION

	0.0f,							// WP_MELEE			// handled by the melee attack function

	0.0f,							// WP_ATST_MAIN
	ATST_SIDE_ALT_SPLASH_RADIUS,	// WP_ATST_SIDE

	0.0f,							// WP_STUN_BATON

	0.0f,							// WP_BRYAR_PISTOL
	0.0f,							// WP_EMPLACED_GUN
	0.0f,							// WP_BOT_LASER
	0.0f,							// WP_TURRET		// handled elsewhere
	0.0f,							// WP_TIE_FIGHTER
	0.0f,							// WP_RAPID_FIRE_CONC

	0.0f,							// WP_JAWA
	0.0f,							// WP_TUSKEN_RIFLE
	0.0f,							// WP_TUSKEN_STAFF
	0.0f,							// WP_SCEPTER
	0.0f,							// WP_NOGHRI_STICK

	0.0f,							// WP_BATTLEDROID
	0.0f,							// WP_THEFIRSTORDER
	0.0f,							// WP_CLONECARBINE
	0.0f,							// WP_REBELBLASTER
	0.0f,							// WP_CLONERIFLE
	CLONECOMMANDO_ALT_SPLASH_RADIUS,// WP_CLONECOMMANDO
	0.0f,							// WP_REBELRIFLE
	0.0f,							// WP_REY
	0.0f,							// WP_JANGO
	0.0f,							// WP_BOBA
	0.0f,							// WP_CLONEPISTOL
};
#pragma endregion

static const size_t numWpnParms = ARRAY_LEN(WpnParms);