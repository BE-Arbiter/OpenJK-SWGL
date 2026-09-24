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
#include <string>
#include <vector>
#include "cg_characters.h"
#include "cg_media.h"
#include "../game/anims.h"

#pragma region External Declarations
const char* CG_DisplayBoxedText(int iBoxX, int iBoxY, int iBoxWidth, int iBoxHeight,
	const char* psText, int iFontHandle, float fScale,
	const vec4_t v4Color);
#pragma endregion

#pragma region Variables & Cvars
int filteredCharactersIndexList[MAX_CHARACTERS];
int filteredCharacters;
static int shownCharacter = -1;
static int shownVariant = 0;
qboolean searchChanged = qtrue;

extern vmCvar_t ui_c_filter_name;
extern vmCvar_t ui_character_screen;
extern vmCvar_t ui_character_selected;
extern vmCvar_t ui_character_page;
extern vmCvar_t ui_character_index;

#pragma endregion

#pragma region Utility
// Number of variants read from the .cha file (variantCount is the size of the list).
int GetVariantCount(const characterInfo_t* character)
{
	int count = 0;
	while (count < character->variantCount && !Q_IsStringEmpty(character->variantList[count].code))
	{
		count++;
	}
	return count;
}
#pragma endregion

#pragma region Actions



void ChangeCharacter(int characterIndex, int variantIndex = 0)
{
	characterInfo_t *currentCharacter = &charactersData[characterIndex];
	characterVariant_t *variant = &currentCharacter->variantList[variantIndex];

	shownCharacter = characterIndex;
	shownVariant = variantIndex;

	// The character .cfg is ext_data/characters/<g_charKey>_<ui_variant_code>[_def|_NPC].cfg.
	cgi_Cvar_Set("ui_char_model", variant->model);
	cgi_Cvar_Set("ui_char_model_angle", "180");
	cgi_Cvar_Set("g_charKey", currentCharacter->code);
	cgi_Cvar_Set("ui_variant_code", variant->code);
	cgi_Cvar_Set("ui_npc_type", variant->npcName);

	// Same sequence as the character buttons of the old menu.
	cgi_UI_Run_Command(va("setitemtext CharBio @%s", variant->descriptionKey));
	cgi_UI_Run_Command("uiScript loadCharacter");
	cgi_UI_Run_Command("uiScript getsaberstyle");
	cgi_UI_Run_Command("uiScript \"char_default_skin\" \"model_default\"");
	cgi_UI_Run_Command("uiScript \"char_skin\"");
	cgi_UI_Run_Command("uiScript character");
	cgi_UI_Run_Command("uiScript rgbsabercvars");
	cgi_UI_Run_Command("uiScript getcharcustom");
	cgi_UI_Run_Command("uiScript \"getNPCcode\" \"\"");
	cgi_UI_Run_Command("uiScript saber_hilt");
	cgi_UI_Run_Command("uiScript saber2_hilt");
	cgi_UI_Run_Command("uiScript char_weapon");
	cgi_UI_Run_Command("uiScript \"char_default_skin\" \"model_default\"");
	cgi_UI_Run_Command("uiScript ui_char_update_model");
}

void CG_Characters_CharacterClick_f()
{
	//Update CVAR
	cgi_Cvar_Update(&ui_character_screen);	
	cgi_Cvar_Update(&ui_character_selected);
	cgi_Cvar_Update(&ui_character_page);
	if (Q_stricmp(ui_character_screen.string, "factions") == 0)
	{
		int selectedFaction = (ui_character_page.integer * 15) + ui_character_selected.integer - 1;
		if(selectedFaction >= loadedFactions || selectedFaction < 0)
		{
			//Draw warning in debug$
			#ifdef DEBUG
			Com_Printf("Invalid faction selected: %d\n", selectedFaction);
			#endif // DEBUG
			return;
		}
		//Toggle the selected faction filter
		factionsData[selectedFaction].selectedFilter = factionsData[selectedFaction].selectedFilter ? qfalse : qtrue;
		searchChanged = qtrue;
		return;
	}
	if (Q_stricmp(ui_character_screen.string, "characters") == 0)
	{
		int selectedCharacter = (ui_character_page.integer * 15) + ui_character_selected.integer - 1;
		if(selectedCharacter < 0 || selectedCharacter >= filteredCharacters)
		{
			//Draw warning in debug
			#ifdef DEBUG
			Com_Printf("Invalid character selected: %d\n", selectedCharacter);
			#endif // DEBUG
			return;
		}
		//Change Character (Load Default, Screen)
		ChangeCharacter(filteredCharactersIndexList[selectedCharacter]);
		//Update View
		cgi_Cvar_Set("ui_character_screen","character");
		cgi_UI_Run_Command("hide selScreen");
		cgi_UI_Run_Command("hide characterButtons");
		cgi_UI_Run_Command("show charScreen");
		cgi_UI_Run_Command("hide tabPowers");
		cgi_UI_Run_Command("hide tabWeapons");
		cgi_UI_Run_Command("hide saberModels");
		cgi_UI_Run_Command("hide StatsTabPc");
		cgi_UI_Run_Command("show PowersTabPc");
		cgi_UI_Run_Command("show WeaponsTabPc");
		cgi_UI_Run_Command("show tabStats");
		cgi_UI_Run_Command("uiScript toggleTeamAvailability");
		
	}
}

// characterVariantClick <slot 1..VARIANT_SLOTS>: show this variant of the character on the screen.
void CG_Characters_VariantClick_f()
{
	const int variantIndex = atoi(CG_Argv(1)) - 1;

	if (shownCharacter < 0 || variantIndex < 0 || variantIndex >= GetVariantCount(&charactersData[shownCharacter])
		|| variantIndex == shownVariant)
	{
		return;
	}
	ChangeCharacter(shownCharacter, variantIndex);
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
	static char lastFilter[MAX_CVAR_VALUE_STRING];

	cgi_Cvar_Update(&ui_c_filter_name);
	if (Q_stricmp(lastFilter, ui_c_filter_name.string) == 0)
	{
		return;
	}
	Q_strncpyz(lastFilter, ui_c_filter_name.string, sizeof(lastFilter));
	searchChanged = qtrue;
	setCurrentPage(0);
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
		&& Q_stristr(character->nameKey, ui_c_filter_name.string) == NULL
		&& Q_stristr(character->code, ui_c_filter_name.string) == NULL
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
		for (int i = 0; i < MAX_CHARACTERS && !Q_IsStringEmpty(charactersData[i].code); i++)
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
void getCharacterName(characterInfo_t* character, char* buffer, int bufferSize)
{
	if (buffer == NULL || bufferSize <= 0)
	{
		return;
	}
	if (character == NULL)
	{
		Q_strncpyz(buffer, "Unknown", bufferSize);
		return;
	}
	if (character->name != NULL && character->name[0] != '\0')
	{
		Q_strncpyz(buffer, character->name, bufferSize);
	}
	else if (character->nameKey != NULL && character->nameKey[0] != '\0')
	{
		cgi_SP_GetStringTextString(character->nameKey, buffer, bufferSize);
	}
	else
	{
		Q_strncpyz(buffer, character->code, bufferSize);
	}
}

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

	UpdateSearchFromCvar();

	cgi_Cvar_Update(&ui_character_page);
	int currentPage = ui_character_page.integer;
	int maxPage = getMaxPage();
	// The filter can leave fewer pages than the current one.
	if (currentPage >= maxPage)
	{
		currentPage = 0;
		setCurrentPage(0);
	}
	int beginIndex = (currentPage * 15);
	int endIndex = beginIndex + 15;
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
		char text[1024] = { 0 };
		getCharacterName(currentCharacter, text, sizeof(text));
		CG_DrawTextInBox(posX + nameOffsetX, posY + nameOffsetY, nameSizeX, nameSizeY,
			text, cgs.media.qhFontSmall, colorTable[CT_WHITE]);

		//switch to next position
		int nextLine = ((i - beginIndex) + 1) / 5;
		int nextColumn = ((i - beginIndex) + 1) % 5;
		posX = startX + nextColumn * (bgSizeX + marginX);
		posY = startY + nextLine * (bgSizeY + marginY);
	}
	//Draw page and total page
	CG_DrawTextInBox(411, 441, 218, 18,
		va("Page %d of %d (showing %d characters)", currentPage + 1, maxPage, filteredCharacters), cgs.media.qhFontSmall, colorTable[CT_WHITE], ALIGN_RIGHT);
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
	int maxPage = getMaxPage();
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

	CG_DrawTextInBox(411, 441, 218, 18,
		va("Page %d of %d (showing %d factions)", currentPage + 1, maxPage, loadedFactions), cgs.media.qhFontSmall, colorTable[CT_WHITE], ALIGN_RIGHT);

}

// Variant squares in the frame 13 43 196 76 of the character screen: 2 rows of 5.
// The buttons variantButton1..10 of IngameSWGLChars.menu have the same rects.
void CG_DrawVariants() {
	int variantSlots = 10;
	int variantColumns = 5;
	int variantSize = 35;
	int variantGap = 4;
	int variantStartX = 15;
	int variantStartY = 44;
	int variantIconInset = 1;

	if (shownCharacter < 0 || shownCharacter >= loadedCharacters)
	{
		return;
	}
	const characterInfo_t *character = &charactersData[shownCharacter];
	const int count = Q_min(GetVariantCount(character), variantSlots);
	const qhandle_t background = cgi_R_RegisterShaderNoMip("gfx/menu/w_skin_icon_bg");
	const qhandle_t backgroundSelected = cgi_R_RegisterShaderNoMip("gfx/menu/w_skin_icon_bg_s");

	for (int i = 0; i < count; i++)
	{
		const int x = variantStartX + (i % variantColumns) * (variantSize + variantGap);
		const int y = variantStartY + (i / variantColumns) * (variantSize + variantGap);

		CG_DrawPic(x, y, variantSize, variantSize, i == shownVariant ? backgroundSelected : background);
		CG_DrawPic(x + variantIconInset, y + variantIconInset,
			variantSize - 2 * variantIconInset, variantSize - 2 * variantIconInset,
			cgi_R_RegisterShaderNoMip(character->variantList[i].icon));
	}
}

void CG_DrawCharactersMenu() {
	// The ownerdraw is the last menu item: without a reset, the pictures take the color of the text painted before.
	cgi_R_SetColor(NULL);
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
		CG_DrawVariants();
	}
	else
	{
		//CG_DrawTags();
	}
}
#pragma endregion

#pragma region Portraits
///----------------------------------------------------------------------------------------------
/// Portraits : "charportraits [size=256] [first job=0]" renders every skin of every variant model to portraits/<model>/<skin>.tga
/// The skins are the model_*.skin files of each variant model; <skin> has no "model_" prefix.
/// Each portrait is drawn on black then on white; the renderer (r_capturePortrait) derives the alpha.
///----------------------------------------------------------------------------------------------
extern int G_ParseAnimFileSet(const char *skeletonName, const char *modelName);
extern qboolean G_StandardHumanoid(const char *GLAName);

// Frame steps for one portrait. A console command runs at the start of the next frame.
// Wide shot on black and white (measure of the head), then close-up on black and white (TGA).
enum
{
	PORTRAIT_STEP_LOAD,
	PORTRAIT_STEP_MEASURE_BLACK_REQUEST = 3,	// warm-up frames before, so the textures are loaded
	PORTRAIT_STEP_MEASURE_BLACK_CAPTURE,
	PORTRAIT_STEP_MEASURE_WHITE_REQUEST,
	PORTRAIT_STEP_MEASURE_WHITE_CAPTURE,
	PORTRAIT_STEP_CLOSE_FRAME = 8,				// one more frame: the renderer writes the measure at the end of the capture frame
	PORTRAIT_STEP_BLACK_REQUEST = 10,
	PORTRAIT_STEP_BLACK_CAPTURE,
	PORTRAIT_STEP_WHITE_REQUEST,
	PORTRAIT_STEP_WHITE_CAPTURE,
	PORTRAIT_STEP_COUNT
};

// "charportraits": portraits per map load, and the cvar that keeps the next job across map loads.
#define PORTRAIT_BATCH			500
#define PORTRAIT_RESUME_CVAR	"cg_portraitsResume"

static const char *Portrait_MenuCvar(const char *cvarName);

// One portrait to render.
struct portraitJob_t
{
	char	model[64];
	char	skin[MAX_QPATH];	// skin name for messages, e.g. "model_red"
	char	skinPath[128];		// skin given to the renderer: a .skin file or "models/players/<model>/|head|torso|lower"
	char	file[MAX_QPATH];	// output TGA
	byte	color[3];			// tint of the model (menu RGB colors)
};

static std::vector<portraitJob_t> portraitJobs;

static struct
{
	qboolean		active;
	qboolean		tableFull;		// the renderer cannot register more skins this level
	size_t			job;
	int				batchLoaded;	// jobs loaded on this map load (the renderer keeps their models and skins)
	int				skipped;		// jobs with a TGA from a previous run
	int				step;
	int				written;
	int				outputSize;
	CGhoul2Info_v	ghoul2;
	qhandle_t		model;
	qhandle_t		skin;
	float			frameBottom;	// vertical range of the shot, in model units
	float			frameTop;
	float			frameSide;		// lateral position of the center of the shot
	float			frameWidth;		// width of the shot, in model units
	qboolean		single;			// "charportrait": one portrait with a frame from the command line
	float			singleX;		// "charportrait" frame: center offset from *head_top (right, down) and size
	float			singleY;
	float			singleWidth;
	float			singleHeight;
	qboolean		humanoid;		// standard humanoid skeleton: close-up on the head, else the full silhouette
	qboolean		hasNeck;		// the skeleton has a cervical bone
	float			neckHeight;		// height of the cervical bone (base of the neck)
	int				wideRetries;	// times the wide shot was enlarged because the silhouette touched an edge
} portrait;

// Position of a bolt (tag or bone) in the frozen pose. Return qfalse if the model does not have it.
static qboolean Portrait_BoltPosition(const char *name, vec3_t &position)
{
	const int bolt = gi.G2API_AddBolt(&portrait.ghoul2[0], name);
	if (bolt < 0)
	{
		return qfalse;
	}
	const vec3_t angles = { 0, 180, 0 };
	mdxaBone_t matrix;
	gi.G2API_GetBoltMatrix(portrait.ghoul2, 0, bolt, &matrix, angles, vec3_origin, cg.time, NULL, vec3_origin);
	gi.G2API_GiveMeVectorFromMatrix(matrix, ORIGIN, position);
	return qtrue;
}

// Close-up: head and top of the shoulders, centered on the head.
// Head top (silhouette) at 10% of the image height from the top, neck base (cervical bone) at 88%,
// head at most 85% of the width. Without a cervical bone, the chin of the silhouette is at 72%.
#define PORTRAIT_HEAD_LINE	0.10f
#define PORTRAIT_NECK_LINE	0.88f
#define PORTRAIT_CHIN_LINE	0.72f
#define PORTRAIT_HEAD_WIDTH	0.85f

// Wide shot of the full body, for the measure of the head (RB_AnalyzePortrait).
// The bolts only give an approximate size: the shot has a large margin.
static void Portrait_WideFrame(void)
{
	vec3_t head = { 0, 0, 42 };
	vec3_t foot = { 0, 0, -21 };

	Portrait_BoltPosition("*head_top", head);
	Portrait_BoltPosition("rtalus", foot);

	// Bones come from the shared skeleton: the cervical bone is reliable on all humanoid models.
	vec3_t neck;
	portrait.hasNeck = Portrait_BoltPosition("cervical", neck);
	portrait.neckHeight = neck[2];
	const float height = Q_max(head[2] - foot[2], 60.0f);

	portrait.frameBottom = foot[2] - 10;
	portrait.frameTop = foot[2] + 1.5f * height;
	portrait.frameSide = 0;
	portrait.frameWidth = portrait.frameTop - portrait.frameBottom;
}

// "charportrait" frame: center at *head_top plus (X right, Y down), size W x H, in model units.
static void Portrait_SingleFrame(void)
{
	vec3_t head = { 0, 0, 42 };

	if (!Portrait_BoltPosition("*head_top", head) && Portrait_BoltPosition("cranium", head))
	{
		head[2] += 8;
	}

	// Image right is -y in the view.
	const float centerZ = head[2] - portrait.singleY;
	portrait.frameSide = head[1] - portrait.singleX;
	portrait.frameTop = centerZ + 0.5f * portrait.singleHeight;
	portrait.frameBottom = centerZ - 0.5f * portrait.singleHeight;
	portrait.frameWidth = portrait.singleWidth;
}

// Silhouette of a non-humanoid: its box plus this margin on each side, in fractions of the box.
#define PORTRAIT_BOX_MARGIN		0.05f
#define PORTRAIT_WIDE_RETRIES	12

enum portraitFrame_t
{
	PORTRAIT_FRAME_OK,
	PORTRAIT_FRAME_RETRY,	// the silhouette touched an edge: the wide shot is now larger, measure again
	PORTRAIT_FRAME_FAIL
};

// Final shot from the measure of the wide shot.
static portraitFrame_t Portrait_CloseFrame(void)
{
	vmCvar_t measure;
	float headLine, chinLine, centerLine, widthLine, leftLine, rightLine, bottomLine;
	int clipped;

	cgi_Cvar_Register(&measure, "r_portraitHead", "", 0);
	cgi_Cvar_Update(&measure);
	if (sscanf(measure.string, "%f %f %f %f %d %f %f %f", &headLine, &chinLine, &centerLine, &widthLine,
			&clipped, &leftLine, &rightLine, &bottomLine) != 8 || chinLine <= headLine)
	{
		return PORTRAIT_FRAME_FAIL;
	}

	// The wide shot is square: its height is also its width. Image line v is at z = top - v * size.
	const float wide = portrait.frameTop - portrait.frameBottom;

	if (clipped && portrait.wideRetries < PORTRAIT_WIDE_RETRIES)
	{
		// Enlarge the shot by 25% around its center.
		portrait.wideRetries++;
		portrait.frameTop += 0.125f * wide;
		portrait.frameBottom -= 0.125f * wide;
		portrait.frameWidth = portrait.frameTop - portrait.frameBottom;
		return PORTRAIT_FRAME_RETRY;
	}

	if (!portrait.humanoid)
	{
		const float box = Q_max(bottomLine - headLine, rightLine - leftLine) * wide * (1.0f + 2.0f * PORTRAIT_BOX_MARGIN);
		const float middle = portrait.frameTop - 0.5f * (headLine + bottomLine) * wide;

		// Image left is +y in the view.
		portrait.frameSide += (0.5f - 0.5f * (leftLine + rightLine)) * wide;
		portrait.frameTop = middle + 0.5f * box;
		portrait.frameBottom = middle - 0.5f * box;
		portrait.frameWidth = box;
		return PORTRAIT_FRAME_OK;
	}

	const float head = portrait.frameTop - headLine * wide;
	const float chin = portrait.frameTop - chinLine * wide;
	float frame = (head - chin) / (PORTRAIT_CHIN_LINE - PORTRAIT_HEAD_LINE);
	if (portrait.hasNeck && portrait.neckHeight < head)
	{
		frame = (head - portrait.neckHeight) / (PORTRAIT_NECK_LINE - PORTRAIT_HEAD_LINE);
	}
	// A head wider than tall (ET, droids) sets the size with its width.
	frame = Q_max(frame, widthLine * wide / PORTRAIT_HEAD_WIDTH);

	// Image left is +y in the view.
	portrait.frameSide += (0.5f - centerLine) * wide;
	portrait.frameTop = head + PORTRAIT_HEAD_LINE * frame;
	portrait.frameBottom = portrait.frameTop - frame;
	portrait.frameWidth = frame;
	return PORTRAIT_FRAME_OK;
}

static qboolean Portrait_Load(const portraitJob_t *job)
{
	char path[MAX_QPATH];

	gi.G2API_CleanGhoul2Models(portrait.ghoul2);

	Com_sprintf(path, sizeof(path), "models/players/%s/model.glm", job->model);
	portrait.model = cgi_R_RegisterModel(path);
	if (!portrait.model || gi.G2API_InitGhoul2Model(portrait.ghoul2, path, portrait.model, NULL_HANDLE, NULL_HANDLE, 0, 0) < 0)
	{
		return qfalse;
	}

	// 0: the skin file is missing or broken (skip this job), or the renderer skin table is full
	// (load the map again). Only a .skin file that exists can mean a full table.
	portrait.skin = cgi_R_RegisterSkin(job->skinPath);
	if (!portrait.skin)
	{
		if (!strchr(job->skinPath, '|') && gi.FS_ReadFile(job->skinPath, NULL) > 0)
		{
			portrait.tableFull = qtrue;
		}
		return qfalse;
	}
	gi.G2API_SetSkin(&portrait.ghoul2[0], portrait.skin, portrait.skin);

	// Freeze the model on the first frame of BOTH_STAND1.
	char *glaName = gi.G2API_GetGLAName(&portrait.ghoul2[0]);
	char skeleton[MAX_QPATH] = "_humanoid";
	if (glaName)
	{
		char animPath[MAX_QPATH];
		Q_strncpyz(animPath, glaName, sizeof(animPath));
		char *slash = strrchr(animPath, '/');
		if (slash)
		{
			*slash = '\0';
		}
		Q_strncpyz(skeleton, COM_SkipPath(animPath), sizeof(skeleton));
	}
	// No model name: that registers the model in the per-level modelsAlreadyDone table (60 slots).
	portrait.humanoid = G_StandardHumanoid(skeleton);
	portrait.wideRetries = 0;
	const int animFileIndex = G_ParseAnimFileSet(skeleton, NULL);
	if (animFileIndex >= 0)
	{
		const animation_t *anim = &level.knownAnimFileSets[animFileIndex].animations[BOTH_STAND1];
		gi.G2API_SetBoneAnim(&portrait.ghoul2[0], "model_root", anim->firstFrame, anim->firstFrame + 1,
			BONE_ANIM_OVERRIDE_FREEZE, 1.0f, cg.time, anim->firstFrame, 0);
	}
	if (portrait.single)
	{
		Portrait_SingleFrame();
	}
	else
	{
		Portrait_WideFrame();
	}
	return qtrue;
}

// Move to the next job that loads. Return qfalse at the end of the list.
static qboolean Portrait_Next(void)
{
	for (; portrait.job < portraitJobs.size(); portrait.job++)
	{
		if (!portrait.single && portrait.batchLoaded >= PORTRAIT_BATCH)
		{
			return qfalse;
		}
		const portraitJob_t *job = &portraitJobs[portrait.job];
		// "charportraits" keeps the portraits of a previous run; delete a file to render it again.
		if (!portrait.single && gi.FS_ReadFile(job->file, NULL) > 0)
		{
			portrait.skipped++;
			continue;
		}
		if (Portrait_Load(job))
		{
			portrait.batchLoaded++;
			portrait.step = PORTRAIT_STEP_LOAD;
			return qtrue;
		}
		if (portrait.tableFull)
		{
			return qfalse;
		}
		Com_Printf(S_COLOR_YELLOW "charportraits: cannot load models/players/%s (%s)\n", job->model, job->skin);
	}
	return qfalse;
}

// List the model_*.skin files of a model, without extension. Fall back to model_default.
static std::vector<std::string> Portrait_ListSkins(const char *model)
{
	std::vector<std::string> skins;
	char list[8192];
	const int count = gi.FS_GetFileList(va("models/players/%s", model), ".skin", list, sizeof(list));
	const char *name = list;

	for (int i = 0; i < count; i++, name += strlen(name) + 1)
	{
		char skin[MAX_QPATH];
		COM_StripExtension(name, skin, sizeof(skin));
		if (Q_stricmpn(skin, "model_", 6) == 0)
		{
			skins.push_back(skin);
		}
	}
	if (skins.empty())
	{
		skins.push_back("model_default");
	}
	return skins;
}

// One job per (model, skin), shared by all the characters and variants that use the model.
static void Portrait_BuildJobs(void)
{
	std::vector<std::string> models;

	portraitJobs.clear();
	for (int c = 0; c < loadedCharacters; c++)
	{
		const characterInfo_t *character = &charactersData[c];
		for (int v = 0; v < character->variantCount; v++)
		{
			const char *model = character->variantList[v].model;
			qboolean known = qfalse;
			for (const std::string &done : models)
			{
				known = (qboolean)(known || !Q_stricmp(done.c_str(), model));
			}
			if (Q_IsStringEmpty(model) || known)
			{
				continue;
			}
			models.push_back(model);

			for (const std::string &skin : Portrait_ListSkins(model))
			{
				portraitJob_t job = {};
				Q_strncpyz(job.model, model, sizeof(job.model));
				Q_strncpyz(job.skin, skin.c_str(), sizeof(job.skin));
				Com_sprintf(job.skinPath, sizeof(job.skinPath), "models/players/%s/%s.skin", model, job.skin);
				Com_sprintf(job.file, sizeof(job.file), "portraits/%s/%s.tga", model, job.skin + 6);	// no "model_" prefix
				job.color[0] = job.color[1] = job.color[2] = 255;
				portraitJobs.push_back(job);
			}
		}
	}
}

static void Portrait_Stop(void)
{
	gi.G2API_CleanGhoul2Models(portrait.ghoul2);
	portraitJobs.clear();
	portrait.active = qfalse;
	Com_Printf("charportraits: %d portraits written in portraits/, %d already there\n", portrait.written, portrait.skipped);
}

/*
The renderer keeps every model and skin until the next map load (MAX_SKINS = 512).
"charportraits" renders PORTRAIT_BATCH portraits, writes "<size> <next job>" to the cvar
PORTRAIT_RESUME_CVAR and loads the same map again. On the new map, CG_DrawCharacterPortrait
reads the cvar and continues. The cvar is empty when the list is done or stopped.
*/
static void Portrait_End(void)
{
	const qboolean remaining = (qboolean)(portrait.job < portraitJobs.size());

	if (portrait.single || !remaining)
	{
		cgi_Cvar_Set(PORTRAIT_RESUME_CVAR, "");
		Portrait_Stop();
		if (!portrait.single)
		{
			Com_Printf("charportraits: done\n");
		}
		return;
	}

	if (portrait.batchLoaded == 0)
	{
		// Nothing loaded on a new map: another reload does not help.
		Com_Printf(S_COLOR_RED "charportraits: renderer skin table full at the start of a map, stopped at %d\n", (int)portrait.job);
		cgi_Cvar_Set(PORTRAIT_RESUME_CVAR, "");
		Portrait_Stop();
		return;
	}

	char mapName[MAX_QPATH];
	const int total = (int)portraitJobs.size();
	Q_strncpyz(mapName, Portrait_MenuCvar("mapname"), sizeof(mapName));
	cgi_Cvar_Set(PORTRAIT_RESUME_CVAR, va("%d %d", portrait.outputSize, (int)portrait.job));
	Portrait_Stop();
	Com_Printf("charportraits: %d / %d, loading %s again\n", (int)portrait.job, total, mapName);
	cgi_SendConsoleCommand(va("map %s\n", mapName));
}

static void Portrait_StartBatch(int outputSize, int firstJob)
{
	portrait.active = qtrue;
	portrait.single = qfalse;
	portrait.outputSize = outputSize;
	portrait.job = Q_max(0, firstJob);
	portrait.batchLoaded = 0;
	portrait.skipped = 0;
	portrait.tableFull = qfalse;
	portrait.written = 0;
	Portrait_BuildJobs();
	Com_Printf("charportraits: from portrait %d of %d, up to %d on this map\n", (int)portrait.job, (int)portraitJobs.size(), PORTRAIT_BATCH);
	if (!Portrait_Next())
	{
		Portrait_End();
	}
}

void CG_Characters_Portraits_f(void)
{
	if (portrait.active)
	{
		cgi_Cvar_Set(PORTRAIT_RESUME_CVAR, "");
		Portrait_Stop();
		return;
	}
	const int outputSize = cgi_Argc() > 1 ? atoi(CG_Argv(1)) : 256;
	cgi_Cvar_Set(PORTRAIT_RESUME_CVAR, va("%d %d", outputSize, 0));
	Portrait_StartBatch(outputSize, cgi_Argc() > 2 ? atoi(CG_Argv(2)) : 0);
	if (cgs.glconfig.vidWidth < outputSize || cgs.glconfig.vidHeight < outputSize)
	{
		Com_Printf(S_COLOR_YELLOW "charportraits: window is smaller than %dx%d, portraits are upscaled\n", outputSize, outputSize);
	}
}

// Value of a character menu cvar. The last three values stay valid.
static const char *Portrait_MenuCvar(const char *cvarName)
{
	static vmCvar_t cvars[3];
	static int index;
	vmCvar_t *cvar = &cvars[index++ % 3];

	cgi_Cvar_Register(cvar, cvarName, "", 0);
	cgi_Cvar_Update(cvar);
	return cvar->string;
}

/*
charportrait [size=256] [X=0] [Y=8] [W=20] [H=20]
One portrait of the character selected in the character menu (model, skin and colors of the menu).
The frame is W x H model units; its center is at the *head_top tag plus X to the right and Y down.
The larger side of the TGA is <size> pixels. Output: portraits/<model>/<skin>.tga.
*/
void CG_Characters_Portrait_f(void)
{
	if (portrait.active)
	{
		Com_Printf("charportrait: a portrait is already in progress\n");
		return;
	}

	char model[64];
	Q_strncpyz(model, Portrait_MenuCvar("ui_char_model"), sizeof(model));
	if (Q_IsStringEmpty(model))
	{
		Com_Printf("charportrait: no character selected in the character menu\n");
		return;
	}

	const char *head = Portrait_MenuCvar("ui_char_skin_head");
	const char *torso = Portrait_MenuCvar("ui_char_skin_torso");
	const char *lower = Portrait_MenuCvar("ui_char_skin_legs");

	portraitJob_t job = {};
	Q_strncpyz(job.model, model, sizeof(job.model));
	Q_strncpyz(job.skin, head, sizeof(job.skin));
	// Same skin string as UI_UpdateCharacterSkin.
	Com_sprintf(job.skinPath, sizeof(job.skinPath), "models/players/%s/|%s|%s|%s", model, head, torso, lower);
	if (!Q_stricmp(head, torso) && !Q_stricmp(head, lower))
	{
		Com_sprintf(job.file, sizeof(job.file), "portraits/%s/%s.tga", model, Q_stricmpn(head, "model_", 6) ? head : head + 6);
	}
	else
	{
		Com_sprintf(job.file, sizeof(job.file), "portraits/%s/%s_%s_%s.tga", model, head, torso, lower);
	}
	job.color[0] = (byte)Com_Clamp(0, 255, atoi(Portrait_MenuCvar("ui_char_color_red")));
	job.color[1] = (byte)Com_Clamp(0, 255, atoi(Portrait_MenuCvar("ui_char_color_green")));
	job.color[2] = (byte)Com_Clamp(0, 255, atoi(Portrait_MenuCvar("ui_char_color_blue")));

	portrait.single = qtrue;
	portrait.outputSize = cgi_Argc() > 1 ? atoi(CG_Argv(1)) : 256;
	portrait.singleX = cgi_Argc() > 2 ? atof(CG_Argv(2)) : 0;
	portrait.singleY = cgi_Argc() > 3 ? atof(CG_Argv(3)) : 8;
	portrait.singleWidth = cgi_Argc() > 4 ? atof(CG_Argv(4)) : 20;
	portrait.singleHeight = cgi_Argc() > 5 ? atof(CG_Argv(5)) : 20;
	if (portrait.outputSize <= 0 || portrait.singleWidth <= 0 || portrait.singleHeight <= 0)
	{
		Com_Printf("usage: charportrait [size] [X] [Y] [W] [H]\n");
		return;
	}

	portraitJobs.clear();
	portraitJobs.push_back(job);
	portrait.job = 0;
	portrait.tableFull = qfalse;
	portrait.written = 0;
	portrait.skipped = 0;
	portrait.batchLoaded = 0;
	portrait.active = qtrue;
	if (!Portrait_Next())
	{
		Portrait_End();
	}
}

void CG_DrawCharacterPortrait(void)
{
	static const vec4_t black = { 0, 0, 0, 1 };
	static const vec4_t white = { 1, 1, 1, 1 };

	if (!portrait.active)
	{
#ifdef _DEBUG
		// "charportraits" continues after its map load, when the new map has run for 3 seconds.
		int outputSize, nextJob;
		if (cg.time > 3000 && sscanf(Portrait_MenuCvar(PORTRAIT_RESUME_CVAR), "%d %d", &outputSize, &nextJob) == 2)
		{
			Portrait_StartBatch(outputSize, nextJob);
		}
#endif
		return;
	}

	// Load the next job before this frame queues any scene: the previous scene still points to the old ghoul2.
	if (portrait.step >= PORTRAIT_STEP_COUNT)
	{
		portrait.job++;
		if (!Portrait_Next())
		{
			Portrait_End();
			return;
		}
	}

	const portraitJob_t *job = &portraitJobs[portrait.job];

	// "charportrait" has its frame from the command line: no measure.
	if (portrait.single && portrait.step == PORTRAIT_STEP_MEASURE_BLACK_REQUEST)
	{
		portrait.step = PORTRAIT_STEP_CLOSE_FRAME + 1;
	}

	if (portrait.step == PORTRAIT_STEP_CLOSE_FRAME)
	{
		const portraitFrame_t result = Portrait_CloseFrame();
		if (result == PORTRAIT_FRAME_FAIL)
		{
			Com_Printf(S_COLOR_YELLOW "charportraits: no silhouette for %s, skipped\n", job->file);
			portrait.step = PORTRAIT_STEP_COUNT;
			return;
		}
		if (result == PORTRAIT_FRAME_RETRY)
		{
			portrait.step = PORTRAIT_STEP_MEASURE_BLACK_REQUEST;
		}
	}
	if (portrait.step == PORTRAIT_STEP_MEASURE_BLACK_REQUEST)
	{
		cgi_Cvar_Set("r_portraitHead", "");
	}

	const qboolean whitePass = (qboolean)(
		(portrait.step >= PORTRAIT_STEP_MEASURE_WHITE_REQUEST && portrait.step < PORTRAIT_STEP_CLOSE_FRAME)
		|| portrait.step >= PORTRAIT_STEP_WHITE_REQUEST);

	CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, whitePass ? white : black);

	// The larger side of the shot uses up to 1024 pixels of the screen, and <size> pixels in the TGA.
	const float bottom = portrait.frameBottom, top = portrait.frameTop;
	const float frameHeight = top - bottom;
	const float frameWidth = portrait.frameWidth;
	const float longSide = Q_max(frameWidth, frameHeight);
	const int screen = Q_min(1024, Q_min(cgs.glconfig.vidWidth, cgs.glconfig.vidHeight));
	const int viewWidth = Q_max(1, (int)(screen * frameWidth / longSide + 0.5f));
	const int viewHeight = Q_max(1, (int)(screen * frameHeight / longSide + 0.5f));
	const int outWidth = Q_max(1, (int)(portrait.outputSize * frameWidth / longSide + 0.5f));
	const int outHeight = Q_max(1, (int)(portrait.outputSize * frameHeight / longSide + 0.5f));

	// tan(15 deg) = 0.268: with a 30 degree vertical fov, this distance shows the half height of the shot.
	const float distance = 0.5f * frameHeight / 0.268f;
	refdef_t refdef;
	memset(&refdef, 0, sizeof(refdef));
	refdef.rdflags = RDF_NOWORLDMODEL;
	refdef.width = viewWidth;
	refdef.height = viewHeight;
	refdef.fov_x = 2.0f * RAD2DEG(atan(0.5f * frameWidth / distance));
	refdef.fov_y = 30;
	refdef.time = cg.time;
	AxisClear(refdef.viewaxis);

	refEntity_t ent;
	memset(&ent, 0, sizeof(ent));
	ent.ghoul2 = &portrait.ghoul2;
	ent.hModel = portrait.model;
	ent.customSkin = portrait.skin;
	ent.radius = 1000;
	ent.renderfx = RF_NOSHADOW;
	ent.shaderRGBA[0] = job->color[0];
	ent.shaderRGBA[1] = job->color[1];
	ent.shaderRGBA[2] = job->color[2];
	ent.shaderRGBA[3] = 255;
	VectorSet(ent.origin, distance, -portrait.frameSide, -0.5f * (top + bottom));
	VectorCopy(ent.origin, ent.oldorigin);
	VectorCopy(ent.origin, ent.lightingOrigin);
	const vec3_t angles = { 0, 180, 0 };
	AnglesToAxis(angles, ent.axis);

	cgi_R_ClearScene();
	cgi_R_AddRefEntityToScene(&ent);
	cgi_R_AddLightToScene(refdef.vieworg, 500, 1, 1, 1);
	cgi_R_RenderScene(&refdef);

	if (portrait.step == PORTRAIT_STEP_MEASURE_BLACK_REQUEST || portrait.step == PORTRAIT_STEP_MEASURE_WHITE_REQUEST)
	{
		cgi_SendConsoleCommand(va("r_capturePortrait %d %d %d 0 0 -\n", whitePass ? 1 : 0, viewWidth, viewHeight));
	}
	if (portrait.step == PORTRAIT_STEP_BLACK_REQUEST || portrait.step == PORTRAIT_STEP_WHITE_REQUEST)
	{
		cgi_SendConsoleCommand(va("r_capturePortrait %d %d %d %d %d %s\n", whitePass ? 1 : 0, viewWidth, viewHeight, outWidth, outHeight, job->file));
	}
	if (portrait.step == PORTRAIT_STEP_WHITE_CAPTURE)
	{
		portrait.written++;
	}

	portrait.step++;
}
#pragma endregion
