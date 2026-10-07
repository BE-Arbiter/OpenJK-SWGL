// Light edit mode: game-side state (the player is a free-flying, harmless, untargetable camera).
#ifndef G_LIGHTEDIT_H
#define G_LIGHTEDIT_H

qboolean	G_LightEdit_Active( void );
void		G_LightEdit_Init( void );
void		G_LightEdit_SetMode( qboolean on );
void		G_LightEdit_Cmd_f( gentity_t *ent );
void		G_LightEdit_FilterUcmd( gentity_t *ent, usercmd_t *ucmd );

#endif // G_LIGHTEDIT_H
