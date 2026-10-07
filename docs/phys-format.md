# Cloth and hair physics (.phys)

A `.phys` file sits next to a `.glm`: `models/players/<name>/model.glm` reads
`models/players/<name>/model.phys`. The model files stay unchanged.

Each group names surfaces of the model. The renderer simulates all surfaces of a
group as one piece of cloth. Vertices at the same position share one particle, also
across surfaces, so a robe in several surfaces stays closed.

Supported by `rdsp-vulkan` (raster path) and `rdsp_swgl`. The path tracer (RTX) draws the
surfaces without the simulation.

## Example

```
cloth cape
{
	surfaces	torso_cape
	pin			gradient thoracic -z 2 22
	proxy		4
	maxdist		26 38
	collide		humanoid
}

hair hair
{
	surfaces	head_hair
	pin			gradient cranium -z -6 0
	mode		lag
	maxdist		4
}
```

More files are in `docs/phys/`.

## Syntax

A group starts with `cloth <name>` or `hair <name>`, followed by `{ ... }`.
`//` starts a comment. Names are not case sensitive.

### surfaces

`surfaces <pattern> [<pattern> ...]`. A pattern is a surface name. `*` and `?` match
characters. A surface that is off (skin `*off`, `_off`, `G2_SetSurfaceOnOff`) leaves the
group until it is on again.

### pin

A pin rule sets the free factor of each particle: 0 is fixed to the animation, 1 is free.
With more than one rule, the smallest factor wins. With no rule, every particle is free.

| Rule | Meaning |
| --- | --- |
| `pin gradient <bone> <axis> <pinned> <free>` | Distance from the bone origin along `x`, `y`, `z`, `-x`, `-y` or `-z` in the bind pose of the model (Z is up). The particle is fixed at `<pinned>` and free at `<free>`. |
| `pin radius <bone> <fixed> <free> [<x> <y> <z>]` | Distance from the bone origin, in the bind pose of the model. The particle is fixed at `<fixed>` and free at `<free>`. The optional offset moves the center, for example to the middle of the head. |
| `pin bones <bone> [<bone> ...]` | Fixed where the vertex weight is on the listed bones. |
| `pin uv <v0> <v1>` | Same as `gradient`, with the texture coordinate v. |

### Solver values

| Keyword | Default cloth / hair | Meaning |
| --- | --- | --- |
| `mode cloth` or `mode lag` | cloth | `lag`: the free particles move as one mass on a spring and turn around the fixed particles. No per vertex solver. A contact with a collider moves the whole mass and removes its speed into the collider, so the hair slides on the body as one piece. Use it for hair in the head surface. |
| `maxdist <r> [<rn>]` | 24 / 8 | Largest distance of a free particle from its animated position, in the surface plane. `rn` is the limit along the surface normal (default: same as `r`). In `lag` mode: largest move of any vertex (the turn around the pivot is limited so that the farthest vertex stays within this distance). |
| `maxspeed` | 700 / 400 | Largest speed relative to the animation, units per second. |
| `proxy <size>` | 4 | Cell size, in units, of the simulation mesh: one particle per cell. The solver runs on this coarse mesh and the full mesh follows it. A smaller value gives more folds and costs more. |
| `follow` | 6 / 14 | Natural frequency of the soft link to the animated pose, radians per second. A low value gives a free cloth that hangs and swings. 0 removes the link. |
| `inertia` | 0.7 / 0.5 | 0 to 1. Part of the motion of the pinned particles that the free particles do not follow. 1 gives the full inertia of the character (the cloth trails far behind), 0 moves the cloth with the character. |
| `damping` | 2 / 3 | Loss of the speed relative to the animation, per second. |
| `aero` | 1 / 0 | Push of the relative air flow on each triangle, per second. The cloth fills with air when it moves. |
| `gravity` | 0.5 / 0.3 | Multiplier of 800 units per second squared. |
| `drag` | 0.25 / 0.5 | Pull of the speed toward the wind, per second. |
| `wind` | 0.5 / 0.3 | Multiplier of the map wind. |
| `stretch` | 0.9 / 1 | Edge stiffness, 0 to 1. The result does not depend on `iterations` or on the frame rate. |
| `bend` | 0.15 / 0.3 | Bend stiffness, 0 to 1. |
| `lagfreq` | 10 | Mode `lag`: natural frequency, radians per second. |
| `lagdamp` | 0.5 | Mode `lag`: damping ratio. 1 is critical, less than 1 swings. |
| `margin` | 0.5 | Distance kept from the colliders. |
| `range` | 0 | Largest distance from the view where the group is simulated. 0 means no limit. |
| `iterations` | 4 | Solver iterations per sub-step (1/120 s at most), 1 to 16. |
| `enabled` | 1 | `enabled 0` switches the group off. |

### collide

| Line | Meaning |
| --- | --- |
| `collide model [<scale>]` | Capsules fitted to the model: for each bone that carries body vertices, a capsule toward its child bone (or a sphere at the end of a chain), with the radius measured on those vertices. The surfaces of the groups of the file and the tags and caps are left out. `scale` multiplies the radius. At most 32 colliders. |
| `collide humanoid [<scale>]` | Capsules for the stock `_humanoid` skeleton: hips, torso, head, arms, legs. `scale` multiplies their radius. |
| `collide radius <bone> <value>` | Sets the radius of the collider that starts on the bone, in the humanoid template or in the explicit ones. |
| `collide remove <bone>` | Removes the collider that starts on the bone. |
| `collide capsule <boneA> <boneB> <radius>` | Capsule between the origins of two bones. The short form `collide <boneA> <boneB> <radius>` does the same. Write one line for each capsule. |
| `collide sphere <bone> <radius> [<x> <y> <z>]` | Sphere on a bone, with an offset in the bind pose of the model. |

A particle never gets closer to a collider than its animated position is, so a collider that already
contains part of the cloth in the animation (a collar, hair on the head) does not push it out. The middle
of each edge of the simulation mesh is tested too, so a triangle does not cut through a collider between
two particles.

A collider with an unknown bone is skipped. The floor is the shadow plane of the entity
(`refEntity_t::shadowPlane`), when the entity has `RF_SHADOW_PLANE`.

## Editor in game

The console command `ph_menu` opens the phys editor on the model of the player.

- **Unused**: the surfaces of the model that are in no group. The tags and the helper surfaces of the
  model are not listed: a surface with less than 20 vertices is a tag, a cap or a helper surface. Select a surface to make it blink on the model, then press **Add** to make a group.
- **Used**: the groups of the file. Select one to edit its text and to make its surfaces blink.
  **Remove** deletes the group.
- **Refresh** applies the text of the group to the model. The preview shows the physics.
- **Apply** writes `models/players/<name>/model.phys` in the game folder of the player and closes.
  **Back** closes and drops the changes.

## Console

| Name | Meaning |
| --- | --- |
| `ph_enable` | 0 switches all physics off. |
| `ph_range` | Multiplier of the `range` of each group. |
| `ph_debug` | 1 prints the loaded files, the groups and the resets. 2 also shows the colliders as orange dots over the models. |
| `ph_reload` | Clears the loaded `.phys` data. The files load again when a model is drawn. |

## Limits

- The solver and the draw use LOD 0 for the surfaces of a group.
- The solver is XPBD on the simulation mesh: fixed sub-steps of 1/120 s at most (4 at most per frame), compliance per constraint, no allocation in a step.
- Shadow volumes and gore use the animated (rigid) surface.
- Hair that is part of the head surface is one soft piece. Hair cards or strips in their own
  surface give the best result.
- The first person model of the player is not simulated.
