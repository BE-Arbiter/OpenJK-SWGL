# Mode « light edit » (RTX, solo)

Le mode light edit permet de placer, déplacer, régler et supprimer en jeu les lumières que le path tracer RTX de `rdsp-vulkan` utilise. Les changements sont visibles à l'image suivante. Ils sont enregistrés dans `maps/<map>.lgt`, qui est relu à chaque chargement de la map, en mode normal comme en mode édition.

Ce document décrit le MVP (outils 1, 2, 3 et 8).

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

À la sortie, l'état d'origine du joueur (noclip, god, notarget, Force) est restauré. S'il reste des changements non sauvegardés, un message l'indique. Ils restent actifs en mémoire jusqu'au prochain chargement de map, sauf avec `discard`.

Le mode n'est jamais sauvegardé dans une partie. Un changement de map ou le chargement d'une partie le désactive.

La cvar `ledit_active` (lecture seule) vaut 1 pendant le mode.

## 3. Contrôles communs

Le mode réutilise les touches des armes :

| Entrée | Commande d'origine | Action |
|---|---|---|
| Touches 1 à 9 | `weapon N` | Choisit l'outil. Seuls 1, 2, 3 et 8 existent dans le MVP. |
| Tir principal | `+attack` | Action principale de l'outil. |
| Tir alternatif | `+altattack` | Action secondaire de l'outil. |
| Molette | `weapnext` / `weapprev` | Valeur de l'outil. |
| Marche (maintenue) | `+speed` / walk | Pas fin pour la molette. |
| Saut / accroupi | `+moveup` / `+movedown` | Monter / descendre (noclip). |

## 4. Outils

### 1. Sélection
- **Tir** : sélectionne la lumière visée. Dans le vide, désélectionne.
- **Alt** : désélectionne.
- **Molette** : passe d'une lumière à l'autre quand plusieurs sont alignées sous le viseur (triées par distance).

La sélection est simple (une seule lumière) dans le MVP.

### 2. Création (sphère)
- Un « fantôme » (cercle vert) suit le point d'impact du viseur, décalé le long de la normale de la surface.
- **Tir** : ajoute une lumière sphérique au fantôme, avec le préréglage (`ledit_preset_*`), puis la sélectionne.
- **Alt** : rien dans le MVP (les spots viendront plus tard).
- **Molette** : décalage par rapport à la surface, ×2 ou ÷2 (±1 avec la marche), de 2 à 256 unités. Valeur initiale : 16.
- Sans impact (ciel, trop loin), le fantôme est placé à 256 unités devant la caméra.
- Le fantôme est rouge dans un solide : la pose est refusée. La pose est aussi refusée si un cluster visible est plein (1024 lumières) ou si la limite de lumières est atteinte ; le message donne la raison.

### 3. Déplacement (« physgun »)
- **Tir maintenu** sur une lumière : la saisit. Elle suit le viseur à distance constante.
- **Molette pendant la saisie** : distance ×1,1 ou ÷1,1 (×1,01 avec la marche).
- **Relâcher** : valide le déplacement (une seule entrée d'undo).
- **Alt sans saisie** : remet la lumière visée (ou sélectionnée) à sa position d'origine.
- Une position dans un solide n'est pas appliquée : la lumière garde sa dernière position valide, et la cible est dessinée en rouge.

### 8. Suppression et restauration
- **Tir** : supprime la lumière visée (ou la sélection). Une lumière ajoutée est effacée. Une lumière d'origine (entité du BSP ou lumière reconstruite) est **désactivée** : elle reste visible en icône rouge barrée et peut être restaurée.
- **Alt** : restaure la lumière désactivée visée.

## 5. Affichage

- **Icônes** (carré coloré à la position de chaque lumière) :
  - bleu : entité `light` du BSP ;
  - jaune : lumière reconstruite (`.lgt`, `pt_lightgen`) ;
  - vert : lumière ajoutée ;
  - orange : lumière d'origine modifiée ;
  - rouge barré, translucide : lumière désactivée ;
  - rouge plein : lumière dans un solide (elle n'émet rien).
- Cadre blanc : lumière sous le viseur. Grand cadre et sphère filaire (3 cercles) : lumière sélectionnée.
- Sans `ledit_xray`, les lumières cachées par un mur, et celles à plus de 3000 unités, ne sont pas affichées.
- **En haut à gauche** : outil actif et aide (Tir / Alt / Molette).
- **À droite** : panneau de la sélection : id, source (`added`, `entity N`, `lgt N`), type, état, origine, couleur, intensité, rayon d'émission, nom. Pour une lumière modifiée, la valeur d'origine est affichée en gris.
- **En bas** : map, compteurs (entités, lgt, ajoutées, modifiées, désactivées), slots utilisés / limite, profondeur d'undo et de redo, `*` s'il y a des changements non sauvegardés, avertissements (cluster plein, lumières dans un solide).

Le **rayon d'émission** n'est pas une portée. Une sphère du tracer éclaire en 1/d², sans coupure. Le rayon d'émission règle seulement la douceur des ombres.

## 6. Commandes

Toutes, sauf `lightedit`, demandent le mode actif.

| Commande | Effet |
|---|---|
| `ledit_save` | Écrit `maps/<map>.lgt` (v2) dans le homepath, après une copie de l'ancien fichier en `maps/<map>.lgt.bak`. |
| `ledit_reload` | Abandonne les changements en mémoire et relit le fichier. Vide l'undo. |
| `ledit_undo` / `ledit_redo` | Annule / rétablit la dernière action. |
| `ledit_history` | Affiche les piles d'undo et de redo. |
| `ledit_set origin x y z` | Position de la sélection. |
| `ledit_set color r g b` | Couleur (0 à 1). |
| `ledit_set intensity v` | Intensité (voir § 8). |
| `ledit_set radius v` | Rayon d'émission. |
| `ledit_set name texte` | Nom. |
| `ledit_get` | Affiche les valeurs de la sélection (et les valeurs d'origine si elle est modifiée). |
| `ledit_delete` | Supprime ou désactive la sélection. |
| `ledit_deselect` | Vide la sélection. |
| `ledit_revert` | Remet toutes les valeurs d'origine de la sélection. |

Commandes de débogage du renderer (utilisables même hors du mode) : `pt_ledit_list`, `pt_ledit_add x y z [intensité] [r g b]`, `pt_ledit_set <id> <origin|color|intensity|radius> <valeurs>`, `pt_ledit_del <id>`, `pt_ledit_restore <id>`, `pt_ledit_stats`.

## 7. Cvars

| Cvar | Défaut | Rôle |
|---|---|---|
| `ledit_xray` | 0 | 1 : icônes visibles à travers les murs, et sélection possible à travers les murs. |
| `ledit_show` | 1 | 0 : aucune icône ; 1 : lumières éditables. |
| `ledit_preset_intensity` | 2000 | Intensité d'une lumière créée. |
| `ledit_preset_radius` | 8 | Rayon d'émission d'une lumière créée. |
| `ledit_preset_color` | `1 0.9 0.8` | Couleur d'une lumière créée. |
| `pt_light_scale_edit` | 0.1 | Échelle unique des lumières **ajoutées**. |
| `ledit_active` | — | Lecture seule, 1 pendant le mode. |

## 8. Intensités et échelles

- Une lumière **ajoutée** a une intensité en unités de la clé `light` de q3map2, multipliée par `pt_light_scale_edit`. Cette échelle est la même partout : déplacer la lumière ne change pas sa luminosité.
- Une **entité** du BSP garde l'unité `light` de q3map2 et la classe d'échelle calculée au chargement à sa position d'origine (`pt_light_scale_ent_spot`, `_sky` ou `_ambient`). La déplacer ou la modifier ne change pas sa classe.
- Une lumière **reconstruite** (`lgt`) garde son unité (unités de lightmap × `pt_lightgen_scale`).

## 9. Binds conseillés

Les touches des armes, le tir et la molette suffisent pour les outils. Exemple de binds pour les commandes (à adapter ; rien n'est exécuté automatiquement) :

```
bind F6 "lightedit"
bind F7 "ledit_save"
bind F8 "ledit_undo"
bind F9 "ledit_redo"
bind DEL "ledit_delete"
bind F10 "ledit_xray"   // puis: set ledit_xray 1 / 0
```

Vérifiez que la molette est liée à `weapnext` / `weapprev` (`bind MWHEELUP weapnext`, `bind MWHEELDOWN weapprev`).

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
```

Règles :
- `lights N` donne le nombre de blocs lgt (information seulement).
- Les blocs lgt sont écrits en premier, dans leur ordre : leur rang (`lgt N`) reste stable d'une sauvegarde à l'autre.
- Chaque clé a 1 ou 3 valeurs. Un ancien binaire (parseur v1) lit le fichier sans se décaler : il ignore les blocs d'intensité négative (surcharges, désactivations) et lit les lumières ajoutées comme des lumières reconstruites.
- Si `ent` et `check` ne correspondent plus au BSP (map recompilée), la surcharge est ignorée avec un avertissement, mais elle est conservée dans le fichier.
- Les blocs de `source` inconnue sont conservés tels quels.
- `pt_lightgen <map>` refuse d'écraser un fichier qui contient des éditions. `pt_lightgen <map> force` l'écrase après une copie en `.lgt.bak`.

## 11. Limitations connues du MVP

- **RTX seulement.** Le raster utilise les lightmaps et la lightgrid, qui ne changent pas.
- **Sphères seulement** à la création. Les spots d'origine (entités avec `target`) se déplacent, se règlent (couleur, intensité) et se désactivent, mais leur direction et leur cône ne sont pas éditables.
- Outils 4 (orientation), 5 (propriétés à la molette), 6 (pipette), 7 (clonage) et 9 (solo) absents. Pas de sélection multiple, de grille, de snap ni de contrainte d'axe.
- **Pas de saisie clavier directe** (Ctrl+Z, Suppr) : utiliser des binds sur les commandes `ledit_*`.
- **Limite de lumières** : 4096 lumières en tout dans le tracer (`MAX_LIGHT_POLYS`, constante des shaders), dont 128 gardées pour les lumières dynamiques (sabres, dlights). Les surfaces émissives comptent aussi : sur kejim_post, 3574 émissives + 301 entités laissent environ 90 places. Au-delà, l'ajout est refusé (`no free light slot`).
- **1024 lumières par cluster** : un ajout est refusé si un cluster visible est plein.
- **Surfaces émissives** (néons, polygones) : non éditables et non affichées.
- **Pas d'icônes au-delà de 3000 unités** sans `ledit_xray`. Les modèles (caisses, portes) ne cachent pas les icônes : seule la géométrie du monde est testée.
- La sélection et la création suivent la trace du monde ; les entités (caisses) ne sont pas touchées par le fantôme.
- Un **vid_restart** recharge la map côté renderer : les changements non sauvegardés sont perdus.
- Chaque ajout ou rechargement consomme un emplacement ; les emplacements des lumières supprimées ne sont réutilisés qu'au prochain chargement de la map. L'agrandissement de la réserve (par blocs de 64) provoque un court arrêt du GPU.
- Les débruiteurs (A-SVGF, etc.) gardent une traînée de quelques images après un changement.
- La sauvegarde de la partie est refusée en silence pendant le mode (comme pour les autres refus).

## 12. Procédure de test pas à pas

1. Lancer le jeu avec `cl_renderer rdsp-vulkan` et `r_rtx 1`, puis `devmap kejim_base`.
2. `lightedit` : le HUD normal disparaît ; l'overlay affiche « LIGHT EDIT tool 1 Select », les icônes bleues des entités et la ligne de compteurs en bas.
3. Touche **2**, viser un mur, **molette** pour régler le décalage, **tir** : une icône verte apparaît, la lumière éclaire la scène à l'image suivante. Le message « added light N » s'affiche.
4. `ledit_set color 0.2 1 0.2` puis `ledit_set intensity 6000` : la couleur et la puissance changent ; `ledit_get` affiche les valeurs.
5. Touche **3**, viser la lumière, **tir maintenu**, tourner la caméra, **molette** pour la distance, relâcher : la lumière est déplacée.
6. `ledit_undo` : la lumière revient ; `ledit_redo` : elle repart. `ledit_history` montre les piles.
7. Touche **8**, viser une icône bleue, **tir** : elle devient rouge barrée et sa lumière s'éteint. **Alt** dessus : elle revient.
8. Touche **1**, viser une icône bleue, **tir** : le panneau de droite affiche « entity N » ; `ledit_set intensity 10` : l'icône devient orange, le panneau montre l'ancienne valeur en gris.
9. `ledit_save` : le message « wrote maps/kejim_base.lgt (…) » s'affiche ; le fichier et le `.bak` sont dans le homepath.
10. `lightedit 0`, puis `devmap kejim_base` : la console affiche « light edit: maps/kejim_base.lgt: … overrides, … added » et les changements sont visibles hors du mode.
11. `pt_lightgen kejim_base` : refus (« holds light edits »).
12. `lightedit`, faire un changement, puis `lightedit 0 discard` : le changement disparaît.
13. Avec `cl_renderer rdsp_swgl` : `lightedit` répond « needs the RTX renderer » et le jeu continue normalement.
