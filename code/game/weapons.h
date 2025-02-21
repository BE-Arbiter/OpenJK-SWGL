/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

// Filename:-	weapons.h
//
// Note that this is now included from both server and game modules, so don't include any other header files
//	within this one that might break stuff...

#ifndef __WEAPONS_H__
#define __WEAPONS_H__

#include "../qcommon/q_shared.h"

typedef enum //# weapon_e
{
	WP_NONE,

	// Player weapons
	WP_SABER,			// player and NPC weapon
	WP_BLASTER_PISTOL,	// player and NPC weapon
	WP_BLASTER,			// player and NPC weapon
	WP_DISRUPTOR,		// player and NPC weapon
	WP_BOWCASTER,		// NPC weapon - player can pick this up, but never starts with them
	WP_REPEATER,		// NPC weapon - player can pick this up, but never starts with them
	WP_DEMP2,			// NPC weapon - player can pick this up, but never starts with them
	WP_FLECHETTE,		// NPC weapon - player can pick this up, but never starts with them
	WP_ROCKET_LAUNCHER,	// NPC weapon - player can pick this up, but never starts with them
	WP_THERMAL,			// player and NPC weapon
	WP_TRIP_MINE,		// NPC weapon - player can pick this up, but never starts with them
	WP_DET_PACK,		// NPC weapon - player can pick this up, but never starts with them
	WP_CONCUSSION,		// NPC weapon - player can pick this up, but never starts with them

	//extras
	WP_MELEE,			// player and NPC weapon - Any ol' melee attack

	//when in atst
	WP_ATST_MAIN,
	WP_ATST_SIDE,

	// These can never be gotten directly by the player
	WP_STUN_BATON,		// stupid weapon, should remove

	//NPC weapons
	WP_BRYAR_PISTOL,	// NPC weapon - player can pick this up, but never starts with them

	WP_EMPLACED_GUN,

	WP_BOT_LASER,		// Probe droid	- Laser blast

	WP_TURRET,			// turret guns

	WP_TIE_FIGHTER,

	WP_RAPID_FIRE_CONC,

	WP_JAWA,
	WP_TUSKEN_RIFLE,
	WP_TUSKEN_STAFF,
	WP_SCEPTER,
	WP_NOGHRI_STICK,

	WP_BATTLEDROID,
	WP_THEFIRSTORDER,
	WP_CLONECARBINE,
	WP_REBELBLASTER,
	WP_CLONERIFLE,
	WP_CLONECOMMANDO,
	WP_REBELRIFLE,
	WP_REY,
	WP_JANGO,
	WP_BOBA,
	WP_CLONEPISTOL,
	WP_CIS_SNIPER,
	WP_SBD,
	WP_DROIDEKA,

	//# #eol
	WP_HC_NUM_WEAPONS
} weapon_t;

#define FIRST_WEAPON		WP_SABER		// this is the first weapon for next and prev weapon switching


typedef enum
{
	FL_NONE = 0,
	FL_MELEE,
	FL_BLASTER,
	FL_BLASTER_CHARGED,
	FL_BOWCASTER,
	FL_BEAM,
	FL_FULL_BEAM,
	FL_BEAM_CHARGED,
	FL_GRENADE_LAUNCHER,
	FL_DEMP2,
	FL_DEMP2_ALT,
	FL_FLECHETTE,
	FL_FLECHETTE_ALT,
	FL_NOGHRI,
	FL_MISSILE,
	FL_MISSILE_AIMED,
	FL_LASER_TRAP,
	FL_PROXIMITY_TRAP,
	FL_EXPLOSIVES,
	FL_GRENADE,
	FL_IMPACT_GRENADE,
	FL_STUNBATON,
	FL_SBD,
	FL_OTHER, // For not yet done weapon
}firingLogic_t;

/*Weapon Bucket for Cycling*/
typedef enum {
	WB_MELEE = -1, //1 Saber, Melee, Stun_Baton
	WB_PISTOLS = -2, //2 All One handed weapons
	WB_BLASTERS = -3, //3 All Light Blasters
	WB_SPECIALISTS = -4, //4 ALL Specialist Weapons (Flechette,Bowcaster,Disruptors)
	WB_HEAVY_WEAPONS = -5, //5 Rocket Launcher, Concussion Rifle,...
	WB_THROWABLES = -6, //6 Explosives & Grenades
	WB_OTHERS = -7, //8 Weapon For Vehicle and npc which should not appears in others buckets
	WB_UNSET = 0 // This No weapon should have this bucket...
} weaponBucket_t;

// AMMO_NONE must be first and AMMO_MAX must be last, cause weapon load validates based off of these vals
typedef enum //# ammo_e
{
	AMMO_NONE,
	AMMO_FORCE,		// AMMO_PHASER
	AMMO_BLASTER,	// AMMO_STARFLEET,
	AMMO_POWERCELL,	// AMMO_ALIEN,
	AMMO_METAL_BOLTS,
	AMMO_ROCKETS,
	AMMO_EMPLACED,
	AMMO_THERMAL,
	AMMO_TRIPMINE,
	AMMO_DETPACK,
	AMMO_HC_MAX
} ammo_t;

typedef enum firingType_s
{
	FT_AUTOMATIC = 1,
	FT_SEMI,
	FT_BURST,
	FT_HIGH_POWERED
} firingType_t;


typedef enum scopeType_s
{
	ST_NONE = 0,
	ST_DISRUPTOR = 2,
	ST_A280 = 4,
	ST_DC17M,
	ST_EE3,
	ST_F11D,
	ST_E5
} scopeType_t;


enum firingOptions
{
	FIRING_TYPE,
	SHOTS_PER_BURST,
	BURST_FIRE_DELAY
};

typedef struct weaponIndexes_s
{
	char weaponClass[32];
	int index;
} weaponIndexes_t;

typedef enum{
	WC_NONE, //Only for weapon none else is "unset"
	WC_MELEE, //Like melee
	WC_STUN_BATON, //For this specific weapons. Is handled by "other case"
	WC_MELEE_1H, //Like Saber
	WC_MELEE_2H, //Like nogri staff
	WC_PISTOL, // Like bryar
	WC_LIGHT, // Like blaster
	WC_SNIPER, // Like Disruptor
	WC_HEAVY, //Like Bowcaster or rocket launcher
	WC_GRENADE, //Like thermal
	WC_EXPLOSIVE, //Like detpack
	WC_MINIGUN //Like a Z6
} weaponCategory_t;

typedef struct weaponAttackData_s
{
	firingLogic_t firingLogic; //The method of fire for this attack

	/* Base Data */
	int		energyPerShot;		// Amount of energy used per shot
	int		fireTime;			// Amount of time between firings
	int		range;				// Range of weapon
	float 	spread;				// Accuracy of shots
	int		damage;				// Damage per shot
	int		defaultDamage;		// Default damage per shot
	float	mVelocity;			// Speed of missile
	int 	fireOption[3];		// Option for the fire (Type, Projectile count,...)

	/* Splash Damage */
	int		splashDamage;		// Splash damage when shot explodes
	float	splashRadius;		// Splash radius when shot explodes

	/* Effects */
	void* missileFunc;		// "FX for the missile"
	char	projectileEffect[64]; // Effect to Override the base Projectile
	char	hitWallEffect[64];  
	char	hitWallEffect2[64]; 
	char	hitWallEffect3[64];
	char	hitDroidEffect[64];
	char	hitFleshEffect[64];
	char	muzzleEffect[64];  // Effect to Override the base muzzle
	char	explosionEffect[64]; // For explosives
	char    shockwaveEffect[64]; // For explosives


	/* Beam Effects */
	char	beamShader[64];
	vec3_t  beamColor;
	char	fullBeamShader[64];
	vec3_t  fullBeamColor;
	
	/* Blaster Bounce Data */
	qboolean bounceWall;		//BounceOnWalls;
	int		bounceCount;		//BounceCounts

	/* Charged Shot Data */
	float	chargeUnitTime;		// Time to Charge one unit of power
	int		maxChargeUnits;		// Max amount of charge you can have.
	char	chargeSnd[64];		// sound to start when the weapon initiates the charging sequence
	char	chargeMuzzleShader[64];


	/* Physical Missile Data */
	char	missileMdl[64];		// Missile Model
	char	missileSound[64];	// Missile flight sound
	float  	missileDlight;		// what is says
	vec3_t 	missileDlightColor;	// ditto
	char	firingSnd[64];		// Sound made when fired
	char	missileHitSound[64];	// Missile impact sound



} weaponAttackData_t;

#define MAX_WEAPON_ATTACKS 4

typedef struct weaponData_s
{
	weaponAttackData_t attackData[MAX_WEAPON_ATTACKS];

	char	classname[32];		// Spawning name
	char	baseclass[32];		// Base Weapon
	int		baseWeaponNum;		// Base WeaponNum;
	char	weaponMdl[64];		// Weapon Model
	char	stopSnd[64];		// Sound made when weapon stops firing
	char	selectSnd[64];		// the sound to play when this weapon gets selected

	int		ammoIndex;			// Index to proper ammo slot
	int		ammoLow;			// Count when ammo is low

	char	weaponIcon[64];		// Name of weapon icon file
	int		numBarrels;			// how many barrels should we expect for this weapon?

	int 	scopeType;

	char	weaponMdl2[64];
	qboolean secondaryMdl;
	qboolean playerUsable;
	weaponCategory_t weaponCategory;
	weaponBucket_t weaponBucket;

} weaponData_t;


typedef struct ammoData_s
{
	char	icon[64];	// Name of ammo icon file
	int		giveWeaponIndex;
	int		max;		// Max amount player can hold of ammo
} ammoData_t;


// High Powered
//--------------
#define HIGH_POWERED_DAMAGE			200

// Attack Options
//--------
#define MAIN_ATTACK 		1
#define ALT_ATTACK			2
#define TERTIARY_ATTACK 	4

// Bryar Pistol
//--------
#define BRYAR_PISTOL_VEL			1800
#define BRYAR_PISTOL_DAMAGE			14
#define BRYAR_CHARGE_UNIT			200.0f	// bryar charging gives us one more unit every 200ms--if you change this, you'll have to do the same in bg_pmove

// E11 Blaster
//---------
#define BLASTER_MAIN_SPREAD			0.5f
#define BLASTER_ALT_SPREAD			1.5f
#define BLASTER_NPC_SPREAD			0.5f
#define BLASTER_VELOCITY			2300
#define BLASTER_NPC_VEL_CUT			0.5f
#define BLASTER_NPC_HARD_VEL_CUT	0.7f
#define BLASTER_DAMAGE				20
#define	BLASTER_NPC_DAMAGE_EASY		6
#define	BLASTER_NPC_DAMAGE_NORMAL	12 // 14
#define	BLASTER_NPC_DAMAGE_HARD		16 // 18

// Tenloss Disruptor
//----------
#define DISRUPTOR_MAIN_DAMAGE			14
#define DISRUPTOR_NPC_MAIN_DAMAGE_EASY	5
#define DISRUPTOR_NPC_MAIN_DAMAGE_MEDIUM	10
#define DISRUPTOR_NPC_MAIN_DAMAGE_HARD	15

#define DISRUPTOR_ALT_DAMAGE			12
#define DISRUPTOR_NPC_ALT_DAMAGE_EASY	15
#define DISRUPTOR_NPC_ALT_DAMAGE_MEDIUM	25
#define DISRUPTOR_NPC_ALT_DAMAGE_HARD	30
#define DISRUPTOR_ALT_TRACES			3		// can go through a max of 3 entities
#define DISRUPTOR_CHARGE_UNIT			150.0f	// distruptor charging gives us one more unit every 150ms--if you change this, you'll have to do the same in bg_pmove

// Wookie Bowcaster
//----------
#define	BOWCASTER_DAMAGE			45
#define	BOWCASTER_VELOCITY			1300
#define	BOWCASTER_NPC_DAMAGE_EASY	12
#define	BOWCASTER_NPC_DAMAGE_NORMAL	24
#define	BOWCASTER_NPC_DAMAGE_HARD	36
#define BOWCASTER_SPLASH_DAMAGE		0
#define BOWCASTER_SPLASH_RADIUS		0
#define BOWCASTER_SIZE				2

#define BOWCASTER_ALT_SPREAD		5.0f
#define BOWCASTER_VEL_RANGE			0.3f
#define BOWCASTER_CHARGE_UNIT		200.0f	// bowcaster charging gives us one more unit every 200ms--if you change this, you'll have to do the same in bg_pmove

// Heavy Repeater
//----------
#define REPEATER_SPREAD				1.4f
#define REPEATER_NPC_SPREAD			0.7f
#define	REPEATER_DAMAGE				8
#define	REPEATER_VELOCITY			1600
#define	REPEATER_NPC_DAMAGE_EASY	2
#define	REPEATER_NPC_DAMAGE_NORMAL	4
#define	REPEATER_NPC_DAMAGE_HARD	6

#define REPEATER_ALT_SIZE				3	// half of bbox size
#define	REPEATER_ALT_DAMAGE				60
#define REPEATER_ALT_SPLASH_DAMAGE		60
#define REPEATER_ALT_SPLASH_RADIUS		128
#define	REPEATER_ALT_VELOCITY			1100
#define	REPEATER_ALT_NPC_DAMAGE_EASY	15
#define	REPEATER_ALT_NPC_DAMAGE_NORMAL	30
#define	REPEATER_ALT_NPC_DAMAGE_HARD	45

// DEMP2
//----------
#define	DEMP2_DAMAGE				15
#define	DEMP2_VELOCITY				1800
#define	DEMP2_NPC_DAMAGE_EASY		6
#define	DEMP2_NPC_DAMAGE_NORMAL		12
#define	DEMP2_NPC_DAMAGE_HARD		18
#define	DEMP2_SIZE					2		// half of bbox size

#define DEMP2_ALT_DAMAGE			15
#define DEMP2_CHARGE_UNIT			500.0f	// demp2 charging gives us one more unit every 500ms--if you change this, you'll have to do the same in bg_pmove
#define DEMP2_ALT_RANGE				4096
#define DEMP2_ALT_SPLASHRADIUS		256

// Golan Arms Flechette
//---------
#define FLECHETTE_SHOTS				6
#define FLECHETTE_SPREAD			4.0f
#define FLECHETTE_DAMAGE			15
#define FLECHETTE_VEL				3500
#define FLECHETTE_SIZE				1

#define FLECHETTE_ALT_DAMAGE		20
#define FLECHETTE_ALT_VEL			950
#define FLECHETTE_ALT_MAX_VEL		1650
#define FLECHETTE_ALT_SPLASH_DAM	20
#define FLECHETTE_ALT_SPLASH_RAD	128
#define FLECHETTE_ALT_MAX_SIZE		3
#define FLECHETTE_ALT_MIN_SIZE		-3

// NOT CURRENTLY USED
#define FLECHETTE_MINE_RADIUS_CHECK		200
#define FLECHETTE_MINE_VEL				1000
#define FLECHETTE_MINE_DAMAGE			100
#define FLECHETTE_MINE_SPLASH_DAMAGE	200
#define FLECHETTE_MINE_SPLASH_RADIUS	200

// Personal Rocket Launcher
//---------
#define	ROCKET_VELOCITY				900
#define	ROCKET_DAMAGE				100
#define	ROCKET_SPLASH_DAMAGE		100
#define	ROCKET_SPLASH_RADIUS		160
#define ROCKET_NPC_DAMAGE_EASY		20
#define ROCKET_NPC_DAMAGE_NORMAL	40
#define ROCKET_NPC_DAMAGE_HARD		60
#define ROCKET_SIZE					3

#define	ROCKET_ALT_VELOCITY			(ROCKET_VELOCITY*0.5)
#define ROCKET_ALT_THINK_TIME		100

// Concussion Rifle
//---------
//primary
#define	CONC_VELOCITY				3000
#define	CONC_DAMAGE					150
#define CONC_NPC_SPREAD				0.7f
#define	CONC_NPC_DAMAGE_EASY		15
#define	CONC_NPC_DAMAGE_NORMAL		30
#define	CONC_NPC_DAMAGE_HARD		50
#define	CONC_SPLASH_DAMAGE			50
#define	CONC_SPLASH_RADIUS			300
//alt
#define CONC_ALT_DAMAGE				225//100
#define CONC_ALT_NPC_DAMAGE_EASY	10
#define CONC_ALT_NPC_DAMAGE_MEDIUM	20
#define CONC_ALT_NPC_DAMAGE_HARD	30

// Emplaced Gun
//--------------
#define EMPLACED_VEL				6000	// very fast
#define EMPLACED_DAMAGE				150		// and very damaging
#define EMPLACED_SIZE				5		// make it easier to hit things

// ATST Main Gun
//--------------
#define ATST_MAIN_VEL				4000	//
#define ATST_MAIN_DAMAGE			25		//
#define ATST_MAIN_SIZE				3		// make it easier to hit things

// ATST Side Gun
//---------------
#define ATST_SIDE_MAIN_DAMAGE				75
#define ATST_SIDE_MAIN_VELOCITY				1300
#define ATST_SIDE_MAIN_NPC_DAMAGE_EASY		30
#define ATST_SIDE_MAIN_NPC_DAMAGE_NORMAL	40
#define ATST_SIDE_MAIN_NPC_DAMAGE_HARD		50
#define ATST_SIDE_MAIN_SIZE					4
#define ATST_SIDE_MAIN_SPLASH_DAMAGE		10	// yeah, pretty small, either zero out or make it worth having?
#define ATST_SIDE_MAIN_SPLASH_RADIUS		16	// yeah, pretty small, either zero out or make it worth having?

#define ATST_SIDE_ALT_VELOCITY				1100
#define ATST_SIDE_ALT_NPC_VELOCITY			600
#define ATST_SIDE_ALT_DAMAGE				130

#define ATST_SIDE_ROCKET_NPC_DAMAGE_EASY	30
#define ATST_SIDE_ROCKET_NPC_DAMAGE_NORMAL	50
#define ATST_SIDE_ROCKET_NPC_DAMAGE_HARD	90

#define	ATST_SIDE_ALT_SPLASH_DAMAGE			130
#define	ATST_SIDE_ALT_SPLASH_RADIUS			200
#define ATST_SIDE_ALT_ROCKET_SIZE			5
#define ATST_SIDE_ALT_ROCKET_SPLASH_SCALE	0.5f	// scales splash for NPC's

// Stun Baton
//--------------
#define STUN_BATON_DAMAGE			22
#define STUN_BATON_ALT_DAMAGE		22
#define STUN_BATON_RANGE			25

// Laser Trip Mine
//--------------
#define LT_DAMAGE			150
#define LT_SPLASH_RAD		256.0f
#define LT_SPLASH_DAM		90

#define LT_VELOCITY			250.0f
#define LT_ALT_VELOCITY		1000.0f

#define PROX_MINE_RADIUS_CHECK		190

#define LT_SIZE				3.0f
#define LT_ALT_TIME			2000
#define	LT_ACTIVATION_DELAY	1000
#define	LT_DELAY_TIME		50

// Thermal Detonator
//--------------
#define TD_DAMAGE			100
#define TD_NPC_DAMAGE_CUT	0.6f	// NPC thrown dets deliver only 60% of the damage that a player thrown one does
#define TD_SPLASH_RAD		128
#define TD_SPLASH_DAM		90
#define TD_VELOCITY			900
#define TD_MIN_CHARGE		0.15f
#define TD_TIME				4000
#define TD_THINK_TIME		300		// don't think too often?
#define TD_TEST_RAD			(TD_SPLASH_RAD * 0.8f) // no sense in auto-blowing up if exactly on the radius edge--it would hardly do any damage
#define TD_ALT_TIME			3000

#define TD_ALT_DAMAGE		100
#define TD_ALT_SPLASH_RAD	128
#define TD_ALT_SPLASH_DAM	90
#define TD_ALT_VELOCITY		600
#define TD_ALT_MIN_CHARGE	0.15f
#define TD_ALT_TIME			3000

// Tusken Rifle Shot
//--------------
#define TUSKEN_RIFLE_VEL			3000	// fast
#define TUSKEN_RIFLE_DAMAGE_EASY	20		// damaging
#define TUSKEN_RIFLE_DAMAGE_MEDIUM	30		// very damaging
#define TUSKEN_RIFLE_DAMAGE_HARD	50		// extremely damaging

// F-11D Blaster
//---------
#define F_11D_MAIN_SPREAD			1.2f
#define F_11D_ALT_SPREAD			0.2f
#define F_11D_NPC_SPREAD			0.4f
#define F_11D_VELOCITY				3000
#define F_11D_DAMAGE				25
#define F_11D_SCOPE_DAMAGE			20

// DC-15S
//---------
#define CLONECARBINE_MAIN_SPREAD		0.5f
#define CLONECARBINE_ALT_SPREAD 		1.0f
#define CLONECARBINE_NPC_SPREAD 		0.5f
#define CLONECARBINE_VELOCITY 			3000
#define CLONECARBINE_DAMAGE 			20
#define CLONECARBINE_ALT_DAMAGE 		25

// DC-17
//---------
#define CLONECOMMANDO_MAIN_SPREAD			1.2f
#define CLONECOMMANDO_NPC_SPREAD			0.5f
#define CLONECOMMANDO_VELOCITY				3000
#define CLONECOMMANDO_ALT_VELOCITY			1000
#define CLONECOMMANDO_DAMAGE				25
#define CLONECOMMANDO_ALT_DAMAGE			225
#define CLONECOMMANDO_SCOPE_DAMAGE			20
#define CLONECOMMANDO_ALT_SIZE				3
#define CLONECOMMANDO_ALT_SPLASH_DAMAGE 	200
#define CLONECOMMANDO_ALT_SPLASH_RADIUS 	225

// A280
//---------
#define REBELRIFLE_MAIN_SPREAD			0.8f
#define REBELRIFLE_ALT_SPREAD			0.2f
#define REBELRIFLE_TERTIARY_SPREAD		0.5f
#define REBELRIFLE_NPC_SPREAD			0.5f
#define REBELRIFLE_VELOCITY 			3000
#define REBELRIFLE_DAMAGE				25
#define REBELRIFLE_SCOPE_DAMAGE 		20

// EE-3 Carbine Rifle
//---------
#define BOBA_MAIN_SPREAD		0.8f
#define BOBA_ALT_SPREAD 		0.2f
#define BOBA_TERTIARY_SPREAD 	0.1f
#define BOBA_NPC_SPREAD 		0.5f
#define BOBA_VELOCITY			3000
#define BOBA_DAMAGE 			25
#define BOBA_SCOPE_DAMAGE 		20

// Super Battle Droid
//---------
#define SBD_VELOCITY			2300
#define SBD_DAMAGE				12
#define SBD_LEFT_SHOT			-3.0f
#define SBD_RIGHT_SHOT			5.0f

// E-5s Sniper Rifle
//---------
#define CIS_SNIPER_MAIN_SPREAD			0.8f
#define CIS_SNIPER_ALT_SPREAD			0.2f
#define CIS_SNIPER_NPC_SPREAD			0.5f
#define CIS_SNIPER_VELOCITY 			4000
#define CIS_SNIPER_DAMAGE				15
#define CIS_SNIPER_SCOPE_DAMAGE 		25

// E5
//---------
#define E5_MAIN_SPREAD			1.2f
#define E5_ALT_SPREAD			1.5f
#define E5_NPC_SPREAD			1.0f
#define E5_VELOCITY 			3000
#define E5_DAMAGE				25
#define E5_ALT_DAMAGE			30

// DH-17
//---------
#define REBELBLASTER_MAIN_SPREAD			1.2f
#define REBELBLASTER_ALT_SPREAD 			0.2f
#define REBELBLASTER_NPC_SPREAD 			0.4f
#define REBELBLASTER_VELOCITY				3000
#define REBELBLASTER_DAMAGE 				25
#define REBELBLASTER_SCOPE_DAMAGE			20

// DC-15A
//---------
#define CLONERIFLE_MAIN_SPREAD			1.2f
#define CLONERIFLE_ALT_SPREAD			0.4f
#define CLONERIFLE_NPC_SPREAD			1.0f
#define CLONERIFLE_VELOCITY 			3000
#define CLONERIFLE_DAMAGE				30
#define CLONERIFLE_ALT_DAMAGE			25

// LPA NN-14
//--------
#define REY_VEL 			3000
#define REY_DAMAGE			20
#define REY_CHARGE_UNIT 	100.0f

// Westar 34
//--------
#define JANGO_MAIN_SPREAD			0.4f
#define JANGO_ALT_SPREAD			0.5f
#define JANGO_NPC_SPREAD			0.4f
#define JANGO_VELOCITY				3200
#define JANGO_DAMAGE				25
#define JANGO_ALT_DAMAGE			20
#define JANGO_NPC_DAMAGE_EASY		6
#define JANGO_NPC_DAMAGE_NORMAL 	8
#define JANGO_NPC_DAMAGE_HARD		10

// DC-17 Hand Pistol
//---------
#define CLONEPISTOL_MAIN_SPREAD 		0.5f
#define CLONEPISTOL_ALT_SPREAD			1.0f
#define CLONEPISTOL_NPC_SPREAD			0.5f
#define CLONEPISTOL_VELOCITY			3000
#define CLONEPISTOL_DAMAGE				20
#define CLONEPISTOL_ALT_DAMAGE			25



#endif//#ifndef __WEAPONS_H__
