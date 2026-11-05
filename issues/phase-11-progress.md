# Phase 11 — WC3 Map Mode — Progress

Everything that makes a converted Warcraft III map playable: a top-down
camera with edge pan and zoom, box select and control groups, right-click
orders, the command card and resource bar, the minimap (docs/009). The
owner's reason for building this client: "the purpose of creating a custom
client is so that we can heavily modify it." Converted maps can already be
viewed (Phases 3 and 4) but not played; the map editor is to become a mode
of this client.

## Issues

| ID | Title | Status | Blocked by |
|----|-------|--------|------------|
| 1108 | [Phase 11 demo](./1108-phase-demo.md) | pending | Phase 4, Phase 7 |
| 1109 | [Test Map: the editor opens its map in the W client](./1109-test-map-from-the-editor.md) | in progress: step 1 built 2026-11-04 (the launch option and the goodbye receipt; on 105a's resolver, not on main yet); step 1b (the converted map opened) 2026-11-04; step 1c (its doodads and buildings, with their own models and pictures, through the resolver) 2026-11-05; step 1d (the player's Warcraft III install as a resolver place, and the converted ground painted with WC3's own tilesets from `war3map.w3e`) 2026-11-05; step 1e (its cliffs as WC3's own cliff models, on their cliff's ground) 2026-11-05; step 1f (its ramps as WC3's ramp models, and its blight) 2026-11-05; step 1g (its ground laid cell by cell by the ground's shader: every tileset in a chunk, each cell's variation) 2026-11-05; step 1h (its cliffs and ramps bent to the ground, and WC3's water read: the first of four water steps) 2026-11-05; step 1i (its water drawn as WC3's, tinted by its `war3map.w3i`, each point at WC3's own surface) 2026-11-05; step 1j (its own fog from `war3map.w3i`) 2026-11-05; step 1k (the baseline set as the resolver's place after the Blizzard installs, and its smallest form made from the converter's tables) 2026-11-05. Decided 2026-10-07, reshaped 2026-11-04: the editor keeps its own renderer | 401, 302, 105a, world-edit-to-execute's W02 |
| 1110 | [A hosted map's models come with it](./1110-a-hosted-maps-models-come-with-it.md) | pending (decided 2026-11-04; from world-edit-to-execute's pull request #227) | 105a, Phase 7, 707, 1101-1107 |

The capstone names the issues to be written when the phase begins
(1101 the RTS camera, 1102 picking, 1103 selection, 1104 orders, 1105 the
command card, 1106 the minimap, 1107 loading a converted map); 1109 shares
1101 to 1103.

2026-11-04: 1109's step 1. `--test-map FOLDER --receipt FILE` (with
`--camera`, `--realm`, `--account`, the password from
`W_CLIENT_PASSWORD`) opens the client with the map's folder first of the
resolver's overlays and writes a goodbye receipt when it ends, which the
editor reads back; the contract is in 1109. 52 checks in the test
program; the main program's part is compile-checked only (no raylib
where it was written).

2026-11-05: 1109's step 1c. A test-map run draws the map's placed
doodads and buildings through the resolver: its own WC3 models
(`.mdx`, from its build folder, sized by `map.lua`'s rule, standing
still), the game's `.m2` from the first place that has them, the
magenta wire-frame box for a model no place has, and the ground's and
models' pictures (the checker for a missing one). Checked on two real
conversions by W02c (War in the Plaguelands: 11034 doodads, 118 of its
own models read; Master Crafters: 2940 doodads); drawing compile-checked
only. Decisions 20 to 31 in 1109.

2026-11-05: 1109's step 1d. The player's own Warcraft III install
(setting `assetWc3`: a folder under `assets` with war3.mpq, War3x.mpq,
War3Patch.mpq, or a link to the install) is a resolver place, read
after the game's archives, so stock WC3 pictures and models a converted
map names come from there before the checker or box. PR #3's `.w3e`
reader merged in, a SYLK reader added (`077-slk.c`, for
`TerrainArt\Terrain.slk`), and a converted map whose build carries
`war3map.w3e` has its ground painted with WC3's own tilesets
(`078-wc3-ground.c`, through 033's ground pictures and blend: no new
shader). On War in the Plaguelands and Master Crafters (their `.w3e`
copied in by hand: the converter doesn't copy it yet) every WC3 cell
lands on one chunk cell, and chunks of four tilesets or fewer match
WC3's own laying pixel for pixel (90.4% and 99.8% of all blend points).
No real WC3 install checked; drawing compile-checked only. Decisions 32
to 41 in 1109.

2026-11-05: 1109's step 1e. With the player's WC3 install, a converted
map's cliffs stand as WC3's own cliff models (`079-wc3-cliffs.c`: each
cell on different cliff levels and off a ramp gets the model its
corners' levels name, from `TerrainArt\CliffTypes.slk`'s folder, read
through the resolver), added to each tile's doodads so 042 draws them
with their cliff tileset's picture, and their cells laid with that
tileset's ground. Checked on War in the Plaguelands (498 cliffs) and
Master Crafters (2002 placed; 980 ramp cells left for the next step)
with made sheets and models: each placed once, named and placed by the
rule, its foot on its own level. Real cliff models unchecked (no
install); drawing compile-checked only. Decisions 42 to 50 in 1109.

2026-11-05: 1109's step 1f. Ramp cells that change level stand as WC3's
own ramp models (from each cliff tileset's `rampModelDir`, ramp corners
lettered L and H), on their own ground; and blighted ground wears the
main tileset's blight picture, named by `UI\WorldEditData.txt`,
blended on its blighted corners as a tileset of its own. Checked on
Master Crafters (905 ramps placed, 184 blighted points) and War in the
Plaguelands (1246 blighted points) with made sheets and models. Real
ramp models and the editor's data unchecked (no install). Decisions 51
to 56 in 1109.

2026-11-05: 1109's step 1g. A converted map's WC3 ground is laid cell
by cell: each cell's pieces (a tileset picture, or half of an extended
one, and the subtile in it) go to the card in a small picture a tile,
and the ground's shader (now `080-ground-shader.c`, read by 038) reads
them per point, so a chunk shows every tileset its cells lay instead
of its four most used, and each cell its own variation. Checked on War
in the Plaguelands (3145728 of 3145728 blend points as WC3 lays them,
90.4% before; 531 chunks lay more than four tilesets) and Master
Crafters (all 4194304, 99.8% before) with made pictures colouring each
subtile its own; the shader itself compiled and drawn on Mesa's
software renderer under a virtual display (outside the tests). Real
WC3 art and a window unchecked. Decisions 57 to 63 in 1109.

2026-11-05: 1109's step 1h. A converted map's cliffs and ramps are
bent to the ground under each of their points, as WC3 bends them (the
ground's heights round the cell carried in the model's name, applied
by 076 when the model is read). Their feet now sit on the converted
ground: at most 0.0000 yards off at their lowest corners on War in the
Plaguelands (206 bent; up to 4.18 unbent) and Master Crafters (1011
bent; up to 23.33 unbent). WC3's water is read without a window
(`081-wc3-water.c`: the water sheet's row, its frames, each point's
colour by depth), checked on both maps' wet points with a made sheet;
drawing it is the plan's next three steps. Real WC3 models, water and
a window unchecked. Decisions 64 to 69 in 1109.

2026-11-05: 1109's step 1i. A converted map's water is drawn as WC3
draws it: WC3's frames and each point's colour by depth (082's points
and shader, 040), tinted by the map's own `war3map.w3i` when it says
(083, a new reader; the converter copies the file since
world-edit-to-execute's draft PR #247), each point at WC3's own
surface. The converter's wet cells match WC3's exactly on War in the
Plaguelands (5463) and Master Crafters (15648); 31 dry-corner points
on Master Crafters sink to WC3's surface. War in the Plaguelands' w3i
tints its water; Master Crafters' doesn't. The shader drawn on Mesa's
software renderer under a virtual display matched on every checked
pixel. Real WC3 water art, the tint's byte order and a window
unchecked. Decisions 70 to 75 in 1109.

2026-11-05: 1109's step 1j. A converted map's own fog, from its
`war3map.w3i`, is drawn in test-map runs in place of the zone's, over
the ground, water, doodads and figures alike (`084-map-fog.c`, a new
file: all of them read the one light each frame). It is on when the
map's "use terrain fog" flag (0x2000) is set, a reading taken from the
18 maps at hand whose settings were read: War in the Plaguelands, the only one with the flag,
is the only one whose fog settings differ from the editor's defaults,
and it now draws its blue fog from 113.9 to 162.8 yards; Master
Crafters and the rest keep the zone's. Every style is drawn linear.
The four shaders drew it on Mesa's software renderer under a virtual
display, matching on every checked pixel. The map's fog in WC3 itself,
the exponential styles' look and a window unchecked. Decisions 76 to
80 in 1109; open question 13 updated.

2026-11-05: 1109's step 1k. The baseline set has its place in the
resolver: after the Blizzard installs (the game's archives, then
Warcraft III's) and before the placeholder, as the owner ordered it. It
is a folder laid out as a pack (setting `assetBaseline`, default
`assets/baseline`), and its answers count as their own place, not as
placeholders. Nothing defined its contents, so the smallest set is
made by a new program, `make-baseline` (085, 086), from the converter's
own tables alone: 179 ground pictures in each row's own colour, one
grey post standing in for 47 doodad models, and the water's 30 frames
(212 files). With it and no install, every ground picture and every
game doodad on War in the Plaguelands and Master Crafters comes from
the set rather than the checker or the box, and their water finds its
frames. The water (040) now asks the resolver too. Buildings,
creatures, WC3's stock art and the client's tables are not in it, so
a run still needs the game's archives to start; the water frames'
names are recalled, unchecked. Decisions 81 to 88 in 1109.

2026-11-05: 1109's step 1l. A test-map run starts with no WoW install
at all. The start before the window is its own part (087), so the
test program runs it as the client does. The baseline set gains the
tables a start needs (`LiquidType`'s one row of water and the light
tables, empty: a grey day) and a stand-in figure, a sand column a
person's height with a blue face and one animation, Stand, for her
(her character needs the game's tables and models) and for creatures
with no model. 041 reads every file through the resolver, 088 saying
which; 026 and 040 read their tables through it, per row for
`LiquidType`, a set's table of another width left out. With no
install and no server files, War in the Plaguelands and Master
Crafters open from their own rows and ground files, find their water
and light, prepare her ground with no checker, and Master Crafters'
19 creature looks are its own WC3 model and the figure. The window
and drawing are unchecked; buildings, creatures' own looks, her
character, WC3 stock art, sounds, interface and sky still need an
install. Decisions 89 to 97 in 1109.

2026-11-05: 1109's step 1m. Her character (025: its tables, race model
and every picture on her) and the read-ahead (034: each tile's
pictures, doodads and buildings) ask the resolver when one is open, so
nothing in a test-map run reads the game's archives past it; an
ordinary run reads them as before. A character can now come from a
pack or the map's own folder (checked with one made in a folder, no
archives), and the read-ahead on War in the Plaguelands' and Master
Crafters' start tiles reads 224 and 29 files with no install, where it
read one before. A character that fails part way now lets go of what
it made. Decisions 98 to 101 in 1109.

2026-11-05: 1109's step 1n. A test map can run with no window:
`--headless MOVES` opens the world as the client does, finds her figure
and every creature's look, walks her through a fixed list of moves over
the map's ground and water, and writes the goodbye receipt with what it
found and where she ended, then exits (0, 1 on an error, 2 on a bad
option). The project's moves are `tests/090-test-map-moves.lua`. Step
5's check runs on it in the test program: the same build twice gives
the same receipt but for its times, on War in the Plaguelands and
Master Crafters, and other moves give another. The client binary
itself was not run (no raylib here), nor two separate processes;
models' floors and walls are left out of the headless world. Decisions
102 to 110 in 1109.

2026-11-05: 1109's step 1o. The headless world has the placed models'
floors and walls: a new window-free part (091) builds a model's and a
building part's solid shape for the doodad drawer and the headless run
alike, places a tile's doodads, buildings, furnishings and buildings'
pools with no drawing, and holds the window's floor and wall rule, so
both runs ask the mover's questions the same way. The receipt counts
the models. Moves files may name their map, and two routes
(`tests/092`, `tests/093`) take her from War in the Plaguelands' and
Master Crafters' starts to their water (21 and 205 steps swimming) and
off their ground's edge (61 steps with nothing under her), the same
twice. With no install, both maps' models (WC3 models and stand-ins)
have no solid shape, so the floors and walls are checked with a tile
made in the test. Decisions 111 to 117 in 1109.

Created 2026-10-09, when the documentation site's progress chart showed this
phase's issues with no progress table.

2026-11-05: 1109's step 1p. A converted map's WC3 pathing is walls: a
new window-free part (094) reads the map's `war3map.wpm` (which the
converter copies into the build folder since world-edit-to-execute's
draft PR #252) and places its cells on the converted ground by map.lua's
rule; the mover's wall rule (091) asks it after the models', in the
window and the headless run alike, and land closed to WC3's walkers
holds her (deep water is still swum). The receipt counts the steps it
held apart from the models' (593 and 375 on the two new routes into the
walls, `tests/095`, `tests/096`; none ends in a closed cell). Moves can
say `turn_to`, a facing, and the two routes were found again with it:
War in the Plaguelands' still leaves its edge, and Master Crafters',
whose pathing is closed all round, ends held at its south border. No
`war3map.wpm` means no pathing walls. Decisions 118 to 127 in 1109.

2026-11-05: 1109's step 1q. The converter's own `war3map.wpm` (draft PR
#252 in world-edit-to-execute) was checked: both test maps converted
with it give, byte for byte, the files step 1p's tests used. The tables
(074) now give a searching reader one list across every place's copy,
the first place's rows first and the others' new numbers after, and
her character's tables (025) are searched that way, lifting decision
98's first-copy limit. A creature whose display or model row no place
has gets a placeholder row (its number and zeros, made once, warned
and counted by the resolver, refused in strict mode), and the stand-in
figure stands for it, so such a map still shows its creatures; the
receipt counts them. Decisions 128 to 133 in 1109.

2026-11-05: 1109's step 1r, issue 106's layouts. `097-table-layouts.c`
says what each column of twenty tables is: its name, kind, placeholder
value and the readers that read it. The converter's seven tables match
world-edit-to-execute's own layouts column by column; the others' read
columns come from the readers' code, and the rest are marked unchecked,
as is every width (no 3.3.5a file to measure). A copy too narrow for
the columns read is refused (074, and 026's light tables); another
width than the layout's is said. Placeholder rows now hold the layout's
values (a size of 1, an opaque alpha, a visible "unknown Map 900"), a
table no place has can take one, and the window's creatures (041) ask
for them as 088 does. The baseline set's widths come from the layouts.
Decisions 134 to 139 in 1109.

2026-11-05: 1109's step 1s, a map's tile list. `098-wdt.c`, in the
reader library, reads a map's WDT file: its flags, which of its 64 x 64
tiles have ground, and for a one-building map that building and where
it stands. The ground, its reading ahead, the heights and the run with
no window now take the WDT's list as the map's tiles, so a tile it
doesn't list is open sea without asking for a file. World-edit-to-
execute's own WDT writer and five maps it converted agree with the
reader tile for tile. Decisions 140 to 144 in 1109.

2026-11-05: 1109's step 1t, the sky and fog from the light tables.
`026-light.c` now reads Light, its colour and distance bands, its
parameters and skyboxes through the client's tables, so a converted
map's lights and the install's are one list, and a band no place has is
a placeholder row the resolver says. Lights are laid farthest first, so
the nearest shows, and a map with no default light takes Light's row 1,
as the original's notes say (not run against it). With the tables in no
place the fixed grey sky and fog stand, said in one line. The run with
no window takes the light where she ends at noon and counts its fog and
sky in the receipt. Decisions 145 to 150 in 1109.

2026-11-05: 1109's step 1u, the original's light rules from issue 306's
notes. Each colour from the light tables is cut down to a whole 255th
after the blend through the day; the fog never ends nearer than 10
yards and its start's share is held to -1 to 1; from map 530 on (the
second fog mode) a negative start counts as 0, while below it the fog
starts behind the eye. Storm sets can be laid over by a storm's
strength, though nothing reads the weather yet. The second fog mode's
curve, zone and override lights and the indoor fog blend are left,
with reasons. The test program's output is line buffered, so a warning
no longer lands inside a check's line. Decisions 151 to 156 in 1109.

2026-11-05: 1109's step 1v, the weather and a building's fogs.
`099-weather.c` reads the server's weather message (its layout from
memory, refused when the length doesn't fit) and eases a storm's
strength from the old weather to the new over ten seconds, which the
light lays its storm colours and fog over by. Until the client talks to
a server, `--weather` and a moves file give it, and the run with no
window counts it in the receipt. A building's fogs and each part's fog
numbers are read, and the outdoor fog can blend into a part's own over
its first 25 yards; which fog applies and whether the eye is inside are
not in the notes, so nothing uses that blend yet. A map converted with
the editor's own light (its draft PR #260) is lit by that light, from
its own rows, and the checks take it; "no row 1" is now said only for a
map with no default light of its own. Decisions 157 to 165 in 1109.

2026-11-05: 1109's step 1w, the tables' layouts held to the converter's.
`tests/100-layouts-match-converter.lua` has world-edit-to-execute's own
table writer write one row of each of the eleven tables it writes (the
light tables of its draft PR #260 among them), reads the bytes back
with this client's own reader and layouts, and compares every width,
column name, kind and value. All eleven agree once 097 marks Light's
third to fifth parameter sets as read, which 026 has read since step
1u (the converter had it right). 097 moved into the reader library so
the check can ask it from Lua. The nine tables the converter doesn't
write keep widths nothing on its side checks. With no converter tree
given, the check says so and passes. Decisions 166 to 171 in 1109.

2026-11-05: 1109's step 1x, the six failures every run carried here.
None was the client's fault. All six needed files that only the owner's
machine has: the server's extracted maps and navigation tiles, and the
original client's install. A check that can't find such a file now
says NEEDS and what is missing. NEEDS are counted apart from passes and
failures, and the run exits 2, so a run missing data never looks clean.
Two of the six were faults in the test. Two slope checks took a missing
server limit as -1, so they failed while their neighbours passed
without testing anything. And a missing install ended the run early,
uncounted, so 82 later checks that need no data never ran here. Both
are fixed. Those checks now run, and all pass. With the run reaching its end, the
leak the sanitized runs always reported turned out to be the early
return's, and is gone. Decisions 172 to 176 in 1109.

2026-11-05: 1109's step 1y, a map's loading screen picture. The
converter writes each map's loading screen (a row in LoadingScreens.dbc
and a picture), and the client had ignored them. `101-loading-screen.c`
follows the map's row to its loading screen and decodes the picture.
The window shows it as it opens, before the world loads, and the run
with no window reports it in its receipt. A missing picture leaves the
screen dark, never the magenta checker. A check has the converter's own
picture writer make files, and this client reads every copy back exactly
as the converter meant it; on the real builds each smaller copy is the
one before halved. The bar, tips and the rest of issue 606 are not
built. Decisions 177 to 182 in 1109.

2026-11-05: 1109's step 1z, a converted map's far-off ground. The
window's horizon draws each map's low-detail ground file (.wdl), and
for a converted map nothing had checked that file against the map's
real ground. Now the test program places every corner exactly as the
horizon does and finds each within half a yard of the ground files
(the converter rounds to whole yards), on every build. The horizon and
the check share the placing arithmetic, so the check covers what is
drawn. A planted mix-up of the tile numbers was caught. Calling the
indoor fog blend was left: the converter writes no buildings to try it
on. Decisions 183 to 185 in 1109.
