-- 096-test-map-walls-master-crafters.lua (issue 1109, step 1p) - a headless Test Map run into Master Crafters's
-- pathing walls
--
-- For a general audience: from where the map's map.lua starts her, she
-- runs out in each of eight directions and back, facing each by turn_to
-- (radians, the game's: 0 north, rising west), and once strafes along
-- what stopped her. Round both test maps' starts the WC3 map's pathing
-- closes cells to walkers within a few yards (a room's walls at War in
-- the Plaguelands, cliffs and trees at Master Crafters), so she meets
-- them, slides along them where she meets them slantwise, and never
-- stands in a closed cell. The test program (tests/044-test-client.c)
-- runs it twice on that build and checks that the pathing's walls
-- stopped her (steps_walled_pathing above 0), that she never stood in a
-- closed cell (steps_on_closed 0) and that the two receipts match. It
-- names its map, so a run on another map refuses it.
return {
    format = 1,
    name   = "into the pathing's walls",
    map    = "WC3_Master_Crafters_Final_Conquest",
    step   = 0.05,
    moves  = {
        { hold = "", seconds = 1 },                          -- standing: she settles on the ground
        { turn_to = 0.0000, hold = "forward", seconds = 4 },   -- north, until something stops her
        { turn_to = 3.1416, hold = "forward", seconds = 4 },   -- and back
        { turn_to = 0.7854, hold = "forward", seconds = 4 },   -- north-west, until something stops her
        { turn_to = 3.9270, hold = "forward", seconds = 4 },   -- and back
        { turn_to = 1.5708, hold = "forward", seconds = 4 },   -- west, until something stops her
        { turn_to = 4.7124, hold = "forward", seconds = 4 },   -- and back
        { turn_to = 2.3562, hold = "forward", seconds = 4 },   -- south-west, until something stops her
        { turn_to = 5.4978, hold = "forward", seconds = 4 },   -- and back
        { turn_to = 3.1416, hold = "forward", seconds = 4 },   -- south, until something stops her
        { turn_to = 0.0000, hold = "forward", seconds = 4 },   -- and back
        { turn_to = 3.9270, hold = "forward", seconds = 4 },   -- south-east, until something stops her
        { turn_to = 0.7854, hold = "forward", seconds = 4 },   -- and back
        { turn_to = 4.7124, hold = "forward", seconds = 4 },   -- east, until something stops her
        { turn_to = 1.5708, hold = "forward", seconds = 4 },   -- and back
        { turn_to = 5.4978, hold = "forward", seconds = 4 },   -- north-east, until something stops her
        { turn_to = 2.3562, hold = "forward", seconds = 4 },   -- and back
        { turn_to = 0.0000, hold = "forward", seconds = 4 },   -- north again, to the wall
        { hold = "strafe_left", seconds = 3 },                  -- along it
        { hold = "", seconds = 1 },
    },
}
