-- 092-test-map-moves-plaguelands.lua (issue 1109, steps 1o and 1p) - a headless Test Map route on War in the Plaguelands
--
-- For a general audience: a route for the client's run with no window
-- (src/089-headless-run.h has the format) on one converted map,
-- world-edit-to-execute's build of the WC3 map War in the Plaguelands,
-- from where its map.lua starts her: to its water, a swim and a dive,
-- then off the north edge of its ground, where she falls with nothing
-- under her (its pathing leaves its last rows there open). The test
-- program (tests/044-test-client.c) runs it twice on that build and
-- checks that she swam and that she left the ground's edge, and that the
-- two receipts match. The route was found in the mover's own steps, over
-- the map's ground, its models, its water and its WC3 pathing's walls (a
-- search over facings 5 degrees apart, each a turn_to, and runs of 2
-- seconds, 2026-11-05), so it holds only while they and the mover stay as
-- they were: a change to any of them may need it found again. It names
-- its map, so a run on another map refuses it.
return {
    format = 1,
    name   = "to the water and off the edge",
    map    = "WC3_War_in_the_Plaguelands_24_B6a",
    step   = 0.05,
    moves  = {
        { hold = "", seconds = 1 },                          -- standing: she settles on the ground
        -- to the water
        { turn_to = 2.5307, hold = "forward", seconds = 2 },
        { turn_to = 3.8397, hold = "forward", seconds = 2 },
        { turn_to = 4.3633, hold = "forward", seconds = 2 },
        { turn_to = 4.7124, hold = "forward", seconds = 2 },
        { turn_to = 5.4978, hold = "forward", seconds = 2 },
        { turn_to = 4.2761, hold = "forward", seconds = 2 },
        { turn_to = 4.6251, hold = "forward", seconds = 2 },
        { turn_to = 4.2761, hold = "forward", seconds = 2 },
        { turn_to = 4.7124, hold = "forward", seconds = 2 },
        { turn_to = 4.2761, hold = "forward", seconds = 2 },
        { turn_to = 3.8397, hold = "forward", seconds = 2 },
        { turn_to = 4.0143, hold = "forward", seconds = 2 },
        { turn_to = 3.6652, hold = "forward", seconds = 2 },
        { turn_to = 2.7925, hold = "forward", seconds = 2 },
        { turn_to = 3.2289, hold = "forward", seconds = 0.90 },
        { hold = "forward", seconds = 2 },
        { hold = "forward swim_down", seconds = 2 },        -- a dive
        -- to the edge of the ground
        { turn_to = 0.0000, hold = "forward", seconds = 2 },
        { turn_to = 0.0000, hold = "forward", seconds = 2 },
        { turn_to = 5.4978, hold = "forward", seconds = 2 },
        { turn_to = 0.0000, hold = "forward", seconds = 2 },
        { turn_to = 0.0000, hold = "forward", seconds = 2 },
        { turn_to = 5.5851, hold = "forward", seconds = 2 },
        { turn_to = 5.9341, hold = "forward", seconds = 2 },
        { turn_to = 0.0000, hold = "forward", seconds = 2 },
        { turn_to = 0.0000, hold = "forward", seconds = 2 },
        { turn_to = 0.0000, hold = "forward", seconds = 2 },
        { turn_to = 0.0000, hold = "forward", seconds = 1.75 },
        { hold = "forward", seconds = 1 },                  -- over the edge
        { hold = "", seconds = 2 },                         -- falling, nothing under her
    },
}
