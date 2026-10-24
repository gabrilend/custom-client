# 043-main

*Written by `scripts/generate-info` from `043-main.c`'s opening comment (a program: it offers no functions, only how it is run); don't edit, re-run it.*

043-main.c (issue 173k; the C twin of src/custom-client/main.lua) - the window

For a general audience: opens a window on the ground of Ashenvale near
Astranaar, read from the server's own map files, with the night elf of
the original game's archives standing on it; moves her with the game's
own controls at 120% speed (W/S forward and back, A/D turn or, with the
right mouse button held, strafe, Q and E strafe, space jump, the
keypad's / toggles walking and running (173i), left drag turns the
camera, right drag turns her with it, the wheel zooms; Escape quits
until the game menu exists). Written in C on Raylib (owner,
2026-10-05: "let's port now. We should build something like an engine
in C anyway, for performance reasons").

Usage: custom-client --maps DIR --client DIR [--settings FILE] [--world DIR] [--shot FILE] [--zoom N] [--front] [--turn DEG] [--pitch DEG]
  --settings FILE   the player's settings (SET name "value" lines; 173b1);
                    without it, every setting is at its default
  --shot FILE       draw a few frames, save a picture there, quit
  --world DIR       where the generated creature lists are (creatures-<map>.tsv, 173t)
  --no-read-ahead   no second thread reading tiles ahead (to measure what it saves; 173u)
  --zoom N          (pictures) the camera N yards back; 0 is first person
  --turn DEG        (pictures) the camera turned DEG degrees round her, held there
  --pitch DEG       (pictures) the camera raised DEG degrees (up to 83)
  --clock MS        (pictures) start the world's animations MS milliseconds in
  --hour H          the time of day, 0 to 24 (default: this machine's clock)
  --face DEG        her start facing turned DEG degrees (left)
  --walk S          (pictures) hold forward for S world-seconds
  --walking         walk toggled on from the start (173i), as the keypad's / does
  --dive S          (pictures) then hold X (swim down) for S world-seconds
  --wait S          (pictures) then wait S world-seconds more
  --wear SLOT:LOOK,...  dress her, over her starting outfit: each slot's item look (ItemDisplayInfo id),
                    e.g. chest:41143,hands:41138,legs:41140,waist:41476,feet:41480
  --goto X Y        (pictures) one world-second in, move her to world (X, Y)
  --map ID X Y      start on map ID (Map.dbc: 0 Eastern Kingdoms, 1 Kalimdor,
                    530 Outland, 571 Northrend) at world (X, Y)
  --pose ID MS      (pictures) hold her in animation ID (AnimationData.dbc:
                    0 stand, 5 run, 13 walk backwards, 38 jump ...) at MS
