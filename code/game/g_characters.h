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

#ifndef __G_CHARACTERS_H__
#define __G_CHARACTERS_H__

#include "../qcommon/q_shared.h"

#define MAX_CHARACTERS 1024
#define MAX_FACTIONS 64

#pragma region Character Data Structures
typedef struct characterFaction_s {
	char code[5];
	char nameKey[64];
	char icon[64];
	qboolean selectedFilter = qfalse;
} characterFaction_t;

typedef struct characterSkin_s {
	char icon[64];
	char skin[64];
} characterSkin_t;

typedef struct characterVariant_s {
	char name[64];
	char npcName[64];
	char descriptionKey[64];
	char icon[64];
	char model[64];

	characterSkin_s *presetList;
	int presetCount;

	characterSkin_s* lowerSkinList;
	int lowerSkinCount;

	characterSkin_s* torsoSkinList;
	int torsoSkinCount;

	characterSkin_s* headSkinList;
	int headSkinCount;

	char weapons[64][6];
	int forcePowers[NUM_FORCE_POWERS];
	//TODO Add mor fields (Saber - Abilities - etc)
} characterVariant_t;

typedef struct characterInfo_s{
	char name[64];
	char icon[64];
	char factions[256]; //LIST OF CODE OF FACTIONS THAT THIS CHARACTER BELONGS TO (SEPARATED BY COMMA)
	char tags[256];
	
	characterVariant_s* variantList;
	int variantCount;
} characterInfo_t;
#pragma endregion

#pragma region Global Character Data
extern characterFaction_t factionsData[MAX_FACTIONS];
extern int loadedFactions;
extern characterInfo_t charactersData[MAX_CHARACTERS];
extern int loadedCharacters;

#pragma endregion

#pragma region Faction Functions
#pragma endregion

#pragma region Character Functions
#pragma endregion

#endif //__G_CHARACTERS_H__
