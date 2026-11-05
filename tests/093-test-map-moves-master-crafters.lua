-- 093-test-map-moves-master-crafters.lua (issue 1109, steps 1o and 1p) - a headless Test Map route on Master Crafters
--
-- For a general audience: a route for the client's run with no window
-- (src/089-headless-run.h has the format) on one converted map,
-- world-edit-to-execute's build of the WC3 map Master Crafters, from
-- where its map.lua starts her: to its water, a swim and a dive, then to
-- its south border, where its WC3 pathing's closed cells hold her (its
-- border is closed all round, so the edge of its ground can't be reached
-- on foot). The test program (tests/044-test-client.c) runs it twice on
-- that build and checks that she swam and that the pathing's walls held
-- her at the border, and that the two receipts match. The route was found
-- in the mover's own steps, over the map's ground, its models, its water
-- and its WC3 pathing's walls (a search over facings 5 degrees apart,
-- each a turn_to, and runs of 3 seconds, 2026-11-05), so it holds only
-- while they and the mover stay as they were: a change to any of them may
-- need it found again. It names its map, so a run on another map refuses
-- it.
return {
    format = 1,
    name   = "to the water and the border",
    map    = "WC3_Master_Crafters_Final_Conquest",
    step   = 0.05,
    moves  = {
        { hold = "", seconds = 1 },                          -- standing: she settles on the ground
        -- to the water
        { turn_to = 0.6981, hold = "forward", seconds = 3 },
        { turn_to = 1.5708, hold = "forward", seconds = 3 },
        { turn_to = 1.5708, hold = "forward", seconds = 3 },
        { turn_to = 0.0000, hold = "forward", seconds = 3 },
        { turn_to = 0.0000, hold = "forward", seconds = 3 },
        { turn_to = 0.0000, hold = "forward", seconds = 3 },
        { turn_to = 0.0000, hold = "forward", seconds = 3 },
        { turn_to = 5.4978, hold = "forward", seconds = 3 },
        { turn_to = 5.2360, hold = "forward", seconds = 3 },
        { turn_to = 5.6723, hold = "forward", seconds = 3 },
        { turn_to = 0.0873, hold = "forward", seconds = 3 },
        { turn_to = 0.6981, hold = "forward", seconds = 3 },
        { turn_to = 0.9599, hold = "forward", seconds = 3 },
        { turn_to = 0.8727, hold = "forward", seconds = 3 },
        { turn_to = 0.9599, hold = "forward", seconds = 3 },
        { turn_to = 1.2217, hold = "forward", seconds = 3 },
        { turn_to = 1.2217, hold = "forward", seconds = 3 },
        { turn_to = 1.2217, hold = "forward", seconds = 3 },
        { turn_to = 0.9599, hold = "forward", seconds = 3 },
        { turn_to = 1.2217, hold = "forward", seconds = 3 },
        { turn_to = 1.2217, hold = "forward", seconds = 3 },
        { turn_to = 0.9599, hold = "forward", seconds = 2.90 },
        { hold = "forward", seconds = 2 },
        { hold = "forward swim_down", seconds = 2 },        -- a dive
        -- to the closed border
        { turn_to = 3.8397, hold = "forward", seconds = 3 },
        { turn_to = 4.3633, hold = "forward", seconds = 3 },
        { turn_to = 5.0615, hold = "forward", seconds = 3 },
        { turn_to = 4.4506, hold = "forward", seconds = 3 },
        { turn_to = 4.3633, hold = "forward", seconds = 3 },
        { turn_to = 4.4506, hold = "forward", seconds = 3 },
        { turn_to = 4.3633, hold = "forward", seconds = 3 },
        { turn_to = 4.4506, hold = "forward", seconds = 3 },
        { turn_to = 4.5379, hold = "forward", seconds = 3 },
        { turn_to = 4.2761, hold = "forward", seconds = 3 },
        { turn_to = 4.4506, hold = "forward", seconds = 3 },
        { turn_to = 4.3633, hold = "forward", seconds = 3 },
        { turn_to = 4.4506, hold = "forward", seconds = 3 },
        { turn_to = 5.1487, hold = "forward", seconds = 2.85 },
        { hold = "forward", seconds = 1 },                  -- against the closed border
        { hold = "", seconds = 1 },
    },
}
