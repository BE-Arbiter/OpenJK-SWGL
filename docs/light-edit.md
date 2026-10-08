# Mode « light edit » (RTX, solo)

Le mode light edit permet de placer, déplacer, orienter, régler et supprimer en jeu les lumières que le path tracer RTX de `rdsp-vulkan` utilise. Les changements sont visibles à l'image suivante. Ils sont enregistrés dans `maps/<map>.lgt`, qui est relu à chaque chargement de la map, en mode normal comme en mode édition.

Ce document décrit le MVP et les itérations 2 et 3 : les neuf outils, les spots, la sélection multiple, la grille, les contraintes d'axe et l'accumulation à vue fixe, les raccourcis clavier, le menu, les étiquettes et les filtres d'affichage.

## 1. Prérequis

- **Renderer** : `cl_renderer rdsp-vulkan` avec `r_rtx 1`. Le raster (sans RTX), `rdsp_swgl` (rd-vanilla) et `rdsp-rend2` n'ont pas l'extension. La commande répond alors `light edit: needs the RTX renderer (cl_renderer rdsp-vulkan, r_rtx 1)` et rien ne change.
- **Cheats** : le mode exige les cheats, comme `noclip`. Lancez la map avec `devmap <map>`, ou mettez `helpusobi 1`.
- **Double renderer** (`g_FastRendererSwitch 1`) : l'extension est trouvée dans celui des deux renderers qui la fournit.

## 2. Entrée et sortie

| Commande | Effet |
|---|---|
| `lightedit` | Bascule le mode. |
| `lightedit 1` | Entre dans le mode (sans effet s'il est déjà actif). |
| `lightedit 0` | Sort du mode. |
| `lightedit 0 discard` | Sort du mode et abandonne les changements non sauvegardés (relit le `.lgt`). |

À l'entrée :
- le joueur passe en noclip, god, notarget, et les pouvoirs de la Force sont coupés ;
- le tir et les pouvoirs ne partent plus (le jeu retire les boutons de tir et de Force) ;
- l'arme en vue subjective n'est plus dessinée ;
- le HUD normal est remplacé par l'overlay du mode ;
- la sauvegarde de la partie est refusée tant que le mode est actif.

À la sortie, le solo et les lumières muettes sont rétablis, et `pt_accumulation_rendering` reprend sa valeur d'avant si `ledit_still_accum` l'avait changée.

À la sortie, l'état d'origine du joueur (noclip, god, notarget, Force) est restauré. S'il reste des changements non sauvegardés, un message l'indique. Ils restent actifs en mémoire jusqu'au prochain chargement de map, sauf avec `discard`.

Le mode n'est jamais sauvegardé dans une partie. Un changement de map ou le chargement d'une partie le désactive.

La cvar `ledit_active` (lecture seule) vaut 1 pendant le mode.

## 3. Contrôles communs

Le mode réutilise les touches des armes :

| Entrée | Commande d'origine | Action |
|---|---|---|
| Touches 1 à 9 | `weapon N` | Choisit l'outil (1 à 9). |
| Tir principal | `+attack` | Action principale de l'outil. |
| Tir alternatif | `+altattack` | Action secondaire de l'outil. |
| Molette | `weapnext` / `weapprev` | Valeur de l'outil. |
| `]` / `[` | `invnext` / `invprev` | Propriété active de l'outil 5. |
| Marche (maintenue) | walk | Pas fin pour la molette. |
| Saut / accroupi | `+moveup` / `+movedown` | Monter / descendre (noclip). |
| Utiliser | `+use` | Place la caméra devant la sélection, tournée vers elle (`ledit_goto`). |

### Raccourcis clavier

Ces touches fonctionnent sans bind, seulement en mode light edit, et seulement quand ni la console ni un menu ne sont ouverts. Toutes les autres touches gardent leur bind.

| Touche | Action |
|---|---|
| Ctrl+Z | Annuler. |
| Ctrl+Y ou Ctrl+Shift+Z | Rétablir. |
| Ctrl+S | Sauvegarder (`ledit_save`). |
| Ctrl+D | Vider la sélection. |
| Ctrl+M | Ouvrir le menu light edit (§ 5 bis). |
| Suppr | Supprimer ou désactiver la sélection. |
| Entrée (ou Entrée du pavé) | Saisie numérique de la propriété active (voir ci-dessous). |

**Saisie numérique.** Entrée ouvre un champ pour la propriété active de l'outil 5 (l'intensité avec les autres outils), rempli avec la valeur de la lumière principale. Un premier caractère remplace la valeur ; chiffres, `.`, `-` et espace s'ajoutent, Retour arrière efface. Entrée applique la valeur à toute la sélection (une entrée d'undo), Échap annule. Pendant la saisie, les chiffres ne changent pas d'outil. Les valeurs : intensité ; teinte (0-360, ou `teinte sat valeur`) ; saturation (0-1) ; température (K) ; rayon ; cône (`ext [int]`). Le champ se ferme à la sortie du mode, au changement de map, quand la sélection est vide ou après 30 s sans touche.

Échap n'est consommé que pendant une saisie ; sinon il ouvre le menu du jeu, comme d'habitude. La touche de la console et Shift+Échap ne sont jamais interceptés.

Ctrl est souvent lié à l'accroupissement : un raccourci Ctrl fait descendre un peu la caméra en noclip.

### Cibles d'une action

- La **sélection** est une liste de lumières. La dernière lumière cliquée est la lumière **principale** : elle porte la sphère ou le cône filaire, et le panneau la décrit.
- Une action vise la lumière **sous le viseur**. Si cette lumière fait partie de la sélection, l'action s'applique à **toute la sélection**. Si rien n'est visé, l'action s'applique à la sélection (outils 3, 4, 5, 7, 8).
- Les actions de groupe gardent les écarts entre lumières (position, intensité relative).
- Une action de groupe compte pour **une seule** entrée d'undo.

## 4. Outils

### 1. Sélection
- **Tir** : sélectionne la lumière visée et remplace la sélection. Dans le vide, vide la sélection.
- **Alt** : ajoute la lumière visée à la sélection, ou l'en retire.
- **Molette** : passe d'une lumière à l'autre quand plusieurs sont alignées sous le viseur (triées par distance).

### 2. Création
- Un « fantôme » suit le point d'impact du viseur, décalé le long de la normale de la surface. Avec `ledit_snap 1`, il est accroché à la grille.
- **Tir** : ajoute une lumière au fantôme avec le préréglage (`ledit_preset_*`, ou le presse-papier de la pipette), puis la sélectionne.
- **Alt** : change le type des nouvelles lumières, sphère ↔ spot. Un spot posé vise l'opposé de la normale de la surface (cônes 35° / 25°, ou ceux du presse-papier).
- **Molette** : décalage par rapport à la surface, ×2 ou ÷2 (±1 avec la marche), de 2 à 256 unités.
- Sans impact (ciel), le fantôme est à 256 unités devant la caméra. Dans un solide, il est rouge et la pose est refusée. Un solo actif (outil 9) est terminé avant la pose.

### 3. Déplacement (« physgun »)
- **Tir maintenu** sur une lumière : la saisit (ou toute la sélection si elle en fait partie). Elle suit le viseur à distance constante.
- **Molette pendant la saisie** : distance ×1,1 ou ÷1,1 (×1,01 avec la marche). Sans effet en mode contraint.
- **Alt pendant la saisie** : contrainte libre → axe X → axe Y → axe Z → plan de la surface → libre.
  - **Axe** : la lumière glisse sur l'axe du monde qui passe par sa position au choix de la contrainte (ligne en pointillés rouge, verte ou bleue).
  - **Plan de la surface** : la lumière suit la surface visée, à la distance qu'elle avait de la surface au début de la saisie (cercle jaune au point visé).
- **Relâcher** : valide le déplacement (une entrée d'undo).
- **Alt sans saisie** : remet les cibles à leur position d'origine.
- Une position dans un solide n'est pas appliquée : la lumière garde sa dernière position valide.
- Avec `ledit_snap 1`, la lumière principale est accrochée à la grille (seule la composante de l'axe en mode contraint).

### 4. Orientation (spots)
- **Tir** : oriente les spots ciblés vers le point visé. Avec `ledit_angle_snap` > 0, le lacet et le tangage de la direction sont arrondis à ce pas.
- **Alt sur un spot** : choisit l'angle que la molette règle (extérieur ou intérieur).
- **Alt sur une sphère** : convertit les sphères ciblées en spots, dirigés vers le point visé (ou vers le bas).
- **Molette** : angle choisi ± `ledit_angle_snap` (1° avec la marche). Extérieur 1-89°, intérieur 0 à l'extérieur. Réduire l'extérieur sous l'intérieur abaisse l'intérieur.
- Affichage : cône filaire du spot principal (axe jusqu'au monde, cercle du bord, 8 génératrices, cercle intérieur, disque éclairé au point d'impact) ; axe et bord seulement pour 3 autres spots sélectionnés au plus.

### 5. Propriétés
- `]` / `[` choisit la propriété active : intensité, teinte, saturation, température, rayon d'émission, cône extérieur, cône intérieur. Elle est en jaune dans le panneau, avec une jauge.
- **Molette** sur les cibles :
  - intensité ×1,1 (×1,01 avec la marche) ;
  - teinte ±10° (±1°) ; saturation ±0,05 (±0,01) ;
  - température ±250 K (±50 K) : la couleur devient celle d'un corps noir de 1000 à 12000 K, normalisée ;
  - rayon d'émission ±1 (±0,1), minimum 0,5 ;
  - cônes comme l'outil 4.
- **Tir** : copie la propriété active de la lumière visée sur toute la sélection.
- **Alt** : remet la propriété active à sa valeur d'origine (pour la température, toute la couleur d'origine).
- Les crans de molette successifs sur les mêmes lumières et la même propriété, à moins de 500 ms d'écart, forment **une** entrée d'undo.
- La teinte n'agit pas sur une couleur grise (saturation 0) : augmentez d'abord la saturation.

### 6. Pipette
- **Tir** : copie la lumière visée dans le presse-papier (type, couleur, intensité, rayon, direction, cônes). Le presse-papier devient le préréglage de l'outil 2 jusqu'au changement de map.
- **Alt** : colle sur la lumière visée (ou la sélection si elle en fait partie), selon le filtre.
- **Molette** : filtre du collage : tout / couleur / intensité / forme (type, cônes, rayon). Coller une forme de spot sur une sphère la convertit et prend la direction du presse-papier.

### 7. Clonage
- **Tir** : duplique la lumière visée (ou la sélection si elle en fait partie). Les copies deviennent la sélection, l'outil 3 s'active et les copies sont saisies tout de suite.
- **Alt** : crée N copies à 1, 2, … N pas de grille (`ledit_grid`) le long de l'axe X ou Y du monde le plus proche de la droite de la vue. Les copies hors du monde sont sautées.
- **Molette** : N, de 1 à 32.
- Une copie est toujours une lumière **ajoutée** ; son intensité est convertie pour garder la même luminosité que la source (voir § 8).

### 8. Suppression et restauration
- **Tir** : supprime la lumière visée (ou la sélection). Une lumière ajoutée est effacée. Une lumière d'origine est **désactivée** : icône rouge barrée, translucide.
- **Alt** : restaure la lumière désactivée visée.

### 9. Solo et muet
- **Tir** : met la lumière visée en **solo** : toutes les autres lumières éditables sont éteintes. Un second tir termine le solo.
- **Alt** : coupe ou rétablit la lumière visée (**muet**).
- **Molette** : filtre des icônes : toutes / ajoutées / modifiées / d'origine / désactivées.
- Le solo et le muet ne sont ni des éditions ni sauvegardés, et ne passent pas par l'undo. Ils sont annulés à la sortie du mode et au changement de map.

## 5. Affichage

- **Icônes** :
  - bleu : entité `light` du BSP ;
  - jaune : lumière reconstruite (`.lgt`, `pt_lightgen`) ;
  - vert : lumière ajoutée ;
  - orange : lumière d'origine modifiée ;
  - rouge barré, translucide : lumière désactivée ;
  - rouge plein : lumière dans un solide (elle n'émet rien) ;
  - contour gris : lumière muette ou éteinte par le solo.
- Cadre blanc : lumière sous le viseur. Cadre de chaque lumière sélectionnée ; sphère filaire (sphère) ou cône filaire (spot) sur la lumière principale.
- Sans `ledit_xray`, les lumières cachées par un mur, et celles à plus de 3000 unités, ne sont pas affichées.
- **En haut à gauche** : outil actif et aide (Tir / Alt / Molette), avec la valeur courante (décalage, angle, propriété, filtre, nombre de copies, contrainte).
- **À droite** : panneau de la sélection. Pour une lumière : id, source, type, état, origine, couleur, teinte/saturation/température, intensité, rayon, direction et cônes (spot), nom ; valeur d'origine en gris si elle a changé. Pour plusieurs : « N lights (primary id) », puis la valeur commune ou « - ».
- **En bas** : map, compteurs, slots utilisés / limite, profondeur d'undo et de redo, `*` si des changements ne sont pas sauvegardés, grille et snap, `SOLO <id>`, filtre d'icônes, avertissements.
- **Étiquettes** (`ledit_label`) : nom (ou `#id`), type et intensité à côté de l'icône.
- **Filtres d'affichage** (`ledit_show`) : 2 ajoute les lumières émissives (points gris), 3 ajoute aussi les lumières dynamiques de l'image précédente (points magenta : dlights, sabres). Elles ne sont pas sélectionnables. Le bas de l'écran affiche « show N ».
- Tout le dessin de l'overlay respecte un budget fixe par image (points et caractères) : un cône, une grande sélection ou beaucoup d'étiquettes ne peuvent pas saturer le buffer de commandes du renderer. Les formes de la sélection et des outils passent en premier, puis les icônes, puis les points émissifs et dynamiques.

## 5 bis. Menu light edit

`ledit_menu` (ou Ctrl+M) ouvre un menu sur la moitié droite de l'écran ; la scène reste visible à gauche. Échap ou « Close » le ferme et rend la main au jeu.

- **En-tête** : map, nombre de lumières, d'émissives, compteurs par source, `* unsaved`.
- **Filtre** (cvar `ui_ledit_filter` : all, added, entity, lgt, modified, disabled, spots ; un clic passe à la valeur suivante) et **recherche** (`ui_ledit_search`, sous-chaîne du nom, sans casse).
- **Liste** : id, source (`added`, `ent N`, `lgt N`), type, intensité, état (M modifiée, D désactivée, S dans un solide, m muette), nom. Un clic sur une ligne la choisit et la sélectionne (`ledit_select`).
- **Boutons** (sur la ligne choisie) : Select, Add to sel., Go to, Delete, Restore, Solo, End solo, Revert, Close.
- **Couleur** : curseurs teinte (0-360), saturation, valeur, température (1000-12000 K) avec un aperçu. « Load from light » lit la couleur, l'intensité et le rayon de la ligne choisie ; « Apply colour » applique la couleur HSV ; « Use temperature » met la couleur du corps noir dans les curseurs ; « Apply temperature » l'applique.
- **Valeurs** : champs intensité et rayon, chacun avec « Apply ».

Le menu envoie des commandes du mode (`ledit_select`, `ledit_set`, `ledit_goto`, `ledit_delete`, `ledit_revert`, `pt_ledit_restore`, `pt_ledit_solo`) : ses changements passent par l'undo (sauf Restore et Solo). Le texte du menu est intégré à l'exe ; un fichier `ui/lightedit.menu` dans les données du jeu le remplace s'il existe (pour retoucher la mise en page sans recompiler). La liste ne suit pas une sélection faite en jeu : les boutons agissent sur la dernière ligne choisie dans le menu.

Le **rayon d'émission** n'est pas une portée. Une sphère du tracer éclaire en 1/d², sans coupure. Le rayon d'émission règle seulement la douceur des ombres.

## 6. Commandes

Toutes, sauf `lightedit`, demandent le mode actif.

| Commande | Effet |
|---|---|
| `ledit_save` | Écrit `maps/<map>.lgt` (v2) dans le homepath, après une copie de l'ancien fichier en `maps/<map>.lgt.bak`. |
| `ledit_reload` | Abandonne les changements en mémoire et relit le fichier. Vide l'undo et la sélection. |
| `ledit_undo` / `ledit_redo` | Annule / rétablit la dernière action (ou le dernier groupe). |
| `ledit_history` | Affiche les piles d'undo et de redo (une ligne par groupe, avec sa taille). |
| `ledit_set origin x y z` | Place la lumière principale ; les autres lumières sélectionnées gardent leur écart. |
| `ledit_set color r g b` | Couleur (0 à 1) de la sélection. |
| `ledit_set intensity v` | Intensité (voir § 8). |
| `ledit_set radius v` | Rayon d'émission. |
| `ledit_set name texte` | Nom. |
| `ledit_set type sphere\|spot` | Convertit la sélection. |
| `ledit_set dir x y z` | Direction (spot). |
| `ledit_set cone ext [int]` | Demi-angles des cônes, en degrés. |
| `ledit_get` | Affiche les valeurs de la sélection (et les valeurs d'origine si elle est modifiée). |
| `ledit_delete` | Supprime ou désactive la sélection. |
| `ledit_deselect` | Vide la sélection. |
| `ledit_revert` | Remet toutes les valeurs d'origine de la sélection. |
| `ledit_grid_next` | Pas de grille suivant (1, 2, 4, 8, 16, 32, 64, 128). |
| `ledit_snap_toggle` | Active ou coupe le snap. |
| `ledit_xray_toggle` | Active ou coupe le mode « à travers les murs ». |
| `ledit_select <id> [add]` / `ledit_select none` | Sélectionne une lumière par son id (ou l'ajoute à la sélection), ou vide la sélection. |
| `ledit_goto` | Place la caméra devant la sélection, tournée vers elle, hors des murs (aussi : Utiliser). |
| `ledit_menu` | Ouvre le menu light edit (aussi : Ctrl+M). |
| `ledit_writebinds [force]` | Écrit `lightedit_binds.cfg` dans le homepath (refuse d'écraser un fichier existant sans `force`), puis indique `exec lightedit_binds.cfg`. |

Commandes de débogage du renderer (utilisables même hors du mode) : `pt_ledit_list`, `pt_ledit_add x y z [intensité] [r g b]`, `pt_ledit_set <id> <origin|color|intensity|radius> <valeurs>`, `pt_ledit_set <id> spot dx dy dz ext int`, `pt_ledit_set <id> sphere`, `pt_ledit_del <id>`, `pt_ledit_restore <id>`, `pt_ledit_mute <id> <0|1>`, `pt_ledit_solo <id|-1>`, `pt_ledit_stats`, `pt_ledit_emissive [n]`, `pt_ledit_dynamic`.

## 7. Cvars

| Cvar | Défaut | Rôle |
|---|---|---|
| `ledit_xray` | 0 | 1 : icônes visibles et sélection possible à travers les murs. |
| `ledit_show` | 1 | 0 : aucune icône ; 1 : lumières éditables ; 2 : + lumières émissives (points gris, à moins de 2048 unités) ; 3 : + lumières dynamiques de l'image précédente (points magenta, 64 au plus). Les lumières émissives et dynamiques ne sont pas sélectionnables. |
| `ledit_label` | 1 | 0 : aucune étiquette ; 1 : la lumière visée et les lumières sélectionnées ; 2 : + les lumières visibles à moins de 1024 unités de l'œil, les plus proches d'abord (24 étiquettes au plus). Une étiquette donne le nom (ou `#id`), puis le type et l'intensité, avec `muted` ou `disabled` si besoin. |
| `ledit_grid` | 16 | Pas de la grille (1, 2, 4, 8, 16, 32, 64, 128). |
| `ledit_snap` | 0 | 1 : accroche les positions à la grille (création, déplacement, clonage). |
| `ledit_angle_snap` | 15 | Pas d'angle (degrés) des cônes et des directions. 0 : libre. |
| `ledit_still_accum` | 0 | 1 : quand la vue ne bouge plus depuis 500 ms (et sans saisie ni bouton), met `pt_accumulation_rendering 1` pour une image de référence sans bruit ; le premier mouvement rétablit la valeur d'avant. La valeur est aussi rétablie à la sortie du mode. |
| `ledit_preset_intensity` | 2000 | Intensité d'une lumière créée. |
| `ledit_preset_radius` | 8 | Rayon d'émission d'une lumière créée. |
| `ledit_preset_color` | `1 0.9 0.8` | Couleur d'une lumière créée. |
| `pt_light_scale_edit` | 0.1 | Échelle unique des lumières **ajoutées**. |
| `ledit_active` | — | Lecture seule, 1 pendant le mode. |

Les cvars `ledit_*` sont archivées : elles gardent leur valeur d'une session à l'autre.

Le renderer redémarre l'accumulation (`pt_accumulation_rendering`) dès que la liste des lumières change.

## 8. Intensités et échelles

- Une lumière **ajoutée** a une intensité en unités de la clé `light` de q3map2, multipliée par `pt_light_scale_edit`. Cette échelle est la même partout : déplacer la lumière ne change pas sa luminosité.
- Une **entité** du BSP garde l'unité `light` de q3map2 et la classe d'échelle calculée au chargement à sa position d'origine (`pt_light_scale_ent_spot`, `_sky` ou `_ambient`). La déplacer ou la modifier ne change pas sa classe.
- Une lumière **reconstruite** (`lgt`) garde son unité (unités de lightmap × `pt_lightgen_scale`).
- Le clonage, la pipette et la copie de l'intensité (outil 5) convertissent l'intensité d'une source à l'autre, pour garder la même luminosité : I2 = I1 × échelle1 / échelle2.
- Convertir une sphère en spot garde la luminosité sur l'axe du spot.


## 9. Binds conseillés

Les touches des armes, le tir, la molette et `[` / `]` suffisent pour les outils. Le fichier `docs/lightedit_binds.cfg` propose des binds sur le pavé numérique ; `ledit_writebinds` l'écrit dans le homepath, `exec lightedit_binds.cfg` l'applique. Rien n'est exécuté automatiquement. Autre exemple (à adapter) :

```
bind F6 "lightedit"
bind F7 "ledit_save"
bind F8 "ledit_undo"
bind F9 "ledit_redo"
bind DEL "ledit_delete"
bind F10 "ledit_xray_toggle"
bind F11 "ledit_snap_toggle"
bind F12 "ledit_grid_next"
```

Vérifiez que la molette est liée à `weapnext` / `weapprev` (`bind MWHEELUP weapnext`, `bind MWHEELDOWN weapprev`) et que `[` / `]` sont liés à `invprev` / `invnext`.

## 10. Le fichier `.lgt` (format v2)

Emplacement : `maps/<map>.lgt` dans le homepath (par exemple `Documents\My Games\SWGL\SWGL\maps`). Une copie de sécurité `maps/<map>.lgt.bak` est faite avant chaque `ledit_save`.

Au chargement de la map, le fichier est **toujours** lu :
- les blocs **lgt** (lumières reconstruites) sont chargés seulement si la map n'a aucune entité `light`, comme avant. Sinon, ils sont conservés tels quels à la sauvegarde ;
- les surcharges d'entités, les désactivations et les lumières ajoutées sont **toujours** appliquées.

Exemple :

```
version 2
// light edit file of kejim_base; lgt blocks, entity overrides, added lights
lights 0
{                         // lumière reconstruite jamais modifiée (forme v1)
	origin 10 20 30
	color 1 0.9 0.8
	intensity 4200
	rays 6
	error 3.1
}
{                         // lumière reconstruite modifiée ou désactivée
	source lgt
	edited 1
	origin -1700 -27000 560
	color 0.5 0.5 1
	intensity -3000       // négative si désactivée
	disabled 1
	rays 4
	error 2
}
{                         // surcharge d'une entité light du BSP
	source entity
	ent 63                // rang parmi les entités "classname light" du BSP
	check 384 -1536 -8    // origine d'origine, pour détecter un BSP recompilé
	origin 384 -1536 -8
	color 1 0 0
	light 20
	radius 16
	intensity -1          // toujours -1
}
{                         // lumière ajoutée
	source added
	origin -2315.5 -1492.7 12.0
	color 0.2 1 0.2
	intensity 6000
	radius 8
}
{                         // spot ajouté
	source added
	origin -1312 -4512 4576
	color 1 0.9 0.8
	intensity 2000
	radius 8
	type spot             // absent pour une sphère
	dir 0 0 -1            // direction du spot
	cone_outer 35         // demi-angle extérieur, en degrés
	cone_inner 25         // demi-angle intérieur, en degrés
}
```

Règles :
- `lights N` donne le nombre de blocs lgt (information seulement).
- Les blocs lgt sont écrits en premier, dans leur ordre : leur rang (`lgt N`) reste stable d'une sauvegarde à l'autre.
- Chaque clé a 1 ou 3 valeurs. Un ancien binaire (parseur v1) lit le fichier sans se décaler : il ignore les blocs d'intensité négative (surcharges, désactivations) et lit les lumières ajoutées comme des lumières reconstruites.
- Si `ent` et `check` ne correspondent plus au BSP (map recompilée), la surcharge est ignorée avec un avertissement, mais elle est conservée dans le fichier.
- Les clés `type`, `dir`, `cone_outer` et `cone_inner` existent aussi pour les blocs lgt édités et les surcharges d'entités. Une surcharge qui change seulement la couleur d'un spot d'origine garde sa direction et ses cônes. `type sphere` note un spot d'origine converti en sphère.
- Les mutes et le solo (outil 9) ne sont jamais écrits.
- Les blocs de `source` inconnue sont conservés tels quels.
- `pt_lightgen <map>` refuse d'écraser un fichier qui contient des éditions. `pt_lightgen <map> force` l'écrase après une copie en `.lgt.bak`.


## 11. Limitations connues

- **RTX seulement.** Le raster utilise les lightmaps et la lightgrid, qui ne changent pas.
- **Pas de lumières rectangulaires**, pas de multiplicateur des surfaces émissives, pas de soleil par map, pas de styles ni de scintillement (itération 4).
- **Clavier** : un raccourci ne marche pas si l'utilisateur a lié une combinaison de touches sur la même touche (le moteur la remplace avant le mode). Le nom d'une lumière se saisit par la console (`ledit_set name`).
- **Menu** : la liste ne suit pas la sélection faite en jeu ; les largeurs de colonnes sont fixes ; pas de champ de nom ni de cônes dans le menu.
- **Sélection** : pas de sélection par boîte, ni de « tout sélectionner de la même couleur ».
- **Limite de lumières** : 4096 lumières en tout dans le tracer (`MAX_LIGHT_POLYS`, constante des shaders), dont 128 gardées pour les lumières dynamiques. Les surfaces émissives comptent aussi : sur kejim_post, environ 90 places restent. Au-delà, l'ajout est refusé (`no free light slot`).
- **1024 lumières par cluster** : un ajout est refusé si un cluster visible est plein. `Restore` et la fin d'un muet font le même contrôle. La fin d'un solo ne peut pas être refusée : si un cluster déborde, un message le dit (« cluster full: N clusters truncated »).
- **Surfaces émissives** (néons, polygones) : non éditables ; affichées en points gris avec `ledit_show 2`, sans test d'occlusion (500 au plus, à moins de 2048 unités) ; la pipette ne peut pas les lire.
- **Une lumière d'origine dans un solide** reste une sphère : on ne peut la convertir en spot qu'après l'avoir sortie du solide.
- **Pas d'icônes au-delà de 3000 unités** sans `ledit_xray`. Les modèles (caisses, portes) ne cachent pas les icônes et ne sont pas touchés par le fantôme ni par le point visé de l'outil 4.
- **Cônes larges** : le cercle du bord est limité à 256 unités de rayon ; il est alors dessiné plus près du sommet du cône.
- **Température** : la teinte d'un corps noir remplace la couleur entière ; une couleur loin de cette courbe saute au premier cran.
- **Undo** : si le renderer refuse une étape d'un groupe (par exemple un cluster plein), le groupe change de pile quand même et l'état peut être partiel ; un message l'indique.
- Un **vid_restart** recharge la map côté renderer : les changements non sauvegardés sont perdus.
- Chaque ajout, clone ou rechargement consomme un emplacement ; les emplacements des lumières supprimées ne sont réutilisés qu'au prochain chargement de la map. L'agrandissement de la réserve (par blocs de 64) provoque un court arrêt du GPU.
- Les débruiteurs gardent une traînée de quelques images après un changement ; `ledit_still_accum 1` donne une image de référence quand la vue est fixe.
- La sauvegarde de la partie est refusée en silence pendant le mode.

## 12. Procédure de test pas à pas

1. Lancer le jeu avec `cl_renderer rdsp-vulkan` et `r_rtx 1`, puis `devmap t1_fatal`, et passer la cinématique (touche Utiliser).
2. `lightedit` : le HUD normal disparaît ; l'overlay affiche « LIGHT EDIT tool 1 Select (keys 1 … 9) ».
3. **Création** : touche **2**, **Alt** (« new lights are spots »), viser le sol, **Tir** : un spot éclaire le sol ; le cône filaire et le disque éclairé apparaissent.
4. **Orientation** : touche **4**, **molette** : le cône s'élargit par pas de 15° ; **Alt** puis **molette** : l'angle intérieur change ; viser un autre point, **Tir** : le spot s'oriente vers lui.
5. **Propriétés** : touche **5**, `]` jusqu'à « hue », **molette** : la couleur tourne ; `]` « temperature », **molette** : la couleur passe du bleu à l'orange. `ledit_history` montre une seule entrée par série de crans.
6. **Pipette** : touche **6**, viser une lumière, **Tir** (« copied light N ») ; touche **2**, **Tir** : la nouvelle lumière prend ses valeurs. Touche **6**, **molette** jusqu'à « colour », viser une autre lumière, **Alt** : seule la couleur est collée.
7. **Sélection multiple** : touche **1**, **Tir** sur une lumière, **Alt** sur une autre : le panneau affiche « 2 lights ». `ledit_set intensity 5000` change les deux.
8. **Clonage** : touche **7**, **molette** pour 4 copies, **Alt** : une rangée de 4 lumières apparaît ; **Tir** sur une lumière : la copie est saisie (outil 3).
9. **Contraintes** : touche **3**, **Tir maintenu** sur une lumière, **Alt** (« constraint X axis »), tourner : la lumière glisse sur X ; relâcher.
10. **Grille** : `ledit_snap_toggle`, `ledit_grid_next` : le bas de l'écran affiche le pas ; un nouvel ajout tombe sur la grille.
11. **Solo / muet** : touche **9**, **Tir** sur une lumière : les autres s'éteignent (`SOLO id` en bas) ; **Tir** à nouveau : retour normal ; **Alt** : la lumière visée s'éteint (icône grise), **Alt** à nouveau : elle revient.
12. **Accumulation** : `ledit_still_accum 1`, ne plus bouger : au bout d'une demi-seconde l'image se stabilise (`pt_accumulation_rendering` vaut 1) ; bouger : la valeur revient à 0.
13. **Durée** : tourner sur soi-même plusieurs fois et voler une minute avec une sélection et un cône affichés : l'image ne gèle pas et la console n'affiche pas « MAX_REFENTITIES ».
14. `ledit_save`, `lightedit 0`, `devmap t1_fatal` : la console affiche « light edit: maps/t1_fatal.lgt: … added » ; les spots sont relus avec leur direction et leurs cônes (`pt_ledit_list`).
15. `pt_lightgen t1_fatal` : refus (« holds light edits »).
16. Avec `cl_renderer rdsp_swgl` : `lightedit` répond « needs the RTX renderer » et le jeu continue normalement.
17. **Clavier** : sélectionner une lumière, **Entrée**, taper `5000`, **Entrée** : l'intensité change ; **Ctrl+Z** : elle revient ; **Ctrl+Y** : elle repart ; **Suppr** : la lumière est supprimée ; **Ctrl+Z** : elle revient. **Entrée**, `9`, **Échap** : rien ne change et le menu du jeu ne s'ouvre pas.
18. **Console** : ouvrir la console, appuyer sur **Ctrl+Z** : rien n'est annulé. Fermer la console.
19. **Aller à** : `ledit_select <id>`, puis **Utiliser** (ou `ledit_goto`) : la caméra se place devant la lumière, tournée vers elle.
20. **Menu** : **Ctrl+M** : le menu s'ouvre à droite ; filtre « Added », recherche `copy` : la liste se réduit ; cliquer une ligne, « Load from light », bouger la teinte, « Apply colour » : la lumière change de couleur, **Ctrl+Z** après fermeture l'annule. **Échap** ferme le menu.
21. **Étiquettes et filtres** : `ledit_label 2`, `ledit_show 3` : noms et valeurs à côté des icônes, points gris des émissives, points magenta des lumières dynamiques (tirer au blaster hors du mode, puis revenir).
22. **Sortie** : `lightedit 0`, puis **Ctrl+Z**, **Suppr**, **Entrée** : ces touches reprennent leur bind normal ; après `devmap`, idem.
