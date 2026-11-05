#!/usr/bin/env luajit
-- 102-loading-screen-matches-converter.lua - a converted map's loading
-- screen picture as world-edit-to-execute's converter writes it (its
-- src/wowbuild/loadscreen.lua, blp2) against what this client's picture
-- reader (src/017-blp.c) reads back from those bytes, copy by copy.
--
-- For a general audience: while a map loads, the client shows its
-- loading screen picture (src/101-loading-screen.c finds it). The
-- converter writes that picture as a BLP2 file of plain pixels with a
-- smaller copy at every half size. This has the converter's own writer
-- make such files from made pictures (square and not, with and without
-- see-through parts, larger than the file's size and smaller), then reads
-- each copy back through this client's reader and compares every pixel
-- with the picture the converter itself meant for that copy (its own
-- resample, the same steps its writer takes). So the two sides agree on
-- the file, from the converter's code rather than from either side's
-- memory of the format.
--
-- Usage: luajit tests/102-loading-screen-matches-converter.lua [PROJECT_DIR] [--build DIR] [--wete DIR]
--   PROJECT_DIR  the W client (default below)
--   --build DIR  where libwreaders.so and wreaders.h are (default PROJECT_DIR/tmp/tmp/build)
--   --wete DIR   a world-edit-to-execute tree (its src/ is enough: an
--                exported commit works). With none given, or one with no
--                src/wowbuild/loadscreen.lua, the check is skipped: said, exit 0.
-- Exit status 0 when every copy agrees (or skipped), 1 when one doesn't.

-- {{{ set_dir
local DIR = "/mnt/kaun/custom-client"
local WETE
local args, i = {}, 1
while i <= #arg do
    local a = arg[i]
    if a == "--build" then args.build = arg[i + 1]; i = i + 2
    elseif a == "--wete" then WETE = arg[i + 1]; i = i + 2
    else DIR = a; i = i + 1 end
end
args.build = args.build or (DIR .. "/tmp/tmp/build")
-- }}}

local function exists(path) local f = io.open(path, "rb"); if f then f:close() end; return f ~= nil end
if not WETE or not exists(WETE .. "/src/wowbuild/loadscreen.lua") then
    print("skipped: no world-edit-to-execute tree given (--wete DIR, with src/wowbuild/loadscreen.lua)")
    os.exit(0)
end

package.path = WETE .. "/src/?.lua;" .. WETE .. "/src/?/init.lua;" .. package.path
local ffi = require("ffi")
local loadscreen = require("wowbuild.loadscreen")
local W = dofile(DIR .. "/scripts/libs/wreaders.lua")
local L = W.load(args.build)

-- {{{ made pictures: w x h, each pixel from its place (alpha too when `see`)
local function made(w, h, see)
    local p = ffi.new("uint8_t[?]", w * h * 4)
    for y = 0, h - 1 do
        for x = 0, w - 1 do
            local d = (y * w + x) * 4
            p[d] = (x * 37 + y * 11) % 256
            p[d + 1] = (x * 5 + y * 53 + 17) % 256
            p[d + 2] = (x * y + 101) % 256
            p[d + 3] = see and ((x * 3 + y * 7) % 256) or 255
        end
    end
    return { width = w, height = h, rgba = ffi.string(p, w * h * 4) }
end
-- }}}

-- {{{ compare one picture
local function compare(name, img, size)
    local problems = {}
    local function say(fmt, ...) problems[#problems + 1] = string.format(fmt, ...) end
    local bytes, nlevels = loadscreen.blp2(img, size)
    local buf = ffi.new("unsigned char[?]", #bytes)
    ffi.copy(buf, bytes, #bytes)
    -- what the converter meant for each copy: its own resample, step by step as its writer goes
    local want = { loadscreen.resample(img, size) }
    local n = size
    while #want < nlevels do n = n / 2; want[#want + 1] = loadscreen.resample(want[#want], n) end
    local why = ffi.new("WWhy")
    for level = 0, nlevels - 1 do
        local out = ffi.new("BlpImage")
        if L.blp_decode_level(buf, #bytes, level, out, why) ~= 0 then
            say("copy %d: 017 refuses it: %s", level, ffi.string(why.text))
        else
            local w = want[level + 1]
            if out.width ~= w.width or out.height ~= w.height then
                say("copy %d: 017 reads %d x %d, the converter wrote %d x %d", level, out.width, out.height, w.width, w.height)
            else
                local got = ffi.string(out.rgba, out.width * out.height * 4)
                if got ~= w.rgba then
                    local at
                    for k = 1, #got do if got:byte(k) ~= w.rgba:byte(k) then at = k - 1; break end end
                    say("copy %d: pixel %d channel %d: 017 reads %d, the converter meant %d", level, math.floor(at / 4), at % 4, got:byte(at + 1), w.rgba:byte(at + 1))
                end
            end
            L.blp_free(out)
        end
    end
    local past = ffi.new("BlpImage")
    if L.blp_decode_level(buf, #bytes, nlevels, past, why) == 0 then
        L.blp_free(past)
        say("a copy past the converter's %d: 017 reads one", nlevels)
    end
    local whole = ffi.new("BlpImage")
    if L.blp_decode(buf, #bytes, whole, why) ~= 0 or whole.width ~= size then say("blp_decode (the full size, as 101 reads it): %s", ffi.string(why.text))
    else L.blp_free(whole) end
    return problems, nlevels
end
-- }}}

-- {{{ run
local CASES = {
    { "a 512 x 512 picture at the converter's size (its own, loadscreen.SIZE)", made(512, 512, false), loadscreen.SIZE },
    { "a wide picture, 300 x 200, with see-through parts, at 64", made(300, 200, true), 64 },
    { "a small picture, 3 x 5, grown to 16", made(3, 5, true), 16 },
    { "a 1 x 1 picture at 1", made(1, 1, true), 1 },
}
local failed = 0
for _, c in ipairs(CASES) do
    local p, nlevels = compare(c[1], c[2], c[3])
    if #p == 0 then
        print(string.format("  ok:   %s: all %d copies read back as the converter meant them", c[1], nlevels))
    else
        failed = failed + 1
        print(string.format("  FAIL: %s: %d disagreement%s", c[1], #p, #p == 1 and "" or "s"))
        for _, s in ipairs(p) do print("          " .. s) end
    end
end
print(string.format("agree: %d of %d pictures written by the converter's loadscreen.blp2", #CASES - failed, #CASES))
os.exit(failed == 0 and 0 or 1)
-- }}}
