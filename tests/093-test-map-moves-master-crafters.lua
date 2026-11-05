-- 093-test-map-moves-master-crafters.lua (issue 1109, step 1o) - a headless Test Map route on Master Crafters
--
-- For a general audience: a route for the client's run with no window
-- (src/089-headless-run.h has the format) on one converted map,
-- world-edit-to-execute's build of the WC3 map Master Crafters, from where
-- its map.lua starts her: to its water, a swim and a dive, then off the edge
-- of its ground, where she falls with nothing under her. The test
-- program (tests/044-test-client.c) runs it twice on that build and
-- checks that she swam, that she left the ground's edge, and that the
-- two receipts match. The route was found in the mover's own steps (a
-- search over turns of a twentieth of a second and runs of 3 seconds,
-- 2026-11-05), so it holds only while the map's ground, its water and
-- the mover stay as they were: a change to any of them may need it found
-- again. It names its map, so a run on another map refuses it.
return {
    format = 1,
    name   = "to the water and off the edge",
    map    = "WC3_Master_Crafters_Final_Conquest",
    step   = 0.05,
    moves  = {
        { hold = "", seconds = 1 },                         -- standing: she settles on the ground
        -- to the water
        { hold = "left", seconds = 0.95 },
        { hold = "forward", seconds = 3 },
        { hold = "left", seconds = 0.4 },
        { hold = "forward", seconds = 3 },
        { hold = "left", seconds = 1.7 },
        { hold = "forward", seconds = 6 },
        { hold = "left", seconds = 0.1 },
        { hold = "forward", seconds = 6 },
        { hold = "left", seconds = 1.9 },
        { hold = "forward", seconds = 9 },
        { hold = "left", seconds = 1.95 },
        { hold = "forward", seconds = 3 },
        { hold = "left", seconds = 0.1 },
        { hold = "forward", seconds = 12 },
        { hold = "left", seconds = 1.95 },
        { hold = "forward", seconds = 3 },
        { hold = "left", seconds = 0.05 },
        { hold = "forward", seconds = 5 },
        { hold = "forward swim_down", seconds = 2 },        -- a dive
        -- to the edge of the ground
        { hold = "left", seconds = 0.35 },
        { hold = "forward", seconds = 3 },
        { hold = "left", seconds = 0.4 },
        { hold = "forward", seconds = 27 },
        { hold = "left", seconds = 0.15 },
        { hold = "forward", seconds = 3 },
        { hold = "left", seconds = 1.85 },
        { hold = "forward", seconds = 3 },
        { hold = "left", seconds = 1.85 },
        { hold = "forward", seconds = 3 },
        { hold = "left", seconds = 0.1 },
        { hold = "forward", seconds = 5.95 },
        { hold = "forward", seconds = 1 },                  -- over the edge
        { hold = "", seconds = 2 },                         -- falling, nothing under her
    },
}
