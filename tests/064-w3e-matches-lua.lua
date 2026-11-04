#!/usr/bin/env luajit
-- 064-w3e-matches-lua.lua - WC3's ground, read and prepared by this
-- client's C reader and by world-edit-to-execute's Lua, compared field by
-- field.
--
-- For a general audience: Warcraft III's ground (a map's war3map.w3e) is
-- now read by two programs, the original one in Lua
-- (world-edit-to-execute's parsers/w3e.lua, with assets/ground.lua working
-- out which ground pictures each cell lays) and its C twin here (issue
-- 302a, src/063-w3e.c). This hands both the same files and compares
-- everything they give back: every field of every tile point, the
-- map's head, each point's heights in the world, the counts, every
-- question the Lua's reader answers, the float arrays the Lua hands its
-- renderer and the bytes of each cell's picture list. Grounds made here
-- come first, built by this script byte by byte (so the check runs with
-- no game files): one drawn by hand, random ones of many sizes, both
-- version numbers, every main tileset, odd numbers in the head, and
-- damaged copies both must refuse. Then strokes as an editor makes
-- them, redone by the C over their area only against the Lua laying the
-- whole ground again. Then every map (.w3x, .w3m) in a folder, read
-- through world-edit-to-execute's own archive reader.
--
-- Usage: luajit tests/064-w3e-matches-lua.lua [PROJECT_DIR] [--build DIR] [--wete DIR]
--                                             [--maps DIR] [--made-only]
--   PROJECT_DIR  the W client (default below)
--   --build DIR  where libwreaders.so and wreaders.h are (default PROJECT_DIR/tmp/tmp/build)
--   --wete DIR   world-edit-to-execute (default below)
--   --maps DIR   a folder of WC3 maps, searched with its folders (default:
--                world-edit-to-execute's assets/; an install's Maps folder works too)
--   --made-only  only the grounds made here
-- Exit status 0 when everything agrees. With maps compared, the report
-- also goes to PROJECT_DIR/tmp/shared-memory/w3e-matches-lua.txt.

-- {{{ set_dir
local DIR = "/mnt/kaun/custom-client"
local WETE = "/mnt/mtwo/programming/ai-stuff/world-edit-to-execute"
local args, i = {}, 1
while i <= #arg do
    local a = arg[i]
    -- a dispatch over the options: each takes the next word as its value
    if a == "--build" then args.build = arg[i + 1]; i = i + 2
    elseif a == "--wete" then WETE = arg[i + 1]; i = i + 2
    elseif a == "--maps" then args.maps = arg[i + 1]; i = i + 2
    elseif a == "--made-only" then args.made_only = true; i = i + 1
    else DIR = a; i = i + 1 end
end
args.build = args.build or (DIR .. "/tmp/tmp/build")
args.maps = args.maps or (WETE .. "/assets")
-- }}}

package.path = WETE .. "/src/?.lua;" .. WETE .. "/src/?/init.lua;" .. package.path
local ffi = require("ffi")
local bit = require("bit")
local w3e = require("parsers.w3e")
local ground = require("assets.ground")
local map_scene = require("demo.wc3map.scene")
local W = dofile(DIR .. "/scripts/libs/wreaders.lua")
local L = W.load(args.build)

-- {{{ comparing
-- Each comparison adds a line naming the first fields that differ; a
-- ground agrees when it adds none. Numbers are compared exactly (a float
-- read by either side is the same double); NaN agrees with NaN.
local diffs
local function differ(path, a, b)
    if #diffs < 12 then diffs[#diffs + 1] = string.format("%s: Lua %s, C %s", path, tostring(a), tostring(b)) end
    diffs.count = (diffs.count or 0) + 1
end
local function same(path, a, b)
    if type(a) == "boolean" then a = a and 1 or 0 end
    if type(a) == "number" and type(b) == "number" then
        if a == b or (a ~= a and b ~= b) then return end
    elseif a == b then return end
    differ(path, a, b)
end
local POINT_FIELDS = { "height_raw", "height", "water_raw", "water_level", "boundary", "water_flags", "ground_texture",
    "is_ramp", "is_blight", "has_water", "is_boundary", "texture_details", "cliff_variation", "layer_height", "cliff_texture" }

-- the Lua's extended table (0-based tileset number -> true) and the C's flags
local function extended_of(pattern, n)
    local lua, c = {}, ffi.new("uint8_t[?]", math.max(1, n))
    for k = 0, n - 1 do
        local e = pattern == "all" or (pattern == "alternate" and k % 2 == 0)
        lua[k] = e
        c[k] = e and 1 or 0
    end
    return lua, c
end

-- the Lua's laying of the whole ground: the bytes ground.lay hands the
-- renderer, through a stand-in renderer that only keeps them
local function lua_lay(t, lua_ext)
    local got = {}
    local used = ground.lay({ land_tiles = function(ids, cells) got.cells = cells; return 0 end }, t, {}, lua_ext)
    local view = { terrain = t, sample = map_scene.sampler(t) }
    local heights, _, water = map_scene.terrain_arrays(view)
    return got.cells, used, heights, water
end

-- the preparation: the Lua's laying against the C's prepared arrays
local function compare_prep(t, cp, lua_ext, label)
    local cells, used, heights, water = lua_lay(t, lua_ext)
    local np, nc = t.width * t.height, (t.width - 1) * (t.height - 1)
    same(label .. " used", used, cp.used)
    if cells ~= ffi.string(cp.cells, nc * 8) then
        local c = ffi.string(cp.cells, nc * 8)
        for k = 1, #cells do
            if cells:byte(k) ~= c:byte(k) then
                local cell = math.floor((k - 1) / 8)
                differ(string.format("%s cell (%d, %d) byte %d", label, cell % (t.width - 1), math.floor(cell / (t.width - 1)), (k - 1) % 8),
                    cells:byte(k), c:byte(k))
                break
            end
        end
    end
    if heights ~= ffi.string(cp.ground_z, np * 4) then differ(label .. " ground heights (floats)", "its array", "another") end
    if water ~= ffi.string(cp.water_z, np * 4) then differ(label .. " water heights (floats)", "its array", "another") end
end

-- {{{ local function compare: one ground read both ways, field by field
local function compare(t, g)
    same("version", t.version, g.version)
    same("tileset_code", t.tileset_code, string.char(bit.band(g.tileset_code, 255)))
    same("tileset", t.tileset, ffi.string(g.tileset))
    same("custom_tileset", t.custom_tileset, g.custom_tileset)
    same("ground tileset count", #t.ground_tilesets, g.nground)
    for k = 1, math.min(#t.ground_tilesets, g.nground) do same("ground tileset " .. k, t.ground_tilesets[k], ffi.string(g.ground_ids + 4 * (k - 1), 4)) end
    same("cliff tileset count", #t.cliff_tilesets, g.ncliff)
    for k = 1, math.min(#t.cliff_tilesets, g.ncliff) do same("cliff tileset " .. k, t.cliff_tilesets[k], ffi.string(g.cliff_ids + 4 * (k - 1), 4)) end
    same("width", t.width, g.width)
    same("height", t.height, g.height)
    same("offset_x", t.offset_x, g.offset_x)
    same("offset_y", t.offset_y, g.offset_y)
    if t.width ~= g.width or t.height ~= g.height then return end
    -- every point, and every question asked of it, one outside the grid too
    for y = -1, t.height do
        for x = -1, t.width do
            local tp, cp = t:get_tile(x, y), L.w3e_point(g, x, y)
            local at = string.format("(%d, %d)", x, y)
            if (tp == nil) ~= (cp == nil) then differ("point " .. at, tp and "there" or "none", cp ~= nil and "there" or "none")
            elseif tp then
                for _, f in ipairs(POINT_FIELDS) do same(at .. "." .. f, tp[f], cp[f]) end
                same(at .. " ground_z", w3e.ground_z(tp), L.w3e_ground_z(cp))
                same(at .. " water_z", w3e.water_z(tp), L.w3e_water_z(cp))
                same(at .. " is_wet", w3e.is_wet(tp), L.w3e_is_wet(cp))
            end
            local name, cname = t:get_texture_name(x, y), L.w3e_texture_name(g, x, y)
            same(at .. " texture name", name or "none", cname ~= nil and ffi.string(cname, 4) or "none")
            same(at .. " is_walkable", t:is_walkable(x, y), L.w3e_walkable(g, x, y))
        end
    end
    -- places, to and from the grid
    local wx, wy = ffi.new("double[1]"), ffi.new("double[1]")
    for _, p in ipairs({ { 0, 0 }, { t.width - 1, t.height - 1 }, { 3, 7 }, { -2, 5 } }) do
        local lw = t:tile_to_world(p[1], p[2])
        L.w3e_tile_to_world(g, p[1], p[2], wx, wy)
        same("tile_to_world x", lw.x, wx[0]); same("tile_to_world y", lw.y, wy[0])
    end
    for _, p in ipairs({ { 0, 0 }, { -1, -1 }, { 1000.5, -2000.25 }, { 127.999, 128 }, { -1e9, 1e9 }, { t.offset_x + 300, t.offset_y + 129 } }) do
        local lt = t:world_to_tile(p[1], p[2])
        L.w3e_world_to_tile(g, p[1], p[2], wx, wy)
        same("world_to_tile x", lt.x, wx[0]); same("world_to_tile y", lt.y, wy[0])
    end
    -- the counts
    local ls, cs = t:stats(), ffi.new("W3eStats")
    L.w3e_stats(g, cs)
    same("stats total", ls.total_tilepoints, cs.total)
    same("stats water", ls.water_count, cs.water)
    same("stats blight", ls.blight_count, cs.blight)
    same("stats ramp", ls.ramp_count, cs.ramp)
    same("stats boundary", ls.boundary_count, cs.boundary)
    same("stats min height", ls.min_height, cs.min_height)
    same("stats max height", ls.max_height, cs.max_height)
    for k = 0, 15 do same("stats layer " .. k, ls.layer_counts[k] or 0, cs.layers[k]) end
end
-- }}}

-- {{{ local function check_one: a file both ways -> "same", "refused", "known" (Lua reads, C refuses) or "differ"
local function check_one(bytes, patterns)
    diffs = {}
    local ok, t = pcall(w3e.parse, bytes)
    local pp, why = ffi.new("W3eGround *[1]"), ffi.new("WWhy")
    local e = L.w3e_read(bytes, #bytes, pp, why)
    if not ok and e ~= 0 then return "refused", { "Lua: " .. tostring(t), "C: " .. ffi.string(why.text) } end
    if ok and e ~= 0 then return "known", { "C: " .. ffi.string(why.text) } end
    if not ok then L.w3e_free(pp[0]); return "differ", { "Lua refused: " .. tostring(t), "C read it" } end
    local g = pp[0]
    compare(t, g)
    for _, pat in ipairs(patterns or { "none", "alternate", "all" }) do
        local lua_ext, c_ext = extended_of(pat, #t.ground_tilesets)
        local prep = ffi.new("W3ePrep *[1]")
        if L.w3e_prep(g, c_ext, #t.ground_tilesets, prep, why) ~= 0 then
            differ("prep (" .. pat .. ")", "laid", ffi.string(why.text))
        else
            local lok, err = pcall(compare_prep, t, prep[0], lua_ext, "prep (" .. pat .. " extended)")
            if not lok then differ("prep (" .. pat .. ")", "error: " .. tostring(err), "laid") end
            L.w3e_prep_free(prep[0])
        end
    end
    L.w3e_free(g)
    local lines = diffs
    if (diffs.count or 0) > 0 then return "differ", lines end
    return "same", lines
end
-- }}}

-- {{{ the grounds made here
local function u32(v) v = v % 4294967296; return string.char(v % 256, math.floor(v / 256) % 256, math.floor(v / 65536) % 256, math.floor(v / 16777216)) end
local function u16(v) v = v % 65536; return string.char(v % 256, math.floor(v / 256)) end
local function f32(v) local b = ffi.new("float[1]", v); return ffi.string(b, 4) end
local function head(version, code, ground_ids, cliff_ids, w, h, ox, oy, raw_ox, raw_oy)
    return "W3E!" .. u32(version) .. code .. u32(1) .. u32(#ground_ids / 4) .. ground_ids .. u32(#cliff_ids / 4) .. cliff_ids
        .. u32(w) .. u32(h) .. (raw_ox or f32(ox)) .. (raw_oy or f32(oy))
end
local function pt(raw_h, water, tex, marks, details, cvar, layer, ctex)
    return u16(raw_h) .. u16(water) .. string.char(tex + marks, details + cvar * 32, layer + ctex * 16)
end

-- the hand-drawn ground of tests/044-test-client.c's 302a block (the same bytes)
local A_TEX = { { 0, 0, 1, 1, 2 }, { 0, 1, 1, 2, 2 }, { 0, 0, 0, 2, 2 }, { 1, 1, 0, 0, 0 } }
local A_DETAILS = { { 0, 5, 7, 0, 9 }, { 3, 0, 20, 1, 2 }, { 16, 4, 6, 11, 3 }, { 0, 0, 0, 0, 0 } }
local function fixture_a(version)
    local out = { head(version, "L", "LdrtLgrsLrok", "CLdi", 5, 4, -256, -192) }
    for y = 0, 3 do
        for x = 0, 4 do
            local raw, marks, layer, ctex, cvar, water = 8192 + 4 * (x * 10 + y * 3), 0, 2, 15, 0, 8192
            if x == 4 and y == 0 then raw = -100 end
            if x == 0 and y == 0 then marks = marks + 0x20 end
            if x == 1 and y == 1 then marks = marks + 0x40; water = 8192 + 4 * 200 end
            if x == 2 and y == 1 then marks = marks + 0x40; water = 8192 + 4 * 10 end
            if x == 0 and y == 3 then marks = marks + 0x80 end
            if x == 4 and y == 3 then water = water + 0x4000 end
            if x == 2 and y == 3 then water = water + 0xC000 end
            if (x == 3 and y == 3) or (x == 4 and y == 3) or (x == 4 and y == 2) then layer = 3 end
            if x == 3 and y == 3 then marks = marks + 0x10 end
            if x == 4 and y == 2 then cvar, ctex = 5, 0 end
            out[#out + 1] = pt(raw, water, A_TEX[y + 1][x + 1], marks, A_DETAILS[y + 1][x + 1], cvar, layer, ctex)
        end
    end
    return table.concat(out)
end

-- a ground of random points (every bit), its tileset numbers within its
-- list unless `past` (then any of 16), levels 2 to 4 where `cliffs`
local function fixture_random(seed, version, code, w, h, nground, past, cliffs)
    local s = seed
    local function rand() s = (s * 1103515245 + 12345) % 2147483648; return math.floor(s / 65536) end
    local ids = {}
    for k = 0, nground - 1 do ids[#ids + 1] = string.format("T%xab", k) end
    local out = { head(version, code, table.concat(ids), "CLdiCLgr", w, h, (rand() % 8000) - 4000 + 0.5, -(rand() % 8000) + 0.25) }
    for _ = 1, w * h do
        local b = {}
        for k = 1, 7 do b[k] = rand() % 256 end
        if not past then b[5] = bit.band(b[5], 0xF0) + (b[5] % 16) % nground end
        if rand() % 4 ~= 0 then b[5] = bit.band(b[5], bit.bnot(0x20)) end
        if cliffs then b[7] = bit.band(b[7], 0xF0) + 2 + rand() % 3 end
        out[#out + 1] = string.char(b[1], b[2], b[3], b[4], b[5], b[6], b[7])
    end
    return table.concat(out)
end

local CODES = "ABCDFGIJKLNOQVWXYZ"
local function made_grounds()
    local a11, a12 = fixture_a(11), fixture_a(12)
    local list = {
        { "the hand-drawn ground, version 11", a11, "same" },
        { "the hand-drawn ground, version 12 (read with 11's layout by both)", a12, "same" },
        { "the hand-drawn ground with bytes after its last point", a11 .. "trailing bytes", "same" },
        { "a single point", head(11, "L", "Ldrt", "", 1, 1, 0, 0) .. pt(8192, 8192, 0, 0, 0, 0, 2, 15), "same" },
        { "a ground one point wide", head(11, "L", "LdrtLgrs", "", 1, 9, 0, 0) .. fixture_random(5, 11, "L", 1, 9, 2):sub(-63), "same" },
        { "no tilesets listed at all", head(11, "L", "", "", 3, 3, 0, 0) .. string.rep(pt(8192, 8192, 0, 0, 0, 0, 2, 15), 9), "same" },
        { "odd numbers in the corner (NaN, a denormal)", head(11, "L", "Ldrt", "", 2, 2, 0, 0, "\0\0\192\127", "\1\0\0\0")
            .. string.rep(pt(8192, 8192, 0, 0, 0, 0, 2, 15), 4), "same" },
        { "odd numbers in the corner (infinity, minus 0)", head(11, "L", "Ldrt", "", 2, 2, 0, 0, "\0\0\128\255", "\0\0\0\128")
            .. string.rep(pt(8192, 8192, 0, 0, 0, 0, 2, 15), 4), "same" },
        { "heights and water at their ends (0, 32767, -32768; 0 and 16383)", head(11, "L", "Ldrt", "", 2, 2, 0, 0)
            .. pt(0, 0, 0, 0x40, 0, 0, 0, 0) .. pt(32767, 16383 + 0xC000, 0, 0x40, 31, 7, 15, 15)
            .. pt(-32768, 0x8000, 0, 0xF0, 0, 0, 0, 15) .. pt(8192, 8192, 0, 0, 0, 0, 2, 15), "same" },
    }
    -- random grounds: sizes, versions, tileset counts, cliffs
    local sizes = { { 2, 2 }, { 3, 7 }, { 7, 3 }, { 17, 17 }, { 33, 25 }, { 65, 65 }, { 129, 97 } }
    for k, sz in ipairs(sizes) do
        local nground = 1 + (k * 5) % 16
        list[#list + 1] = { string.format("random %d x %d, %d tilesets, cliffs", sz[1], sz[2], nground),
            fixture_random(k * 7919, 11, "L", sz[1], sz[2], nground, false, true), "same" }
        list[#list + 1] = { string.format("random %d x %d, version 12, every bit random", sz[1], sz[2]),
            fixture_random(k * 104729, 12, "Z", sz[1], sz[2], 16, false, false), "same" }
        list[#list + 1] = { string.format("random %d x %d, tileset numbers past a list of 3", sz[1], sz[2]),
            fixture_random(k * 31, 11, "A", sz[1], sz[2], 3, true, true), "same" }
    end
    -- every main tileset, and letters that aren't one
    for k = 1, #CODES do
        list[#list + 1] = { "main tileset " .. CODES:sub(k, k), fixture_random(k, 11, CODES:sub(k, k), 4, 3, 4, false, true), "same" }
    end
    list[#list + 1] = { "main tileset P (unknown)", fixture_random(3, 11, "P", 4, 3, 4, false, true), "same" }
    list[#list + 1] = { "main tileset l (lower case: unknown)", fixture_random(3, 11, "l", 4, 3, 4, false, true), "same" }
    -- damaged: both refuse
    list[#list + 1] = { "not a ground (W3X!)", "W3X!" .. a11:sub(5), "refused" }
    list[#list + 1] = { "an empty file", "", "refused" }
    for cut = 0, #a11 - 1 do list[#list + 1] = { "cut to " .. cut .. " bytes", a11:sub(1, cut), "refused", quiet = true } end
    list[#list + 1] = { "100000 tilesets listed in a short file", "W3E!" .. u32(11) .. "L" .. u32(0) .. u32(100000) .. "abcd", "refused" }
    list[#list + 1] = { "more points than bytes (65536 x 65536)", head(11, "L", "", "", 65536, 65536, 0, 0) .. pt(0, 0, 0, 0, 0, 0, 2, 15), "refused" }
    -- known differences (063-w3e.h): the Lua reads these, the C refuses them
    list[#list + 1] = { "a ground tileset count of -1", "W3E!" .. u32(11) .. "L" .. u32(0) .. u32(-1) .. u32(0)
        .. u32(2) .. u32(2) .. f32(0) .. f32(0) .. string.rep(pt(8192, 8192, 0, 0, 0, 0, 2, 15), 4), "known" }
    list[#list + 1] = { "a ground 0 points wide", head(11, "L", "", "", 0, 3, 0, 0), "known" }
    list[#list + 1] = { "a ground -1 x -1 points", head(11, "L", "", "", -1, -1, 0, 0) .. pt(8192, 8192, 0, 0, 0, 0, 2, 15), "known" }
    return list
end

local function made_run()
    local n, good, by = 0, 0, {}
    for _, case in ipairs(made_grounds()) do
        n = n + 1
        local outcome, lines = check_one(case[2])
        by[outcome] = (by[outcome] or 0) + 1
        local ok = outcome == case[3]
        if ok then good = good + 1 end
        if not ok or not case.quiet then
            print(string.format("  %s %s (%s)", ok and "ok:  " or "FAIL:", case[1], outcome))
        end
        if not ok or outcome == "known" then for _, l in ipairs(lines) do print("        " .. l) end end
    end
    print(string.format("%d of %d made grounds as expected: %d identical, %d refused by both, %d known differences, %d disagreeing",
        good, n, by.same or 0, by.refused or 0, by.known or 0, by.differ or 0))
    return good == n
end
-- }}}

-- {{{ local function strokes: an editor's strokes, the C redoing only their area
-- On a ground read both ways, change a rectangle of points the same way in
-- both, then lay the whole ground again in Lua and redo the area in C:
-- the bytes must agree after every stroke.
local function strokes(bytes, count, seed, label)
    local t = w3e.parse(bytes)
    local pp, why = ffi.new("W3eGround *[1]"), ffi.new("WWhy")
    assert(L.w3e_read(bytes, #bytes, pp, why) == 0, ffi.string(why.text))
    local g = pp[0]
    local n = #t.ground_tilesets
    local lua_ext, c_ext = extended_of("alternate", n)
    local prep = ffi.new("W3ePrep *[1]")
    assert(L.w3e_prep(g, c_ext, n, prep, why) == 0, ffi.string(why.text))
    local s = seed
    local function rand(m) s = (s * 1103515245 + 12345) % 2147483648; return math.floor(s / 65536) % m end
    local good = 0
    for k = 1, count do
        diffs = {}
        local x0, y0 = rand(t.width + 4) - 2, rand(t.height + 4) - 2
        local x1, y1 = x0 + rand(6), y0 + rand(6)
        for y = math.max(0, y0), math.min(t.height - 1, y1) do
            for x = math.max(0, x0), math.min(t.width - 1, x1) do
                local tp, cp = t.tilepoints[y][x], g.points + (y * t.width + x)
                local tex, layer, details = rand(math.max(1, n)), 2 + rand(3), rand(32)
                local ramp, blight, water = rand(5) == 0, rand(7) == 0, rand(2) == 0
                local height, level = rand(800) / 4 - 100, rand(800) / 4 - 100
                tp.ground_texture, cp.ground_texture = tex, tex
                tp.layer_height, cp.layer_height = layer, layer
                tp.texture_details, cp.texture_details = details, details
                tp.is_ramp, cp.is_ramp = ramp, ramp and 1 or 0
                tp.is_blight, cp.is_blight = blight, blight and 1 or 0
                tp.has_water, cp.has_water = water, water and 1 or 0
                tp.height, cp.height = height, height
                tp.water_level, cp.water_level = level, level
            end
        end
        local e = L.w3e_prep_area(prep[0], g, x0, y0, x1, y1, nil, why)
        if e ~= 0 and e ~= L.W_ERR_REFUSED then differ("stroke " .. k, "redone", ffi.string(why.text)) end
        compare_prep(t, prep[0], lua_ext, "stroke " .. k)
        if (diffs.count or 0) == 0 then good = good + 1
        else for _, l in ipairs(diffs) do print("        " .. l) end end
    end
    L.w3e_prep_free(prep[0])
    L.w3e_free(g)
    print(string.format("  %s %s: %d of %d strokes, the C's area against the Lua's whole ground", good == count and "ok:  " or "FAIL:", label, good, count))
    return good == count
end
-- }}}

-- {{{ local function maps_run: every map in the folder, its war3map.w3e both ways
local function maps_run(lines)
    -- the folder and every folder in it (an install's Maps\ holds several)
    local p = io.popen(string.format("cd %q 2>/dev/null && find . -type f \\( -iname '*.w3x' -o -iname '*.w3m' \\) | sort", args.maps))
    local names = {}
    for f in p:lines() do names[#names + 1] = f:gsub("^%./", "") end
    p:close()
    if #names == 0 then return nil end
    local mpq = require("mpq")
    local n, same_, refused, differ_ = 0, 0, 0, 0
    for _, f in ipairs(names) do
        local a, err = mpq.open(args.maps .. "/" .. f)
        local bytes = a and a:has("war3map.w3e") and a:extract("war3map.w3e")
        if a then a:close() end
        if bytes then
            n = n + 1
            local outcome, ls = check_one(bytes)
            local t = w3e.parse(bytes)
            local line = string.format("%s\t%s\t%d x %d tile points, %d ground tilesets, version %d", outcome, f, t.width, t.height,
                #t.ground_tilesets, t.version)
            if outcome == "same" then same_ = same_ + 1
            elseif outcome == "refused" then refused = refused + 1
            else differ_ = differ_ + 1; line = line .. "\t" .. table.concat(ls, "; ") end
            lines[#lines + 1] = line
        else
            lines[#lines + 1] = string.format("unread\t%s\t%s", f, a and "no war3map.w3e in it" or tostring(err))
        end
    end
    lines[#lines + 1] = string.format("%d WC3 maps' grounds (%s): %d identical, %d refused by both, %d disagreeing",
        n, args.maps, same_, refused, differ_)
    return differ_ == 0 and n > 0
end
-- }}}

-- {{{ local function main
local function main()
    print("grounds made here")
    local made_ok = made_run()
    print("an editor's strokes")
    local strokes_ok = strokes(fixture_a(11), 300, 17, "the hand-drawn ground")
    strokes_ok = strokes(fixture_random(4242, 11, "L", 40, 33, 9, false, true), 300, 5, "a random 40 x 33 ground") and strokes_ok
    if args.made_only then return (made_ok and strokes_ok) and 0 or 1 end
    local lines = {}
    local maps_ok = maps_run(lines)
    if maps_ok == nil then
        print("no WC3 maps (.w3x, .w3m) in " .. args.maps .. ": only the grounds made here were compared")
        return (made_ok and strokes_ok) and 0 or 1
    end
    local report = table.concat(lines, "\n") .. "\n"
    io.write(report)
    local path = DIR .. "/tmp/shared-memory/w3e-matches-lua.txt"
    local f = io.open(path, "w")
    if not f then io.stderr:write("WARNING: the report could not be kept at " .. path .. " (run scripts/build, which makes tmp/)\n"); return 1 end
    f:write(report); f:close()
    return (made_ok and strokes_ok and maps_ok) and 0 or 1
end
-- }}}

os.exit(main())
