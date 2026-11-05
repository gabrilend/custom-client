-- 092-test-map-moves-plaguelands.lua (issue 1109, step 1o) - a headless Test Map route on War in the Plaguelands
--
-- For a general audience: a route for the client's run with no window
-- (src/089-headless-run.h has the format) on one converted map,
-- world-edit-to-execute's build of the WC3 map War in the Plaguelands, from where
-- its map.lua starts her: to its water, a swim and a dive, then off the edge
-- of its ground, where she falls with nothing under her. The test
-- program (tests/044-test-client.c) runs it twice on that build and
-- checks that she swam, that she left the ground's edge, and that the
-- two receipts match. The route was found in the mover's own steps (a
-- search over turns of a twentieth of a second and runs of 2 seconds,
-- 2026-11-05), so it holds only while the map's ground, its water and
-- the mover stay as they were: a change to any of them may need it found
-- again. It names its map, so a run on another map refuses it.
return {
    format = 1,
    name   = "to the water and off the edge",
    map    = "WC3_War_in_the_Plaguelands_24_B6a",
    step   = 0.05,
    moves  = {
        { hold = "", seconds = 1 },                         -- standing: she settles on the ground
        -- to the water
        { hold = "forward", seconds = 2 },
        { hold = "left", seconds = 1.65 },
        { hold = "forward", seconds = 2 },
        { hold = "left", seconds = 0.45 },
        { hold = "forward", seconds = 2 },
        { hold = "left", seconds = 1.95 },
        { hold = "forward", seconds = 6 },
        { hold = "left", seconds = 0.05 },
        { hold = "forward", seconds = 6 },
        { hold = "left", seconds = 1.9 },
        { hold = "forward", seconds = 3.9 },
        { hold = "forward swim_down", seconds = 2 },        -- a dive
        -- to the edge of the ground
        { hold = "left", seconds = 0.6 },
        { hold = "forward", seconds = 8 },
        { hold = "left", seconds = 0.2 },
        { hold = "forward", seconds = 2 },
        { hold = "left", seconds = 0.05 },
        { hold = "forward", seconds = 16 },
        { hold = "forward", seconds = 1 },                  -- over the edge
        { hold = "", seconds = 2 },                         -- falling, nothing under her
    },
}
