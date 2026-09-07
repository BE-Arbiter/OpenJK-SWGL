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
#include "cg_media.h"

#pragma region External Declarations
const char* CG_DisplayBoxedText(int iBoxX, int iBoxY, int iBoxWidth, int iBoxHeight,
	const char* psText, int iFontHandle, float fScale,
	const vec4_t v4Color);
#pragma endregion

#pragma region Variables & Cvars
int filteredCharactersIndexList[MAX_CHARACTERS];
int filteredCharacters;
qboolean searchChanged = qtrue;

extern vmCvar_t ui_c_filter_name;
extern vmCvar_t ui_character_screen;
extern vmCvar_t ui_character_selected;
extern vmCvar_t ui_character_page;
extern vmCvar_t ui_character_index;

#pragma endregion

#pragma region Actions
void CG_Characters_CharacterClick_f()
{
	//Update CVAR
	cgi_Cvar_Update(&ui_character_screen);	
	cgi_Cvar_Update(&ui_character_selected);
	cgi_Cvar_Update(&ui_character_page);
	if (Q_stricmp(ui_character_screen.string, "factions") == 0)
	{
		int selectedChar = (ui_character_page.integer * 15) + ui_character_selected.integer - 1;
		if(selectedChar >= loadedFactions || selectedChar < 0)
		{
			//Draw warning in debug$
			#ifdef DEBUG
			Com_Printf("Invalid faction selected: %d\n", selectedChar);
			#endif // DEBUG
			return;
		}
		//Toggle the selected faction filter
		factionsData[selectedChar].selectedFilter = factionsData[selectedChar].selectedFilter ? qfalse : qtrue;
		return;
	}
	if (Q_stricmp(ui_character_screen.string, "characters") == 0)
	{
		//Todo Select Character
		//Force ui update?
	}
}

int getMaxPage() {
	cgi_Cvar_Update(&ui_character_screen);
	if (Q_stricmp(ui_character_screen.string, "factions") == 0)
	{
		return (loadedFactions / 15) + 1;
	}
	if (Q_stricmp(ui_character_screen.string, "characters") == 0)
	{
		return (filteredCharacters / 15) + 1;
	}
	return 0;
}

void setCurrentPage(int currentPage) {
	char buffer[32] = { 0 };
	itoa(currentPage, buffer, 10);
	cgi_Cvar_Set("ui_character_page", buffer);
}

void CG_Characters_PreviousPage_f() 
{
	cgi_Cvar_Update(&ui_character_page);

	int maxPage = getMaxPage();
	int currentPage = ui_character_page.integer;
	if (currentPage == 0)
	{
		currentPage = maxPage - 1;
	}
	else {
		currentPage--;
	}

	setCurrentPage(currentPage);
}

void CG_Characters_NextPage_f() 
{
	cgi_Cvar_Update(&ui_character_screen);
	cgi_Cvar_Update(&ui_character_page);

	int maxPage = getMaxPage();
	int currentPage = ui_character_page.integer;
	if (currentPage == maxPage - 1)
	{
		currentPage = 0;
	}
	else {
		currentPage++;
	}

	setCurrentPage(currentPage);
}
void CG_Characters_SearchChanged_f()
{
	searchChanged = qtrue;
}
#pragma endregion

#pragma region Search
qboolean filterFunction(characterInfo_t *character)
{
	//Faction filter
	qboolean hasFaction = qfalse;
	qboolean hasSelectedFaction = qfalse;
	for (int i = 0; i < MAX_FACTIONS && factionsData[i].code != 0 && factionsData[i].code[0] != '\0' && !hasFaction; i++)
	{
		characterFaction_t* faction = &factionsData[i];
		if (faction->selectedFilter)
		{
			hasSelectedFaction = qtrue;
			if (Q_stristr(character->factions, faction->code) != NULL)
			{
				hasFaction = qtrue;
				break;
			}
		}
	}
	if (!hasFaction && hasSelectedFaction)
	{
		return qfalse;
	}
	//Search if the name is found in this character's name, if not, return false
	if (ui_c_filter_name.string != NULL && ui_c_filter_name.string[0] != '\0'
		&& Q_stristr(character->name, ui_c_filter_name.string) == NULL)
	{
		return qfalse;
	}
	return qtrue;
}

void UpdateSearchFromCvar()
{

	//Refilter if needed
	if (searchChanged)
	{
		cgi_Cvar_Update(&ui_c_filter_name);

		//TODO : Improvement, only keep 15 Filter Results, and update at pagination
		//Reinit selectedCharacters
		memset(filteredCharactersIndexList, -1, sizeof(filteredCharactersIndexList));
		filteredCharacters = 0;

		//Do filter
		for (int i = 0; i < MAX_CHARACTERS && !Q_IsStringEmpty(charactersData[i].name); i++)
		{
			if (filterFunction(&charactersData[i]))
			{
				filteredCharactersIndexList[filteredCharacters] = i;
				filteredCharacters++;
			}
		}
		searchChanged = qfalse;
 	}
}
#pragma endregion

#pragma region Draw Functions

void CG_DrawCharacters() {
	int marginX = 5, marginY = 4;
	int startX = 114, startY = 88;
	
	int bgSizeX = 94, bgSizeY = 114;

	int iconSizeX = 90, iconSizeY = 90;
	int iconOffsetX = 2, iconOffsetY = 2;

	int nameSizeX = 82, nameSizeY = 16;
	int nameOffsetX = 6, nameOffsetY = 90;

	int posX = startX, posY = startY;

	char text[1024] = { 0 };
	qhandle_t background = cgi_R_RegisterShaderNoMip("gfx/menu/w_character_icon_bg");

	cgi_Cvar_Update(&ui_character_page);
	int currentPage = ui_character_page.integer;
	int beginIndex = (currentPage * 15);
	int endIndex = beginIndex + 15;

	UpdateSearchFromCvar();
	//draw characters based on filteredCharactersIndexList
	for (int i = beginIndex;filteredCharactersIndexList[i] != -1 && i < endIndex; i++)
	{
		int currentCharacterIndex = filteredCharactersIndexList[i];
		characterInfo_t *currentCharacter = &charactersData[currentCharacterIndex];
		//Draw Background
		CG_DrawPic(posX, posY, bgSizeX, bgSizeY, background);
		//Draw Character Icons
		qhandle_t icon = cgi_R_RegisterShaderNoMip(currentCharacter->icon);
		CG_DrawPic(posX + iconOffsetX, posY + iconOffsetY, iconSizeX, iconSizeY, icon);

		//Translate & draw name
		if (!cgi_SP_GetStringTextString(va("%s", currentCharacter->name), text, sizeof(text)))
		{
			Com_sprintf(text, sizeof(currentCharacter->name), currentCharacter->name);
		}
		CG_DrawTextInBox(posX + nameOffsetX, posY + nameOffsetY, nameSizeX, nameSizeY,
			text, cgs.media.qhFontSmall, colorTable[CT_WHITE]);

		//switch to next position
		int nextLine = ((i - beginIndex) + 1) / 5;
		int nextColumn = ((i - beginIndex) + 1) % 5;
		posX = startX + nextColumn * (bgSizeX + marginX);
		posY = startY + nextLine * (bgSizeY + marginY);
	}
}

void CG_DrawFactions() {
	int marginX = 5, marginY = 4;
	int startX = 114, startY = 88;

	int bgSizeX = 94, bgSizeY = 114;

	int iconSizeX = 72, iconSizeY = 72;
	int iconOffsetX = 11, iconOffsetY = 11;

	int nameSizeX = 82, nameSizeY = 16;
	int nameOffsetX = 6, nameOffsetY = 90;

	int posX = startX, posY = startY;

	char text[1024] = { 0 };
	qhandle_t background = cgi_R_RegisterShaderNoMip("gfx/menu/w_character_icon_bg");
	qhandle_t background_selected = cgi_R_RegisterShaderNoMip("gfx/menu/w_character_icon_bg_s");

	cgi_Cvar_Update(&ui_character_page);
	int currentPage = ui_character_page.integer;
	int beginIndex = (currentPage * 15);
	int endIndex = beginIndex + 15;

	//DrawsFactions
	for (int i = beginIndex;factionsData[i].code != 0 && factionsData[i].code[0] != '\0' && i < endIndex; i++)
	{
		characterFaction_t *faction = &factionsData[i];
		//Draw Background
		CG_DrawPic(posX, posY, bgSizeX, bgSizeY, faction->selectedFilter ? background_selected : background);
		//Draw Character Icons
		qhandle_t icon = cgi_R_RegisterShaderNoMip(faction->icon);
		CG_DrawPic(posX + iconOffsetX, posY + iconOffsetY, iconSizeX, iconSizeY, icon);

		//Translate & draw name
		if (!cgi_SP_GetStringTextString(va("%s", faction->nameKey), text, sizeof(text)))
		{
			Com_sprintf(text, sizeof(faction->nameKey), faction->nameKey);
		}
		CG_DrawTextInBox(posX + nameOffsetX, posY + nameOffsetY, nameSizeX, nameSizeY,
			text, cgs.media.qhFontSmall, colorTable[CT_WHITE]);

		//switch to next position
		int nextLine = ((i-beginIndex) + 1) / 5;
		int nextColumn = ((i - beginIndex) + 1) % 5;
		posX = startX + nextColumn * (bgSizeX + marginX);
		posY = startY + nextLine * (bgSizeY + marginY);
	}

}

void CG_DrawCharactersMenu() {
	cgi_Cvar_Update(&ui_character_screen);
	if (Q_stricmp(ui_character_screen.string, "factions") == 0)
	{
		CG_DrawFactions();
	}
	else if (Q_stricmp(ui_character_screen.string, "characters") == 0)
	{
		CG_DrawCharacters();
	}
	else if (Q_stricmp(ui_character_screen.string, "character") == 0)
	{
		//CG_DrawSearch();
	}
	else
	{
		//CG_DrawTags();
	}
}
#pragma endregion
