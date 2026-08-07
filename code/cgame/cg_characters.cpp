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

#include "cg_headers.h"
#include "cg_characters.h"

#pragma region External Declarations
const char* CG_DisplayBoxedText(int iBoxX, int iBoxY, int iBoxWidth, int iBoxHeight,
	const char* psText, int iFontHandle, float fScale,
	const vec4_t v4Color);
#pragma endregion

#pragma region Search
int filteredCharactersIndexList[MAX_CHARACTERS];
char searchString[64];
char searchFaction[5][64];
char searchTags[5][64];

qboolean filterFunction(characterInfo_t *character)
{
	//TODO use filter function to filter characters based on searchString, searchFaction, and searchTags
	return qtrue;
}

void UpdateSearchFromCvar()
{
	//Load CVARs
	
	qboolean searchChanged = qfalse;
	//Compare to variable


	//Refilter if needed
	if (searchChanged)
	{
		//TODO : Improvement, only keep 15 Filter Results, and update at pagination
		//Reinit selectedCharacters
		memset(filteredCharactersIndexList, -1, sizeof(filteredCharactersIndexList));
		int filterIndex = 0;

		//Do filter
		for (int i = 0; i < MAX_CHARACTERS && !Q_IsStringEmpty(charactersData[i].name); i++)
		{
			if (filterFunction(&charactersData[i]))
			{
				filteredCharactersIndexList[filterIndex] = i;
				filterIndex++;
			}
		}
	}
}
#pragma endregion

#pragma region Draw Functions
void CG_DrawCharacters() {
	int marginX = 5, marginY = 6;
	int startX = 114, startY = 50;
	
	int bgSizeX = 94, bgSizeY = 125;

	int iconSizeX = 90, iconSizeY = 90;
	int iconOffsetX = 2, iconOffsetY = 2;

	int nameSizeX = 86, nameSizeY = 11;
	int nameOffsetX = 4, nameOffsetY = 96;

	int posX = startX, posY = startY;

	char text[1024] = { 0 };
	qhandle_t background = cgi_R_RegisterShaderNoMip("gfx/menu/w_character_icon_bg");



	UpdateSearchFromCvar();
	//draw characters based on filteredCharactersIndexList
	for (int i = 0; i < MAX_CHARACTERS && filteredCharactersIndexList[i] != -1; i++)
	{
		int currentCharacterIndex = filteredCharactersIndexList[i];
		//Draw Background
		CG_DrawPic(posX, posY, bgSizeX, bgSizeY, background);
		//Draw Character Icons
		qhandle_t icon = cgi_R_RegisterShaderNoMip(charactersData[currentCharacterIndex].icon);
		CG_DrawPic(posX + iconOffsetX, posY + iconOffsetY, iconSizeX, iconSizeY, icon);
		//Translate Name
		if (!cgi_SP_GetStringTextString(va("%s_DESC", charactersData[currentCharacterIndex].name), text, sizeof(text)))
		{
			Com_sprintf(text, sizeof(charactersData[currentCharacterIndex].name), charactersData[currentCharacterIndex].name);
		}
		//Draw Name
		/* TODO Draw Correctly
		int w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 1.0f, cgs.widthRatioCoef);
		int x = (SCREEN_WIDTH - w) / 2;
		cgi_R_Font_DrawString(x, (SCREEN_HEIGHT - 24) + yOffset, text, textColor, cgs.media.qhFontSmall, -1, 1.0f, cgs.widthRatioCoef);
		*/
	}
}


void CG_DrawFactions() {

}
#pragma endregion
