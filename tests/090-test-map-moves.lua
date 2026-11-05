-- 090-test-map-moves.lua (issue 1109, step 1n) - the project's own moves
-- for a headless Test Map run
--
-- For a general audience: a test map can run with no window
-- (w-client --test-map FOLDER --receipt FILE --headless THIS_FILE). The
-- client walks her through these moves, one fixed step at a time, and
-- writes where she ended and what it found into its goodbye receipt; two
-- runs of the same build with these moves must write the same counts
-- (tests/044-test-client.c checks it). Data only, read by the client's
-- data-only reader (src/089-headless-run.h has the format): each move
-- holds its keys (forward, back, left, right, strafe_left, strafe_right,
-- jump, swim_down) for its seconds.
return {
    format = 1,
    name   = "round the start",
    step   = 0.05,
    moves  = {
        { hold = "", seconds = 1 },                          -- standing: she settles on the ground
        { hold = "forward", seconds = 4 },                   -- a run ahead
        { hold = "forward left", seconds = 1 },              -- curving left
        { hold = "forward jump", seconds = 1 },              -- a jump on the run
        { hold = "forward", seconds = 2 },
        { hold = "right", seconds = 1.5 },                   -- turning on the spot
        { hold = "strafe_left", seconds = 1.5 },
        { hold = "strafe_right", seconds = 1 },
        { hold = "back", seconds = 2 },
        { hold = "forward", seconds = 3, walking = true },   -- walking, not running
        { hold = "forward right", seconds = 2 },
        { hold = "forward swim_down", seconds = 2 },         -- in water she dives; on land it is a run
        { hold = "jump", seconds = 0.5 },                    -- a standing jump
        { hold = "", seconds = 1.5 },                        -- landed, standing
    },
}
