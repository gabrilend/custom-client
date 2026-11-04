#!/usr/bin/env luajit
-- 070-mdx-anim-matches-lua.lua - WC3's models posed by this client's C
-- (src/069-mdx-anim.c) and by world-edit-to-execute's Lua
-- (assets/anim.lua), compared matrix by matrix.
--
-- For a general audience: a Warcraft III model moves by its skeleton,
-- and two programs now work out where every bone is at a given moment:
-- the original in Lua (world-edit-to-execute's anim.lua, which hands its
-- renderer a "pose") and its C twin here (issue 402 step 4). This hands
-- both the same models and asks both for the same sequences at many
-- moments (at keys, between them, before a sequence and past its end, on
-- several clocks for the animations that loop on their own), and compares
-- everything that comes back: every node's matrix, every matrix the
-- points follow, every piece's opacity, every attachment point's place.
-- Models made here come first, built byte by byte (so the check runs with
-- no game files); then, when WC3's own archives are there, every model in
-- them, split across every processor core as tests/062 does.
--
-- How close: exactly. Both work in doubles, step for step in the same
-- order, so every node's matrix is compared bit for bit (NaN agreeing
-- with NaN), and the floats the drawing gets (the groups' matrices and the
-- alphas) too. The one place the two could part is the sine and arc
-- cosine of a turn along the sphere (each calls its own maths library);
-- the report counts any value that isn't identical, and the check passes
-- only when none is.
--
-- Where the C is meant to differ (decided in issue 402), a model is
-- "decided" rather than compared: the Lua's own order breaks (a parent
-- loop, or parents more than 64 deep, which its guard cuts), or a geoset
-- the C poses that the Lua leaves standing (more matrix groups than its
-- renderer's 128, or a geoset with nothing drawn).
--
-- Usage: luajit tests/070-mdx-anim-matches-lua.lua [PROJECT_DIR] [--build DIR] [--wete DIR] [--made-only]
--                                                  [--workers N] [--limit N]
--   PROJECT_DIR  the W client (default below)
--   --build DIR  where libwreaders.so and wreaders.h are (default PROJECT_DIR/tmp/tmp/build)
--   --wete DIR   world-edit-to-execute (default below)
--   --made-only  only the models made here
--   --workers N  how many processes share WC3's models (default: the core count)
--   --limit N    only the first N models of each archive (a quick look)
-- Exit status 0 when every model agrees. With WC3's archives the report
-- also goes to PROJECT_DIR/tmp/shared-memory/mdx-anim-matches-lua.txt.
-- (internal: --part I N runs one worker and prints its counts)

-- {{{ set_dir
local DIR = "/mnt/kaun/custom-client"
local WETE = "/mnt/mtwo/programming/ai-stuff/world-edit-to-execute"
local ARCHIVES = { "war3.mpq", "War3x.mpq" }   -- WC3's base and Frozen Throne art
local args, i = {}, 1
while i <= #arg do
    local a = arg[i]
    -- a dispatch over the options: each takes the next word as its value
    if a == "--workers" then args.workers = tonumber(arg[i + 1]); i = i + 2
    elseif a == "--limit" then args.limit = tonumber(arg[i + 1]); i = i + 2
    elseif a == "--build" then args.build = arg[i + 1]; i = i + 2
    elseif a == "--wete" then WETE = arg[i + 1]; i = i + 2
    elseif a == "--made-only" then args.made_only = true; i = i + 1
    elseif a == "--part" then args.part, args.nparts = tonumber(arg[i + 1]), tonumber(arg[i + 2]); i = i + 3
    else DIR = a; i = i + 1 end
end
args.build = args.build or (DIR .. "/tmp/tmp/build")
-- }}}

package.path = WETE .. "/src/?.lua;" .. WETE .. "/src/?/init.lua;" .. package.path
local ffi = require("ffi")
local mdx = require("parsers.mdx")
local gpu = require("assets.gpu")
local anim = require("assets.anim")
local bw = require("parsers.binwrite")
local W = dofile(DIR .. "/scripts/libs/wreaders.lua")
local L = W.load(args.build)

-- {{{ comparing
-- Each disagreement adds a line (the first dozen are kept); counts says
-- how many values were compared.
local diffs, counts
local function differ(path, a, b)
    if #diffs < 12 then diffs[#diffs + 1] = string.format("%s: Lua %s, C %s", path, tostring(a), tostring(b)) end
    diffs.count = (diffs.count or 0) + 1
end
local function same(path, a, b)
    counts.values = counts.values + 1
    if a == b or (a ~= a and b ~= b) then return true end
    if type(a) == "number" and type(b) == "number" then
        local d = math.abs(a - b)
        if d > counts.worst then counts.worst = d end
    end
    differ(path, a, b)
    return false
end
-- }}}

-- {{{ the Lua's rig, through gpu.lua's build with stand-ins that find every picture (as tests/062 does)
local function lua_rig(lm)
    local render = {
        tex_create = function() return {} end,
        mesh_create = function(verts, idx, skin) return { skin = skin } end,
        model_create = function(parts) return 1 end,
    }
    local assets = { texture = function(self, path) return { width = 1, height = 1, rgba = "p" } end }
    local cache = gpu.new(render, assets)
    local id = cache:build(lm, "made")
    if not id then return nil, {} end
    local rig = cache:rig(id)
    for k, gi in ipairs(cache.meta[id].skins) do rig.skins[k].gi = gi end   -- each skin's geoset (0-based), gpu.lua's list
    return rig, cache.meta[id].parts
end
-- }}}

-- {{{ local function structure: the posing order, parents, pivots, groups.
-- Returns nil to compare the poses, or the reason the model is decided.
local function structure(lm, cm, rig, meta, cr)
    -- the Lua's own order breaking: a parent read before it is posed (a
    -- loop), or a parent its depth guard dropped (more than 64 deep)
    for k, nd in ipairs(rig.nodes) do
        if nd.parent and nd.parent >= k then return "a parent loop (the Lua reads a parent before posing it)" end
    end
    local depth = {}
    for k = 0, cr.nnodes - 1 do
        local p = cr.nodes[k].parent
        depth[k] = p >= 0 and depth[p] + 1 or 0
        if depth[k] > 64 then return "parents more than 64 deep (the Lua's guard cuts the chain where its walk began)" end
    end
    same("node count", #rig.nodes, cr.nnodes)
    if #rig.nodes ~= cr.nnodes then return nil end
    for k, nd in ipairs(rig.nodes) do
        local c, p = cr.nodes[k - 1], "nodes[" .. k .. "]"
        same(p .. ".id", nd.id, c.id)
        same(p .. ".parent", nd.parent and nd.parent - 1 or -1, c.parent)
        for j = 1, 3 do same(p .. ".pivot[" .. j .. "]", nd.pivot[j], c.pivot[j - 1]) end
        same(p .. ".attachment", nd.attach and 1 or 0, c.attachment)
        same(p .. ".name", nd.name, ffi.string(cm.nodes[c.node].name))
    end
    same("on the clock", (rig.global_nodes or rig.global_parts) and 1 or 0, cr.on_clock)
    same("units", #rig.parts, cr.nunits)
    -- skins: every Lua skin is a geoset the C poses with the same groups;
    -- a geoset only the C poses is decided when the Lua's renderer limit
    -- or its drawing is why
    local skinned = {}
    for _, sk in ipairs(rig.skins) do skinned[sk.gi] = sk end
    local drawn = {}
    for _, p in ipairs(meta) do drawn[p.geoset] = true end
    for gi = 0, cm.ngeosets - 1 do
        local sp = cr.geoset_groups[gi]
        local sk = skinned[gi]
        local p = "geosets[" .. gi .. "]"
        if sk then
            same(p .. " groups", #sk.groups, sp.count)
            for k, members in ipairs(sk.groups) do
                if k <= sp.count then
                    local g = cr.groups[cr.group_of[sp.first + k - 1]]
                    same(p .. ".groups[" .. k .. "] size", #members, g.count)
                    for j = 1, math.min(#members, g.count) do
                        same(p .. ".groups[" .. k .. "][" .. j .. "]", members[j] - 1, cr.members[g.first + j - 1])
                    end
                end
            end
        elseif sp.count > 0 then
            if cm.geosets[gi].group_sizes.count > anim.MAX_GROUPS or not drawn[gi] then
                counts.decided_geosets = counts.decided_geosets + 1
            else
                differ(p, "standing", "posed")
            end
        end
    end
    return nil
end
-- }}}

-- {{{ local function poses: both at every moment asked, compared
local wv, wvc, av, pv, out3 = nil, nil, nil, nil, ffi.new("double[3]")   -- wvc: wv as the const pointer the C takes
local function poses(lm, cm, rig, cr, seq, frames, clocks)
    local why = ffi.new("WWhy")
    for _, clock in ipairs(clocks) do
        for _, frame in ipairs(frames) do
            local s = anim.pose_at(rig, seq, frame, clock)
            local e = L.mdx_rig_pose(cr, seq - 1, frame, clock, wv, av, why)
            local p = string.format("%s at %s, clock %s", lm.sequences[seq].name, tostring(frame), tostring(clock))
            if s == nil or e ~= 0 then
                same(p .. " posed", s ~= nil, e == 0)
            else
                counts.poses = counts.poses + 1
                local Wl = rig.world
                for k = 0, cr.nnodes - 1 do
                    for c = 0, 11 do same(p .. " node " .. (k + 1) .. "[" .. c .. "]", Wl[k * 12 + c], wv[k][c]) end
                end
                local f = ffi.cast("const float *", s)
                for k = 0, cr.nunits - 1 do same(p .. " alpha " .. (k + 1), f[k], av[k]) end
                L.mdx_rig_groups(cr, wvc, pv)
                for _, sk in ipairs(rig.skins) do
                    local sp = cr.geoset_groups[sk.gi]
                    for k = 1, math.min(#sk.groups, sp.count) do
                        local g = cr.group_of[sp.first + k - 1]
                        local o = sk.offset + (k - 1) * 12
                        for row = 0, 2 do
                            for col = 0, 3 do
                                same(p .. " geoset " .. sk.gi .. " group " .. k .. "[" .. row .. "][" .. col .. "]",
                                     f[o + row * 4 + col], pv[g][col * 4 + row])
                            end
                        end
                    end
                end
            end
        end
    end
end
-- the attachment points, through the Lua's playhead (anim.pose), whose
-- frame and clock are whole steps of anim.QUANTUM: chosen as such here
local function attachments(lm, cm, rig, cr, seq)
    local sq = lm.sequences[seq]
    local q = anim.QUANTUM
    local why = ffi.new("WWhy")
    for _, t in ipairs({ 0, 5 * q, math.floor((sq.finish - sq.start) / q / 2) * q, sq.finish - sq.start + 10 * q }) do
        for _, clk in ipairs({ 0, 39 * q }) do
            local st = { seq = seq, t = t, clock = clk, name = "x" }
            if anim.pose(rig, st) then
                local frame = math.min(sq.start + math.floor(t / q + 0.5) * q, sq.finish)
                local clock = (rig.global_nodes or rig.global_parts) and math.floor(clk / q) * q or 0
                L.mdx_rig_pose(cr, seq - 1, frame, clock, wv, nil, why)
                for _, nd in ipairs(rig.nodes) do
                    if nd.attach then
                        local lp = anim.attachment(rig, st, nd.id)
                        local got = L.mdx_rig_attachment(cr, wvc, nd.id, out3)
                        local p = string.format("attachment %d in %s at %d", nd.id, sq.name, frame)
                        same(p .. " found", lp ~= nil, got == 1)
                        if lp and got == 1 then for c = 1, 3 do same(p .. "[" .. c .. "]", lp[c], out3[c - 1]) end end
                        counts.attachments = counts.attachments + 1
                    end
                end
            end
        end
    end
end
-- }}}

-- {{{ local function check_one: one model's bytes, posed by both
-- "same", "decided" (with why), "unread" (the readers refuse it: 062's
-- to compare) or "differ" with the lines saying where. `thorough` asks
-- for more moments (the made models).
local function moments(lm, rig, seq, thorough)
    local sq = lm.sequences[seq]
    local frames, seen = {}, {}
    local function add(f) if not seen[f] then seen[f] = true; frames[#frames + 1] = f end end
    add(sq.start - 50); add(sq.start); add(sq.finish); add(sq.finish + 250)
    local steps = thorough and 12 or 6
    for j = 1, steps - 1 do add(sq.start + (sq.finish - sq.start) * j / steps) end
    if thorough then
        for _, nd in ipairs(rig.nodes) do
            for _, t in ipairs({ nd.T or false, nd.R or false, nd.S or false }) do
                if t then for _, k in ipairs(t.keys) do
                    if k.frame >= sq.start - 1 and k.frame <= sq.finish + 1 then add(k.frame); add(k.frame + 0.5); add(k.frame - 1) end
                end end
            end
        end
    end
    local clocks = thorough and { 0, 250, 777.25, 1500, 4321.5 } or { 0, 777.25 }
    if not (rig.global_nodes or rig.global_parts) then clocks = { 0 } end
    return frames, clocks
end

local function check_one(bytes, thorough)
    diffs, counts = {}, { values = 0, poses = 0, attachments = 0, worst = 0, decided_geosets = 0 }
    local ok, lm = pcall(mdx.parse, bytes)
    local cm, why = ffi.new("MdxModel"), ffi.new("WWhy")
    local e = L.mdx_read(bytes, #bytes, cm, why)
    if not ok or e ~= 0 then
        if e == 0 then L.mdx_free(cm) end
        return "unread", { "Lua: " .. (ok and "read" or tostring(lm)), "C: " .. (e == 0 and "read" or ffi.string(why.text)) }, counts
    end
    local cr = ffi.new("MdxRig")
    local built = L.mdx_rig_build(cm, cr, why)
    local rok, rig, meta = pcall(lua_rig, lm)
    local outcome
    if built ~= 0 then
        differ("rig", rok and "built" or tostring(rig), ffi.string(why.text))
    elseif not rok then
        differ("rig", tostring(rig), "built")
    elseif not rig then
        -- nothing drawable: the Lua has no rig; the C's nodes still pose (nothing to compare)
        outcome = "same"
    else
        local decided = structure(lm, cm, rig, meta, cr)
        if decided then
            outcome = "decided"
            diffs.decided = decided
        elseif (diffs.count or 0) == 0 then
            wv = ffi.new("double[?][12]", math.max(1, cr.nnodes))
            wvc = ffi.cast("const double (*)[12]", wv)
            av = ffi.new("float[?]", math.max(1, cr.nunits))
            pv = ffi.new("float[?][16]", math.max(1, cr.ngroups))
            for seq = 1, #lm.sequences do
                local frames, clocks = moments(lm, rig, seq, thorough)
                poses(lm, cm, rig, cr, seq, frames, clocks)
                attachments(lm, cm, rig, cr, seq)
            end
            -- a sequence the model hasn't: neither poses
            same("a sequence past the last posed", anim.pose_at(rig, #lm.sequences + 1, 0, 0) ~= nil,
                 L.mdx_rig_pose(cr, #lm.sequences, 0, 0, wv, av, why) == 0)
        end
    end
    if built == 0 then L.mdx_rig_free(cr) end
    L.mdx_free(cm)
    if not outcome then outcome = (diffs.count or 0) == 0 and "same" or "differ" end
    return outcome, diffs, counts
end
-- }}}

-- {{{ models made here
-- Writers for the format's pieces (as tests/062's): a chunk is its tag,
-- its byte count and its body; a record with a size of its own counts itself.
local function u32(v) return bw.new():u32(v):done() end
local function i32(v) return bw.new():i32(v):done() end
local function f32(v) return bw.new():f32(v):done() end
local function u16(v) return bw.new():u16(v):done() end
local function fl(t) local b = bw.new() for _, v in ipairs(t) do b:f32(v) end return b:done() end
local function name(s, len) return s .. string.rep("\0", len - #s) end
local function chunk(tag, body) return tag .. u32(#body) .. body end
local function sized(body) return u32(#body + 4) .. body end
local function ext(r) return fl({ r, -r, -r, -r, r, r, r }) end
-- a track: tag, way, global (-1 none), keys { frame, value (number or table), in, out }
local function track(tag, way, global, keys)
    local parts = { tag, u32(#keys), u32(way), i32(global) }
    local function v(x) if type(x) == "table" then return fl(x) end return f32(x) end
    for _, k in ipairs(keys) do
        parts[#parts + 1] = i32(k[1]) .. v(k[2])
        if way > 1 then parts[#parts + 1] = v(k[3] or k[2]) .. v(k[4] or k[2]) end
    end
    return table.concat(parts)
end
local function node(nm, id, parent, flags, trk) return sized(name(nm, 80) .. i32(id) .. i32(parent) .. u32(flags) .. (trk or "")) end

-- a model from a description: seqs { name, start, finish, non_looping },
-- globals, materials { { layers = { { filter, alpha, tracks } } } },
-- geosets { v (points), groups (each point's), sizes, mats, material },
-- geoas { alpha, geoset, tracks }, bones / helpers / attachments
-- { name, id, parent, flags, tracks }, npivots
local function build(d)
    local out = { "MDLX", chunk("VERS", u32(800)) }
    out[#out + 1] = chunk("MODL", name("Posed", 80) .. name("", 260) .. ext(150) .. u32(150))
    local sb = {}
    for _, q in ipairs(d.seqs or {}) do
        sb[#sb + 1] = name(q[1], 80) .. u32(q[2]) .. u32(q[3]) .. f32(0) .. u32(q[4] and 1 or 0) .. f32(0) .. u32(0) .. ext(100)
    end
    if #sb > 0 then out[#out + 1] = chunk("SEQS", table.concat(sb)) end
    if d.globals then
        local g = {}
        for _, v in ipairs(d.globals) do g[#g + 1] = u32(v) end
        out[#out + 1] = chunk("GLBS", table.concat(g))
    end
    out[#out + 1] = chunk("TEXS", u32(0) .. name("Textures\\A.blp", 260) .. u32(0) .. u32(0) .. name("Textures\\B.blp", 260) .. u32(0))
    if d.materials then
        local mb = {}
        for _, mat in ipairs(d.materials) do
            local lb = {}
            for _, ly in ipairs(mat.layers) do
                lb[#lb + 1] = sized(u32(ly[1]) .. u32(0) .. u32(0) .. i32(-1) .. u32(0) .. f32(ly[2]) .. (ly[3] or ""))
            end
            mb[#mb + 1] = sized(i32(0) .. u32(0) .. "LAYS" .. u32(#lb) .. table.concat(lb))
        end
        out[#out + 1] = chunk("MTLS", table.concat(mb))
    end
    if d.geosets then
        local gb = {}
        for _, g in ipairs(d.geosets) do
            local nv = #g.v / 3
            local n = {}
            for k = 1, nv do n[#n + 1] = 0; n[#n + 1] = 0; n[#n + 1] = 1 end
            local corners = {}
            for k = 0, nv - nv % 3 - 1 do corners[#corners + 1] = u16(k) end
            local vg = {}
            for _, k in ipairs(g.groups) do vg[#vg + 1] = string.char(k) end
            local sizes, mats = {}, {}
            for _, k in ipairs(g.sizes) do sizes[#sizes + 1] = u32(k) end
            for _, k in ipairs(g.mats) do mats[#mats + 1] = u32(k) end
            local uv = {}
            for k = 1, nv * 2 do uv[k] = 0 end
            gb[#gb + 1] = sized(table.concat({
                "VRTX" .. u32(nv) .. fl(g.v), "NRMS" .. u32(nv) .. fl(n),
                "PTYP" .. u32(1) .. u32(4), "PCNT" .. u32(1) .. u32(#corners), "PVTX" .. u32(#corners) .. table.concat(corners),
                "GNDX" .. u32(#vg) .. table.concat(vg), "MTGC" .. u32(#sizes) .. table.concat(sizes), "MATS" .. u32(#mats) .. table.concat(mats),
                u32(g.material) .. u32(0) .. u32(0), ext(50), u32(0), "UVAS" .. u32(1) .. "UVBS" .. u32(nv) .. fl(uv),
            }))
        end
        out[#out + 1] = chunk("GEOS", table.concat(gb))
    end
    if d.geoas then
        local ab = {}
        for _, a in ipairs(d.geoas) do ab[#ab + 1] = sized(f32(a[1]) .. u32(0) .. fl({ 1, 1, 1 }) .. u32(a[2]) .. (a[3] or "")) end
        out[#out + 1] = chunk("GEOA", table.concat(ab))
    end
    local function nodes(list, tail)
        local b = {}
        for _, n in ipairs(list) do b[#b + 1] = tail(node(n[1], n[2], n[3], n[4] or 0, n[5]), n) end
        return table.concat(b)
    end
    if d.bones then out[#out + 1] = chunk("BONE", nodes(d.bones, function(s) return s .. i32(-1) .. i32(-1) end)) end
    if d.helpers then out[#out + 1] = chunk("HELP", nodes(d.helpers, function(s) return s end)) end
    if d.attachments then
        out[#out + 1] = chunk("ATCH", nodes(d.attachments, function(s, n) return sized(s .. name("", 260) .. u32(n[2])) end))
    end
    if d.events then out[#out + 1] = chunk("EVTS", nodes(d.events, function(s) return s end)) end
    local piv = {}
    for k = 0, (d.npivots or 0) - 1 do piv[#piv + 1] = fl({ k * 3 + 1, -k * 2, k * 5 - 4 }) end
    out[#out + 1] = chunk("PIVT", table.concat(piv))
    return table.concat(out)
end

local s = math.sqrt(0.5)
local near_z = math.sqrt(1 - 0.99955 ^ 2)            -- two turns this close go in a straight line, not along the sphere
local NaN = 0 / 0
local SEQS = { { "Stand 1", 0, 1000 }, { "Walk", 1100, 2100 }, { "Death", 3100, 4000, true }, { "Attack - 2", 4100, 4600 },
               { "Spell", 5000, 5000 }, { "Decay", 6000, 7000, true } }

-- One model with every way between keys, on every track a node has
local function every_way()
    local q1, q2, q3 = { 0, 0, s, s }, { s, 0, 0, s }, { 0, 0, 1, 0 }
    -- a geoset of 130 points, each its own group: past the Lua renderer's 128
    local big = { v = {}, groups = {}, sizes = {}, mats = {}, material = 0 }
    for k = 0, 129 do
        big.v[#big.v + 1] = k; big.v[#big.v + 1] = k % 7; big.v[#big.v + 1] = 1
        big.groups[#big.groups + 1] = k
        big.sizes[#big.sizes + 1] = 1
        big.mats[#big.mats + 1] = k % 9
    end
    return build({
        seqs = SEQS, globals = { 1500, 0, 333 }, npivots = 12,
        materials = {
            { layers = { { 2, 0.8, track("KMTA", 3, -1, { { 0, 0.2, 0.1, 0.5 }, { 1000, 0.9, 1.5, 0.25 }, { 3100, 1 }, { 3900, 0, -1, 2 } }) },
                         { 0, 1 } } },
            { layers = { { 1, 0.5, track("KMTA", 1, 0, { { 0, 0 }, { 1500, 1 } }) } } },
        },
        geosets = {
            { v = { 0, 0, 0, 10, 0, 0, 10, 10, 0, 0, 10, 0 }, groups = { 0, 1, 2, 2 }, sizes = { 1, 2, 3 }, mats = { 0, 1, 2, 2, 3, 4 }, material = 0 },
            -- an empty group, a member no node has, a member twice, a group the first geoset has too
            { v = { 0, 0, 5, 5, 0, 5, 5, 5, 5, 1, 1, 1, 2, 2, 2, 3, 3, 3 }, groups = { 0, 1, 2, 3, 4, 1 }, sizes = { 1, 2, 0, 2, 2 },
              mats = { 4, 1, 2, 99, 5, 5, 5 }, material = 1 },
            -- a point's group past the geoset's groups: stands as made
            { v = { 1, 2, 3, 4, 5, 6, 7, 8, 9 }, groups = { 0, 1, 2 }, sizes = { 1, 1 }, mats = { 0, 1 }, material = 1 },
            big,
            -- no such material: one plain layer; a member a helper and a later bone share
            { v = { 3, 3, 3, 4, 4, 4, 5, 5, 3 }, groups = { 0, 0, 1 }, sizes = { 1, 3 }, mats = { 6, 7, 8, 9 }, material = 9 },
            -- fewer group numbers than points: stands as made
            { v = { 6, 6, 6, 7, 7, 7, 8, 8, 6 }, groups = { 0 }, sizes = { 1 }, mats = { 0 }, material = 0 },
        },
        geoas = {
            { 0.25, 0 },                                                                         -- overruled by the next
            { 1, 0, track("KGAO", 1, -1, { { 0, 1 }, { 3100, 1 }, { 3500, 0 }, { 6000, 0 }, { 6500, 0.5 } }) },
            { 1, 1, track("KGAO", 0, 0, { { 0, 1 }, { 750, 0 } }) },
            { 0.5, 4 },
            { 1, 5, track("KGAO", 1, -1, { { 0, NaN }, { 1000, 1 } }) },
        },
        bones = {
            { "root", 0, -1, 256, track("KGTR", 1, -1, { { 0, { 0, 0, 0 } }, { 1000, { 10, 0, 0 } }, { 1099, { 50, 0, 0 } }, { 1100, { 0, 0, 0 } }, { 2100, { 100, 0, 0 } } })
                .. track("KGRT", 0, -1, { { 3100, q1 }, { 3500, q2 } })
                .. track("KGSC", 2, -1, { { 0, { 1, 1, 1 }, { 0, 0, 0 }, { 2, 2, 2 } }, { 1000, { 2, 2, 2 }, { 1, 0, 1 }, { 0, 0, 0 } } }) },
            { "spine", 1, 0, 256, track("KGRT", 1, -1, { { 0, { 0, 0, 0, 1 } }, { 500, q1 }, { 1000, q3 }, { 1100, { 0, 0, 0, 1 } },
                                                          { 1102, { 0, 0, near_z, 0.99955 } }, { 1103, { 0, 0, 0.6, -0.8 } }, { 2100, q2 } })
                .. track("KGTR", 3, -1, { { 0, { 0, 0, 0 }, { 0, 0, 0 }, { 5, 5, 0 } }, { 1000, { 0, 0, 20 }, { 1, 2, 3 }, { 0, 0, 0 } },
                                          { 4100, { 1, 1, 1 } }, { 4600, { -1, -1, -1 }, { 3, 3, 3 }, { -3, 0, 3 } } }) },
            { "chest", 2, 1, 256, track("KGRT", 2, -1, { { 0, { 0, 0, 0, 1 } }, { 1000, q2 }, { 1100, q1 }, { 2100, { 0, s, 0, s } } })
                .. track("KGSC", 3, -1, { { 1100, { 1, 1, 1 }, { 0.5, 0.5, 0.5 }, { 3, 1, 1 } }, { 2100, { 1, 2, 3 }, { 1, 1, 1 }, { 1, 1, 1 } } }) },
            { "arm", 3, 2, 256, track("KGRT", 1, 2, { { 0, { 0, 0, 0, 1 } }, { 111, q1 }, { 333, q3 } })
                .. track("KGTR", 1, 1, { { 0, { 1, 2, 3 } }, { 10, { 4, 5, 6 } } }) },
            -- a NaN key; repeated keys; keys out of order
            { "hand", 4, 3, 256, track("KGTR", 1, -1, { { 0, { 0, 0, 0 } }, { 500, { NaN, 1, 1 } }, { 1000, { 0, 0, 0 } } })
                .. track("KGSC", 1, -1, { { 1100, { 1, 1, 1 } }, { 1500, { 2, 1, 1 } }, { 1500, { 3, 1, 1 } }, { 1300, { 1, 4, 1 } }, { 2100, { 1, 1, 1 } } })
                .. track("KGRT", 1, -1, { { 0, { 0, 0, 0, 0 } }, { 1000, { 0, 0, 0, 0 } } }) },
            -- billboarded (WC3 turns it to the camera; both turn it with its parent)
            { "finger", 5, 4, 256 + 8, track("KGRT", 1, -1, { { 0, q1 }, { 1000, q2 }, { 1100, q3 }, { 6000, q1 }, { 7000, q2 } }) },
            { "tail", 6, 99, 256, track("KGTR", 0, -1, { { -100, { 0, 0, 3 } }, { 50, { 0, 0, 6 } } }) },
            -- keys out of order across sequences: each sequence's are found by reading them all
            { "itself", 7, 7, 256, track("KGSC", 1, -1, { { 0, { 1, 1, 1 } }, { 7000, { 3, 3, 3 } } })
                .. track("KGRT", 1, -1, { { 0, q1 }, { 6500, q2 }, { 500, q3 }, { 1000, q1 }, { 1500, q2 } }) },
            { "shared", 9, 0, 256 },
        },
        helpers = {
            { "hand ref", 8, 4, 0, track("KGTR", 1, -1, { { 0, { 0, 0, 0 } }, { 4000, { 0, 0, 40 } } }) },
            { "shared", 9, 5, 0, track("KGRT", 1, -1, { { 0, { 0, 0, 0, 1 } }, { 7000, q3 } }) },    -- a later kind wins id 9
        },
        attachments = { { "Origin Ref", 10, 0, 2048 }, { "Hand Right Ref", 11, 4, 2048 }, { "Overhead Ref", 12, 5, 2048 } },
    })
end

-- parents round in a loop: the Lua's order reads one before posing it
local function loop()
    return build({ seqs = SEQS, npivots = 3, bones = {
        { "a", 0, 1, 256, track("KGTR", 1, -1, { { 0, { 0, 0, 0 } }, { 1000, { 1, 0, 0 } } }) },
        { "b", 1, 0, 256, track("KGRT", 1, -1, { { 0, { 0, 0, 0, 1 } }, { 1000, { 0, 0, s, s } } }) },
        { "c", 2, 1, 256 } },
        materials = { { layers = { { 0, 1 } } } },
        geosets = { { v = { 0, 0, 0, 1, 0, 0, 0, 1, 0 }, groups = { 0, 0, 0 }, sizes = { 1 }, mats = { 2 }, material = 0 } } })
end

-- a chain of parents `n` deep
local function chain(n)
    local bones = {}
    for k = 0, n - 1 do
        bones[#bones + 1] = { "link " .. k, k, k - 1, 256,
            track("KGRT", 1, -1, { { 0, { 0, 0, 0, 1 } }, { 1000, { 0, 0, 0.1, math.sqrt(0.99) } } }) .. track("KGTR", 1, -1, { { 0, { 0, 0, 0 } }, { 1000, { 0.5, 0, 0 } } }) }
    end
    return build({ seqs = SEQS, npivots = n, bones = bones, materials = { { layers = { { 0, 1 } } } },
        geosets = { { v = { 0, 0, 0, 1, 0, 0, 0, 1, 0 }, groups = { 0, 1, 2 }, sizes = { 1, 1, 2 }, mats = { 0, n - 1, n - 1, 3 }, material = 0 } } })
end

local function made_models()
    return {
        { "every way between keys, a global sequence, six deep, billboarded", every_way(), "same" },
        { "a skeleton nine deep", chain(9), "same" },
        { "parents round in a loop", loop(), "decided" },
        { "parents seventy deep", chain(70), "decided" },
        { "nodes but no sequences", build({ npivots = 1, bones = { { "only", 0, -1, 256 } }, materials = { { layers = { { 0, 1 } } } },
            geosets = { { v = { 0, 0, 0, 1, 0, 0, 0, 1, 0 }, groups = { 0, 0, 0 }, sizes = { 1 }, mats = { 0 }, material = 0 } } }), "same" },
        { "sequences but no nodes", build({ seqs = SEQS, materials = { { layers = { { 0, 1 } } } },
            geosets = { { v = { 0, 0, 0, 1, 0, 0, 0, 1, 0 }, groups = { 0, 0, 0 }, sizes = { 1 }, mats = { 3 }, material = 0 } } }), "same" },
        { "an empty model (MDLX alone)", "MDLX", "same" },
    }
end
-- }}}

-- {{{ local function made_run: the models made here, each to its expected outcome
local function made_run()
    local n, good, values, posed, att, decided_g, worst = 0, 0, 0, 0, 0, 0, 0
    for _, case in ipairs(made_models()) do
        n = n + 1
        local outcome, lines, c = check_one(case[2], true)
        values, posed, att, decided_g = values + c.values, posed + c.poses, att + c.attachments, decided_g + c.decided_geosets
        if c.worst > worst then worst = c.worst end
        local ok = outcome == case[3]
        if ok then good = good + 1 end
        print(string.format("  %s %s (%s%s; %d poses, %d values)", ok and "ok:  " or "FAIL:", case[1], outcome,
            lines.decided and (": " .. lines.decided) or "", c.poses, c.values))
        if not ok or outcome == "differ" then for _, l in ipairs(lines) do print("        " .. l) end end
    end
    print(string.format("%d of %d made models as expected (%d poses, %d attachment points, %d values compared, every one identical%s; %d geoset%s posed only by the C, as decided)",
        good, n, posed, att, values, worst > 0 and string.format(" but for differences up to %g", worst) or "", decided_g, decided_g == 1 and "" or "s"))
    return good == n
end
-- }}}

-- {{{ local function worker: one share of WC3's models, every nparts-th
local function worker(part, nparts)
    local mpq = require("mpq")
    local n, same_, decided, unread, differ_ = 0, 0, 0, 0, 0
    for _, an in ipairs(ARCHIVES) do
        local a = mpq.open(WETE .. "/wc3-installs/frozen-throne/" .. an)
        local k = 0
        for _, f in ipairs(a:list()) do
            if f:lower():match("%.mdx$") then
                k = k + 1
                if args.limit and k > args.limit then break end
                if k % nparts == part then
                    local bytes = a:extract(f)
                    n = n + 1
                    local outcome, lines = check_one(bytes, false)
                    if outcome == "same" then same_ = same_ + 1
                    elseif outcome == "decided" then decided = decided + 1; print(string.format("decided\t%s\t%s\t%s", an, f, lines.decided))
                    elseif outcome == "unread" then unread = unread + 1
                    else differ_ = differ_ + 1; print(string.format("differ\t%s\t%s\t%s", an, f, table.concat(lines, "; "))) end
                end
            end
        end
        a:close()
    end
    print(string.format("counts\t%d\t%d\t%d\t%d\t%d", n, same_, decided, unread, differ_))
end
-- }}}

-- {{{ local function main
local function main()
    if args.part then worker(args.part, args.nparts); return 0 end
    print("models made here")
    local made_ok = made_run()
    local have = io.open(WETE .. "/wc3-installs/frozen-throne/" .. ARCHIVES[1], "rb")
    if have then have:close() end
    if args.made_only or not have then
        if not have and not args.made_only then
            print("WC3's archives aren't at " .. WETE .. "/wc3-installs/frozen-throne/: only the models made here were compared")
        end
        return made_ok and 0 or 1
    end
    local p = io.popen("nproc")
    local nparts = args.workers or tonumber(p:read("*l"))
    p:close()
    local pipes = {}
    for part = 0, nparts - 1 do
        pipes[#pipes + 1] = io.popen(string.format("luajit %q %q --build %q --wete %q --part %d %d%s", arg[0], DIR, args.build, WETE,
            part, nparts, args.limit and (" --limit " .. args.limit) or ""))
    end
    local n, same_, decided, unread, differ_, lines, reported = 0, 0, 0, 0, 0, {}, 0
    for _, pipe in ipairs(pipes) do
        for line in pipe:lines() do
            local a, b, c, d, e = line:match("^counts\t(%d+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)$")
            if a then
                n, same_, decided, unread, differ_ = n + a, same_ + b, decided + c, unread + d, differ_ + e
                reported = reported + 1
            else
                lines[#lines + 1] = line
            end
        end
        pipe:close()
    end
    if reported ~= nparts then
        lines[#lines + 1] = string.format("ERROR: %d of %d workers reported (a worker failed; its error is above)", reported, nparts)
    end
    lines[#lines + 1] = string.format("%d WC3 models (MDX, %s): %d posed identically, %d decided, %d not read by both (tests/062's), %d disagreeing",
        n, table.concat(ARCHIVES, " and "), same_, decided, unread, differ_)
    local report = table.concat(lines, "\n") .. "\n"
    io.write(report)
    local path = DIR .. "/tmp/shared-memory/mdx-anim-matches-lua.txt"
    local f = io.open(path, "w")
    if not f then io.stderr:write("WARNING: the report could not be kept at " .. path .. " (run scripts/build, which makes tmp/)\n"); return 1 end
    f:write(report); f:close()
    return (made_ok and differ_ == 0 and reported == nparts and n > 0) and 0 or 1
end
-- }}}

os.exit(main())
